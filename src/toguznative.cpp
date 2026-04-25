#include "toguznative.h"

#include <cstring>
#include <immintrin.h>
#include <mutex>
#include <random>

#define HASH_SEED 123456789

alignas(32) u_int8_t REMAINDER_MASKS[18][18][32];


namespace {

std::once_flag remainder_masks_init_once;

inline bool is_on_second_player_side(u_int8_t pit_idx) {
    return pit_idx >= 9;
}

inline u_int8_t mirrored_opponent_tuzdek(u_int8_t opponent_tuzdek, bool player) {
    return static_cast<u_int8_t>(
        static_cast<int>(opponent_tuzdek) + (player ? -9 : 9)
    );
}

inline bool is_forbidden_tuzdek_pit(u_int8_t pit_idx, bool player) {
    return pit_idx == static_cast<u_int8_t>(player ? 8 : 17);
}

inline bool can_create_tuzdek(const ToguzNative& game, bool player, u_int8_t pit_idx) {
    if (game.tuzdeks[player] != NO_TUZDEK) {
        return false;
    }

    if (is_forbidden_tuzdek_pit(pit_idx, player)) {
        return false;
    }

    const u_int8_t opponent_tuzdek = game.tuzdeks[1 - player];
    if (opponent_tuzdek != NO_TUZDEK && pit_idx == mirrored_opponent_tuzdek(opponent_tuzdek, player)) {
        return false;
    }

    return true;
}

inline void collect_tuzdek_stones(ToguzNative& game, bool player, Move& move_record) {
    const u_int8_t tuzdek_idx = game.tuzdeks[player];
    if (tuzdek_idx == NO_TUZDEK) {
        return;
    }

    const u_int8_t collected = game.cells[tuzdek_idx];
    if (collected == 0) {
        return;
    }

    game.scores[player] += collected;
    move_record.delta_scores[player] += collected;
    game.cells[tuzdek_idx] = 0;
}

alignas(32) const u_int8_t ACTIVE_CELL_MASK[32] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
};

inline void sow_scalar(std::array<u_int8_t, 32>& cells, u_int8_t idx, int full_rounds, int remainder) {
    for (int i = 0; i < 18; ++i) {
        cells[i] += full_rounds;
    }
    for (int i = 0; i < remainder; ++i) {
        cells[(idx + i) % 18] += 1;
    }
}

#if defined(__AVX2__)
inline void sow_avx2(std::array<u_int8_t, 32>& cells, u_int8_t idx, int full_rounds, int remainder) {
    u_int8_t* p_cells = cells.data(); 
    
    __m256i v_cells = _mm256_load_si256(reinterpret_cast<const __m256i*>(p_cells));
    __m256i v_full = _mm256_set1_epi8(static_cast<char>(full_rounds));
    __m256i v_active = _mm256_load_si256(reinterpret_cast<const __m256i*>(ACTIVE_CELL_MASK));
    __m256i v_rem = _mm256_load_si256(reinterpret_cast<const __m256i*>(REMAINDER_MASKS[idx][remainder]));

    v_full = _mm256_and_si256(v_full, v_active);
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
    std::call_once(remainder_masks_init_once, []() {
        std::memset(REMAINDER_MASKS, 0, sizeof(REMAINDER_MASKS));

        for (int idx = 0; idx < 18; ++idx) {
            for (int rem = 0; rem < 18; ++rem) {
                for (int i = 0; i < rem; ++i) {
                    int target = (idx + i) % 18;
                    REMAINDER_MASKS[idx][rem][target] = 1;
                }
            }
        }
    });
}

ToguzNative::ToguzNative(){
        this->cells.fill(9);
        this->tuzdeks.fill(NO_TUZDEK);
        this->scores.fill(0);
        init_masks();

        for (int i = 0; i < 1024; ++i) {
            move_history[i] = Move{};
        }
}

void ToguzNative::move(u_int8_t idx) {
    Move& move_record = move_history[++last_move_index];
    move_record.prev_cells = this->cells;
    move_record.prev_tuzdeks = this->tuzdeks;
    move_record.prev_scores = this->scores;
    move_record.delta_scores[0] = 0;
    move_record.delta_scores[1] = 0;
    move_record.is_caused_tuzdek = false;
    move_record.idx = idx;
    move_record.player = is_on_second_player_side(idx);

    int pits = this->cells[idx];

    move_record.idx_before = pits;
    this->cells[idx] = 0;

    if (pits == 1) {
        const u_int8_t next_idx = (idx + 1) >= 18 ? (idx + 1 - 18) : (idx + 1);
        const u_int8_t stones_before = this->cells[next_idx];
        this->cells[next_idx] += 1;
        move_record.id = next_idx;
        move_record.id_before = stones_before;
        
        bool player = move_record.player;
        bool next_side = is_on_second_player_side(next_idx);
        
        if (player != next_side) {
            u_int8_t target_stones = this->cells[next_idx];

            if ((target_stones & 1) == 0) {
                this->scores[player] += target_stones;
                move_record.delta_scores[player] += target_stones;
                this->cells[next_idx] = 0;
            } else if (target_stones == 3 && can_create_tuzdek(*this, player, next_idx)) {
                this->scores[player] += 3;
                this->tuzdeks[player] = next_idx;
                this->cells[next_idx] = 0;

                move_record.is_caused_tuzdek = true;
                move_record.delta_scores[player] += 3;
            }
        }

        collect_tuzdek_stones(*this, false, move_record);
        collect_tuzdek_stones(*this, true, move_record);
        return;
    }

    // Вычисляем один раз и передаем в AVX и для вычисления ID
    const int full_rounds = pits / 18;
    const int remainder = pits % 18;

    int last_offset = (remainder == 0) ? 17 : (remainder - 1);
    u_int8_t id = idx + last_offset;
    if (id >= 18) { id -= 18; }
    move_record.id = id;
    move_record.id_before = this->cells[id]; 
    sow_cells(this->cells, idx, full_rounds, remainder);

    bool player = move_record.player;
    bool last_ball_side = is_on_second_player_side(id);

    if (player != last_ball_side) {
        u_int8_t target_stones = this->cells[id];
        
        if ((target_stones & 1) == 0) {
            this->scores[player] += target_stones;
            move_record.delta_scores[player] += target_stones;
            this->cells[id] = 0;
        } else if (target_stones == 3 && can_create_tuzdek(*this, player, id)) {
            this->scores[player] += 3;
            this->tuzdeks[player] = id;
            this->cells[id] = 0; 

            move_record.is_caused_tuzdek = true;
            move_record.delta_scores[player] += 3;
        }
    }

    collect_tuzdek_stones(*this, false, move_record);
    collect_tuzdek_stones(*this, true, move_record);
}

void ToguzNative::unmove() {
    if (last_move_index < 0) return; // Нет ходов для отмены

    const Move& last_move = move_history[last_move_index];

    this->cells = last_move.prev_cells;
    this->tuzdeks = last_move.prev_tuzdeks;
    this->scores = last_move.prev_scores;

    --last_move_index;
}

bool ToguzNative::is_atsyrau(bool player) const {
    for (int i = player * 9; i < 9 * player + 9; ++i) {
        if (this->cells[i] > 0) {
            return false;
        }
    }
    return true;
}

void ToguzNative::get_legal_moves(bool player, std::array<u_int8_t, 9>& legal_moves, int& count) const {
    count = 0;
    const u_int8_t base_idx = player ? 9 : 0;
    
    const u_int8_t* p_cells = this->cells.data() + base_idx;
    u_int8_t* p_moves = legal_moves.data();

    p_moves[count] = base_idx + 0; count += (p_cells[0] > 0);
    p_moves[count] = base_idx + 1; count += (p_cells[1] > 0);
    p_moves[count] = base_idx + 2; count += (p_cells[2] > 0);
    p_moves[count] = base_idx + 3; count += (p_cells[3] > 0);
    p_moves[count] = base_idx + 4; count += (p_cells[4] > 0);
    p_moves[count] = base_idx + 5; count += (p_cells[5] > 0);
    p_moves[count] = base_idx + 6; count += (p_cells[6] > 0);
    p_moves[count] = base_idx + 7; count += (p_cells[7] > 0);
    p_moves[count] = base_idx + 8; count += (p_cells[8] > 0);
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
    std::mt19937_64 rng(HASH_SEED);
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

u_int64_t ZobristHash::hash_cells(const std::array<u_int8_t, 18>& cells, bool player_turn) const {
    u_int64_t h = 0;

    for (int i = 0; i < 18; ++i) {
        u_int8_t stone_count = cells[i];
        h ^= cell_hashes[i * 162 + stone_count];
    }

    if (player_turn) {
        h ^= turn_hash;
    }

    return h;
}

// Calls when player has no legal moves
void atsyrau(ToguzNative& game, bool player) {
    for (int i = 0; i < 18; ++i) {
        game.scores[i / 9] += game.cells[i];
        game.cells[i] = 0;
    }
}
