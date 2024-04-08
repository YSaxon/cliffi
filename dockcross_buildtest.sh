#!/bin/bash
set -e
./profile_script.sh
# ./update_cmake.sh
mkdir -p build
rm -rf build/* CMakeCache.txt CMakeFiles
conan install . --output-folder=build --build=missing -pr build -g=CMakeDeps
cd build
cmake ..
make
ctest --output-on-failure || ctest --rerun-failed --output-on-failure --extra-verbose
