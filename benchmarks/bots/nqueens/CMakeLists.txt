cmake_minimum_required(VERSION 3.14)

project(cpplessBenchmarksBotsNqueens CXX)

include(../../../cmake/project-is-top-level.cmake)
include(../../../cmake/folders.cmake)
include(../../../cmake/aws.cmake)

add_executable("benchmark_bots_nqueens" benchmark.cpp common.cpp dispatcher.cpp graph.cpp serial.cpp threads.cpp)
target_link_libraries("benchmark_bots_nqueens" PRIVATE cppless::cppless)
target_link_libraries("benchmark_bots_nqueens" PRIVATE boost::ut)
target_compile_features("benchmark_bots_nqueens" PRIVATE cxx_std_20)
aws_lambda_target("benchmark_bots_nqueens")
aws_lambda_serverless_target("benchmark_bots_nqueens")

add_executable("benchmark_bots_nqueens_cli" main.cpp common.cpp dispatcher.cpp graph.cpp serial.cpp threads.cpp)
target_link_libraries("benchmark_bots_nqueens_cli" PRIVATE cppless::cppless)
target_link_libraries("benchmark_bots_nqueens_cli" PRIVATE boost::ut)
target_compile_features("benchmark_bots_nqueens_cli" PRIVATE cxx_std_20)
aws_lambda_target("benchmark_bots_nqueens_cli")
aws_lambda_serverless_target("benchmark_bots_nqueens_cli")