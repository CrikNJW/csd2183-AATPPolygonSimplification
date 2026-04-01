CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -pedantic

SRC := src/main.cpp src/csv_io.cpp src/polygon_dcel.cpp
OBJ := $(SRC:.cpp=.o)

.PHONY: all clean

all: simplify

simplify: $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f src/*.o simplify simplify.exe
