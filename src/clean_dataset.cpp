#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <algorithm>
#include <queue>
#include <filesystem>

namespace fs = std::filesystem;

#pragma pack(push, 1)
struct PositionRecord {
    uint8_t cells[18];
    uint8_t tuzdeks[2];
    uint8_t scores[2];

    // Оператор сравнения для сортировки и дедупликации
    bool operator<(const PositionRecord& other) const {
        return std::lexicographical_compare(cells, cells + 18, other.cells, other.cells + 18) ||
               (std::equal(cells, cells + 18, other.cells) && 
                (std::tie(tuzdeks[0], tuzdeks[1], scores[0], scores[1]) < 
                 std::tie(other.tuzdeks[0], other.tuzdeks[1], other.scores[0], other.scores[1])));
    }

    bool operator==(const PositionRecord& other) const {
        return std::equal(cells, cells + 18, other.cells) &&
               tuzdeks[0] == other.tuzdeks[0] && tuzdeks[1] == other.tuzdeks[1] &&
               scores[0] == other.scores[0] && scores[1] == other.scores[1];
    }
    
    bool operator>(const PositionRecord& other) const { return other < *this; }
};
#pragma pack(pop)

// Структура для управления чтением из временных файлов во время слияния
struct MergeNode {
    PositionRecord record;
    int chunk_idx;

    bool operator>(const MergeNode& other) const {
        return record > other.record; // Для min-heap в priority_queue
    }
};

void sort_and_deduplicate(const std::string& input_path, const std::string& output_path) {
    const size_t MEMORY_LIMIT = 1024 * 1024 * 1024; // 1 ГБ RAM
    const size_t RECORDS_PER_CHUNK = MEMORY_LIMIT / sizeof(PositionRecord);
    
    std::ifstream in(input_path, std::ios::binary);
    if (!in) { std::cerr << "Cannot open input file\n"; return; }

    std::vector<std::string> temp_files;
    std::vector<PositionRecord> buffer;
    buffer.reserve(RECORDS_PER_CHUNK);

    std::cout << "Step 1: Splitting and sorting chunks...\n";
    
    int chunk_counter = 0;
    while (in) {
        buffer.clear();
        for (size_t i = 0; i < RECORDS_PER_CHUNK; ++i) {
            PositionRecord rec;
            if (!in.read(reinterpret_cast<char*>(&rec), sizeof(PositionRecord))) break;
            buffer.push_back(rec);
        }
        
        if (buffer.empty()) break;

        std::sort(buffer.begin(), buffer.end());
        
        std::string temp_name = "chunk_" + std::to_string(chunk_counter++) + ".tmp";
        std::ofstream out_temp(temp_name, std::ios::binary);
        out_temp.write(reinterpret_cast<char*>(buffer.data()), buffer.size() * sizeof(PositionRecord));
        temp_files.push_back(temp_name);
        std::cout << "Sorted chunk " << chunk_counter << " saved.\n";
    }
    in.close();

    std::cout << "Step 2: Merging and removing duplicates...\n";

    std::priority_queue<MergeNode, std::vector<MergeNode>, std::greater<MergeNode>> pq;
    std::vector<std::ifstream> streams(temp_files.size());
    
    for (int i = 0; i < temp_files.size(); ++i) {
        streams[i].open(temp_files[i], std::ios::binary);
        PositionRecord rec;
        if (streams[i].read(reinterpret_cast<char*>(&rec), sizeof(PositionRecord))) {
            pq.push({rec, i});
        }
    }

    std::ofstream final_out(output_path, std::ios::binary);
    PositionRecord last_written;
    bool first = true;
    uint64_t unique_count = 0;

    while (!pq.empty()) {
        MergeNode top = pq.top();
        pq.pop();

        if (first || !(top.record == last_written)) {
            final_out.write(reinterpret_cast<char*>(&top.record), sizeof(PositionRecord));
            last_written = top.record;
            first = false;
            unique_count++;
        }

        // Подгружаем следующий элемент из того же файла
        PositionRecord next_rec;
        if (streams[top.chunk_idx].read(reinterpret_cast<char*>(&next_rec), sizeof(PositionRecord))) {
            pq.push({next_rec, top.chunk_idx});
        }
    }

    // Очистка
    for (auto& s : streams) s.close();
    for (const auto& f : temp_files) fs::remove(f);

    std::cout << "Success! Unique positions saved: " << unique_count << "\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <input_binary_file>\n";
        return 1;
    }

    std::string input = argv[1];
    std::string output = "unique_" + input;

    auto start = std::chrono::steady_clock::now();
    sort_and_deduplicate(input, output);
    auto end = std::chrono::steady_clock::now();

    auto diff = std::chrono::duration_cast<std::chrono::seconds>(end - start).count();
    std::cout << "Total time: " << diff << " seconds.\n";

    return 0;
}