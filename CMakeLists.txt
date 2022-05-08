cmake_minimum_required(VERSION 3.14)

set(CMAKE_C_COMPILER "${CMAKE_SOURCE_DIR}/llvm-project/build/bin/clang" CACHE PATH "clang")
set(CMAKE_CXX_COMPILER "${CMAKE_SOURCE_DIR}/llvm-project/build/bin/clang++" CACHE PATH "clang++")


enable_language(CXX)
message(STATUS "System name: ${CMAKE_SYSTEM_NAME}")
if(CMAKE_CXX_COMPILER_LOADED)
  message(STATUS "Compiler path: ${CMAKE_CXX_COMPILER}")
  message(STATUS "Compiler ID: ${CMAKE_CXX_COMPILER_ID}")
  message(STATUS "Compiler version: ${CMAKE_CXX_COMPILER_VERSION}")
  message(STATUS "Compiler is part of GCC: ${CMAKE_COMPILER_IS_GNUCXX}")
endif()


message(STATUS "ID: ${CMAKE_CXX_COMPILER_VERSION}")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON )

include(cmake/prelude.cmake)
include(FetchContent)

project(
    cppless
    VERSION 0.1.0
    DESCRIPTION "Short description"
    HOMEPAGE_URL "https://spcl.inf.ethz.ch"
    LANGUAGES CXX
)

include(cmake/project-is-top-level.cmake)
include(cmake/variables.cmake)

# ---- Download Dependencies ----

list(APPEND CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR})
list(APPEND CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR})


add_definitions("-std=c++20")
if(NOT EXISTS "${CMAKE_BINARY_DIR}/conan.cmake")
  message(STATUS "Downloading conan.cmake from https://github.com/conan-io/cmake-conan")
  file(DOWNLOAD "https://raw.githubusercontent.com/lukasmoellerch/cmake-conan/lukasmoellerch-patch-2/conan.cmake"
                "${CMAKE_BINARY_DIR}/conan.cmake"
                TLS_VERIFY ON)
endif()

include(${CMAKE_BINARY_DIR}/conan.cmake)

# openssl is overriden to the latest version
# libcurl is needed
conan_cmake_configure(REQUIRES cereal/1.3.1
                               nlohmann_json/3.10.5
                               libnghttp2/1.46.0
                               boost-ext-ut/1.1.8
                               openssl/3.0.2
                               libcurl/7.80.0
                               argparse/2.3
                      GENERATORS cmake_find_package
                      OPTIONS libnghttp2:with_asio=True
                              boost:without_locale=True
                              boost:without_iostreams=True
                              boost:without_log=True
                              boost:without_context=True
                              boost:without_coroutine=True
                              boost:without_fiber=True
                              boost:without_stacktrace=True)

# The find-package conan generator uses the variables which are not necessarily defined
set(_out_libraries "")
set(_out_libraries_target "")
set(0 1)
set(xcode_sysroot_option "")

# Use auto-detection for host settings (will use our custom cpp compiler)
conan_cmake_autodetect(CONAN_SETTINGS)


include(cmake/conan-utils.cmake)
# We need some setting for the build system to force cross compilation
conan_utils_host_system_name(CONAN_HOST_OS)

# Construct host compilation environment
set(ENV_HOST "")
if (DEFINED CMAKE_C_COMPILER) 
  list(APPEND ENV_HOST "CC=${CMAKE_C_COMPILER}")
endif()
if (DEFINED CMAKE_CXX_COMPILER) 
  list(APPEND ENV_HOST "CXX=${CMAKE_CXX_COMPILER}")
endif()
if (DEFINED AR) 
  list(APPEND ENV_HOST "AR=${AR}")
endif()
if (DEFINED RANLIB) 
  list(APPEND ENV_HOST "RANLIB=${RANLIB}")
endif()
if (DEFINED CMAKE_C_FLAGS)
  list(APPEND ENV_HOST "CFLAGS=${CMAKE_C_FLAGS}")
endif()
if (DEFINED CMAKE_CXX_FLAGS)
  list(APPEND ENV_HOST "CXXFLAGS=${CMAKE_CXX_FLAGS}")
endif()
if (DEFINED LINK_FLAGS)
  list(APPEND ENV_HOST "LDFLAGS=${LINK_FLAGS}")
endif()

conan_cmake_install(PATH_OR_REFERENCE .
                    BUILD missing
                    ENV_HOST "${ENV_HOST}"
                    REMOTE conancenter
                    SETTINGS_BUILD os=${CONAN_HOST_OS}
                    SETTINGS_HOST ${CONAN_SETTINGS})


# ---- Declare library ----

FetchContent_Declare(aws-lambda-cpp
    GIT_REPOSITORY https://github.com/awslabs/aws-lambda-cpp
    GIT_TAG 2ebbf8e5759841b68709e7bd1c9d90808854a204   
)
message(STATUS "FetchContent_Declare(aws-lambda-cpp) ${CMAKE_CXX_FLAGS}")

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

find_package(libnghttp2 REQUIRED)
target_link_libraries(cppless_cppless INTERFACE libnghttp2::libnghttp2)

find_package(argparse REQUIRED)
target_link_libraries(cppless_cppless INTERFACE argparse::argparse)

# ---- Tools ----
add_subdirectory(tools)

# ---- Install rules ----

if(NOT CMAKE_SKIP_INSTALL_RULES)
  include(cmake/install-rules.cmake)
endif()

# ---- Examples ----

if(PROJECT_IS_TOP_LEVEL OR DEFINED CPPLESS_SERVERLESS)
  option(BUILD_EXAMPLES "Build examples tree." "${cppless_DEVELOPER_MODE}")
  if(BUILD_EXAMPLES OR DEFINED CPPLESS_SERVERLESS)
    add_subdirectory(examples)
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
