#include <cstdint>
#include <numeric>
#include <type_traits>
#include <unordered_map>
#include <random>
#include <string>

#include <gtest/gtest.h>

#include "toguznative.h"

namespace {

template <typename GameType>
class ToguzDerivedTest : public ::testing::Test {
  static_assert(std::is_same<GameType, ToguzNative>::value,
                "GameType must be ToguzNative");
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

struct GameSnapshot {
  std::array<std::uint8_t, 32> cells;
  std::array<std::uint8_t, 2> tuzdeks;
  std::array<std::uint8_t, 2> scores;
  int last_move_index;
};

GameSnapshot Snapshot(const ToguzNative& game) {
  return {game.cells, game.tuzdeks, game.scores, game.last_move_index};
}

void ExpectSameState(const ToguzNative& game, const GameSnapshot& expected) {
  EXPECT_EQ(game.cells, expected.cells);
  EXPECT_EQ(game.tuzdeks, expected.tuzdeks);
  EXPECT_EQ(game.scores, expected.scores);
  EXPECT_EQ(game.last_move_index, expected.last_move_index);
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

TEST(ToguzNativeUnmoveTest, UnmoveWithoutHistoryDoesNothing) {
  ToguzNative game;
  const auto before = Snapshot(game);

  game.unmove();

  ExpectSameState(game, before);
}

TEST(ToguzNativeUnmoveTest, UnmoveRestoresStateAfterRegularMove) {
  ToguzNative game;
  const auto before = Snapshot(game);

  game.move(0);
  ASSERT_NE(game.cells, before.cells);

  game.unmove();

  ExpectSameState(game, before);
}

TEST(ToguzNativeUnmoveTest, UnmoveRestoresStateAfterSingleStoneCapture) {
  ToguzNative game;
  game.cells[8] = 1;
  game.cells[9] = 5;
  init_masks();
  const auto before = Snapshot(game);

  game.move(8);
  ASSERT_NE(game.scores, before.scores);

  game.unmove();

  ExpectSameState(game, before);
}

TEST(ToguzNativeUnmoveTest, UnmoveRestoresStateAfterTuzdekCreation) {
  ToguzNative game;
  game.cells.fill(0);
  game.scores.fill(0);
  game.cells[7] = 4;
  game.cells[10] = 2;
  init_masks();
  const auto before = Snapshot(game);

  game.move(7);
  ASSERT_NE(game.tuzdeks, before.tuzdeks);

  game.unmove();

  ExpectSameState(game, before);
}

TEST(ToguzNativeUnmoveTest, UnmoveIsLifoAcrossMultipleMoves) {
  ToguzNative game;
  init_masks();

  const auto before_first = Snapshot(game);
  game.move(0);

  const auto before_second = Snapshot(game);
  game.move(9);
  ASSERT_NE(game.cells, before_second.cells);

  game.unmove();
  ExpectSameState(game, before_second);

  game.unmove();
  ExpectSameState(game, before_first);
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

// std::vector<uint8_t> game_indices = {
//     7,7,9,8,6,3,5,4,3,1,
//     5,7,1,9,2,2,7,7,9,9,
//     5,3,1,9,7,2,6,1,3,2,
//     7,2,6,4,1,8,7,3,2,7,
//     2,5,8,9,1,4,1,8,1,1,
//     3,7,1,8,3,2,8,3,7,1,
//     6,3,5,1,8,8,2,8,7,1,8,7,2,4,6,3,8,8,9,1,8,8,7,7,7,8,8,8,6,5,7,8,8,7,5,4,7,8,6,4,6,5,7,7,5,8,3,2,9,1,7,2,8,1,6,4,5,2,2,7,1,3,2,4,3,5
//   };

// TEST(ToguzRealGameTest, FullSimulation) {
//     ToguzNative game;
//     init_masks();
    
//     for (size_t i = 0; i < game_indices.size(); ++i) {
//         bool is_black = (i % 2 != 0);
//         std::cout << "\n\nMove " << (i + 1) / 2 << ": " << std::endl;
//         std::cout << game << std::endl;
//         uint8_t hole = game_indices[i];
//         uint8_t idx = (hole - 1) + (is_black ? 9 : 0);
        
//         ASSERT_EQ(game.cells[idx] > 0, true) << "Attempting to move from an empty cell at move " << i + 1 << " (index " << static_cast<int>(idx) << ")";
//         ASSERT_NO_FATAL_FAILURE(game.move(idx)) << "Crashed at move " << i + 1;
//     }
// }

TEST(ZobristHashTest, Determinism) {
    ZobristHash hasher1;
    ZobristHash hasher2;
    
    ToguzNative game;
    EXPECT_EQ(hasher1.hash(game, true), hasher2.hash(game, true));
    EXPECT_EQ(hasher1.hash(game, false), hasher2.hash(game, false));
}

TEST(ZobristHashTest, Basic) {
    ZobristHash hasher;
    ToguzNative game1;
    ToguzNative game2;
    
    // Same state -> same hash
    EXPECT_EQ(hasher.hash(game1, true), hasher.hash(game2, true));
    
    // Different turn -> different hash
    EXPECT_NE(hasher.hash(game1, true), hasher.hash(game2, false));
    
    // One stone change
    game2.cells[0] -= 1;
    game2.cells[1] += 1;
    EXPECT_NE(hasher.hash(game1, true), hasher.hash(game2, true));
    
    // Tuzdek change
    game1.tuzdeks[0] = 5;
    EXPECT_NE(hasher.hash(game1, true), hasher.hash(game2, false)); // Wait, even with same turn it changes.
    EXPECT_NE(hasher.hash(game1, true), hasher.hash(game2, true));
    game2.tuzdeks[0] = 5;
    
    // Score change
    game1.scores[0] += 5;
    EXPECT_NE(hasher.hash(game1, true), hasher.hash(game2, true));
}

struct ToguzStateStr {
    static std::string serialize(const ToguzNative& game, bool turn) {
        std::string s;
        s.reserve(18 * 3 + 20);
        for (int i = 0; i < 18; ++i) s += std::to_string(game.cells[i]) + ",";
        s += std::to_string(game.tuzdeks[0]) + ",";
        s += std::to_string(game.tuzdeks[1]) + ",";
        s += std::to_string(game.scores[0]) + ",";
        s += std::to_string(game.scores[1]) + ",";
        s += turn ? "1" : "0";
        return s;
    }
};

TEST(ZobristHashTest, Collisions) {
    ZobristHash hasher;
    std::unordered_map<uint64_t, std::string> seen_hashes;
    
    ToguzNative game;
    init_masks();
    
    std::mt19937 rng(42);
    bool turn = true;
    
    // Generate random walk
    for (int i = 0; i < 50000; ++i) {
        uint64_t h = hasher.hash(game, turn);
        std::string state_str = ToguzStateStr::serialize(game, turn);
        
        auto it = seen_hashes.find(h);
        if (it != seen_hashes.end()) {
            EXPECT_EQ(it->second, state_str) << "Hash collision detected!";
        } else {
            seen_hashes[h] = state_str;
        }
        
        // Random move
        std::array<std::uint8_t, 9> legal_moves;
        int count = 0;
        game.get_legal_moves(turn, legal_moves, count);
        
        if (count > 0) {
            std::uniform_int_distribution<int> dist(0, count - 1);
            game.move(legal_moves[dist(rng)]);
        } else {
            // No moves available, reset game to generate more states
            game = ToguzNative();
        }
        turn = !turn;
    }
}

}  // namespace
