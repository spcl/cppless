cmake_minimum_required(VERSION 3.14)

project(cpplessBenchmarksCustomSerialization CXX)

include(../../../cmake/project-is-top-level.cmake)
include(../../../cmake/folders.cmake)

add_executable("benchmark_custom_serialization" benchmark.cpp)
target_link_libraries("benchmark_custom_serialization" PRIVATE cppless::cppless)
target_link_libraries("benchmark_custom_serialization" PRIVATE boost::ut)

add_executable("benchmark_custom_serialization_extended" benchmark_extended.cpp)
target_link_libraries("benchmark_custom_serialization_extended" PRIVATE cppless::cppless)
