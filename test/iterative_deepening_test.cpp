#include <algorithm>
#include <array>
#include <atomic>

#include <gtest/gtest.h>

#include "search.h"

namespace {

struct PositionSnapshot {
  std::array<u_int8_t, 32> cells;
  std::array<u_int8_t, 2> tuzdeks;
  std::array<u_int8_t, 2> scores;
  int last_move_index;
};

PositionSnapshot Snapshot(const ToguzNative& game) {
  return {game.cells, game.tuzdeks, game.scores, game.last_move_index};
}

bool IsLegalMove(const ToguzNative& game, bool player, u_int8_t move) {
  std::array<u_int8_t, 9> legal_moves{};
  int legal_count = 0;
  game.get_legal_moves(player, legal_moves, legal_count);

  return std::find(legal_moves.begin(), legal_moves.begin() + legal_count, move) !=
         (legal_moves.begin() + legal_count);
}

TEST(IterativeDeepeningTest, DoesNotMutateInputPosition) {
  ToguzNative game;
  init_masks();

  game.move(0);
  game.move(9);

  const PositionSnapshot before = Snapshot(game);

  TT tt;
  ZobristHash hasher;
  u_int8_t best_move = NOMOVE;

  const int score_value =
      iterative_deepening(1000, game, false, best_move, tt, hasher, 3);

  EXPECT_EQ(game.cells, before.cells);
  EXPECT_EQ(game.tuzdeks, before.tuzdeks);
  EXPECT_EQ(game.scores, before.scores);
  EXPECT_EQ(game.last_move_index, before.last_move_index);

  EXPECT_NE(score_value, LOSE);
  EXPECT_NE(best_move, NOMOVE);
  EXPECT_TRUE(IsLegalMove(game, false, best_move));
}

TEST(IterativeDeepeningTest, DepthOneMatchesDirectSearch) {
  ToguzNative game_for_iterative;
  ToguzNative game_for_direct;

  TT tt_for_iterative;
  TT tt_for_direct;
  ZobristHash hasher;

  u_int8_t iterative_move = NOMOVE;
  const int iterative_score =
      iterative_deepening(1000,
                          game_for_iterative,
                          false,
                          iterative_move,
                          tt_for_iterative,
                          hasher,
                          1);

  std::atomic<bool> stop_search(false);
  u_int8_t direct_move = NOMOVE;
  const int direct_score = search_depth(game_for_direct,
                                        1,
                                        LOSE,
                                        WIN,
                                        false,
                                        direct_move,
                                        tt_for_direct,
                                        hasher,
                                        stop_search);

  EXPECT_EQ(iterative_score, direct_score);
  EXPECT_EQ(iterative_move, direct_move);

  EXPECT_NE(iterative_move, NOMOVE);
  EXPECT_TRUE(IsLegalMove(game_for_iterative, false, iterative_move));
}

TEST(IterativeDeepeningTest, ZeroTimeLimitReturnsFallbackValues) {
  ToguzNative game;
  const PositionSnapshot before = Snapshot(game);

  TT tt;
  ZobristHash hasher;
  u_int8_t best_move = NOMOVE;

  const int score_value =
      iterative_deepening(0, game, false, best_move, tt, hasher, 5);

  EXPECT_EQ(score_value, LOSE);
  EXPECT_EQ(best_move, NOMOVE);

  EXPECT_EQ(game.cells, before.cells);
  EXPECT_EQ(game.tuzdeks, before.tuzdeks);
  EXPECT_EQ(game.scores, before.scores);
  EXPECT_EQ(game.last_move_index, before.last_move_index);
}

}  // namespace
