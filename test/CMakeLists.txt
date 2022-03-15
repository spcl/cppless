cmake_minimum_required(VERSION 3.14)

project(cpplessTests LANGUAGES CXX)

include(../cmake/project-is-top-level.cmake)
include(../cmake/folders.cmake)
include(../cmake/windows-set-path.cmake)

if(PROJECT_IS_TOP_LEVEL)
  find_package(cppless REQUIRED)
  enable_testing()
endif()

add_executable(cppless_test source/cppless_test.cpp)
target_link_libraries(cppless_test PRIVATE cppless::cppless)
target_compile_features(cppless_test PRIVATE cxx_std_20)

add_test(NAME cppless_test COMMAND cppless_test)

add_folders(Test)
