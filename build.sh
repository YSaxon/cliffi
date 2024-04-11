if [ -f "CMakeLists.txt" ]; then
    mkdir -p build_cmake && cd build_cmake
    cmake  .. -DCMAKE_BUILD_TYPE=Debug
    make && ctest .. --output-on-failure && \
    lcov --capture --directory . --output-file ../coverage.info
    genhtml ../coverage.info --output-directory ../coverage-html
    cd ..
else
    echo "Please run this from the project root directory."
fi
