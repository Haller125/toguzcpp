#include "search.h"
#include "toguznative.h"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    int min_depth = 1;
    int max_depth = 10;
    std::string output_path = "search_depth_timings.txt";

    if (argc >= 2) {
        max_depth = std::stoi(argv[1]);
    }
    if (argc >= 3) {
        min_depth = std::stoi(argv[2]);
    }
    if (argc >= 4) {
        output_path = argv[3];
    }

    if (min_depth <= 0 || max_depth <= 0 || min_depth > max_depth) {
        std::cerr << "Invalid depth range. Use positive values and ensure min_depth <= max_depth.\n";
        std::cerr << "Usage: ./search_depth_benchmark [max_depth] [min_depth] [output_file]\n";
        return 1;
    }

    std::ofstream out(output_path, std::ios::app);
    if (!out) {
        std::cerr << "Failed to open output file: " << output_path << '\n';
        return 1;
    }

    init_masks();

    ZobristHash zobrist_hasher;

    for (int depth = min_depth; depth <= max_depth; ++depth) {
        ToguzNative node;
        TT tt;
        u_int8_t best_move = 18;

        const auto start = std::chrono::steady_clock::now();
        const int evaluation = search_depth(node, depth, LOSE, WIN, false, best_move, tt, zobrist_hasher);
        const auto end = std::chrono::steady_clock::now();

        const double elapsed_ms =
            std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(end - start).count();

        out << depth << " - " << std::fixed << std::setprecision(3) << elapsed_ms << " ms\n";

        std::cout << "depth=" << depth
                  << " eval=" << evaluation
                  << " best_move=" << static_cast<int>(best_move)
                  << " time=" << std::fixed << std::setprecision(3) << elapsed_ms << " ms\n";
    }

    std::cout << "Results appended to: " << output_path << '\n';
    return 0;
}
