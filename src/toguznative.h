#pragma once

#include <array>
#include <cstddef>
#include <iostream>
#include <sys/types.h>

#define NO_TUZDEK 18

bool toguznative_compiled_with_avx2();

void init_masks();

struct Toguz{
    alignas(32) std::array<u_int8_t, 32> cells; 
    std::array<u_int8_t, 2> tuzdeks;
    std::array<u_int8_t, 2> scores;

    virtual ~Toguz() = default;
    virtual void move(u_int8_t idx) = 0;
};

struct ToguzNative : public Toguz{

    ToguzNative();
    void move(u_int8_t idx) override;
    void get_legal_moves(bool player, std::array<u_int8_t, 9>& legal_moves, int& count) const;

    friend std::ostream& operator<<(std::ostream& os, const ToguzNative& game);


    void who_is_winner(int8_t winner) const;
};

struct ZobristHash {
    std::array<u_int64_t, 18*162> cell_hashes; // [cell_index*162+stone_count]
    std::array<u_int64_t, 16> tuzdek_hashes; // [tuzdek_index] doesn't matter what player, tuzdek ids are unique across both players
    std::array<u_int64_t, 82*2> score_hashes; // [player*82+score_value]
    u_int64_t turn_hash; // For player to move

    ZobristHash();
    u_int64_t hash(const ToguzNative& game) const;
    ~ZobristHash() = default;
};

void atsyrau(ToguzNative& game, bool player);