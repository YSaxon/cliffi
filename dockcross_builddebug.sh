#!/bin/bash
set -e
./profile_script.sh
mkdir -p build
rm -rf build/* CMakeCache.txt CMakeFiles
conan install . --output-folder=build --build=missing -pr build -g=CMakeDeps -s build_type=Debug
cd build
cmake .. -DUSE_FIND_PACKAGE=ON -DCMAKE_C_FLAGS="-Werror" -DENABLE_SANITIZERS=ON -DCMAKE_BUILD_TYPE=Debug
make