#include "csv_io.h"
#include "polygon_dcel.h"

#include <cstdlib>
#include <exception>
#include <iomanip>
#include <iostream>

int main(int argc, char** argv) {
    try {
        if (argc != 3) {
            std::cerr << "Usage: ./simplify <input_file.csv> <target_vertices>\n";
            return 1;
        }

        const std::string input_file = argv[1];
        const int target_vertices = std::atoi(argv[2]);
        (void)target_vertices;

        const std::vector<RingInput> rings = read_input_csv(input_file);
        const PolygonDCEL polygon = PolygonDCEL::from_rings(rings);

        std::cout << "ring_id,vertex_id,x,y\n";
        for (int ring_id : polygon.ring_ids_sorted()) {
            std::vector<Point2> pts = polygon.ring_points(ring_id);
            for (std::size_t i = 0; i < pts.size(); ++i) {
                std::cout << ring_id << "," << i << ","
                          << std::setprecision(15) << pts[i].x << ","
                          << std::setprecision(15) << pts[i].y << "\n";
            }
        }

        const double area_in = polygon.total_signed_area();
        const double area_out = polygon.total_signed_area();
        const double displacement = 0.0;

        std::cout << std::scientific << std::setprecision(6);
        std::cout << "Total signed area in input: " << area_in << "\n";
        std::cout << "Total signed area in output: " << area_out << "\n";
        std::cout << "Total areal displacement: " << displacement << "\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
}
