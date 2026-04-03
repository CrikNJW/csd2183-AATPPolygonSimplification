#include <iostream>
#include <cstdlib>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>

int main() {
    std::string results_file = "benchmark_data_v2.csv";
    
    // 1. Initialize the fresh results CSV and write the header
    std::ofstream out_file(results_file);
    if (out_file.is_open()) {
        out_file << "Dataset,TargetVertices,ActualVertices,TimeSeconds,PeakMemoryKB\n";
        out_file.close();
    } else {
        std::cerr << "Failed to create results file.\n";
        return 1;
    }

    // 2. Define the test cases: {filepath, target_vertices}
    // We target half the vertices to ensure the algorithm has to do heavy collapsing
    std::vector<std::pair<std::string, int>> test_cases = {
        {"tests/lake_1000_vertices.csv", 500},
        {"tests/lake_10000_vertices.csv", 5000},
        {"tests/lake_50000_vertices.csv", 25000},
        {"tests/lake_100000_vertices.csv", 50000}
    };

    std::cout << "Starting benchmarks...\n";

    // 3. Loop through and run the main simplify program
    for (const auto& test : test_cases) {
        std::string filename = test.first;
        int target = test.second;

        if (!std::filesystem::exists(filename)) {
            std::cerr << "  [!] Skipping: File not found: " << filename << "\n";
            continue;
        }

        std::cout << "Running " << filename << " -> targeting " << target << " vertices...\n";

        // 4. Build the terminal command
        // > /dev/null throws away the massive standard output so it doesn't flood your console
        // 2>> appends standard error (your STATS timer) to the results file
        std::string command = "./simplify " + filename + " " + std::to_string(target) + 
                              " > /dev/null 2>> " + results_file;

        // 5. Tell the OS to execute
        int result = std::system(command.c_str());

        if (result != 0) {
            std::cerr << "  [!] Warning: Program returned an error on " << filename << "\n";
        }
    }

    std::cout << "\nDone! Open 'benchmark_data_v2.csv' to see your data.\n";
    return 0;
}