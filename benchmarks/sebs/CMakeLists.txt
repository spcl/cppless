cmake_minimum_required(VERSION 3.14)

project(cpplessBenchmarksSeBS CXX)

include(../../cmake/project-is-top-level.cmake)
include(../../cmake/folders.cmake)

if(PROJECT_IS_TOP_LEVEL)
  find_package(cppless REQUIRED)
endif()

add_subdirectory(thumbnailer)
add_subdirectory(image-recognition)
add_subdirectory(graph-pagerank)
