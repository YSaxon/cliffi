#!/bin/bash
set -e
./profile_script.sh
conan install . --output-folder=build --build=missing -pr build
./update_cmake.sh
mkdir -p build
rm -rf build/* CMakeCache.txt CMakeFiles
cd build
cmake ..
make
ctest --output-on-failure
