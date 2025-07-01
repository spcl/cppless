#!/bin/bash
set -e

# Cross-compilation settings based on our toolchain
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOOLCHAIN_TRIPLE="aarch64-amazon-linux"
COMPILER_BIN="${ROOT_DIR}/../../../llvm-project/build/bin/"
SYSROOT="${ROOT_DIR}"/sysroot
GCC_TOOLCHAIN="${SYSROOT}/usr/"
LLD_PATH="${COMPILER_BIN}/ld.lld"

# Compiler and flags
CC="${COMPILER_BIN}/clang"
CXX="${COMPILER_BIN}/clang++"
COMMON_FLAGS="--target=${TOOLCHAIN_TRIPLE} --sysroot=${SYSROOT} --gcc-toolchain=${GCC_TOOLCHAIN}"
CFLAGS="${COMMON_FLAGS}"
CXXFLAGS="${COMMON_FLAGS}"
LDFLAGS="${COMMON_FLAGS} -fuse-ld=${LLD_PATH} --gcc-toolchain=${GCC_TOOLCHAIN}"

# Install dependencies into sysroot
INSTALL_PREFIX="${ROOT_DIR}/sysroot"
BUILD_DIR="${ROOT_DIR}/arm-build"
SOURCE_DIR="${ROOT_DIR}/arm-source"
CORES=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 2)

# Export compilation environment variables
export CC CXX
export CFLAGS CXXFLAGS LDFLAGS
export PKG_CONFIG_PATH="${INSTALL_PREFIX}/lib/pkgconfig"
export LD_LIBRARY_PATH="${INSTALL_PREFIX}/lib"
export BOOST_ROOT="${INSTALL_PREFIX}"
export Boost_DIR="${INSTALL_PREFIX}/lib/cmake/Boost"

# Create necessary directories
mkdir -p $INSTALL_PREFIX $BUILD_DIR $SOURCE_DIR

echo "=== Starting cross-compilation for ARM architecture ==="
echo "- Using toolchain triple: $TOOLCHAIN_TRIPLE"
echo "- Compiler bin directory: $COMPILER_BIN"
echo "- Sysroot: $SYSROOT"
echo "- GCC toolchain: $GCC_TOOLCHAIN"
echo "- LLD path: $LLD_PATH"
echo "- Install prefix: $INSTALL_PREFIX"
echo "- Build directory: $BUILD_DIR"
echo "- Source directory: $SOURCE_DIR"
echo "- Using $CORES cores for compilation"

# Function to download and extract source tarball
download_and_extract() {
  local NAME=$1
  local VERSION=$2
  local URL=$3
  local DIR_NAME=$4 # Optional custom directory name

  if [ -z "$DIR_NAME" ]; then
    DIR_NAME="${NAME}-${VERSION}"
  fi

  if [ ! -d "$SOURCE_DIR/$DIR_NAME" ]; then
    echo "Downloading $NAME $VERSION..."
    mkdir -p "$SOURCE_DIR"
    cd "$SOURCE_DIR"
    wget -q "$URL" -O "${NAME}-${VERSION}.tar.gz"
    tar -xzf "${NAME}-${VERSION}.tar.gz"
    rm "${NAME}-${VERSION}.tar.gz"
    echo "$NAME sources extracted to $SOURCE_DIR/$DIR_NAME"
  else
    echo "$NAME sources already exist at $SOURCE_DIR/$DIR_NAME"
  fi
}

# Function to configure a CMake project with the proper cross-compilation settings
configure_cmake() {
  local SRC_DIR=$1
  local BUILD_DIR=$2
  local EXTRA_ARGS=$3

  cmake -S "$SRC_DIR" -B "$BUILD_DIR" \
    -DCMAKE_INSTALL_PREFIX=$INSTALL_PREFIX \
    -DCMAKE_SYSTEM_NAME=Linux \
    -DCMAKE_SYSTEM_PROCESSOR=ARM \
    -DCMAKE_C_COMPILER=$CC \
    -DCMAKE_CXX_COMPILER=$CXX \
    -DCMAKE_C_FLAGS="$CFLAGS" \
    -DCMAKE_CXX_FLAGS="$CXXFLAGS" \
    -DCMAKE_EXE_LINKER_FLAGS="$LDFLAGS" \
    -DCMAKE_SHARED_LINKER_FLAGS="$LDFLAGS" \
    -DCMAKE_MODULE_PATH="$INSTALL_PREFIX/lib/cmake:$INSTALL_PREFIX/lib64/cmake" \
    -DCMAKE_PREFIX_PATH="$INSTALL_PREFIX" \
    -DCMAKE_FIND_ROOT_PATH=$INSTALL_PREFIX \
    -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER \
    -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY \
    -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY \
    -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=ONLY \
    -DCMAKE_CROSSCOMPILING=1 \
    $EXTRA_ARGS
}

# 1. Install zlib
echo "=== Building zlib 1.2.12 ==="
download_and_extract "zlib" "1.2.12" "https://zlib.net/fossils/zlib-1.2.12.tar.gz"
mkdir -p "$BUILD_DIR/zlib"
configure_cmake "$SOURCE_DIR/zlib-1.2.12" "$BUILD_DIR/zlib" ""
cd "$BUILD_DIR/zlib"
make -j$CORES
make install
echo "zlib installed successfully"

# 2. Install OpenSSL (no async)
echo "=== Building OpenSSL 3.0.3 ==="
download_and_extract "openssl" "3.0.3" "https://www.openssl.org/source/openssl-3.0.3.tar.gz"
cd "$SOURCE_DIR/openssl-3.0.3"
# For OpenSSL, we need to set CC directly without the full path to work with its build system
export CC_TEMP=$CC
export CC="clang $CFLAGS"
./Configure linux-aarch64 --prefix="$INSTALL_PREFIX" --openssldir="$INSTALL_PREFIX" \
  no-async shared \
  -L$INSTALL_PREFIX/lib -I$INSTALL_PREFIX/include
make -j$CORES
make install_sw
# Restore CC
export CC=$CC_TEMP
unset CC_TEMP
echo "OpenSSL installed successfully"

# 3. Install cereal (header-only)
echo "=== Building cereal 1.3.1 ==="
download_and_extract "cereal" "1.3.1" "https://github.com/USCiLab/cereal/archive/v1.3.1.tar.gz"
mkdir -p "$BUILD_DIR/cereal"
configure_cmake "$SOURCE_DIR/cereal-1.3.1" "$BUILD_DIR/cereal" \
  "-DJUST_INSTALL_CEREAL=ON -DSKIP_PERFORMANCE_COMPARISON=ON"
cd "$BUILD_DIR/cereal"
make -j$CORES
make install
echo "cereal installed successfully"

# 4. Install nlohmann_json (header-only)
echo "=== Building nlohmann_json 3.10.5 ==="
download_and_extract "nlohmann_json" "3.10.5" "https://github.com/nlohmann/json/archive/v3.10.5.tar.gz" "json-3.10.5"
mkdir -p "$BUILD_DIR/nlohmann_json"
configure_cmake "$SOURCE_DIR/json-3.10.5" "$BUILD_DIR/nlohmann_json" \
  "-DJSON_BuildTests=OFF"
cd "$BUILD_DIR/nlohmann_json"
make -j$CORES
make install
echo "nlohmann_json installed successfully"

# 5. Install argparse
echo "=== Building argparse 2.3 ==="
download_and_extract "argparse" "2.3" "https://github.com/p-ranav/argparse/archive/v2.3.tar.gz" "argparse-2.3"
mkdir -p "$BUILD_DIR/argparse"
configure_cmake "$SOURCE_DIR/argparse-2.3" "$BUILD_DIR/argparse" \
  "-DARGPARSE_BUILD_TESTS=OFF -DARGPARSE_BUILD_EXAMPLES=OFF"
cd "$BUILD_DIR/argparse"
make -j$CORES
make install
echo "argparse installed successfully"

7. Install Boost with specified components
echo "=== Building Boost 1.78.0 ==="
download_and_extract "boost" "1.78.0" "https://archives.boost.io/release/1.78.0/source/boost_1_78_0.tar.gz" "boost_1_78_0"
cd "$SOURCE_DIR/boost_1_78_0"

# Special handling for Boost - create a user-config.jam file
cat >user-config.jam <<EOF
using clang : cross : ${CXX} :
    <cxxflags>"${CXXFLAGS}"
    <linkflags>"${LDFLAGS}"
    ;
EOF

# Bootstrap with the system compiler (not the cross compiler)
./bootstrap.sh --prefix=$INSTALL_PREFIX --with-toolset=clang --with-libraries=system,thread,atomic,graph

# Build boost with cross-compilation settings
./b2 --user-config=user-config.jam \
  toolset=clang-cross \
  target-os=linux \
  architecture=arm \
  address-model=64 \
  link=shared \
  threading=multi \
  variant=release \
  --prefix=$INSTALL_PREFIX \
  --layout=system \
  -j$CORES \
  install
echo "Boost installed successfully"

# 7. Install libnghttp2 with asio (requires Boost)
echo "=== Building libnghttp2 1.47.0 ==="
download_and_extract "libnghttp2" "1.47.0" "https://github.com/nghttp2/nghttp2/releases/download/v1.47.0/nghttp2-1.47.0.tar.gz" "nghttp2-1.47.0"
mkdir -p "$BUILD_DIR/nghttp2"

# Set boost-specific cmake parameters to help find Boost
configure_cmake "$SOURCE_DIR/nghttp2-1.47.0" "$BUILD_DIR/nghttp2" \
  "-DENABLE_ASIO_LIB=ON \
     -DENABLE_EXAMPLES=OFF \
     -DENABLE_APP=OFF \
     -DENABLE_PYTHON_BINDINGS=OFF \
     -DBOOST_ROOT=$INSTALL_PREFIX \
     -DBoost_NO_SYSTEM_PATHS=ON \
     -DBoost_NO_BOOST_CMAKE=ON"

cd "$BUILD_DIR/nghttp2"
make -j$CORES
make install
echo "libnghttp2 installed successfully"

mkdir -p "$BUILD_DIR/curl/"
cat >"$BUILD_DIR/curl/curl_cache.cmake" <<EOF
# Pre-set variables that would normally be determined by try_run
SET(HAVE_POLL_FINE_EXITCODE 0 CACHE STRING "Result from TRY_RUN" FORCE)
SET(HAVE_POLL_FINE_WORKS 1 CACHE INTERNAL "Result from TRY_RUN" FORCE)

# Other potentially needed variables
SET(HAVE_FCNTL_O_NONBLOCK 1 CACHE INTERNAL "Have fcntl O_NONBLOCK" FORCE)
SET(HAVE_IOCTL_FIONBIO 1 CACHE INTERNAL "Have ioctl FIONBIO" FORCE)
SET(HAVE_IOCTL_SIOCGIFADDR 1 CACHE INTERNAL "Have ioctl SIOCGIFADDR" FORCE)
SET(HAVE_SOCK_NONBLOCK 1 CACHE INTERNAL "Have SOCK_NONBLOCK" FORCE)
SET(HAVE_SOCKETPAIR 1 CACHE INTERNAL "Have socketpair" FORCE)
EOF

# 8. Install libcurl (depends on zlib and openssl)
echo "=== Building libcurl 7.80.0 ==="
download_and_extract "curl" "7.80.0" "https://curl.se/download/curl-7.80.0.tar.gz" "curl-7.80.0"
configure_cmake "$SOURCE_DIR/curl-7.80.0" "$BUILD_DIR/curl" \
  "-DCURL_USE_OPENSSL=ON -DCURL_USE_LIBSSH2=OFF -DBUILD_TESTING=OFF -DCMAKE_USE_LIBSSH2=OFF -C $BUILD_DIR/curl/curl_cache.cmake"
cd "$BUILD_DIR/curl"
make -j$CORES
make install
echo "libcurl installed successfully"

# 9. Install boost-ext-ut
echo "=== Building libcurl 7.80.0 ==="
download_and_extract "boost-ext-ut" "1.1.8" "https://github.com/boost-ext/ut/archive/refs/tags/v1.1.8.tar.gz" "ut-1.1.8"
mkdir -p "$BUILD_DIR/boost-ext-ut"
configure_cmake "$SOURCE_DIR/ut-1.1.8" "$BUILD_DIR/boost-ext" "-DBOOST_UT_BUILD_TESTS=OFF -DBOOST_UT_BUILD_EXAMPLES=OFF -DBOOST_UT_BUILD_BENCHMARKS=OFF"
cd "$BUILD_DIR/boost-ext"
make -j$CORES
make install
echo "boost-ext installed successfully"

echo "=== All dependencies built and installed successfully ==="
echo "Installation prefix: $INSTALL_PREFIX"
echo
echo "To use these libraries in your project, set in your CMakeLists.txt:"
echo "  set(CMAKE_PREFIX_PATH \"$INSTALL_PREFIX\")"
echo
echo "Then you can use find_package() to locate each dependency."
