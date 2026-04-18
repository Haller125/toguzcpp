#include <array>
#include <span>
#include <unordered_map>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <random>
#include "toguznative.h"

#define WIN_QVALUE 10000.0f

u_int8_t NO_LEGAL_MOVE = 255;

namespace qlrng{
    float k = 0.0f;
    float min_epsilon = 0.0001f;

    using QActionValues = std::array<float, 9>;
    
    struct State{
        // 3 = is even, is attacking opponent side, is being attacked 
        std::array<bool, 9 * 3> state{};
        bool player_turn = 0;

        bool operator==(const State& other) const {
            return state == other.state && player_turn == other.player_turn;
        }
    };

    State make_state(const ToguzNative& game, bool player_turn) {
    State s;
    s.player_turn = player_turn;

    int my_offset = player_turn * 9;
    int op_offset = (!player_turn) * 9;

    // 1. Local buffer to prevent threading issues
    std::array<bool, 9> opponent_attacks_my_cells = {false};

    // 2. Safely calculate which of MY cells the opponent is attacking
    for (std::size_t i = 0; i < 9; ++i) {
        int idx = op_offset + i;
        int cell_value = game.cells[idx];
        
        if (cell_value == 0) continue; // Empty holes don't attack

        // Toguz Kumalak landing logic: 
        // If 1 stone, lands in idx + 1. If >1, lands in idx + stones - 1.
        int target_idx = (cell_value == 1) ? (idx + 1) % 18 : (idx + cell_value - 1) % 18;

        // If the landing hole is on MY side, flag it in the buffer
        if (target_idx / 9 == player_turn) {
            opponent_attacks_my_cells[target_idx % 9] = true;
        }
    }

    // 3. Evaluate the current player's side accurately
    for (std::size_t i = 0; i < 9; ++i) {
        int idx = my_offset + i;
        int cell_value = game.cells[idx];

        bool is_empty = (cell_value == 0);
        
        // State 1: Is Even (and NOT empty)
        bool is_even = (!is_empty && cell_value % 2 == 0);

        // State 2: Is Attacking Opponent Side
        bool is_attacking = false;
        if (!is_empty) {
            int target_idx = (cell_value == 1) ? (idx + 1) % 18 : (idx + cell_value - 1) % 18;
            is_attacking = (target_idx / 9 != player_turn); // Lands on opponent side
        }

        // State 3: Is Being Attacked
        bool is_being_attacked = opponent_attacks_my_cells[i];

        // Assign to the linear array
        int offset = i * 3;
        s.state[offset] = is_even;
        s.state[offset + 1] = is_attacking;
        s.state[offset + 2] = is_being_attacked;
    }

    return s;
}

    struct StateHash {
        std::size_t operator()(const State& s) const {
            std::size_t hash = std::hash<bool>()(s.player_turn);
            for (const auto& b : s.state) {
                hash = hash * 31 + std::hash<bool>()(b);
            }
            return hash;
        }
    };


    struct QTable{
        std::unordered_map<State, QActionValues, StateHash> table;
        float learning_rate = 0.1f;
        float discount_factor = 0.9f;
        float epsilon = 0.1f;
        float epsilon_decay = 1.0f;

        QTable(float lr = 0.1f, float df = 0.9f, float eps = 0.1f, float eps_decay = 1.0f) : learning_rate(lr), discount_factor(df), epsilon(eps), epsilon_decay(eps_decay) {}

        QActionValues& get_or_create_q_values(const State& state) {
            auto it = table.find(state);
            if (it == table.end()) {
                auto [new_it, _] = table.emplace(state, QActionValues{});
                return new_it->second;
            }
            return it->second;
        }

        void update_q_value(const State& current_state, const State& next_state, u_int8_t action_idx, float reward) {
            auto& current_q_values = get_or_create_q_values(current_state);
            auto& next_q_values = get_or_create_q_values(next_state);

            float max_next_q = *std::max_element(next_q_values.begin(), next_q_values.end());
            u_int8_t relative_action = action_idx % 9;
            current_q_values[relative_action] += learning_rate * (reward + discount_factor * max_next_q - current_q_values[relative_action]);
        }

        void decay_epsilon(int episode) {
            float e = epsilon * (k/(k+episode));

            if (e < min_epsilon) {
                epsilon = min_epsilon;
            } else {
                epsilon = e;
            }
        }

        u_int8_t select_best_action(const State& state) {
            auto& q_values = get_or_create_q_values(state);
            return std::distance(q_values.begin(), std::max_element(q_values.begin(), q_values.end()));
        }

        u_int8_t select_action(const State& state) {
            if (static_cast<float>(rand()) / RAND_MAX < epsilon) {
                return rand() % 9; 
            }
            return select_best_action(state); 
        }

        float get_q_value(const State& state, u_int8_t action_idx) {
            auto& q_values = get_or_create_q_values(state);
            return q_values[action_idx % 9];
        }
        
        void save_to_file(const std::string& filename) {
            std::ofstream file(filename);
            for (const auto& [state, q_values] : table) {
                file << state.player_turn << " ";
                for (const auto& cell : state.state) {
                    file << static_cast<int>(cell) << " ";
                }
                for (const auto& q_value : q_values) {
                    file << q_value << " ";
                }
                file << "\n";
            }
        }

        void load_from_file(const std::string& filename) {
            std::ifstream file(filename);
            if (!file.is_open()) {
                return;
            }
            table.clear();
            std::string line;
            while (std::getline(file, line)) {
                std::istringstream iss(line);
                State state;
                iss >> state.player_turn;
                for (auto& cell : state.state) {
                    int cell_value;
                    iss >> cell_value;
                    cell = static_cast<u_int8_t>(cell_value);
                }
                QActionValues q_values;
                for (auto& q_value : q_values) {
                    iss >> q_value;
                }
                table[state] = q_values;
            }
        }
    };

    // q table, but it works board directly
    struct QToguzTable { 
        QTable q_table;

        QToguzTable(float lr = 0.1f, float df = 0.9f, float eps = 1.0f, float eps_decay = 1.0f) : q_table(lr, df, eps, eps_decay) {}

        QActionValues& get_or_create_q_values(const ToguzNative& game, bool player_turn) {
            State state = make_state(game, player_turn);
            return q_table.get_or_create_q_values(state);
        }

        void decay_epsilon(int episode) {
            q_table.decay_epsilon(episode);
        }

        void update_q_value(const ToguzNative& current_game, const ToguzNative& next_game, bool current_player_turn, u_int8_t action_idx, float reward) {
            State current_state = make_state(current_game, current_player_turn);
            State next_state = make_state(next_game, !current_player_turn);
            q_table.update_q_value(current_state, next_state, action_idx, reward);
        }

        u_int8_t select_best_action(const ToguzNative& game, bool player_turn) {
            State state = make_state(game, player_turn);
            return q_table.select_best_action(state);
        }

        u_int8_t select_action(const ToguzNative& game, bool player_turn) {
            State state = make_state(game, player_turn);
            return q_table.select_action(state);
        }

        float get_q_value(const ToguzNative& game, bool player_turn, u_int8_t action_idx) {
            State state = make_state(game, player_turn);
            return q_table.get_q_value(state, action_idx);
        }

        void save_to_file(const std::string& filename) {
            q_table.save_to_file(filename);
        }

        void load_from_file(const std::string& filename) {
            q_table.load_from_file(filename);
        }

        void print_q_table() {
            std::cout << "Q-table size: " << q_table.table.size() << std::endl;
        }
    };

    u_int8_t select_legal_action(const ToguzNative& game, bool player_turn, QToguzTable& q_toguz_table) {
        std::array<u_int8_t, 9> legal_moves;
        int legal_count = 0;
        game.get_legal_moves(player_turn, legal_moves, legal_count);

        // 1. Handle terminal/no-move states
        if (legal_count == 0) {
            return NO_LEGAL_MOVE;
        }

        // 2. Exploration: Epsilon-Greedy Random Choice
        // Using <random> is preferred over rand() for better distribution
        float roll = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
        if (roll < q_toguz_table.q_table.epsilon) {
            return legal_moves[rand() % legal_count]; 
        }

        // 3. Exploitation: Pick the best move among LEGAL options
        u_int8_t best_move = legal_moves[0];
        float max_q = -std::numeric_limits<float>::infinity();

        for (int i = 0; i < legal_count; ++i) {
            u_int8_t move = legal_moves[i];
            float q_val = q_toguz_table.get_q_value(game, player_turn, move);
            
            if (q_val > max_q) {
                max_q = q_val;
                best_move = move;
            }
        }

        return best_move;
    }

    u_int8_t select_random_legal_action(const ToguzNative& game, bool player_turn) {
        std::array<u_int8_t, 9> legal_moves;
        int legal_count = 0;
        game.get_legal_moves(player_turn, legal_moves, legal_count);

        if (legal_count == 0) {
            return NO_LEGAL_MOVE;
        }

        return legal_moves[rand() % legal_count];
    }


    struct QSelfLearningProcess {
        QToguzTable q_toguz_table;

        void train(int episodes, const std::string& save_filename) {
            init_masks();
            for (int i = 0; i < episodes; ++i) {
                ToguzNative game;
                run_episode(game, i);
                if ((i + 1) % 1000 == 0) {
                    float progress = static_cast<float>(i) / episodes;
                    std::cout << "Progress: " << progress << " completed. Q-table size: " << q_toguz_table.q_table.table.size() << std::endl;
                    sanity_test_against_random(1000); // Run a quick sanity test every 1000 episodes
                }
            }
            save_q_table(save_filename);
        }

        void save_q_table(const std::string& filename) {
            q_toguz_table.save_to_file(filename);
        }

        void load_q_table(const std::string& filename) {
            q_toguz_table.load_from_file(filename);
        }
        
        float calculate_reward(const ToguzNative& game, const ToguzNative& next_game, bool player_turn) {
            if (next_game.scores[0] >= 82) {
                return (player_turn == 0) ? WIN_QVALUE : -WIN_QVALUE;
            } else if (next_game.scores[1] >= 82) {
                return (player_turn == 1) ? WIN_QVALUE : -WIN_QVALUE;
            } else if (next_game.scores[0] == 81 || next_game.scores[1] == 81) {
                return 0.0f; 
            } 

            // float score_diff_before = game.scores[player_turn] - game.scores[!player_turn];
            float score_diff_after = next_game.scores[player_turn] - next_game.scores[!player_turn];
            return score_diff_after  
            // - score_diff_before
            ;
        }

        void handle_atsyrau(ToguzNative& game, bool player_turn) {
            if (game.is_atsyrau(player_turn)) {
                atsyrau(game, player_turn);
            }
            if (game.is_atsyrau(!player_turn)) {
                atsyrau(game, !player_turn);
            }
        }

        void run_episode(ToguzNative& game, int episode, bool player_turn = 0) {
            bool switch_loop = true;
            while (switch_loop) {
                ToguzNative next_game = game;                
                u_int8_t action_idx = select_legal_action(next_game, player_turn, q_toguz_table);

                next_game.move(action_idx);
                handle_atsyrau(next_game, player_turn);
                float reward = calculate_reward(game, next_game, player_turn);
                
                q_toguz_table.q_table.discount_factor *= -1.0f;
                q_toguz_table.update_q_value(game, next_game, player_turn, action_idx, reward);
                q_toguz_table.q_table.discount_factor *= -1.0f;

                if (next_game.scores[0] >= 81 || next_game.scores[1] >= 81 || next_game.is_atsyrau(player_turn) || next_game.is_atsyrau(!player_turn)) {
                    switch_loop = false;
                }
                
                game = next_game;
                player_turn = !player_turn;
            }

            q_toguz_table.decay_epsilon(episode);
        }

        void sanity_test_against_random(int games = 200, bool alternate_sides = true) {
            if (games <= 0) {
                std::cout << "Sanity test skipped: games must be > 0." << std::endl;
                return;
            }

            float original_epsilon = q_toguz_table.q_table.epsilon;
            q_toguz_table.q_table.epsilon = 0.0f;

            int wins = 0;
            int losses = 0;
            int draws = 0;

            for (int game_idx = 0; game_idx < games; ++game_idx) {
                ToguzNative game;
                bool learner_side = (alternate_sides && (game_idx % 2 == 1)) ? 1 : 0;
                bool player_turn = 0;

                bool switch_loop = true;
                while (switch_loop) {
                    u_int8_t action_idx = NO_LEGAL_MOVE;

                    if (player_turn == learner_side) {
                        action_idx = select_legal_action(game, player_turn, q_toguz_table);
                    } else {
                        action_idx = select_random_legal_action(game, player_turn);
                    }

                    if (action_idx == NO_LEGAL_MOVE) {
                        atsyrau(game, player_turn);
                    } else {
                        game.move(action_idx);
                    }

                    handle_atsyrau(game, player_turn);

                    if (game.scores[0] >= 81 || game.scores[1] >= 81 || game.is_atsyrau(0) || game.is_atsyrau(1)) {
                        switch_loop = false;
                    }

                    player_turn = !player_turn;
                }

                if (game.scores[learner_side] > game.scores[!learner_side]) {
                    ++wins;
                } else if (game.scores[learner_side] < game.scores[!learner_side]) {
                    ++losses;
                } else {
                    ++draws;
                }
            }

            q_toguz_table.q_table.epsilon = original_epsilon;

            float win_rate = static_cast<float>(wins) * 100.0f / static_cast<float>(games);
            std::cout << "Sanity test vs random: games=" << games
                      << " W/L/D=" << wins << "/" << losses << "/" << draws
                      << " win_rate=" << win_rate << "%" << std::endl;
        }
    };
}

int main(int argc, char* argv[]) {
    srand(static_cast<unsigned int>(42));

    std::string mode = "self";
    int episodes = 50000;
    int sanity_games = 0;
    std::string load_file = "";
    std::string save_file = "q_table.txt";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "-m" || arg == "--mode") && i + 1 < argc) {
            mode = argv[++i];
        } else if ((arg == "-e" || arg == "--episodes") && i + 1 < argc) {
            episodes = std::stoi(argv[++i]);
        } else if (arg == "--sanity-games" && i + 1 < argc) {
            sanity_games = std::stoi(argv[++i]);
        } else if ((arg == "-l" || arg == "--load") && i + 1 < argc) {
            load_file = argv[++i];
        } else if ((arg == "-s" || arg == "--save") && i + 1 < argc) {
            save_file = argv[++i];
        } else if (arg == "-h" || arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [options]\n"
                      << "Options:\n"
                      << "  -m, --mode <random|self>   Training mode (default: self)\n"
                      << "  -e, --episodes <num>       Number of episodes (default: 50000)\n"
                      << "  --sanity-games <num>       Run self-policy sanity test vs random after training\n"
                      << "  -l, --load <file>          Load Q-table from file\n"
                      << "  -s, --save <file>          Save Q-table to file (default: q_table.txt)\n"
                      << "  -h, --help                 Show this help message\n";
            return 0;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            return 1;
        }
    }
    qlrng::k = episodes;

    if (mode == "self") {
        qlrng::QSelfLearningProcess self_process;
        if (!load_file.empty()) {
            self_process.load_q_table(load_file);
        }
        self_process.train(episodes, save_file);
        if (sanity_games > 0) {
            self_process.sanity_test_against_random(sanity_games);
        }
    } else {
        std::cerr << "Invalid mode: " << mode << ". Use 'random' or 'self'.\n";
        return 1;
    }

    return 0;
}
