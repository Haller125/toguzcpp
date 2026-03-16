#include <cstdint>
#include <numeric>
#include <type_traits>

#include <gtest/gtest.h>

#include "toguznative.h"

namespace {

template <typename GameType>
class ToguzDerivedTest : public ::testing::Test {
  static_assert(std::is_base_of<Toguz, GameType>::value,
                "GameType must derive from Toguz");
};

using ToguzImplementations = ::testing::Types<ToguzNative>;
TYPED_TEST_SUITE(ToguzDerivedTest, ToguzImplementations);


template <typename GameType>
std::uint32_t TotalStones(const GameType& game) {
  std::uint32_t total = 0;
  for (int i = 0; i < 18; ++i) {
    total += game.cells[i];
  }
  for (const auto score : game.scores) {
    total += score;
  }
  return total;
}

TYPED_TEST(ToguzDerivedTest, InitialState) {
  TypeParam game;

  // turned off because of the change to 32 cells with padding
  // EXPECT_EQ(game.cells.size(), 18u);
  EXPECT_TRUE(std::all_of(game.cells.begin(), game.cells.end(),
                          [](std::uint8_t c) { return c == 9; }));
  EXPECT_EQ(game.tuzdeks[0], NO_TUZDEK);
  EXPECT_EQ(game.tuzdeks[1], NO_TUZDEK);
  EXPECT_EQ(game.scores[0], 0);
  EXPECT_EQ(game.scores[1], 0);
}

TYPED_TEST(ToguzDerivedTest, MoveKeepsTotalStoneCount) {
  TypeParam game;
  const auto total_before = TotalStones(game);

  init_masks();

  game.move(0);

  const auto total_after = TotalStones(game);
  EXPECT_EQ(total_before, total_after);
}

TYPED_TEST(ToguzDerivedTest, MoveLeavesOneStoneInSourceCell) {
  TypeParam game;
  constexpr std::uint8_t idx = 5;

  init_masks();
  game.move(idx);

  EXPECT_EQ(game.cells[idx], 1);
}

TYPED_TEST(ToguzDerivedTest, SimpleSowingIncrementsFollowingCells) {
  TypeParam game;

  init_masks();
  game.move(0);

  for (std::uint8_t i = 1; i < 9; ++i) {
    EXPECT_EQ(game.cells[i], 10) << "Cell index: " << static_cast<int>(i);
  }
}

TYPED_TEST(ToguzDerivedTest, MoveFromEmptyCellDoesNotChangeOtherCells) {
  TypeParam game;
  constexpr std::uint8_t idx = 2;

  game.cells[idx] = 0;
  init_masks();
  game.move(idx);

  for (std::size_t i = 0; i < game.cells.size(); ++i) {
    if (i == idx) {
      EXPECT_EQ(game.cells[i], 0);
    } else {
      EXPECT_EQ(game.cells[i], 9) << "Cell index: " << i;
    }
  }
}

TYPED_TEST(ToguzDerivedTest, CircularMoveFromEdgeIndices) {
  constexpr std::uint8_t edge_indices[] = {0, 8, 9, 17};

  for (const auto start_idx : edge_indices) {
    TypeParam game;
    game.cells[start_idx] = 20;

    const auto total_before = TotalStones(game);
    init_masks();
    ASSERT_NO_FATAL_FAILURE(game.move(start_idx));
    const auto total_after = TotalStones(game);

    EXPECT_EQ(total_before, total_after)
        << "Failed index: " << static_cast<int>(start_idx);
  }
}

TYPED_TEST(ToguzDerivedTest, MoveFromCellWithOneStoneLeavesItEmpty) {
  TypeParam game;
  constexpr std::uint8_t src_idx = 0;
  constexpr std::uint8_t dst_idx = 1;

  game.cells[src_idx] = 1;
  game.cells[dst_idx] = 9;

  init_masks();

  game.move(src_idx);

  EXPECT_EQ(game.cells[src_idx], 0);
  EXPECT_EQ(game.cells[dst_idx], 10);
}

TYPED_TEST(ToguzDerivedTest, CaptureWithSingleStoneMove) {
  TypeParam game;
  constexpr std::uint8_t src_idx = 8;  // Last cell of player 1
  constexpr std::uint8_t dst_idx = 9;  // First cell of player 2

  game.cells[src_idx] = 1;
  game.cells[dst_idx] = 5; // 5 + 1 = 6 (Even), triggers capture

  init_masks();
  game.move(src_idx);

  EXPECT_EQ(game.cells[src_idx], 0);
  EXPECT_EQ(game.cells[dst_idx], 0); // Captured
  EXPECT_EQ(game.scores[0], 6); // Player 1 captured 6 stones
}

}  // namespace
