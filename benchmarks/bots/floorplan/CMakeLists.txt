cmake_minimum_required(VERSION 3.14)

project(cpplessBenchmarksBotsNqueens CXX)

include(../../../cmake/project-is-top-level.cmake)
include(../../../cmake/folders.cmake)
include(../../../cmake/aws.cmake)

add_executable("benchmark_bots_floorplan" benchmark.cpp common.cpp serial.cpp threads.cpp dispatcher.cpp)
target_link_libraries("benchmark_bots_floorplan" PRIVATE cppless::cppless)
target_link_libraries("benchmark_bots_floorplan" PRIVATE boost::ut)
target_compile_features("benchmark_bots_floorplan" PRIVATE cxx_std_20)
aws_lambda_target("benchmark_bots_floorplan")
aws_lambda_serverless_target("benchmark_bots_floorplan")

add_executable("benchmark_bots_floorplan_cli" main.cpp common.cpp serial.cpp threads.cpp dispatcher.cpp)
target_link_libraries("benchmark_bots_floorplan_cli" PRIVATE cppless::cppless)
target_link_libraries("benchmark_bots_floorplan_cli" PRIVATE boost::ut)
target_compile_features("benchmark_bots_floorplan_cli" PRIVATE cxx_std_20)
aws_lambda_target("benchmark_bots_floorplan_cli")
aws_lambda_serverless_target("benchmark_bots_floorplan_cli")