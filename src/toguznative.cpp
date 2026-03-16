#include "toguznative.h"

#include <algorithm>
#include <immintrin.h>
#include <cstring>

alignas(32) u_int8_t REMAINDER_MASKS[18][18][32];

namespace {

inline void sow_scalar(std::array<u_int8_t, 32>& cells, u_int8_t idx, int pits) {
    for (int i = 0; i < pits; ++i) {
        const u_int8_t id = static_cast<u_int8_t>((idx + i) % 18);
        cells[id] += 1;
    }
}

#if defined(__AVX2__)
inline void sow_avx2(std::array<u_int8_t, 32>& cells, u_int8_t idx, int& pits) {
    const int full_rounds = pits / 18;
    const int remainder = pits % 18;

    __m256i v_cells = _mm256_load_si256(reinterpret_cast<const __m256i*>(cells.data()));

    __m256i v_full = _mm256_set1_epi8(full_rounds);

    __m256i v_rem = _mm256_load_si256(reinterpret_cast<const __m256i*>(REMAINDER_MASKS[idx][remainder]));

    v_cells = _mm256_add_epi8(v_cells, v_full);
    v_cells = _mm256_add_epi8(v_cells, v_rem);

    _mm256_store_si256(reinterpret_cast<__m256i*>(cells.data()), v_cells);

    // remainder <= 17
    // if (remainder > 0) {
    //     int end_idx = idx + remainder;
        
    //     if (end_idx <= 18) {
    //         // Без перехода через границу (индекс 17)
    //         for (int i = idx; i < end_idx; ++i) {
    //             cells[i]++;
    //         }
    //     } else {
    //         // С переходом через границу
    //         for (int i = idx; i < 18; ++i) {
    //             cells[i]++;
    //         }
    //         for (int i = 0; i < end_idx - 18; ++i) {
    //             cells[i]++;
    //         }
    //     }
    // }
}
#endif

inline void sow_cells(std::array<u_int8_t, 32>& cells, u_int8_t idx, int& pits) {
#if defined(__AVX2__)
    sow_avx2(cells, idx, pits);
#else
    sow_scalar(cells, idx, pits);
#endif
}

}  // namespace

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

void ToguzNative::move(u_int8_t idx){
    int pits = this->cells[idx];
    if (pits == 1) {
        const u_int8_t next_idx = static_cast<u_int8_t>((idx + 1) % 18);
        this->cells[idx] = 0;
        this->cells[next_idx] += 1;
        bool is_capture = (!(this->cells[next_idx] & 1) && (idx / 9 != next_idx / 9));
        this->scores[idx / 9] += is_capture * this->cells[next_idx];
        this->cells[next_idx] *= !is_capture;
        return;
    }

    this->cells[idx] = 0;
    sow_cells(this->cells, idx, pits);

    // u_int8_t id = (idx + pits - 1) % 18;

    int last_offset = (pits == 0) ? 17 : (pits - 1);
    u_int8_t id = idx + last_offset;
    if (id >= 18) {
        id -= 18; // Заменяем % 18 на одно быстрое вычитание
    }

    bool player = idx / 9;
    bool last_ball_side = id / 9;
    // from -1 0 1 to 0 1
    // bool is_on_opposite = (last_ball_side - player) * (last_ball_side - player);

    // if ( player != last_ball_side && !(this->cells[id] & 1)) {
    //     this->scores[player] += this->cells[id];
    // }

    bool is_capture = (!(this->cells[id] & 1) && (player != last_ball_side));
    this->scores[player] += is_capture * this->cells[id];
    this->cells[id] *= !is_capture;

    // if (
    //     player != last_ball_side &&
    //     this->cells[id] == 3 &&
    //     id != this->tuzdeks[1 - player] &&
    //     this->tuzdeks[player] == NO_TUZDEK
    // ){
    //     this->scores[player] += this->cells[id];
    //     this->tuzdeks[player] = id;
    // }

    bool is_tuzdek = (player != last_ball_side) && (this->cells[id] == 3) && (id != this->tuzdeks[1 - player]) && (this->tuzdeks[player] == NO_TUZDEK);
    this->scores[player] += is_tuzdek * this->cells[id];
    this->tuzdeks[player] = is_tuzdek * id + (1 - is_tuzdek) * this->tuzdeks[player];

    // if (this->tuzdeks[0] != NO_TUZDEK){
    //     this->scores[0] += this->cells[this->tuzdeks[0]];
    //     this->cells[this->tuzdeks[0]] = 0;
    // }

    bool is_tuzdek_0 = (this->tuzdeks[0] != NO_TUZDEK);
    if (is_tuzdek_0) {
        this->scores[0] += this->cells[this->tuzdeks[0]];
        this->cells[this->tuzdeks[0]] = 0;
    }

    // if (this->tuzdeks[1] != NO_TUZDEK){
    //     this->scores[1] += this->cells[this->tuzdeks[1]];
    //     this->cells[this->tuzdeks[1]] = 0;
    // }

    bool is_tuzdek_1 = (this->tuzdeks[1] != NO_TUZDEK);
    if (is_tuzdek_1) {
        this->scores[1] += this->cells[this->tuzdeks[1]];
        this->cells[this->tuzdeks[1]] = 0;
    }
}

void ToguzNative::get_legal_moves(bool player, std::array<u_int8_t, 9>& legal_moves, int& count) const {
    count = 0;
    const u_int8_t offset = player * 9;
    for (u_int8_t i = 0; i < 9; i++) {
        legal_moves[count] = i;
        // Если > 0, вернет 1 (сдвигаем индекс записи). Если 0, вернет 0 (перезапишем на след. шаге).
        count += (this->cells[i + offset] > 0); 
    }
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
    os << "Cells: ";
    for (const auto cell : game.cells) {
        os << static_cast<int>(cell) << " ";
    }
    os << "\nTuzdeks: " << static_cast<int>(game.tuzdeks[0]) << " " << static_cast<int>(game.tuzdeks[1]);
    os << "\nScores: " << static_cast<int>(game.scores[0]) << " " << static_cast<int>(game.scores[1]);
    return os;
}
