#include "toguznative.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>

namespace {

constexpr std::uint32_t kDefaultEpisodes = 100000;
constexpr double kDefaultAlpha = 0.1;
constexpr double kDefaultGamma = 0.99;
constexpr double kDefaultEpsilonStart = 0.2;
constexpr double kDefaultEpsilonMin = 0.02;
constexpr double kDefaultEpsilonDecay = 0.99995;
constexpr std::uint32_t kDefaultLogEvery = 1000;
constexpr std::uint32_t kMaxTurnsPerEpisode = 5000;

constexpr double kDefaultDeltaScale = 1.0;
constexpr double kDefaultTerminalBonus = 100.0;
constexpr double kDefaultTerminalScoreScale = 1.0;

struct Config {
    std::uint32_t episodes = kDefaultEpisodes;
    double alpha = kDefaultAlpha;
    double gamma = kDefaultGamma;
    double epsilon_start = kDefaultEpsilonStart;
    double epsilon_min = kDefaultEpsilonMin;
    double epsilon_decay = kDefaultEpsilonDecay;
    std::uint32_t log_every = kDefaultLogEvery;

    std::string qtable_out_path = "qtable_selfplay.txt";
    std::string qtable_in_path;

    double delta_scale = kDefaultDeltaScale;
    double terminal_bonus = kDefaultTerminalBonus;
    double terminal_score_scale = kDefaultTerminalScoreScale;
};

struct StateKey {
    std::array<u_int8_t, 18> cells{};
    std::array<u_int8_t, 2> tuzdeks{};
    u_int8_t player_turn = 0;

    bool operator==(const StateKey& other) const {
        return cells == other.cells && tuzdeks == other.tuzdeks && player_turn == other.player_turn;
    }
};

struct StateKeyHasher {
    std::size_t operator()(const StateKey& s) const {
        std::uint64_t h = 1469598103934665603ULL;  // FNV-1a offset basis
        for (u_int8_t v : s.cells) {
            h ^= static_cast<std::uint64_t>(v);
            h *= 1099511628211ULL;
        }
        for (u_int8_t v : s.tuzdeks) {
            h ^= static_cast<std::uint64_t>(v);
            h *= 1099511628211ULL;
        }
        h ^= static_cast<std::uint64_t>(s.player_turn);
        h *= 1099511628211ULL;
        return static_cast<std::size_t>(h);
    }
};

using QActionValues = std::array<double, 9>;
using QTable = std::unordered_map<StateKey, QActionValues, StateKeyHasher>;

struct EpisodeStats {
    int winner = 0;  // 1 = player0, -1 = player1, 0 = draw
    std::uint32_t turns = 0;
    double total_reward_p0 = 0.0;
    double total_reward_p1 = 0.0;
};

template <typename T>
bool parse_arg(const char* text, T& out) {
    if (!text) {
        return false;
    }

    std::istringstream iss(text);
    iss >> out;
    return !iss.fail() && iss.eof();
}

bool parse_config(int argc, char** argv, Config& cfg) {
    // Usage:
    // qlearning_selfplay [episodes] [alpha] [gamma] [eps_start] [eps_min] [eps_decay]
    //                    [log_every] [qtable_out] [qtable_in]
    //                    [delta_scale] [terminal_bonus] [terminal_score_scale]
    if (argc > 1 && !parse_arg(argv[1], cfg.episodes)) return false;
    if (argc > 2 && !parse_arg(argv[2], cfg.alpha)) return false;
    if (argc > 3 && !parse_arg(argv[3], cfg.gamma)) return false;
    if (argc > 4 && !parse_arg(argv[4], cfg.epsilon_start)) return false;
    if (argc > 5 && !parse_arg(argv[5], cfg.epsilon_min)) return false;
    if (argc > 6 && !parse_arg(argv[6], cfg.epsilon_decay)) return false;
    if (argc > 7 && !parse_arg(argv[7], cfg.log_every)) return false;
    if (argc > 8) cfg.qtable_out_path = argv[8];
    if (argc > 9) cfg.qtable_in_path = argv[9];
    if (argc > 10 && !parse_arg(argv[10], cfg.delta_scale)) return false;
    if (argc > 11 && !parse_arg(argv[11], cfg.terminal_bonus)) return false;
    if (argc > 12 && !parse_arg(argv[12], cfg.terminal_score_scale)) return false;

    if (cfg.episodes == 0 || cfg.log_every == 0) return false;
    if (cfg.alpha <= 0.0 || cfg.alpha > 1.0) return false;
    if (cfg.gamma < 0.0 || cfg.gamma > 1.0) return false;
    if (cfg.epsilon_start < 0.0 || cfg.epsilon_start > 1.0) return false;
    if (cfg.epsilon_min < 0.0 || cfg.epsilon_min > 1.0) return false;
    if (cfg.epsilon_decay <= 0.0 || cfg.epsilon_decay > 1.0) return false;
    if (cfg.epsilon_min > cfg.epsilon_start) return false;

    return true;
}

void print_usage(const char* exe_name) {
    std::cout
        << "Usage: " << exe_name
        << " [episodes] [alpha] [gamma] [eps_start] [eps_min] [eps_decay]"
        << " [log_every] [qtable_out] [qtable_in] [delta_scale] [terminal_bonus]"
        << " [terminal_score_scale]\n\n"
        << "Defaults:\n"
        << "  episodes=" << kDefaultEpisodes << " alpha=" << kDefaultAlpha
        << " gamma=" << kDefaultGamma << " eps_start=" << kDefaultEpsilonStart
        << " eps_min=" << kDefaultEpsilonMin << " eps_decay=" << kDefaultEpsilonDecay
        << " log_every=" << kDefaultLogEvery
        << " qtable_out=qtable_selfplay.txt"
        << " delta_scale=" << kDefaultDeltaScale
        << " terminal_bonus=" << kDefaultTerminalBonus
        << " terminal_score_scale=" << kDefaultTerminalScoreScale
        << "\n";
}

StateKey make_state_key(const ToguzNative& game, bool player_turn) {
    StateKey key;
    for (std::size_t i = 0; i < key.cells.size(); ++i) {
        key.cells[i] = game.cells[i];
    }
    key.tuzdeks = game.tuzdeks;
    key.player_turn = static_cast<u_int8_t>(player_turn ? 1 : 0);
    return key;
}

QActionValues& get_or_create_q_values(QTable& table, const StateKey& key) {
    auto [it, inserted] = table.emplace(key, QActionValues{});
    if (inserted) {
        it->second.fill(0.0);
    }
    return it->second;
}

std::size_t select_greedy_action_index(const QActionValues& q_values,
                                       const std::array<u_int8_t, 9>& legal_moves,
                                       int legal_count,
                                       std::mt19937_64& rng) {
    double best_value = -std::numeric_limits<double>::infinity();
    std::array<std::size_t, 9> best_indices{};
    std::size_t ties = 0;

    for (int i = 0; i < legal_count; ++i) {
        const std::size_t action_idx = static_cast<std::size_t>(legal_moves[i]);
        const double q = q_values[action_idx];
        if (q > best_value) {
            best_value = q;
            ties = 0;
            best_indices[ties++] = action_idx;
        } else if (q == best_value) {
            best_indices[ties++] = action_idx;
        }
    }

    std::uniform_int_distribution<std::size_t> tie_pick(0, ties - 1);
    return best_indices[tie_pick(rng)];
}

std::size_t select_action_index(const QActionValues& q_values,
                                const std::array<u_int8_t, 9>& legal_moves,
                                int legal_count,
                                double epsilon,
                                std::mt19937_64& rng) {
    std::uniform_real_distribution<double> choose_prob(0.0, 1.0);

    if (choose_prob(rng) < epsilon) {
        std::uniform_int_distribution<int> random_pick(0, legal_count - 1);
        return static_cast<std::size_t>(legal_moves[random_pick(rng)]);
    }

    return select_greedy_action_index(q_values, legal_moves, legal_count, rng);
}

double max_q_over_legal(const QActionValues& q_values,
                        const std::array<u_int8_t, 9>& legal_moves,
                        int legal_count) {
    double best = -std::numeric_limits<double>::infinity();
    for (int i = 0; i < legal_count; ++i) {
        const double q = q_values[legal_moves[i]];
        if (q > best) {
            best = q;
        }
    }
    return best;
}

int winner_from_scores(const ToguzNative& game) {
    if (game.scores[0] > game.scores[1]) return 1;
    if (game.scores[1] > game.scores[0]) return -1;
    return 0;
}

bool save_qtable(const QTable& qtable, const std::string& path) {
    std::ofstream out(path);
    if (!out) {
        return false;
    }

    out << qtable.size() << '\n';
    out << std::setprecision(17);

    for (const auto& [state, q] : qtable) {
        for (std::size_t i = 0; i < state.cells.size(); ++i) {
            out << static_cast<int>(state.cells[i]) << ' ';
        }
        out << static_cast<int>(state.tuzdeks[0]) << ' '
            << static_cast<int>(state.tuzdeks[1]) << ' '
            << static_cast<int>(state.player_turn);

        for (double v : q) {
            out << ' ' << v;
        }
        out << '\n';
    }

    return true;
}

bool load_qtable(const std::string& path, QTable& qtable) {
    if (path.empty()) {
        return true;
    }

    std::ifstream in(path);
    if (!in) {
        return false;
    }

    std::size_t rows = 0;
    in >> rows;
    if (!in) {
        return false;
    }

    qtable.clear();
    qtable.reserve(rows);

    for (std::size_t row = 0; row < rows; ++row) {
        StateKey state;
        int tmp = 0;

        for (std::size_t i = 0; i < state.cells.size(); ++i) {
            in >> tmp;
            if (!in || tmp < 0 || tmp > 255) return false;
            state.cells[i] = static_cast<u_int8_t>(tmp);
        }

        in >> tmp;
        if (!in || tmp < 0 || tmp > 255) return false;
        state.tuzdeks[0] = static_cast<u_int8_t>(tmp);

        in >> tmp;
        if (!in || tmp < 0 || tmp > 255) return false;
        state.tuzdeks[1] = static_cast<u_int8_t>(tmp);

        in >> tmp;
        if (!in || (tmp != 0 && tmp != 1)) return false;
        state.player_turn = static_cast<u_int8_t>(tmp);

        QActionValues q{};
        for (double& qv : q) {
            in >> qv;
            if (!in) return false;
        }

        qtable.emplace(state, q);
    }

    return true;
}

EpisodeStats run_episode(QTable& qtable,
                         std::mt19937_64& rng,
                         double alpha,
                         double gamma,
                         double epsilon,
                         double delta_scale,
                         double terminal_bonus,
                         double terminal_score_scale) {
    ToguzNative game;
    EpisodeStats stats;
    bool player_turn = false;  // false -> player 0, true -> player 1

    std::array<u_int8_t, 9> legal_moves{};
    int legal_count = 0;

    for (std::uint32_t turn = 0; turn < kMaxTurnsPerEpisode; ++turn) {
        const StateKey state = make_state_key(game, player_turn);
        QActionValues& q_values = get_or_create_q_values(qtable, state);

        game.get_legal_moves(player_turn, legal_moves, legal_count);
        const u_int8_t player_idx = static_cast<u_int8_t>(player_turn ? 1 : 0);
        const u_int8_t opponent_idx = static_cast<u_int8_t>(1 - player_idx);

        const int score_before_player = game.scores[player_idx];

        if (legal_count == 0) {
            atsyrau(game, player_turn);
            stats.winner = winner_from_scores(game);
            const int final_diff = static_cast<int>(game.scores[player_idx]) -
                                   static_cast<int>(game.scores[opponent_idx]);

            double reward = 0.0;
            reward += delta_scale * static_cast<double>(static_cast<int>(game.scores[player_idx]) - score_before_player);
            reward += terminal_score_scale * static_cast<double>(final_diff);
            if ((stats.winner == 1 && player_idx == 0) || (stats.winner == -1 && player_idx == 1)) {
                reward += terminal_bonus;
            } else if (stats.winner != 0) {
                reward -= terminal_bonus;
            }

            const std::size_t forced_action = 0;
            q_values[forced_action] += alpha * (reward - q_values[forced_action]);

            if (player_idx == 0) stats.total_reward_p0 += reward;
            else stats.total_reward_p1 += reward;
            stats.turns = turn + 1;
            return stats;
        }

        const std::size_t action_idx = select_action_index(q_values, legal_moves, legal_count, epsilon, rng);
        const u_int8_t board_idx = static_cast<u_int8_t>(action_idx + (player_turn ? 9 : 0));

        game.move(board_idx);

        const int score_after_player = game.scores[player_idx];
        double reward = delta_scale * static_cast<double>(score_after_player - score_before_player);

        const bool reached_terminal = (game.scores[0] >= 82 || game.scores[1] >= 82);
        if (reached_terminal) {
            stats.winner = winner_from_scores(game);
            const int final_diff = static_cast<int>(game.scores[player_idx]) -
                                   static_cast<int>(game.scores[opponent_idx]);
            reward += terminal_score_scale * static_cast<double>(final_diff);
            if ((stats.winner == 1 && player_idx == 0) || (stats.winner == -1 && player_idx == 1)) {
                reward += terminal_bonus;
            } else if (stats.winner != 0) {
                reward -= terminal_bonus;
            }

            q_values[action_idx] += alpha * (reward - q_values[action_idx]);

            if (player_idx == 0) stats.total_reward_p0 += reward;
            else stats.total_reward_p1 += reward;
            stats.turns = turn + 1;
            return stats;
        }

        const bool next_turn = !player_turn;
        const StateKey next_state = make_state_key(game, next_turn);
        QActionValues& next_q = get_or_create_q_values(qtable, next_state);

        std::array<u_int8_t, 9> next_legal_moves{};
        int next_legal_count = 0;
        game.get_legal_moves(next_turn, next_legal_moves, next_legal_count);

        // Zero-sum self-play: value from current player's perspective is negative of next player's best value.
        double bootstrap = 0.0;
        if (next_legal_count > 0) {
            bootstrap = -max_q_over_legal(next_q, next_legal_moves, next_legal_count);
        }

        const double target = reward + gamma * bootstrap;
        q_values[action_idx] += alpha * (target - q_values[action_idx]);

        if (player_idx == 0) stats.total_reward_p0 += reward;
        else stats.total_reward_p1 += reward;

        player_turn = next_turn;
        stats.turns = turn + 1;
    }

    // Safety cutoff: evaluate winner by current scores.
    stats.winner = winner_from_scores(game);
    return stats;
}

}  // namespace

int main(int argc, char** argv) {
    Config cfg;
    if (!parse_config(argc, argv, cfg)) {
        print_usage(argv[0]);
        return 1;
    }

    init_masks();

    QTable qtable;
    if (!cfg.qtable_in_path.empty()) {
        if (!load_qtable(cfg.qtable_in_path, qtable)) {
            std::cerr << "Failed to load Q-table from: " << cfg.qtable_in_path << '\n';
            return 2;
        }
        std::cout << "Loaded Q-table entries: " << qtable.size() << "\n";
    }

    std::random_device rd;
    std::mt19937_64 rng(rd());

    std::uint32_t win_p0 = 0;
    std::uint32_t win_p1 = 0;
    std::uint32_t draws = 0;
    std::uint64_t total_turns = 0;
    double total_reward_p0 = 0.0;
    double total_reward_p1 = 0.0;

    double epsilon = cfg.epsilon_start;

    for (std::uint32_t ep = 1; ep <= cfg.episodes; ++ep) {
        const EpisodeStats stats = run_episode(
            qtable,
            rng,
            cfg.alpha,
            cfg.gamma,
            epsilon,
            cfg.delta_scale,
            cfg.terminal_bonus,
            cfg.terminal_score_scale);

        if (stats.winner > 0) ++win_p0;
        else if (stats.winner < 0) ++win_p1;
        else ++draws;

        total_turns += stats.turns;
        total_reward_p0 += stats.total_reward_p0;
        total_reward_p1 += stats.total_reward_p1;

        epsilon = std::max(cfg.epsilon_min, epsilon * cfg.epsilon_decay);

        if ((ep % cfg.log_every) == 0 || ep == cfg.episodes) {
            const double played = static_cast<double>(ep);
            std::cout << "ep=" << ep
                      << " epsilon=" << std::setprecision(6) << epsilon
                      << " win_p0=" << (100.0 * static_cast<double>(win_p0) / played) << "%"
                      << " win_p1=" << (100.0 * static_cast<double>(win_p1) / played) << "%"
                      << " draw=" << (100.0 * static_cast<double>(draws) / played) << "%"
                      << " avg_turns=" << (static_cast<double>(total_turns) / played)
                      << " q_states=" << qtable.size()
                      << " avg_r_p0=" << (total_reward_p0 / played)
                      << " avg_r_p1=" << (total_reward_p1 / played)
                      << '\n';
        }
    }

    if (!save_qtable(qtable, cfg.qtable_out_path)) {
        std::cerr << "Failed to save Q-table to: " << cfg.qtable_out_path << '\n';
        return 3;
    }

    std::cout << "Saved Q-table entries: " << qtable.size()
              << " to " << cfg.qtable_out_path << '\n';

    return 0;
}
