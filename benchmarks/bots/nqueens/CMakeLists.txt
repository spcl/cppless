cmake_minimum_required(VERSION 3.14)

project(cpplessBenchmarksBotsNqueens CXX)

include(../../../cmake/project-is-top-level.cmake)
include(../../../cmake/folders.cmake)
include(../../../cmake/aws.cmake)

add_executable("benchmark_bots_nqueens_serial" serial.cpp)
target_link_libraries("benchmark_bots_nqueens_serial" PRIVATE cppless::cppless)
target_compile_features("benchmark_bots_nqueens_serial" PRIVATE cxx_std_20)

add_executable("benchmark_bots_nqueens_threads" threads.cpp)
target_link_libraries("benchmark_bots_nqueens_threads" PRIVATE cppless::cppless)
target_compile_features("benchmark_bots_nqueens_threads" PRIVATE cxx_std_20)

add_executable("benchmark_bots_nqueens_dispatcher_aws" dispatcher.cpp)
target_link_libraries("benchmark_bots_nqueens_dispatcher_aws" PRIVATE cppless::cppless)
target_compile_features("benchmark_bots_nqueens_dispatcher_aws" PRIVATE cxx_std_20)
aws_lambda_target("benchmark_bots_nqueens_dispatcher_aws")
aws_lambda_serverless_target("benchmark_bots_nqueens_dispatcher_aws")