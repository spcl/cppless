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
set(AR x86_64-unknown-linux-musl-ar)
set(CMAKE_AR ${AR})
set(RANLIB x86_64-unknown-linux-musl-ranlib)
set(CMAKE_RANLIB ${RANLIB})

set(DOCKER_IMAGE aws-example-tc)

set(SYSROOT ${ROOT_DIR}cmake/toolchains/linux-musl/sysroot)
set(OSX_SYSROOT ${SYSROOT})
set(CMAKE_SYSROOT ${SYSROOT})

set(CMAKE_CPP_COMPILER_TARGET "x86_64-alpine-linux-musl")

set(COMMON_FLAGS "--sysroot=${SYSROOT} --prefix=/usr/local/bin/x86_64-unknown-linux-musl- --target=x86_64-alpine-linux-musl")
set(COMPILER_FLAGS "${COMMON_FLAGS}")

set(CMAKE_CXX_FLAGS "${COMPILER_FLAGS} -stdlib=libc++")
set(CMAKE_C_FLAGS "${COMPILER_FLAGS}")

set(LINK_FLAGS "${COMMON_FLAGS} -static -L${ROOT_DIR}cmake/toolchains/linux-musl/sysroot/lib --gcc-toolchain=${ROOT_DIR}cmake/toolchains/linux-musl/sysroot/usr")
set(CMAKE_BUILD_TYPE "Debug")

# these variables tell CMake to avoid using any binary it finds in 
# the sysroot, while picking headers and libraries exclusively from it 
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)