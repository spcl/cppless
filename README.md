# cppless

Cppless is a single source programming model for high performance serverless. It enables you to write applications which offload work to serverless services, without the need to write the glue code needed to connect them.

# Building and installing

Building application that use cppless requires using a custom fork of the clang compiler. The modified version of the llvm-project is included as a submodule in the repository, it can be built by following these steps:
- fetch the submodule
  - `git submodule update --depth` (this will fetch only the latest revision, but will sped up the download)
  - Shallow clones cannot be used for contributing, you'll need to unshallow the clone in order to be able to commit changes to the submodule.
- run `mkdir build && cd build` in the root of the repository (`llvm-project` doesn't support in-tree builds)
- run `cmake -DLLVM_ENABLE_PROJECTS="clang" -DCMAKE_BUILD_TYPE=Release -G "Unix Makefiles" ../llvm`
  - This will build the modified `clang` compiler in a release configuration
- run `make clang` (or `make clang -j <concurrency>` if you want multiple tasks to run in parallel)
- this should produce a `clang` (and `clang++`) executable in the `build/bin` directory

The cmake configuration of this project will automatically use the `clang` executable from the `build/bin` directory, if you want to use cppless in your own project you'll have to instruct your build tool to use that version of clang accordingly

See the [BUILDING](BUILDING.md) document.

# Contributing

See the [CONTRIBUTING](CONTRIBUTING.md) document.

# Benchmarks

Implementations

- fft - (partially implemented, probably doesn't make sense)
- sort - (partially implemented, probably doesn't make sense)

- fib - (slow)
- floorplan - implemented
- knapsack - implemented

- nqueens - implemented
- ray-casting - implemented

Ideas
- Mandelbrot?
- k-means
- jacobi iteration (s3)
- nbody (complex?)

- hole punching

# Licensing
