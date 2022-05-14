#!/usr/bin/env bash
set -ex

# Install:
#  * git
#  * cmake
#  * clang
#  * compiler-rt
#  * libc++
#  * libc++abi
#  * libunwind

install_tools() {
	echo "Installing tools"
	yum install -y gcc gcc-c++ git-core tar make
}

build_and_install_cmake() {
	echo "Installing recent cmake..."
	version_minor=3.7
	version=3.7.2
	src=/tmp/cmake
	md5=79bd7e65cd81ea3aa2619484ad6ff25a
	mkdir -p $src

	curl https://cmake.org/files/v${version_minor}/cmake-${version}.tar.gz > $src/cmake-${version}.tar.gz
	echo "$md5 cmake-${version}.tar.gz" > $src/cmake-${version}.tar.gz.md5
	( cd $src && md5sum -c $src/cmake-${version}.tar.gz.md5 )
	tar xfz $src/cmake-${version}.tar.gz -C $src
	( cd $src/cmake-${version} && ./configure && make && make install )
}

download_llvm() {
	echo "Downloading llvm..."
	version=llvmorg-14.0.3
	src=/tmp/llvm
	url=https://github.com/llvm/llvm-project
	mkdir -p $src

	args="--depth 1 --branch $version --single-branch"
	git clone $args $url/llvm.git $src

#	( cd $src/tools && git clone $args $url/clang.git )
	( cd $src/projects && git clone $args $url/compiler-rt )
	( cd $src/projects && git clone $args $url/libcxx )
	( cd $src/projects && git clone $args $url/libcxxabi )
	( cd $src/projects && git clone $args $url/libunwind )
}

build_and_install_llvm() {
	echo "Building llvm..."
	src=/tmp/llvm
	mkdir $src/build
	( cd $src/build && cmake .. -DCMAKE_BUILD_TYPE=Release && make && make install )
}

clean_up() {
	echo "Cleaning up..."
	rm -rf /tmp/cmake /tmp/llvm
	yum autoremove -y
	yum clean all
}

install_tools
build_and_install_cmake
download_llvm
build_and_install_llvm
clean_up