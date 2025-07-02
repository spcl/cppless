

Build instructions

### OpenCV

```
wget --no-check-certificate https://github.com/opencv/opencv/archive/refs/tags/4.5.0.zip
unzip 4.5.0.zip
mkdir build && cd build
cmake -DCMAKE_CXX_COMPILER=cppless/llvm-project/build/bin/clang++ -D CMAKE_BUILD_TYPE=RELEASE  -D BUILD_EXAMPLES=Off -D OPENCV_GENERATE_PKGCONFIG=ON -DCMAKE_INSTALL_PREFIX=$(pwd)/../install -DBUILD_LIST=core,imgproc,imgcodecs ../opencv-4.5.0
make && make install
```

## Torch

```
wget --no-check-certificate -q https://download.pytorch.org/libtorch/cpu/libtorch-cxx11-abi-shared-with-deps-1.11.0%2Bcpu.zip
unzip libtorch-cxx11-abi-shared-with-deps-1.11.0+cpu.zip

wget --no-check-certificate -q https://github.com/pytorch/vision/archive/refs/tags/v0.10.0.zip
unzip -q v0.10.0.zip

mkdir build_torchvision && cd build_torchvision
cmake -DCMAKE_CXX_COMPILER=cppless/llvm-project/build/bin/clang++ -D CMAKE_BUILD_TYPE=RELEASE -DCMAKE_INSTALL_PREFIX=$(pwd)/../install -DCMAKE_PREFIX_PATH=libtorch/share/cmake ../vision-0.10.0
make && make install
```

## igraph

```
wget https://github.com/igraph/igraph/releases/download/0.10.15/igraph-0.10.15.tar.gz

mkdir build_igraph && cd build_igraph
cmake -DCMAKE_INSTALL_PREFIX=$(pwd)/../install -DIGRAPH_OPENMP_SUPPORT=OFF ../igraph-0.10.15
make && make install
```

### Cppless Flags


```
OpenCV_DIR=install/lib/cmake/opencv4 cmake -DCPPLESS_TOOLCHAIN=native -DBUILD_EXAMPLES=OFF -DBUILD_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="install/lib/cmake/opencv4;install/share/cmake/TorchVision;libtorch/share/cmake/Torch;install/lib/cmake/igraph" cppless
```
