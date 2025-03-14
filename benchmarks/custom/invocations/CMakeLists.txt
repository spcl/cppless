cmake_minimum_required(VERSION 3.14)

project(cpplessBenchmarksCustomRay CXX)

add_executable("benchmark_custom_invocations" dispatcher.cpp)
target_compile_options("benchmark_custom_invocations" PRIVATE "-ffast-math")
target_link_libraries("benchmark_custom_invocations" PRIVATE cppless::cppless)
target_compile_features("benchmark_custom_invocations" PRIVATE cxx_std_20)
aws_lambda_target("benchmark_custom_invocations")
aws_lambda_serverless_target("benchmark_custom_invocations")
