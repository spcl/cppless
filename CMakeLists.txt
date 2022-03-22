cmake_minimum_required(VERSION 3.14)

set( CMAKE_SYSTEM_NAME Darwin )

set( COMPILER_BIN ${CMAKE_SOURCE_DIR}/llvm-project/build/bin )
set( CMAKE_C_COMPILER ${COMPILER_BIN}/clang CACHE PATH "clang" )
set( CMAKE_CXX_COMPILER ${COMPILER_BIN}/clang++ CACHE PATH "clang++" )
set( CMAKE_EXPORT_COMPILE_COMMANDS ON )

include(cmake/prelude.cmake)

project(
    cppless
    VERSION 0.1.0
    DESCRIPTION "Short description"
    HOMEPAGE_URL "https://spcl.inf.ethz.ch"
    LANGUAGES NONE
)

include(cmake/project-is-top-level.cmake)
include(cmake/variables.cmake)

# ---- Declare library ----

add_library(cppless_cppless INTERFACE)
add_library(cppless::cppless ALIAS cppless_cppless)

set_property(
    TARGET cppless_cppless PROPERTY
    EXPORT_NAME cppless
)

target_include_directories(
    cppless_cppless ${warning_guard}
    INTERFACE
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
)

target_compile_features(cppless_cppless INTERFACE cxx_std_20)

find_package(nlohmann_json REQUIRED)
target_link_libraries(cppless_cppless INTERFACE nlohmann_json::nlohmann_json)

find_package(cereal REQUIRED)
target_link_libraries(cppless_cppless INTERFACE cereal::cereal)

# ---- Install rules ----

if(NOT CMAKE_SKIP_INSTALL_RULES)
  include(cmake/install-rules.cmake)
endif()

# ---- Examples ----

if(PROJECT_IS_TOP_LEVEL)
  option(BUILD_EXAMPLES "Build examples tree." "${cppless_DEVELOPER_MODE}")
  if(BUILD_EXAMPLES)
    add_subdirectory(example)
  endif()
endif()

# ---- Developer mode ----

if(NOT cppless_DEVELOPER_MODE)
  return()
elseif(NOT PROJECT_IS_TOP_LEVEL)
  message(
      AUTHOR_WARNING
      "Developer mode is intended for developers of cppless"
  )
endif()

include(cmake/dev-mode.cmake)
