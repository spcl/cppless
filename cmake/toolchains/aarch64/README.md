
## Cross-compilation for aarch64 using Docker

To deploy ARM functions from x86 machine, you need to set up a sysroot
and compile everything against it - cppless, AWS Lambda SDK, and dependencies.
We provide automatic scripts and Docker containers to simplify creating ARM-compatible environment.

## Create sysroot

First, enable [QEMU virtualization](https://docs.docker.com/build/building/multi-platform/) in Docker on your machine.
Otherwise you won't be able to run arm64 images.

Docker image is provided as `spcleth/cppless:lambda-arm64` in our repository. Alternatively, you can build it locally:

```
docker buildx build --platform linux/arm64 -t spcleth/cppless:lambda-arm64 --output=type=registry .
```

Then, follow these steps to extract sysroot from the image:

```
container_id=$(docker run  -d --platform linux/arm64 -it --entrypoint bash spcleth/cppless:lambda-arm64)
docker cp ${container_id}:/sysroot.tar.gz sysroot.tar.gz
docker stop -t0 ${container_id}
mkdir sysroot && cd sysroot && tar -xf ../sysroot.tar.gz && cd ..
chmod -R 774 sysroot
```

## Build dependencies

Cppless uses several libraries like cURL, Boost, or argparse. For the default build without cross-compilation,
we build them automatically with conan. For cross-compilation, we need to build them manually as a cross-compilation 
build with conan is far from simple.

You can build all dependencies using the provided script `build.sh`. It will download, configure, build, and install
all required dependencies into the sysroot directory.
If you want to build dependencies manually, then make sure to install them inside `sysroot`.

## Build cppless

Finally, we can build cppless itself:

```
cmake -DCPPLESS_TOOLCHAIN=aarch64 -DBUILD_BENCHMARKS=ON -DBUILD_EXAMPLES=ON -DCMAKE_BUILD_TYPE=Release -G "Unix Makefiles" ../../cppless/
make serverless_benchmark_custom_pi
```
