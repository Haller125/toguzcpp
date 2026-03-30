#pragma once
#include "toguznative.h"
#include <climits>
#include <vector>

#define WHITEWINS INT_MAX
#define BLACKWINS INT_MIN

enum class TTFlag : uint8_t {
    Exact,
    LowerBound,
    UpperBound
};

struct TTEntry {
    u_int64_t hash;
    int depth;
    int score;
    TTFlag flag;
    u_int8_t best_move;
};

struct TT{
    std::vector<TTEntry> tt_table;
    size_t sizeMask;

    TT (size_t size = 1048576) : tt_table(size, {0, -1, 0, TTFlag::Exact, 0}), sizeMask(size - 1) {}

    void clear();

    bool probe(const u_int64_t hash, const int depth, int& score, u_int8_t& best_move, TTFlag& flag);

    void store(const u_int64_t hash, const int depth, const int score, const u_int8_t best_move, const TTFlag flag);
};

int search(ToguzNative& node, int depth, int alpha, int beta, bool player, int& best_move, std::array<u_int8_t, 9>& legal_moves_buffer, int& legal_count_buffer);

int score(const ToguzNative& game, bool player);