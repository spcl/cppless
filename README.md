# Cppless

Cppless is a single-source programming model for high-performance serverless. It enables you to write applications that offload work to serverless services, without the need to write the glue code needed to connect them.

The architecture of the compiler, including benchmark evaluation, details can be found in the thesis [Cppless: A single-source programming model for high-performance serverless](https://mcopik.github.io/assets/pdf/students/2022_cppless_moeller.pdf).

## Building and installing

Building applications that use cppless requires using a custom fork of the clang compiler. The modified version of the llvm-project is included as a submodule in the repository, it can be built by following these steps:
- fetch the submodule
  - `git submodule update --init --recursive` (this will fetch only the latest revision, but will sped up the download)
  - Shallow clones cannot be used for contributing, you'll need to unshallow the clone in order to be able to commit changes to the submodule.
- run `mkdir build && cd build` in the root of the repository (`llvm-project` doesn't support in-tree builds)
- run `cmake -DLLVM_ENABLE_PROJECTS="clang" -DCMAKE_BUILD_TYPE=Release -G "Unix Makefiles" ../llvm`
  - This will build the modified `clang` compiler in a release configuration
- run `make clang` (or `make clang -j <concurrency>` if you want multiple tasks to run in parallel)
- this should produce a `clang` (and `clang++`) executable in the `build/bin` directory

The cmake configuration of this project will automatically use the `clang` executable from the `build/bin` directory, if you want to use cppless in your own project you'll have to instruct your build tool to use that version of clang accordingly

See the [BUILDING](BUILDING.md) document.

### ARM Cross-compilation

To deploy ARM functions from an x86 system, check the documentation on setting up sysroot and toolchain in `cmake/toolchains/aarch64`.

## Benchmarks

Implementations:
- Fib
- Floorplan
- Knapsack
- NQueens
- Raycasting

# Contributing

See the [CONTRIBUTING](CONTRIBUTING.md) document.
