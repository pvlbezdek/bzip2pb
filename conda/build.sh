#!/bin/bash
set -ex

cmake -B build -S . \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DBZIP2_INCLUDE_DIR="$PREFIX/include" \
    -DBZIP2_LIBRARIES="$PREFIX/lib/libbz2.a"

cmake --build build
cmake --install build
