CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -pedantic

COMMON_SRC := src/csv_io.cpp src/polygon_dcel.cpp src/spatial_index.cpp
COMMON_OBJ := $(COMMON_SRC:.cpp=.o)

SIMPLIFY_OBJ := src/main.o $(COMMON_OBJ)
BENCHMARK_OBJ := src/benchmark.o
DATASET_OBJ := src/generate_datasets.o

.PHONY: all clean

all: simplify benchmark generate_datasets

simplify: $(SIMPLIFY_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(SIMPLIFY_OBJ)

benchmark: $(BENCHMARK_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(BENCHMARK_OBJ)

generate_datasets: $(DATASET_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(DATASET_OBJ)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f src/*.o simplify simplify.exe benchmark benchmark.exe generate_datasets generate_datasets.exe
