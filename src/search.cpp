#include "search.h"
#include <algorithm> // for std::max, std::min, std::swap

std::vector<TTEntry> tt_table;
ZobristHash zobrist_hasher;

void init_tt() {
    tt_table.resize(TT_SIZE, {0, -1, 0, TTFlag::Exact, 0});
}

int search(ToguzNative& node, int depth, int alpha, int beta, bool player, int& best_move, std::array<u_int8_t, 9>& legal_moves_buffer, int& legal_count_buffer, TT& tt, ZobristHash& zobrist_hasher) {
    if (depth == 0) {
        return score(node, player);
    }

    int original_alpha = alpha;
    
    u_int64_t hash = zobrist_hasher.hash(node);
    if (player) {
        hash ^= zobrist_hasher.turn_hash;
    }

    u_int8_t tt_move = 18; // Invalid move index by default
    TTFlag flag;
    if (tt.probe(hash, depth, alpha, best_move, flag)) {
        return alpha; // TT hit with a valid score
    }

    node.get_legal_moves(player, legal_moves_buffer, legal_count_buffer);

    if (legal_count_buffer == 0) {
        return player ? BLACKWINS : WHITEWINS; // Opponent wins if no legal moves
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

    int value;
    int local_best_move = legal_moves_buffer[0]; // Default best move
    
    for (int i = 0; i < legal_count_buffer; ++i) {
        ToguzNative child = node;
        u_int8_t move_idx = legal_moves_buffer[i] + (player ? 9 : 0);
        child.move(move_idx);
        
        // Pass distinct buffers for child node searches
        int child_best_move;
        
        value = -search(child, depth - 1, -beta, -alpha, !player, child_best_move, moves_buffer, count_buffer, tt, zobrist_hasher);
        
        if (value > alpha) {
            alpha = value;
            local_best_move = legal_moves_buffer[i];
        }
        if (alpha >= beta) {
            break; // Beta cut-off
        }
    }
    
    best_move = local_best_move;
    
    if (alpha <= original_alpha) {
        new_entry.flag = TTFlag::UpperBound;
    } else if (alpha >= beta) {
        new_entry.flag = TTFlag::LowerBound;
    } else {
        new_entry.flag = TTFlag::Exact;
    }
    
    tt.store(hash, depth, alpha, static_cast<u_int8_t>(local_best_move), new_entry.flag);

    return alpha;
}

int score(const ToguzNative& game, bool player) {
    return game.scores[0] - game.scores[1] * (-player * 2 + 1); // Positive if player 0 is winning, negative if player 1 is winning
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
            if (entry.flag == TTFlag::LowerBound && entry.score >= beta) {
                score = entry.score;
                flag = entry.flag;
                return true;
            }
            if (entry.flag == TTFlag::UpperBound && entry.score <= alpha) {
                score = entry.score;
                flag = entry.flag;
                return true;
            }
        }
    }
    return false;
}
