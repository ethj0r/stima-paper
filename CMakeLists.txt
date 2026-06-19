cmake_minimum_required(VERSION 3.15)
project(bess_dp_arbitrage CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_CXX_FLAGS_RELEASE "-O2")

add_executable(bess_dp
  src/cpp/main.cpp
  src/cpp/dp_solver.cpp
  src/cpp/baselines.cpp
)
target_include_directories(bess_dp PRIVATE src/cpp)
