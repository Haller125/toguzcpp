#pragma once
#include "toguznative.h"
#include <climits>
#include <vector>
#include <chrono>
#include <atomic>
#include <future>
#include <thread>

#define WIN INT_MAX
#define LOSE INT_MIN
#define NOMOVE 18

enum class TTFlag : u_int8_t {
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

    TT (size_t size = 1048576) : tt_table(size, {0, -1, 0, TTFlag::Exact, NOMOVE}), sizeMask(size - 1) {}

    void clear();

    bool probe(const u_int64_t hash, const int depth, int& score, u_int8_t& best_move, TTFlag& flag);

    void store(const u_int64_t hash, const int depth, const int score, const u_int8_t best_move, const TTFlag flag);
};

int search_depth(ToguzNative& node, int depth, int alpha, int beta, bool player, u_int8_t& best_move, TT& tt, ZobristHash& zobrist_hasher, std::atomic<bool>& stop_search );

int score(const ToguzNative& game, bool player);

int iterative_deepening(u_int64_t time_limit_ms, ToguzNative& node, bool player, u_int8_t& best_move, TT& tt, ZobristHash& zobrist_hasher, int max_depth = 20);