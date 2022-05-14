cmake_minimum_required(VERSION 3.14)

project(cpplessBenchmarks CXX)

include(../cmake/project-is-top-level.cmake)
include(../cmake/folders.cmake)

if(PROJECT_IS_TOP_LEVEL)
  find_package(cppless REQUIRED)
endif()

add_subdirectory(bots)