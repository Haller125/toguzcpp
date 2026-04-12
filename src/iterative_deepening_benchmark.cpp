#include "search.h"
#include "toguznative.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <string>
#include <vector>

namespace {

struct BenchmarkStats {
    double mean_ms = 0.0;
    double median_ms = 0.0;
    double min_ms = 0.0;
    double max_ms = 0.0;
    double stddev_ms = 0.0;
    double p90_ms = 0.0;
    double p95_ms = 0.0;
};

double percentile_sorted(const std::vector<double>& sorted_values, double p) {
    if (sorted_values.empty()) {
        return 0.0;
    }

    const double pos = p * static_cast<double>(sorted_values.size() - 1);
    const size_t lo = static_cast<size_t>(std::floor(pos));
    const size_t hi = static_cast<size_t>(std::ceil(pos));
    const double frac = pos - static_cast<double>(lo);
    return sorted_values[lo] * (1.0 - frac) + sorted_values[hi] * frac;
}

BenchmarkStats compute_stats(std::vector<double> values_ms) {
    BenchmarkStats stats;
    if (values_ms.empty()) {
        return stats;
    }

    std::sort(values_ms.begin(), values_ms.end());

    stats.min_ms = values_ms.front();
    stats.max_ms = values_ms.back();

    const double sum = std::accumulate(values_ms.begin(), values_ms.end(), 0.0);
    stats.mean_ms = sum / static_cast<double>(values_ms.size());

    const size_t n = values_ms.size();
    if ((n % 2) == 0) {
        stats.median_ms = (values_ms[n / 2 - 1] + values_ms[n / 2]) / 2.0;
    } else {
        stats.median_ms = values_ms[n / 2];
    }

    double variance = 0.0;
    for (double v : values_ms) {
        const double d = v - stats.mean_ms;
        variance += d * d;
    }
    variance /= static_cast<double>(values_ms.size());
    stats.stddev_ms = std::sqrt(variance);

    stats.p90_ms = percentile_sorted(values_ms, 0.90);
    stats.p95_ms = percentile_sorted(values_ms, 0.95);

    return stats;
}

int apply_random_legal_moves(ToguzNative& game, bool& player_to_move, int random_moves, std::mt19937_64& rng) {
    int applied = 0;

    for (int i = 0; i < random_moves; ++i) {
        std::array<u_int8_t, 9> legal_moves{};
        int count = 0;
        game.get_legal_moves(player_to_move, legal_moves, count);
        if (count == 0) {
            break;
        }

        std::uniform_int_distribution<int> pick(0, count - 1);
        const u_int8_t local_move = legal_moves[pick(rng)];
        const u_int8_t board_idx = local_move;
        game.move(board_idx);

        player_to_move = !player_to_move;
        ++applied;
    }

    return applied;
}

void print_usage(const char* program) {
    std::cerr << "Usage: " << program
              << " [max_depth] [time_limit_ms] [simulations] [random_moves] [output_file]\n";
}

} // namespace

int main(int argc, char* argv[]) {
    int max_depth = 10;
    u_int64_t time_limit_ms = 1000;
    int simulations = 30;
    int random_moves = 20;
    std::string output_path = "iterative_deepening_timings.txt";

    if (argc >= 2) {
        max_depth = std::stoi(argv[1]);
    }
    if (argc >= 3) {
        time_limit_ms = static_cast<u_int64_t>(std::stoull(argv[2]));
    }
    if (argc >= 4) {
        simulations = std::stoi(argv[3]);
    }
    if (argc >= 5) {
        random_moves = std::stoi(argv[4]);
    }
    if (argc >= 6) {
        output_path = argv[5];
    }

    if (max_depth <= 0 || time_limit_ms == 0 || simulations <= 0 || random_moves < 0) {
        std::cerr << "Invalid arguments. Ensure max_depth > 0, time_limit_ms > 0, simulations > 0, random_moves >= 0.\n";
        print_usage(argv[0]);
        return 1;
    }

    std::ofstream out(output_path, std::ios::app);
    if (!out) {
        std::cerr << "Failed to open output file: " << output_path << '\n';
        return 1;
    }

    init_masks();

    ZobristHash zobrist_hasher;
    std::random_device rd;
    std::mt19937_64 rng(rd());

    std::vector<double> elapsed_times_ms;
    elapsed_times_ms.reserve(static_cast<size_t>(simulations));

    out << "=== Monte Carlo Iterative Deepening Benchmark ===\n";
    out << "depth=" << max_depth
        << " time_limit_ms=" << time_limit_ms
        << " simulations=" << simulations
        << " random_moves=" << random_moves << "\n";

    for (int sim = 1; sim <= simulations; ++sim) {
        ToguzNative node;
        TT tt;
        bool player_to_move = false;

        const int applied_random = apply_random_legal_moves(node, player_to_move, random_moves, rng);

        u_int8_t best_move = NOMOVE;
        const auto start = std::chrono::steady_clock::now();
        const int evaluation = iterative_deepening(
            time_limit_ms,
            node,
            player_to_move,
            best_move,
            tt,
            zobrist_hasher,
            max_depth);
        const auto end = std::chrono::steady_clock::now();

        const double elapsed_ms =
            std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(end - start).count();
        elapsed_times_ms.push_back(elapsed_ms);

        out << "sim=" << sim
            << " random_applied=" << applied_random
            << " player=" << (player_to_move ? 1 : 0)
            << " eval=" << evaluation
            << " best_move=" << static_cast<int>(best_move)
            << " time_ms=" << std::fixed << std::setprecision(3) << elapsed_ms << "\n";

        std::cout << "sim=" << sim
                  << " random_applied=" << applied_random
                  << " eval=" << evaluation
                  << " best_move=" << static_cast<int>(best_move)
                  << " time=" << std::fixed << std::setprecision(3) << elapsed_ms << " ms\n";
    }

    const BenchmarkStats stats = compute_stats(elapsed_times_ms);

    std::cout << "\n--- Summary ---\n"
              << "runs=" << simulations << '\n'
              << "mean_ms=" << std::fixed << std::setprecision(3) << stats.mean_ms << '\n'
              << "median_ms=" << stats.median_ms << '\n'
              << "min_ms=" << stats.min_ms << '\n'
              << "max_ms=" << stats.max_ms << '\n'
              << "stddev_ms=" << stats.stddev_ms << '\n'
              << "p90_ms=" << stats.p90_ms << '\n'
              << "p95_ms=" << stats.p95_ms << '\n';

    out << "--- Summary ---\n"
        << "runs=" << simulations << '\n'
        << "mean_ms=" << std::fixed << std::setprecision(3) << stats.mean_ms << '\n'
        << "median_ms=" << stats.median_ms << '\n'
        << "min_ms=" << stats.min_ms << '\n'
        << "max_ms=" << stats.max_ms << '\n'
        << "stddev_ms=" << stats.stddev_ms << '\n'
        << "p90_ms=" << stats.p90_ms << '\n'
        << "p95_ms=" << stats.p95_ms << "\n\n";

    std::cout << "Results appended to: " << output_path << '\n';
    return 0;
}
