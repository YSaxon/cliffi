#!/bin/bash
set -e
./profile_script.sh
mkdir -p build
rm -rf build/* CMakeCache.txt CMakeFiles
conan install . --output-folder=build --build=missing -pr build -g=CMakeDeps
# ./update_cmake.sh
cd build
cmake .. -DUSE_FIND_PACKAGE=ON -DCMAKE_C_FLAGS="-Werror"
make
