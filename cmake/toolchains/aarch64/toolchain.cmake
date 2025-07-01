# based on the linux-musl toolchain, but for aarch64

set(CMAKE_SYSTEM_NAME "Linux")
set(CMAKE_SYSTEM_PROCESSOR "ARM")
set(TOOLCHAIN_TRIPLE "aarch64-amazon-linux")

set(CMAKE_CROSSCOMPILING 1)
set(ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/../../..")
cmake_path(SET ROOT_DIR NORMALIZE "${ROOT_DIR}")

set(CPPLESS_SERVERLESS 1)
set(CPPLESS_STATIC_LINKAGE 1)

set(COMPILER_BIN ${ROOT_DIR}/llvm-project/build/bin/)

set(CMAKE_ASM_COMPILER ${COMPILER_BIN}/clang)
set(CMAKE_ASM_COMPILER_TARGET ${TOOLCHAIN_TRIPLE})
set(CMAKE_C_COMPILER clang)
set(CMAKE_C_COMPILER ${COMPILER_BIN}/clang)
set(CMAKE_C_COMPILER_TARGET ${TOOLCHAIN_TRIPLE})
set(CMAKE_CXX_COMPILER ${COMPILER_BIN}/clang++)
set(CMAKE_CXX_COMPILER_TARGET ${TOOLCHAIN_TRIPLE})

#set(SYSROOT "/work/2023/serverless/cppless/experiments_arm_11_03/sysroot")
set(SYSROOT "/work/2023/serverless/cppless/arm_replication/sysroot")
set(CMAKE_SYSROOT ${SYSROOT})

set(COMMON_FLAGS "--target=aarch64-amazon-linux --sysroot=${SYSROOT} --gcc-toolchain=${SYSROOT}/usr/")
set(COMPILER_FLAGS "${COMMON_FLAGS}")

set(CMAKE_C_FLAGS "${COMPILER_FLAGS} ")
set(CMAKE_CXX_FLAGS "${COMPILER_FLAGS} ")

set(LINK_FLAGS "${COMMON_FLAGS} -fuse-ld=${ROOT_DIR}/llvm-project/build/bin/ld.lld --gcc-toolchain=${SYSROOT}/usr/")
set(CMAKE_EXE_LINKER_FLAGS "${COMMON_FLAGS} -fuse-ld=${ROOT_DIR}/llvm-project/build/bin/ld.lld --gcc-toolchain=${SYSROOT}/usr/")
set(CMAKE_SHARED_LINKER_FLAGS "${COMMON_FLAGS} -fuse-ld=${ROOT_DIR}/llvm-project/build/bin/ld.lld --gcc-toolchain=${SYSROOT}/usr/")

# these variables tell CMake to avoid using any binary it finds in 
# the sysroot, while picking headers and libraries exclusively from it 
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
