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

TYPED_TEST(ToguzDerivedTest, CaptureWithMultipleStones) {
  TypeParam game;
  game.cells.fill(0);
  game.scores.fill(0);
  
  // Игрок 0 ходит из лунки 7. Там 5 камней.
  // Они упадут в лунки: 8, 9, 10, 11 (последняя).
  game.cells[7] = 5; 
  game.cells[11] = 9; // Лунка Игрока 1. 9 + 1 = 10 (четное -> захват)
  
  init_masks();
  game.move(7);

  EXPECT_EQ(game.cells[11], 0) << "Cell 11 should be captured and emptied";
  EXPECT_EQ(game.scores[0], 10) << "Player 0 should have 10 captured stones";
}

TYPED_TEST(ToguzDerivedTest, CreateTuzdek) {
  TypeParam game;
  game.cells.fill(0);
  game.scores.fill(0);
  
  // Игрок 0 ходит из лунки 7. Там 4 камня.
  // Они упадут в лунки: 8, 9, 10 (последняя).
  game.cells[7] = 4; 
  game.cells[10] = 2; // Лунка Игрока 1. 2 + 1 = 3 (создание Туздыка)
  
  init_masks();
  game.move(7);

  // std::cout << game << std::endl; // Для отладки, чтобы увидеть состояние после хода

  EXPECT_EQ(game.tuzdeks[0], 10) << "Player 0 should get a Tuzdek at index 10";
  EXPECT_EQ(game.cells[10], 0) << "Stones from a new Tuzdek are moved to kazan";
  EXPECT_EQ(game.scores[0], 3) << "Player 0 gets 3 stones from the new Tuzdek";
}

TYPED_TEST(ToguzDerivedTest, CollectStonesFromTuzdek) {
  TypeParam game;
  game.cells.fill(0);
  game.scores.fill(0);
  
  // У Игрока 0 уже есть Туздык на стороне Игрока 1 (индекс 10)
  game.tuzdeks[0] = 10; 
  game.cells[10] = 0;   // Туздык технически пуст
  
  // Игрок 1 делает ход из лунки 9, там 5 камней.
  // Они упадут в 10, 11, 12, 13, 14.
  game.cells[9] = 5;
  
  init_masks();
  game.move(9);

  EXPECT_EQ(game.cells[10], 0) << "Tuzdek must remain empty on the board";
  // 1 камень упал в Туздык Игрока 0 во время хода Игрока 1
  EXPECT_EQ(game.scores[0], 1) << "Player 0 should collect 1 stone that fell into their Tuzdek";
}

TYPED_TEST(ToguzDerivedTest, TuzdekRestrictions) {
  TypeParam game;
  game.cells.fill(0);
  game.scores.fill(0);
  
  // --- Правило 1: Нельзя ставить Туздык в 9-ю лунку (индексы 8 и 17) ---
  game.cells[8] = 10; // Ход Игрока 1. Падают в 16, 17, 0. Последний в 17.
  game.cells[17] = 2; // 2 + 1 = 3
  
  init_masks();
  game.move(8);

  EXPECT_EQ(game.tuzdeks[1], NO_TUZDEK) << "Cannot create Tuzdek on the 9th hole (index 17)";
  EXPECT_EQ(game.cells[17], 3) << "Stones should just remain in the 9th hole";
  EXPECT_EQ(game.scores[1], 0);

  // --- Правило 2: Симметрия ---
  // Если у Игрока 0 туздык на лунке №2 противника (индекс 10)...
  game.tuzdeks[0] = 10;
  game.cells[10] = 0;
  
  // ...то Игрок 1 не может взять туздык на лунке №2 Игрока 0 (индекс 1).
  game.cells[17] = 3; // Ход Игрока 1. Падают в 0, 1, 2. Последний в 1.
  game.cells[1] = 2;  // 2 + 1 = 3
  
  game.move(17);
  
  EXPECT_EQ(game.tuzdeks[1], NO_TUZDEK) << "Cannot create a symmetrical Tuzdek";
  EXPECT_EQ(game.cells[1], 3);
}

}  // namespace
