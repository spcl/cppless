FROM public.ecr.aws/amazonlinux/amazonlinux:2023.6.20250303.0

ENV SYSROOT_DIR=/opt/lambda-sysroot
ENV PATH="${PATH}:/usr/local/bin"

RUN dnf update -y && \
    dnf install -y \
    tar \
    gzip \
    git \
    wget \
    gcc \
    gcc-c++ \
    make \
    cmake \
    ninja-build \
    pkg-config \
    autoconf \
    automake \
    libtool \
    file \
    patch \
    libcurl-devel \
    openssl-devel \
    zlib-devel \
    libstdc++-devel \
    libstdc++-static \
    glibc-devel \
    glibc-static \
    && dnf clean all

# Create sysroot directory structure
RUN mkdir -p ${SYSROOT_DIR}/{include,lib,lib64,bin}

# Install AWS C++ Lambda Runtime
WORKDIR /tmp
ADD CMakeLists.txt .
RUN git clone --recurse-submodules https://github.com/awslabs/aws-lambda-cpp.git && \
    cd aws-lambda-cpp && git checkout 934d2547fd573ece89ccbdaad2161a7e4d46fd1e &&  cp /tmp/CMakeLists.txt . &&\
    mkdir build && \
    cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release \
             -DBUILD_SHARED_LIBS=OFF && \
    make -j $(nproc) && \
    make install

# Create the sysroot file.
# Make sure that the non-zero output from tar does not convince Docker it failed
RUN tar --dereference -czf /sysroot.tar.gz /usr/include/ /usr/lib64/ /usr/lib/ /lib /lib64 || true

WORKDIR /

# Set the entrypoint to bash
ENTRYPOINT ["/bin/bash"]
