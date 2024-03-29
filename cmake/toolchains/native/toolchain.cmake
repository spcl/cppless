set(CMAKE_SYSTEM_NAME "Linux")
set(CMAKE_SYSTEM_PROCESSOR "x86_64")

set(CMAKE_CROSSCOMPILING 1)
set(ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/../../..")
cmake_path(SET ROOT_DIR NORMALIZE "${ROOT_DIR}")

set(CPPLESS_SERVERLESS 1)
set(CPPLESS_STATIC_LINKAGE 1)

set(COMPILER_BIN ${ROOT_DIR}llvm-project/build/bin/)

set(CMAKE_C_COMPILER ${COMPILER_BIN}clang)
set(CMAKE_CXX_COMPILER ${COMPILER_BIN}clang++)

set(CMAKE_CPP_COMPILER_TARGET "x86_64-alpine-linux-musl")

set(COMMON_FLAGS "")
set(COMPILER_FLAGS "${COMMON_FLAGS} -O3")

set(CMAKE_CXX_FLAGS "${COMPILER_FLAGS} -march=haswell")
# Convince libcurl that the target platform has socket and fcntl_o_nonblock
set(CMAKE_C_FLAGS "${COMPILER_FLAGS} -march=haswell -DHAVE_SOCKET -DHAVE_FCNTL_O_NONBLOCK")

