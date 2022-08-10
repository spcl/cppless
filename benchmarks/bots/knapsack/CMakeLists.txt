cmake_minimum_required(VERSION 3.14)

project(cpplessBenchmarksBotsNqueens CXX)

include(../../../cmake/project-is-top-level.cmake)
include(../../../cmake/folders.cmake)
include(../../../cmake/aws.cmake)

add_executable("benchmark_bots_knapsack" benchmark.cpp common.cpp dispatcher.cpp serial.cpp threads.cpp)
target_link_libraries("benchmark_bots_knapsack" PRIVATE cppless::cppless)
target_link_libraries("benchmark_bots_knapsack" PRIVATE boost::ut)
target_compile_features("benchmark_bots_knapsack" PRIVATE cxx_std_20)
aws_lambda_target("benchmark_bots_knapsack")
aws_lambda_serverless_target("benchmark_bots_knapsack")

add_executable("benchmark_bots_knapsack_cli" main.cpp common.cpp dispatcher.cpp serial.cpp threads.cpp)
target_link_libraries("benchmark_bots_knapsack_cli" PRIVATE cppless::cppless)
target_link_libraries("benchmark_bots_knapsack_cli" PRIVATE boost::ut)
target_compile_features("benchmark_bots_knapsack_cli" PRIVATE cxx_std_20)
aws_lambda_target("benchmark_bots_knapsack_cli")
aws_lambda_serverless_target("benchmark_bots_knapsack_cli")