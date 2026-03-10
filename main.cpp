#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <random>

#include "toguznative.h"

#define GAME_CLASS ToguzNative

namespace {

constexpr std::uint32_t kMaxTurnsPerGame = 5000;

std::uint64_t play_single_game(std::mt19937& rng) {
    GAME_CLASS game;
    bool player = false;
    std::uint32_t turn_count = 0;

    std::array<u_int8_t, 9> legal_moves{};
    int legal_count = 0;

    while (turn_count < kMaxTurnsPerGame) {
        game.get_legal_moves(player, legal_moves, legal_count);

        std::uniform_int_distribution<int> pick_move(0, legal_count - 1);
        const u_int8_t move_idx = legal_moves[static_cast<std::size_t>(pick_move(rng))];
        game.move(static_cast<u_int8_t>(move_idx + (player ? 9 : 0)));

        if (legal_count == 0) {
            for (int i = 9 - (player * 9); i < 18 - (player * 9); ++i) {
                game.scores[1 - player] += game.cells[i];
                game.cells[i] = 0;
            }
        }
        // check if game is over
        if (legal_count == 0 || game.scores[0] >= 82 || game.scores[1] >= 82) {
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

  std::random_device rd;
  std::mt19937 rng(rd());

  std::uint64_t total_turns = 0;
  const auto start = std::chrono::steady_clock::now();

  for (std::uint32_t game_idx = 0; game_idx < games_to_play; ++game_idx) {
    total_turns += play_single_game(rng);
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
