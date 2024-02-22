if [ -f "CMakeLists.txt" ]; then
    mkdir -p build_cmake && cd build_cmake
    cmake  .. -DCMAKE_BUILD_TYPE=Debug
    make
    cd ..
else
    echo "Please run this from the project root directory."
fi
