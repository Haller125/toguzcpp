#include <iostream>
#include <fstream>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <random>
#include <cstring>
#include "toguznative.h"

#define GAME_CLASS ToguzNative
#define KMAXTURNS 5000

struct PositionRecord{
    u_int8_t data[22]; // 18 байт для клеток, 2 байта для туздеков, 2 байта для счетов
};

uint32_t rng_state = 123456789;

bool cpu_supports_avx2() {
    #if (defined(__x86_64__) || defined(__i386__)) && (defined(__GNUC__) || defined(__clang__))
        return __builtin_cpu_supports("avx2");
    #else
        return false;
    #endif
}

inline uint32_t xorshift32(uint32_t& state) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

inline uint32_t fast_rand_bound(uint32_t random_val, uint32_t range) {
    return static_cast<uint32_t>((static_cast<uint64_t>(random_val) * range) >> 32);
}


std::uint64_t play_single_game() {
    GAME_CLASS game;
    bool player = false;
    std::uint32_t turn_count = 0;

    std::array<u_int8_t, 9> legal_moves{};
    int legal_count = 0;

    while (turn_count < 5000) {
        game.get_legal_moves(player, legal_moves, legal_count);

        if (legal_count == 0) {
            for (int i = 9 - (player * 9); i < 18 - (player * 9); ++i) {
                game.scores[1 - player] += game.cells[i];
                game.cells[i] = 0;
            }
            break;
        }

       
        uint32_t r = xorshift32(rng_state);
        const u_int8_t move_idx = legal_moves[fast_rand_bound(r, legal_count)];
        game.move(static_cast<u_int8_t>(move_idx + (player ? 9 : 0)));

        // check if game is over
        if (game.scores[0] >= 82 || game.scores[1] >= 82) {
                break;
            }

        player = !player;
        ++turn_count;

    }

    return turn_count;
}



class DatasetGenerator {
public:
    DatasetGenerator(const std::string& filename, uint64_t target_count)
        : out_file(filename, std::ios::binary), total_target(target_count), global_count(0) {
        init_masks(); // Инициализация AVX масок один раз
    }

    void start(int thread_count) {
        std::vector<std::jthread> workers;
        auto start_time = std::chrono::steady_clock::now();

        for (int i = 0; i < thread_count; ++i) {
            workers.emplace_back(&DatasetGenerator::worker_thread, this, i);
        }

        // Поток мониторинга скорости
        while (global_count < total_target) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            auto now = std::chrono::steady_clock::now();
            double elapsed = std::chrono::duration<double>(now - start_time).count();
            uint64_t current = global_count.load();
            std::cout << "\rProgress: " << current << " / " << total_target  << "|\t " << static_cast<float>  (current)/total_target
                      << " | Speed: " << static_cast<uint64_t>(current / elapsed) << " pos/sec    " << std::flush;
        }
        std::cout << "\nDone!" << std::endl;
    }

private:
    void worker_thread(int thread_id) {
        const size_t buffer_capacity = 100'000; // 100к позиций в буфере (~2.2 МБ)
        std::vector<PositionRecord> buffer;
        buffer.reserve(buffer_capacity);

        uint32_t seed = std::random_device{}() + thread_id;

        while (global_count < total_target) {
            ToguzNative game;
            bool player = false;
            std::array<uint8_t, 9> moves;
            int move_count;
            int turns;


            // Играем партию до конца
            while (true) {
                game.get_legal_moves(player, moves, move_count);
                if (move_count == 0 || game.scores[0] >= 82 || game.scores[1] >= 82) break;

                uint32_t r = xorshift32(seed);
                const u_int8_t move_idx = moves[fast_rand_bound(r, move_count)];
                game.move(static_cast<u_int8_t>(move_idx + (player ? 9 : 0)));
                seed = r;

                // Сохраняем позицию в буфер
                if (turns >= 6) { // Сохраняем позиции, начиная с 7-го хода
                    PositionRecord rec;
                    std::memcpy(rec.data, game.cells.data(), 18);
                    rec.data[18] = game.tuzdeks[0];
                    rec.data[19] = game.tuzdeks[1];
                    rec.data[20] = game.scores[0];
                    rec.data[21] = game.scores[1];
                    buffer.push_back(rec);
                }
                if (buffer.size() >= buffer_capacity) {
                    flush_buffer(buffer);
                }
                
                player = !player;
                turns++;
            }
        }
    }

    void flush_buffer(std::vector<PositionRecord>& buffer) {
        std::lock_guard<std::mutex> lock(file_mutex);
        out_file.write(reinterpret_cast<const char*>(buffer.data()), buffer.size() * sizeof(PositionRecord));
        global_count += buffer.size();
        buffer.clear();
    }

    std::ofstream out_file;
    std::mutex file_mutex;
    std::atomic<uint64_t> global_count;
    uint64_t total_target;
};

int main(int argc, char* argv[]) {

    // first argument: number of positions to generate, default 1 million
    // second argument: output filename, default "toguz_data.bin"

    uint64_t positions_to_generate = 1000000; // default 1 million
    std::string filename = "toguz_data.bin";
    if (argc > 1) {
        const int parsed = std::atoi(argv[1]);
        if (parsed > 0) {
            positions_to_generate = static_cast<std::uint64_t>(parsed);
        }
        if (argc > 2) {
            filename = argv[2];
        }
    }
    DatasetGenerator gen(filename, positions_to_generate);
    gen.start(std::thread::hardware_concurrency());

    return 0;
}