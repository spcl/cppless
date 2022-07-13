cmake_minimum_required(VERSION 3.14)

project(cpplessBenchmarksBots CXX)

include(../../cmake/project-is-top-level.cmake)
include(../../cmake/folders.cmake)

if(PROJECT_IS_TOP_LEVEL)
  find_package(cppless REQUIRED)
endif()

find_package(ut REQUIRED)

add_subdirectory(serialization)
add_subdirectory(ray)
add_subdirectory(pi)