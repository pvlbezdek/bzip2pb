#!/bin/bash
set -ex

cmake -B build -S . \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$PREFIX"

cmake --build build
cmake --install build
