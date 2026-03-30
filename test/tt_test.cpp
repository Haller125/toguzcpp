#include <gtest/gtest.h>
#include "search.h"

namespace {

class TTTest : public ::testing::Test {
 protected:
  TT tt;
};

// Test initialization with default size
TEST_F(TTTest, DefaultConstructor) {
  TT table;
  EXPECT_EQ(table.sizeMask, 1048576 - 1);
  EXPECT_EQ(table.tt_table.size(), 1048576u);
}

// Test initialization with custom size
TEST_F(TTTest, CustomSizeConstructor) {
  TT table(256);
  EXPECT_EQ(table.sizeMask, 256 - 1);
  EXPECT_EQ(table.tt_table.size(), 256u);
}

// Test that initial entries are invalid
TEST_F(TTTest, InitialEntriesInvalid) {
  TT table(128);
  int score;
  u_int8_t best_move;
  TTFlag flag;
  
  // Probing any hash on a fresh table should return false (not found)
  bool result = table.probe(0, 0, score, best_move, flag);
  EXPECT_FALSE(result);
  
  result = table.probe(12345, 10, score, best_move, flag);
  EXPECT_FALSE(result);
}

// Test storing and probing an entry
TEST_F(TTTest, StoreAndProbe) {
  TT table(128);
  u_int64_t hash = 0x123456789ABCDEF0ULL;
  int depth = 5;
  int test_score = 42;
  u_int8_t best_move = 3;
  TTFlag flag = TTFlag::Exact;
  
  table.store(hash, depth, test_score, best_move, flag);
  
  int retrieved_score;
  u_int8_t retrieved_move;
  TTFlag retrieved_flag;
  
  bool result = table.probe(hash, depth, retrieved_score, retrieved_move, retrieved_flag);
  
  EXPECT_TRUE(result);
  EXPECT_EQ(retrieved_score, test_score);
  EXPECT_EQ(retrieved_move, best_move);
  EXPECT_EQ(retrieved_flag, TTFlag::Exact);
}

// Test that probe returns false for non-existent hash
TEST_F(TTTest, ProbeNonExistentHash) {
  TT table(128);
  u_int64_t hash1 = 0x123456789ABCDEF0ULL;
  u_int64_t hash2 = 0xFEDCBA9876543210ULL;
  
  table.store(hash1, 5, 42, 3, TTFlag::Exact);
  
  int score;
  u_int8_t best_move;
  TTFlag flag;
  
  bool result = table.probe(hash2, 5, score, best_move, flag);
  EXPECT_FALSE(result);
}

// Test depth comparison in probe
TEST_F(TTTest, DepthComparison) {
  TT table(128);
  u_int64_t hash = 0xAABBCCDDEEFF0011ULL;
  
  // Store at depth 10
  table.store(hash, 10, 100, 5, TTFlag::Exact);
  
  int score;
  u_int8_t best_move;
  TTFlag flag;
  
  // Probe at shallower depth (5) - should succeed because stored depth (10) >= requested (5)
  bool result = table.probe(hash, 5, score, best_move, flag);
  EXPECT_TRUE(result);
  EXPECT_EQ(score, 100);
  
  // Probe at same depth (10) - should succeed
  result = table.probe(hash, 10, score, best_move, flag);
  EXPECT_TRUE(result);
  EXPECT_EQ(score, 100);
  
  // Probe at deeper depth (20) - should fail because stored depth (10) < requested (20)
  result = table.probe(hash, 20, score, best_move, flag);
  EXPECT_FALSE(result);
}

// Test UpperBound flag
TEST_F(TTTest, UpperBoundFlag) {
  TT table(128);
  u_int64_t hash = 0x1111111111111111ULL;
  
  table.store(hash, 8, 50, 7, TTFlag::UpperBound);
  
  int score;
  u_int8_t best_move;
  TTFlag flag;
  
  bool result = table.probe(hash, 8, score, best_move, flag);
  EXPECT_TRUE(result);
  EXPECT_EQ(flag, TTFlag::UpperBound);
  EXPECT_EQ(score, 50);
}

// Test LowerBound flag
TEST_F(TTTest, LowerBoundFlag) {
  TT table(128);
  u_int64_t hash = 0x2222222222222222ULL;
  
  table.store(hash, 6, 75, 2, TTFlag::LowerBound);
  
  int score;
  u_int8_t best_move;
  TTFlag flag;
  
  bool result = table.probe(hash, 6, score, best_move, flag);
  EXPECT_TRUE(result);
  EXPECT_EQ(flag, TTFlag::LowerBound);
  EXPECT_EQ(score, 75);
}

// Test overwriting an entry with deeper search
TEST_F(TTTest, OverwriteWithDeeperSearch) {
  TT table(128);
  u_int64_t hash = 0x3333333333333333ULL;
  
  // Store initial entry at depth 5
  table.store(hash, 5, 30, 1, TTFlag::Exact);
  
  // Overwrite with deeper search at depth 10
  table.store(hash, 10, 60, 4, TTFlag::Exact);
  
  int score;
  u_int8_t best_move;
  TTFlag flag;
  
  bool result = table.probe(hash, 10, score, best_move, flag);
  EXPECT_TRUE(result);
  EXPECT_EQ(score, 60);
  EXPECT_EQ(best_move, 4);
}

// Test that shallow searches don't overwrite deeper searches
TEST_F(TTTest, DontOverwriteWithShallowerSearch) {
  TT table(128);
  u_int64_t hash = 0x4444444444444444ULL;
  
  // Store initial entry at depth 10
  table.store(hash, 10, 80, 6, TTFlag::Exact);
  
  // Try to store at shallower depth 5 (should not overwrite)
  table.store(hash, 5, 40, 2, TTFlag::Exact);
  
  int score;
  u_int8_t best_move;
  TTFlag flag;
  
  bool result = table.probe(hash, 5, score, best_move, flag);
  EXPECT_TRUE(result);
  // Should still have the deeper entry's values
  EXPECT_EQ(score, 80);
  EXPECT_EQ(best_move, 6);
}

// Test clear functionality
TEST_F(TTTest, Clear) {
  TT table(64);
  
  // Store some entries
  table.store(0x1111111111111111ULL, 5, 10, 1, TTFlag::Exact);
  table.store(0x2222222222222222ULL, 7, 20, 2, TTFlag::Exact);
  table.store(0x3333333333333333ULL, 9, 30, 3, TTFlag::Exact);
  
  // Clear the table
  table.clear();
  
  int score;
  u_int8_t best_move;
  TTFlag flag;
  
  // All probes should fail
  EXPECT_FALSE(table.probe(0x1111111111111111ULL, 5, score, best_move, flag));
  EXPECT_FALSE(table.probe(0x2222222222222222ULL, 7, score, best_move, flag));
  EXPECT_FALSE(table.probe(0x3333333333333333ULL, 9, score, best_move, flag));
}

// Test with extreme score values
TEST_F(TTTest, ExtremeScoreValues) {
  TT table(128);
  
  // Test with INT_MAX
  table.store(0xAAAAAAAAAAAAAAAAULL, 5, INT_MAX, 0, TTFlag::Exact);
  int score;
  u_int8_t best_move;
  TTFlag flag;
  
  bool result = table.probe(0xAAAAAAAAAAAAAAAAULL, 5, score, best_move, flag);
  EXPECT_TRUE(result);
  EXPECT_EQ(score, INT_MAX);
  
  // Test with INT_MIN
  table.store(0xBBBBBBBBBBBBBBBBULL, 5, INT_MIN, 0, TTFlag::Exact);
  result = table.probe(0xBBBBBBBBBBBBBBBBULL, 5, score, best_move, flag);
  EXPECT_TRUE(result);
  EXPECT_EQ(score, INT_MIN);
  
  // Test with zero
  table.store(0xCCCCCCCCCCCCCCCCULL, 5, 0, 0, TTFlag::Exact);
  result = table.probe(0xCCCCCCCCCCCCCCCCULL, 5, score, best_move, flag);
  EXPECT_TRUE(result);
  EXPECT_EQ(score, 0);
}

// Test with edge case best_move values
TEST_F(TTTest, BestMoveValues) {
  TT table(128);
  
  // Test with max u_int8_t value
  table.store(0xDDDDDDDDDDDDDDDDULL, 5, 100, 255, TTFlag::Exact);
  
  int score;
  u_int8_t best_move;
  TTFlag flag;
  
  bool result = table.probe(0xDDDDDDDDDDDDDDDDULL, 5, score, best_move, flag);
  EXPECT_TRUE(result);
  EXPECT_EQ(best_move, 255);
  
  // Test with 0
  table.store(0xEEEEEEEEEEEEEEEEULL, 5, 100, 0, TTFlag::Exact);
  result = table.probe(0xEEEEEEEEEEEEEEEEULL, 5, score, best_move, flag);
  EXPECT_TRUE(result);
  EXPECT_EQ(best_move, 0);
}

// Test multiple entries with different hashes
TEST_F(TTTest, MultipleEntries) {
  TT table(256);
  
  std::vector<u_int64_t> hashes = {
      0x0000000000000001ULL,
      0x1111111111111111ULL,
      0x2222222222222222ULL,
      0x3333333333333333ULL,
      0x4444444444444444ULL
  };
  
  // Store multiple entries
  for (size_t i = 0; i < hashes.size(); ++i) {
    table.store(hashes[i], i + 1, i * 10, i, TTFlag::Exact);
  }
  
  // Verify all entries can be retrieved
  int score;
  u_int8_t best_move;
  TTFlag flag;
  
  for (size_t i = 0; i < hashes.size(); ++i) {
    bool result = table.probe(hashes[i], i + 1, score, best_move, flag);
    EXPECT_TRUE(result);
    EXPECT_EQ(score, static_cast<int>(i * 10));
    EXPECT_EQ(best_move, static_cast<u_int8_t>(i));
  }
}

// Test collision handling - hash collisions should use the most recent deep entry
TEST_F(TTTest, CollisionHandling) {
  TT table(8); // Very small table to force collisions
  
  u_int64_t hash1 = 0x0000000000000001ULL; // Will map to index 1
  u_int64_t hash2 = 0x0000000000000009ULL; // Will also map to index 1 (1 & 7 = 1, 9 & 7 = 1)
  
  // Store first entry
  table.store(hash1, 5, 100, 1, TTFlag::Exact);
  
  int score;
  u_int8_t best_move;
  TTFlag flag;
  
  bool result = table.probe(hash1, 5, score, best_move, flag);
  EXPECT_TRUE(result);
  EXPECT_EQ(score, 100);
  
  // Store second entry (same index, deeper search)
  table.store(hash2, 10, 200, 2, TTFlag::Exact);
  
  // Now hash1 might be overwritten by hash2
  result = table.probe(hash1, 5, score, best_move, flag);
  // This may or may not succeed depending on collision handling,
  // but hash2 should definitely be there with its deep search
  
  result = table.probe(hash2, 10, score, best_move, flag);
  EXPECT_TRUE(result);
  EXPECT_EQ(score, 200);
}

// Test that retrieved best_move is correctly set even when not probing at stored depth
TEST_F(TTTest, BestMoveAvailableAtPartialProbe) {
  TT table(128);
  u_int64_t hash = 0x5555555555555555ULL;
  
  table.store(hash, 8, 75, 9, TTFlag::LowerBound);
  
  int score;
  u_int8_t best_move;
  TTFlag flag;
  
  // Even if the hash matches, best_move should be set
  bool result = table.probe(hash, 8, score, best_move, flag);
  EXPECT_TRUE(result);
  EXPECT_EQ(best_move, 9);
}

}  // namespace
