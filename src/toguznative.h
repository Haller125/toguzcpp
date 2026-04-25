#pragma once

#include <array>
#include <cstddef>
#include <iostream>
#include <sys/types.h>

#define NO_TUZDEK 18

bool toguznative_compiled_with_avx2();

void init_masks();

// object that stores one move in the game, used for unmove
struct Move{
    // index of the cell that was sown from
    u_int8_t idx;
    // index of the cell that received the last stone
    u_int8_t id;
    bool player;
    bool is_caused_tuzdek;
    int8_t delta_scores[2];
    // number of stones in idx before the move
    u_int8_t idx_before;
    // number of stones in id before the move
    u_int8_t id_before;

    // full state snapshot before move, used for exact unmove restoration
    std::array<u_int8_t, 32> prev_cells;
    std::array<u_int8_t, 2> prev_tuzdeks;
    std::array<u_int8_t, 2> prev_scores;
};

struct ToguzNative{

    alignas(32) std::array<u_int8_t, 32> cells; 
    std::array<u_int8_t, 2> tuzdeks;
    std::array<u_int8_t, 2> scores;

    std::array<Move, 1024> move_history;
    int last_move_index = -1;

    ToguzNative();
    ~ToguzNative() = default;
    void move(u_int8_t idx);
    //unmoves last move
    void unmove();
    void get_legal_moves(bool player, std::array<u_int8_t, 9>& legal_moves, int& count) const;

    friend std::ostream& operator<<(std::ostream& os, const ToguzNative& game);
    bool is_atsyrau(bool player) const;

    void who_is_winner(int8_t winner) const;
};

struct ZobristHash {
    std::array<u_int64_t, 18*162> cell_hashes; // [cell_index*162+stone_count]
    std::array<u_int64_t, 16> tuzdek_hashes; // [tuzdek_index] doesn't matter what player, tuzdek ids are unique across both players
    std::array<u_int64_t, 82*2> score_hashes; // [player*82+score_value]
    u_int64_t turn_hash; // For player to move

    ZobristHash();
    u_int64_t hash(const ToguzNative& game, bool player_turn) const;
    // for qlearning
    u_int64_t hash_cells(const std::array<u_int8_t, 18>& cells, bool player_turn) const; 
    ~ZobristHash() = default;
};

void atsyrau(ToguzNative& game, bool player);