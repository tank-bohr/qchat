#!/usr/bin/env bash
set -euo pipefail

# Build the same compiler and runtime, with no unused TLS/compression/event-loop dependencies.
prefix="${1:?Usage: build-thrift.sh INSTALL_PREFIX [WORK_DIRECTORY]}"
work="${2:-$PWD/build/thrift-dependency}"
mkdir -p "$work"
archive="$work/thrift-0.25.0.tar.gz"
curl --fail --location --retry 3 \
    https://codeload.github.com/apache/thrift/tar.gz/refs/tags/v0.25.0 -o "$archive"
printf '%s  %s\n' f6aa5fadd01320e17c73e0c1f3b22c858bd235fe5214a5dd9cd5c830822c4529 "$archive" \
    | sha256sum -c
tar -xzf "$archive" -C "$work"
cmake -S "$work/thrift-0.25.0" -B "$work/build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$prefix" \
    -DBUILD_SHARED_LIBS=OFF -DBUILD_TESTING=OFF -DBUILD_TUTORIALS=OFF \
    -DWITH_JAVA=OFF -DWITH_JAVASCRIPT=OFF -DWITH_NODEJS=OFF \
    -DWITH_PYTHON=OFF -DWITH_C_GLIB=OFF -DWITH_QT6=OFF -DWITH_QT5=OFF \
    -DWITH_OPENSSL=OFF -DWITH_LIBEVENT=OFF -DWITH_ZLIB=OFF
cmake --build "$work/build" --parallel "${CMAKE_BUILD_PARALLEL_LEVEL:-4}"
cmake --install "$work/build"
mkdir -p "$prefix/share/thrift"
cat "$work/thrift-0.25.0/LICENSE" "$work/thrift-0.25.0/NOTICE" \
    > "$prefix/share/thrift/copyright"
