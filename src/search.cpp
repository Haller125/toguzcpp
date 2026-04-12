#include "search.h"
#include <algorithm> // for std::max, std::min, std::swap

ZobristHash zobrist_hasher;

int nodes_count = 0;
auto deadline = std::chrono::steady_clock::now();

namespace {

constexpr int QUIESCENCE_MAX_PLY = 32;

bool should_stop_due_to_time(std::atomic<bool>& stop_search) {
    if ((nodes_count++ & 2047) == 0) {
        if (std::chrono::steady_clock::now() >= deadline) {
            stop_search.store(true);
        }
        nodes_count = 0;
    }
    return stop_search.load();
}

bool is_tactical_transition(const ToguzNative& before, const ToguzNative& after, bool player) {
    return before.scores[player] != after.scores[player] ||
           before.scores[1 - player] != after.scores[1 - player] ||
           before.tuzdeks[player] != after.tuzdeks[player] ||
           before.tuzdeks[1 - player] != after.tuzdeks[1 - player];
}

int quiescence_search(ToguzNative& node,
                      int alpha,
                      int beta,
                      bool player,
                      u_int8_t& best_move,
                      std::atomic<bool>& stop_search,
                      int ply) {
    if (should_stop_due_to_time(stop_search) || ply >= QUIESCENCE_MAX_PLY) {
        return score(node, player);
    }

    const int stand_pat = score(node, player);
    if (stand_pat >= beta) {
        return stand_pat;
    }
    if (stand_pat > alpha) {
        alpha = stand_pat;
    }

    std::array<u_int8_t, 9> legal_moves_buffer;
    int legal_count_buffer = 0;
    node.get_legal_moves(player, legal_moves_buffer, legal_count_buffer);

    if (legal_count_buffer == 0) {
        return LOSE;
    }

    bool found_tactical_move = false;
    u_int8_t local_best_move = NOMOVE;

    for (int i = 0; i < legal_count_buffer; ++i) {
        const u_int8_t move_idx = legal_moves_buffer[i];
        ToguzNative child = node;
        child.move(move_idx);

        if (!is_tactical_transition(node, child, player)) {
            continue;
        }

        found_tactical_move = true;

        u_int8_t child_best_move = NOMOVE;
        const int value = -quiescence_search(child,
                                             -beta,
                                             -alpha,
                                             !player,
                                             child_best_move,
                                             stop_search,
                                             ply + 1);

        if (value >= beta) {
            best_move = move_idx;
            return value;
        }

        if (value > alpha) {
            alpha = value;
            local_best_move = move_idx;
        }
    }

    if (found_tactical_move) {
        best_move = local_best_move;
    }

    return alpha;
}

} // namespace

u_int8_t killer_moves[64][2] = {{NOMOVE, NOMOVE}}; // [depth][2 killers]
int history_table[2][9] = {{0}};


int search_depth(ToguzNative& node, int depth, int alpha, int beta, bool player, u_int8_t& best_move, TT& tt, ZobristHash& zobrist_hasher, std::atomic<bool>& stop_search) {
    if (depth == 0) {
        return quiescence_search(node, alpha, beta, player, best_move, stop_search, 0);
    }

    if (should_stop_due_to_time(stop_search)) {
        return score(node, player);
    }

    int original_alpha = alpha;
    
    u_int64_t hash = zobrist_hasher.hash(node, player);

    int tt_score;
    u_int8_t tt_move = NOMOVE;
    TTFlag flag;
    
    if (tt.probe(hash, depth, tt_score, tt_move, flag)) {
        if (flag == TTFlag::Exact) {
            best_move = tt_move;
            return tt_score;
        }
        if (flag == TTFlag::LowerBound && tt_score >= beta) {
            best_move = tt_move;
            return tt_score;
        }
        if (flag == TTFlag::UpperBound && tt_score <= alpha) {
            best_move = tt_move;
            return tt_score;
        }
    }

    std::array<u_int8_t, 9> legal_moves_buffer;
    int legal_count_buffer;

    node.get_legal_moves(player, legal_moves_buffer, legal_count_buffer);

    if (legal_count_buffer == 0) {
        return LOSE; // Opponent wins if no legal moves
    }

    int move_scores[9];
    for (int i = 0; i < legal_count_buffer; ++i) {
        u_int8_t move = legal_moves_buffer[i];
        move_scores[i] = 0;

        if (move == tt_move) {
            move_scores[i] += 1000000; 
            continue; 
        }

        if (move == killer_moves[depth][0]) {
            move_scores[i] += 900000;
        } else if (move == killer_moves[depth][1]) {
            move_scores[i] += 800000;
        }
        // else {
        //     move_scores[i] += history_table[player][move];
        // }
    }

    for (int i = 0; i < legal_count_buffer - 1; ++i) {
        int best_idx = i;
        for (int j = i + 1; j < legal_count_buffer; ++j) {
            if (move_scores[j] > move_scores[best_idx]) {
                best_idx = j;
            }
        }

        std::swap(legal_moves_buffer[i], legal_moves_buffer[best_idx]);
        std::swap(move_scores[i], move_scores[best_idx]);
    }

    int best_score = LOSE;
    u_int8_t local_best_move = legal_moves_buffer[0]; // Default best move
    
    for (int i = 0; i < legal_count_buffer; ++i) {
        ToguzNative child = node;
        u_int8_t move_idx = legal_moves_buffer[i];
        child.move(move_idx);
        
        u_int8_t child_best_move;
        
        int value = -search_depth(child, depth - 1, -beta, -alpha, !player, child_best_move, tt, zobrist_hasher, stop_search);
        
        if (value > best_score) {
            best_score = value;
            local_best_move = legal_moves_buffer[i];
        }

        alpha = std::max(alpha, best_score);
        if (alpha >= beta) {
            
            u_int8_t cutoff_move = legal_moves_buffer[i];

            if (killer_moves[depth][0] != cutoff_move) {
                killer_moves[depth][1] = killer_moves[depth][0];
                killer_moves[depth][0] = cutoff_move;
            }

            // history_table[player][cutoff_move] += (depth * depth); //caused slowdown

            break; // Beta cut-off
        }
    }
    
    best_move = local_best_move;

    if (!stop_search.load()) {
        TTEntry new_entry;
        
        if (best_score <= original_alpha) {
            new_entry.flag = TTFlag::UpperBound;
        } else if (best_score >= beta) {
            new_entry.flag = TTFlag::LowerBound;
        } else {
            new_entry.flag = TTFlag::Exact;
        }
        
        tt.store(hash, depth, best_score, static_cast<u_int8_t>(local_best_move), new_entry.flag);
    }
    return best_score;
}

int score(const ToguzNative& game, bool player) {
    if (game.scores[0] >= 82) return player ? LOSE : WIN;
    if (game.scores[1] >= 82) return player ? WIN : LOSE;

    return game.scores[player] - game.scores[1 - player]; 
}

void TT::clear() {
    std::fill(tt_table.begin(), tt_table.end(), TTEntry{0, -1, 0, TTFlag::Exact, NOMOVE});
}

void TT::store(const u_int64_t hash, const int depth, const int score, const u_int8_t best_move, const TTFlag flag) {
    size_t index = hash & sizeMask;
    if (tt_table[index].hash != hash || tt_table[index].depth <= depth) {
        tt_table[index] = {hash, depth, score, flag, best_move};
    }
}

bool TT::probe(const u_int64_t hash, const int depth, int& score, u_int8_t& best_move, TTFlag& flag) {
    size_t index = hash & sizeMask;
    const TTEntry& entry = tt_table[index];
    if (entry.hash == hash) {
        best_move = entry.best_move;
        if (entry.depth >= depth) {
            if (entry.flag == TTFlag::Exact) {
                score = entry.score;
                flag = entry.flag;
                return true;
            }
            if (entry.flag == TTFlag::LowerBound) {
                score = entry.score;
                flag = entry.flag;
                return true;
            }
            if (entry.flag == TTFlag::UpperBound) {
                score = entry.score;
                flag = entry.flag;
                return true;
            }
        }
    }
    return false;
}

std::atomic<bool> stop_search(false);

// static int nodes_count = 0;
// auto deadline = std::chrono::steady_clock::now();
int iterative_deepening(u_int64_t time_limit_ms, ToguzNative& node, bool player, u_int8_t& best_move, TT& tt, ZobristHash& zobrist_hasher, int max_depth) {
    int last_best_score = LOSE;
    int last_best_move = NOMOVE;

    stop_search.store(false);
    std::atomic<bool> timer_cancel(false);

    deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(time_limit_ms);

    // std::thread timer_thread([&]() {
    //     const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(time_limit_ms);
    //     while (!timer_cancel.load(std::memory_order_relaxed)) {
    //         if (std::chrono::steady_clock::now() >= deadline) {
    //             stop_search.store(true);
    //             return;
    //         }
    //         std::this_thread::sleep_for(std::chrono::milliseconds(1));
    //     }
    // });

    for (int depth = 1; depth <= max_depth; ++depth) {

        u_int8_t current_best_move = NOMOVE;
        int current_best_score = search_depth(node, depth, LOSE, WIN, player, current_best_move, tt, zobrist_hasher, stop_search);
        if (stop_search.load()) {
            break; 
        }
        last_best_score = current_best_score;
        last_best_move = current_best_move;
    }

    timer_cancel.store(true, std::memory_order_relaxed);
    // if (timer_thread.joinable()) {
    //     timer_thread.join();
    // }

    best_move = last_best_move;
    return last_best_score;
}