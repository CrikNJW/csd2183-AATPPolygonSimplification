# Area and Topology Preserving Polygon Simplification

**C++ computational geometry project** that reduces polygon vertex counts while preserving topology and controlling area error. It includes a separate validator, automated test cases, and benchmark results.

## At a glance

- **Problem:** simplify polygons without losing important structure such as holes or islands.
- **Approach:** run the C++ simplifier against a chosen target vertex count, then independently validate its geometry and area tolerance.
- **Validation:** a test script runs **15 test cases** and the repository includes benchmark results.
- **Environment:** Linux or Windows Subsystem for Linux (WSL), C++ compiler, and Make.

## Run an example

```bash
make simplify validate_output
./simplify test_cases/input_rectangle_with_two_holes.csv 7 > /tmp/polygon-sample.txt
./validate_output --input test_cases/input_rectangle_with_two_holes.csv --output /tmp/polygon-sample.txt --target 7 --tol 0.01
```

## Tests and evidence

Run `bash build_and_test_wsl.sh` to rebuild the tools, regenerate the files in `outputs/`, and execute the 15 supplied cases.

- [Test inputs](test_cases/)
- [Output examples](outputs/)
- [Benchmark data](benchmark_data_v2.csv)
- [Method reference](area_and_topology_preserving_polygon_simplification.pdf)
- [Source code](src/)
- [Tests](tests/)

## What this repository demonstrates

C++ implementation, computational geometry, algorithm validation, repeatable testing, and comparison against recorded benchmark data.

**For recruiters:** This is a shared coursework repository. Review the commit history and individual contributions before attributing specific implementation work to any one contributor.
