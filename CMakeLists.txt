cmake_minimum_required(VERSION 4.3)
project(backtester)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

include(FetchContent)

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)

FetchContent_MakeAvailable(googletest)

add_library(backtester_core
  src/analytics.cpp
  src/backtest.cpp
  src/datafeed.cpp
  src/state.cpp
  src/strategy.cpp
)



target_include_directories(backtester_core PUBLIC include)

add_executable(backtester
  src/main.cpp
)

target_compile_options(backtester PRIVATE -Wall -Wextra)

target_link_libraries(backtester PRIVATE backtester_core)

enable_testing()

add_executable(backtesting_test
  tests/backtesting_test.cc
)

target_link_libraries(backtesting_test PRIVATE
  GTest::gtest_main
  backtester_core
)

include (GoogleTest)
gtest_discover_tests(backtesting_test)

# setting up google benchmark

# configures google benchmark to skip its own tests and docs
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  googlebenchmark
  GIT_REPOSITORY https://github.com/google/benchmark.git
  GIT_TAG v1.9.5
)
FetchContent_MakeAvailable(googlebenchmark)

add_executable(backtester_bench benchmarks/main.cpp)
target_link_libraries(backtester_bench PRIVATE
  benchmark::benchmark
  benchmark::benchmark_main
  backtester_core
)

