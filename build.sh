if [ -f "CMakeLists.txt" ]; then
    mkdir -p build_cmake && cd build_cmake
    if [[ -x "$(command -v brew)" ]]; then
        brew sh -c "cmake  .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_FLAGS="-Werror""
    else
    cmake  .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_FLAGS="-Werror"
    fi
    find . -name "*.gcda" -delete
    make && ctest .. --output-on-failure && \
    lcov --capture --directory . --output-file ../coverage.info && \
    genhtml ../coverage.info --output-directory ../coverage-html
    cd ..
else
    echo "Please run this from the project root directory."
fi
