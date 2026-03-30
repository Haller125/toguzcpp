#include "search.h"
#include <algorithm> // for std::max, std::min, std::swap

ZobristHash zobrist_hasher;

int search_depth(ToguzNative& node, int depth, int alpha, int beta, bool player, u_int8_t& best_move, TT& tt, ZobristHash& zobrist_hasher) {
    if (depth == 0) {
        return score(node, player);
    }

    std::array<u_int8_t, 9> legal_moves_buffer;
    int legal_count_buffer;

    node.get_legal_moves(player, legal_moves_buffer, legal_count_buffer);

    if (legal_count_buffer == 0) {
        return LOSE; // Opponent wins if no legal moves
    }

    int original_alpha = alpha;
    
    u_int64_t hash = zobrist_hasher.hash(node, player);

    int tt_score;
    u_int8_t tt_move = 18;
    TTFlag flag;
    
    if (tt.probe(hash, depth, tt_score, tt_move, flag)) {
        if (flag == TTFlag::Exact) return tt_score;
        if (flag == TTFlag::LowerBound && tt_score >= beta) return tt_score;
        if (flag == TTFlag::UpperBound && tt_score <= alpha) return tt_score;
    }


    // Move Ordering: Place TT move first
    if (tt_move < 18) {
        for (int i = 0; i < legal_count_buffer; ++i) {
            if (legal_moves_buffer[i] == tt_move) {
                std::swap(legal_moves_buffer[0], legal_moves_buffer[i]);
                break;
            }
        }
    }

    int best_score = LOSE;
    u_int8_t local_best_move = legal_moves_buffer[0]; // Default best move
    
    for (int i = 0; i < legal_count_buffer; ++i) {
        ToguzNative child = node;
        u_int8_t move_idx = legal_moves_buffer[i] + (player ? 9 : 0);
        child.move(move_idx);
        
        u_int8_t child_best_move;
        
        int value = -search_depth(child, depth - 1, -beta, -alpha, !player, child_best_move, tt, zobrist_hasher);
        
        if (value > best_score) {
            best_score = value;
            local_best_move = legal_moves_buffer[i];
        }

        alpha = std::max(alpha, best_score);
        if (alpha >= beta) {
            break; // Beta cut-off
        }
    }
    
    best_move = local_best_move;

    TTEntry new_entry;
    
    if (best_score <= original_alpha) {
        new_entry.flag = TTFlag::UpperBound;
    } else if (best_score >= beta) {
        new_entry.flag = TTFlag::LowerBound;
    } else {
        new_entry.flag = TTFlag::Exact;
    }
    
    tt.store(hash, depth, best_score, static_cast<u_int8_t>(local_best_move), new_entry.flag);

    return best_score;
}

int score(const ToguzNative& game, bool player) {
    if (game.scores[0] >= 82) return player ? LOSE : WIN;
    if (game.scores[1] >= 82) return player ? WIN : LOSE;

    return game.scores[player] - game.scores[1 - player]; 
}

void TT::clear() {
    std::fill(tt_table.begin(), tt_table.end(), TTEntry{0, -1, 0, TTFlag::Exact, 0});
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
