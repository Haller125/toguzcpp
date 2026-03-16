#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <random>

#include "toguznative.h"

#define GAME_CLASS ToguzNative


namespace {

uint32_t rng_state = 123456789;

constexpr std::uint32_t kMaxTurnsPerGame = 5000;

bool cpu_supports_avx2() {
#if (defined(__x86_64__) || defined(__i386__)) && (defined(__GNUC__) || defined(__clang__))
  return __builtin_cpu_supports("avx2");
#else
  return false;
#endif
}


inline uint32_t xorshift32(uint32_t& state) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

inline uint32_t fast_rand_bound(uint32_t random_val, uint32_t range) {
    return static_cast<uint32_t>((static_cast<uint64_t>(random_val) * range) >> 32);
}


std::uint64_t play_single_game() {
    GAME_CLASS game;
    bool player = false;
    std::uint32_t turn_count = 0;

    std::array<u_int8_t, 9> legal_moves{};
    int legal_count = 0;

    while (turn_count < kMaxTurnsPerGame) {
        game.get_legal_moves(player, legal_moves, legal_count);

      if (legal_count == 0) {
        for (int i = 9 - (player * 9); i < 18 - (player * 9); ++i) {
          game.scores[1 - player] += game.cells[i];
          game.cells[i] = 0;
        }
        break;
      }

        // std::uniform_int_distribution<int> pick_move(0, legal_count - 1);
        // const u_int8_t move_idx = legal_moves[static_cast<std::size_t>(pick_move(rng))];

        uint32_t r = xorshift32(rng_state);
        const u_int8_t move_idx = legal_moves[fast_rand_bound(r, legal_count)];
        game.move(static_cast<u_int8_t>(move_idx + (player ? 9 : 0)));

        // check if game is over
      if (game.scores[0] >= 82 || game.scores[1] >= 82) {
            break;
        }

        player = !player;
        ++turn_count;

    }

    return turn_count;
}

}  // namespace

int main(int argc, char* argv[]) {
  std::uint32_t games_to_play = 1000;

  if (argc > 1) {
    const int parsed = std::atoi(argv[1]);
    if (parsed > 0) {
      games_to_play = static_cast<std::uint32_t>(parsed);
    }
  }

  const bool lib_has_avx2 = toguznative_compiled_with_avx2();
  const bool cpu_has_avx2 = cpu_supports_avx2();
  const bool avx2_active = lib_has_avx2 && cpu_has_avx2;

  std::cout << "AVX2 in togguznative build: " << (lib_has_avx2 ? "yes" : "no") << '\n';
  std::cout << "AVX2 supported by CPU: " << (cpu_has_avx2 ? "yes" : "no") << '\n';
  std::cout << "AVX2 path active: " << (avx2_active ? "yes" : "no") << '\n';

  init_masks();

  std::uint64_t total_turns = 0;
  const auto start = std::chrono::steady_clock::now();

  for (std::uint32_t game_idx = 0; game_idx < games_to_play; ++game_idx) {
    total_turns += play_single_game();
  }

  const auto end = std::chrono::steady_clock::now();
  const auto elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

  const double avg_ms =
      games_to_play > 0 ? static_cast<double>(elapsed_ms) / games_to_play : 0.0;
  const double avg_turns =
      games_to_play > 0 ? static_cast<double>(total_turns) / games_to_play : 0.0;

  std::cout << "Played games: " << games_to_play << '\n';
  std::cout << "Total time: " << elapsed_ms << " ms\n";
  std::cout << "Average time per game: " << avg_ms << " ms\n";
  std::cout << "Average turns per game: " << avg_turns << '\n';

  return 0;
}
