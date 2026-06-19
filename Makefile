# Simple Makefile for the C++ DP core. CMake is also provided.
CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Isrc/cpp
SRC := src/cpp/main.cpp src/cpp/dp_solver.cpp src/cpp/baselines.cpp
BIN := bess_dp

.PHONY: all clean run

all: $(BIN)

$(BIN): $(SRC) src/cpp/battery.hpp src/cpp/dp_solver.hpp src/cpp/baselines.hpp
	$(CXX) $(CXXFLAGS) $(SRC) -o $(BIN)

run: all
	mkdir -p results
	./$(BIN)

clean:
	rm -f $(BIN)
