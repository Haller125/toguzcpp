#include "search.h"
#include "toguznative.h"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    int max_depth = 10;
    u_int64_t time_limit_ms = 1000000;
    std::string output_path = "iterative_deepening_timings.txt";

    if (argc >= 2) {
        max_depth = std::stoi(argv[1]);
    }
    if (argc >= 3) {
        time_limit_ms = static_cast<u_int64_t>(std::stoull(argv[2]));
    }
    if (argc >= 4) {
        output_path = argv[3];
    }

    if (max_depth <= 0 || time_limit_ms == 0) {
        std::cerr << "Invalid arguments. Ensure max_depth is positive and time_limit_ms > 0.\n";
        std::cerr << "Usage: ./iterative_deepening_benchmark [max_depth] [time_limit_ms] [output_file]\n";
        return 1;
    }

    std::ofstream out(output_path, std::ios::app);
    if (!out) {
        std::cerr << "Failed to open output file: " << output_path << '\n';
        return 1;
    }

    init_masks();

    ZobristHash zobrist_hasher;

    for (int depth = 1; depth <= max_depth; ++depth) {
        ToguzNative node;
        TT tt;
        u_int8_t best_move = NOMOVE;

        const auto start = std::chrono::steady_clock::now();
        const int evaluation = iterative_deepening(time_limit_ms, node, false, best_move, tt, zobrist_hasher, depth);
        const auto end = std::chrono::steady_clock::now();

        tt.clear();

        std::this_thread::sleep_for(std::chrono::milliseconds(time_limit_ms));

        const double elapsed_ms =
            std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(end - start).count();

        out << depth << " - " << std::fixed << std::setprecision(3) << elapsed_ms << " ms\n";

        std::cout << "depth=" << depth
                  << " eval=" << evaluation
                  << " best_move=" << static_cast<int>(best_move)
                  << " time_limit=" << time_limit_ms << " ms"
                  << " time=" << std::fixed << std::setprecision(3) << elapsed_ms << " ms\n";
    }

    std::cout << "Results appended to: " << output_path << '\n';
    return 0;
}
