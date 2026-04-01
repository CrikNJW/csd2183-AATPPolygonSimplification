#include <iostream>
#include <fstream>
#include <cmath>
#include <random>
#include <iomanip>
#include <filesystem>
#include <string>

// To use M_PI in some compilers
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void generate_polygon(const std::string& filename, int num_vertices, double noise_level = 0.1) {
    std::cout << "Generating " << filename << " with " << num_vertices << " vertices...\n";
    
    std::ofstream out(filename);
    if (!out.is_open()) {
        std::cerr << "Error: Could not open " << filename << "\n";
        return;
    }

    // Write the exact CSV header required by the project brief
    out << "ring_id,vertex_id,x,y\n";

    // Use a fixed seed so the "random" shapes are exactly the same every time you run it
    std::mt19937 gen(42); 
    std::uniform_real_distribution<double> dist(-noise_level, noise_level);

    int ring_id = 0; // Exterior ring
    double base_radius = 100.0;

    for (int i = 0; i < num_vertices; ++i) {
        double angle = 2.0 * M_PI * i / num_vertices;
        
        // Add procedural noise to make the boundary jagged
        double radius = base_radius + (dist(gen) * 10.0); 

        double x = radius * std::cos(angle);
        double y = radius * std::sin(angle);

        // Format to 6 decimal places to match standard output
        out << ring_id << "," << i << ","
            << std::fixed << std::setprecision(6) << x << "," << y << "\n";
    }
    out.close();
}

int main() {
    // Create a tests directory automatically using C++17 filesystem
    std::filesystem::create_directory("tests");

    // Generate datasets to prove asymptotic scaling
    generate_polygon("tests/lake_1000_vertices.csv", 1000);
    generate_polygon("tests/lake_10000_vertices.csv", 10000);
    generate_polygon("tests/lake_50000_vertices.csv", 50000);
    generate_polygon("tests/lake_100000_vertices.csv", 100000); // The massive test case

    std::cout << "All test datasets generated successfully!\n";
    return 0;
}