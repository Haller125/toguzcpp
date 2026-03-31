#include "toguznative.h"

#include <algorithm>
#include <immintrin.h>
#include <cstring>
#include <random>

#define HASH_SEED 123456789

alignas(32) u_int8_t REMAINDER_MASKS[18][18][32];



namespace {

inline void sow_scalar(std::array<u_int8_t, 32>& cells, u_int8_t idx, int full_rounds, int remainder) {
    // Скалярная версия тоже должна работать с готовыми кругами, чтобы не делать лишних циклов
    for (int i = 0; i < 32; ++i) {
        if (i < 18) cells[i] += full_rounds;
    }
    for (int i = 0; i < remainder; ++i) {
        cells[(idx + i) % 18] += 1;
    }
}

#if defined(__AVX2__)
// Передаем full_rounds и remainder напрямую, чтобы не вычислять их дважды
inline void sow_avx2(std::array<u_int8_t, 32>& cells, u_int8_t idx, int full_rounds, int remainder) {
    u_int8_t* p_cells = cells.data(); // Избавляемся от абстракций std::array
    
    __m256i v_cells = _mm256_load_si256(reinterpret_cast<const __m256i*>(p_cells));
    __m256i v_full = _mm256_set1_epi8(static_cast<char>(full_rounds));
    __m256i v_rem = _mm256_load_si256(reinterpret_cast<const __m256i*>(REMAINDER_MASKS[idx][remainder]));

    v_cells = _mm256_add_epi8(v_cells, v_full);
    v_cells = _mm256_add_epi8(v_cells, v_rem);

    _mm256_store_si256(reinterpret_cast<__m256i*>(p_cells), v_cells);
}
#endif

inline void sow_cells(std::array<u_int8_t, 32>& cells, u_int8_t idx, int full_rounds, int remainder) {
#if defined(__AVX2__)
    sow_avx2(cells, idx, full_rounds, remainder);
#else
    sow_scalar(cells, idx, full_rounds, remainder);
#endif
}

}// namespace

bool toguznative_compiled_with_avx2() {
#if defined(__AVX2__)
    return true;
#else
    return false;
#endif
}

void init_masks() {
    std::memset(REMAINDER_MASKS, 0, sizeof(REMAINDER_MASKS));
    
    for (int idx = 0; idx < 18; ++idx) {
        for (int rem = 0; rem < 18; ++rem) {
            // Имитируем распределение для каждого возможного случая
            for (int i = 0; i < rem; ++i) {
                int target = (idx + i) % 18;
                REMAINDER_MASKS[idx][rem][target] = 1;
            }
        }
    }
}

ToguzNative::ToguzNative(){
        this->cells.fill(9);
        this->tuzdeks.fill(NO_TUZDEK);
        this->scores.fill(0);
}

void ToguzNative::move(u_int8_t idx) {
    int pits = this->cells[idx];
    this->cells[idx] = 0;

    if (pits == 1) {
        const u_int8_t next_idx = (idx + 1) >= 18 ? (idx + 1 - 18) : (idx + 1);
        this->cells[next_idx] += 1;
        
        // Быстрые логические проверки вместо деления
        bool player = idx >= 9;
        bool next_side = next_idx >= 9;
        
        if (player != next_side && (this->cells[next_idx] % 2 == 0)) {
            this->scores[player] += this->cells[next_idx];
            this->cells[next_idx] = 0;
        }
        return;
    }

    // Вычисляем один раз и передаем в AVX и для вычисления ID
    const int full_rounds = pits / 18;
    const int remainder = pits % 18;

    sow_cells(this->cells, idx, full_rounds, remainder);

    // ИСПРАВЛЕННАЯ ЛОГИКА: используем remainder, а не pits!
    int last_offset = (remainder == 0) ? 17 : (remainder - 1);
    u_int8_t id = idx + last_offset;
    if (id >= 18) { id -= 18; }

    bool player = idx >= 9; // Замена деления (idx / 9) на быстрое сравнение
    bool last_ball_side = id >= 9;

    if (player != last_ball_side) {
        u_int8_t target_stones = this->cells[id];
        
        if ((target_stones & 1) == 0) { // Захват (четное)
            this->scores[player] += target_stones;
            this->cells[id] = 0;
        } else if (target_stones == 3 && id != this->tuzdeks[1 - player] + (9 * (1 - 2*player)) && this->tuzdeks[player] == NO_TUZDEK && id != (9 * (-player + 2) - 1)) { // Туздык
            this->scores[player] += 3;
            this->tuzdeks[player] = id;
            this->cells[id] = 0; 
        }
    }

    // Сбор Туздыков
    if (this->tuzdeks[0] != NO_TUZDEK) {
        u_int8_t t_id = this->tuzdeks[0];
        if (this->cells[t_id] > 0) {
            this->scores[0] += this->cells[t_id];
            this->cells[t_id] = 0;
        }
    }

    if (this->tuzdeks[1] != NO_TUZDEK) {
        u_int8_t t_id = this->tuzdeks[1];
        if (this->cells[t_id] > 0) {
            this->scores[1] += this->cells[t_id];
            this->cells[t_id] = 0;
        }
    }
}

void ToguzNative::get_legal_moves(bool player, std::array<u_int8_t, 9>& legal_moves, int& count) const {
    count = 0;
    
    // Используем raw pointers чтобы избежать вызова операторов []
    const u_int8_t* p_cells = this->cells.data() + (player ? 9 : 0);
    u_int8_t* p_moves = legal_moves.data();

    // Развернутый цикл: 0 ветвлений, строго линейное выполнение
    p_moves[count] = 0; count += (p_cells[0] > 0);
    p_moves[count] = 1; count += (p_cells[1] > 0);
    p_moves[count] = 2; count += (p_cells[2] > 0);
    p_moves[count] = 3; count += (p_cells[3] > 0);
    p_moves[count] = 4; count += (p_cells[4] > 0);
    p_moves[count] = 5; count += (p_cells[5] > 0);
    p_moves[count] = 6; count += (p_cells[6] > 0);
    p_moves[count] = 7; count += (p_cells[7] > 0);
    p_moves[count] = 8; count += (p_cells[8] > 0);
}
// first player - 1, second player - -1, 0 - draw
void ToguzNative::who_is_winner(int8_t winner) const {
    // A player wins by collecting 82 or more stones in their score.
    if (this->scores[0] >= 82) {
        winner = 1;
    } else if (this->scores[1] >= 82) {
        winner = -1;
    } else if (this->scores[0] + this->scores[1] == 162 && this->scores[0] == this->scores[1]) {
        winner = 0;
    } 
}

std::ostream& operator<<(std::ostream& os, const ToguzNative& game) {
    os << "Cells: \n";
    for (int i = 17; i >= 9; --i) {
        os << static_cast<int>(game.cells[i]) << " ";
    }
    os << "\n";  
    for (int i = 0; i < 9; ++i) {
        os << static_cast<int>(game.cells[i]) << " ";
    } 
    os << "\nTuzdeks: " << static_cast<int>(game.tuzdeks[0]) << " " << static_cast<int>(game.tuzdeks[1]);
    os << "\nScores: " << static_cast<int>(game.scores[0]) << " " << static_cast<int>(game.scores[1]);
    return os;
}

ZobristHash::ZobristHash() {
    std::mt19937_64 rng(HASH_SEED); // Фиксированный сид для воспроизводимости
    std::uniform_int_distribution<u_int64_t> dist;

    for (auto& h : cell_hashes) h = dist(rng);
    for (auto& h : tuzdek_hashes) h = dist(rng);
    for (auto& h : score_hashes) h = dist(rng);
    turn_hash = dist(rng);
}

u_int64_t ZobristHash::hash(const ToguzNative& game, bool player_turn) const {
    u_int64_t h = 0;

    for (int i = 0; i < 18; ++i) {
        u_int8_t stone_count = game.cells[i];
        h ^= cell_hashes[i * 162 + stone_count];
    }

    for (int p = 0; p < 2; ++p) {
        if (game.tuzdeks[p] != NO_TUZDEK) {
            h ^= tuzdek_hashes[game.tuzdeks[p]];
        }
        h ^= score_hashes[p * 82 + game.scores[p]];
    }

    if (player_turn) {
        h ^= turn_hash;
    }

    return h;
}

// Calls when player has no legal moves
void atsyrau(ToguzNative& game, bool player) {
    for (int i = 9 - (player * 9); i < 18 - (player * 9); ++i) {
        game.scores[1 - player] += game.cells[i];
        game.cells[i] = 0;
    }
}
