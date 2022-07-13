cmake_minimum_required(VERSION 3.14)

project(cpplessBenchmarksCustomRay CXX)

add_executable("benchmark_custom_pi" dispatcher.cpp)
target_compile_options("benchmark_custom_pi" PRIVATE "-ffast-math" "-march=native")
target_link_libraries("benchmark_custom_pi" PRIVATE cppless::cppless)
target_compile_features("benchmark_custom_pi" PRIVATE cxx_std_20)
aws_lambda_target("benchmark_custom_pi")
aws_lambda_serverless_target("benchmark_custom_pi")