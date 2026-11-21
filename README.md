# Area and Topology Preserving Polygon Simplification

This C++ project reduces the number of vertices in a polygon while preserving its topology and controlling area error. The validator checks the resulting geometry against the input and target. The test cases include polygons with holes, islands, and irregular boundaries.

## Run an example

From Linux or Windows Subsystem for Linux, use a C++ compiler and Make to build the simplifier and validator:

    make simplify validate_output
    ./simplify test_cases/input_rectangle_with_two_holes.csv 7 > /tmp/polygon-sample.txt
    ./validate_output --input test_cases/input_rectangle_with_two_holes.csv --output /tmp/polygon-sample.txt --target 7 --tol 0.01

The [test script](build_and_test_wsl.sh) rebuilds the tools, replaces the files under outputs, and runs all 15 cases. The [benchmark data](benchmark_data_v2.csv) and [method reference](area_and_topology_preserving_polygon_simplification.pdf) are also included.
