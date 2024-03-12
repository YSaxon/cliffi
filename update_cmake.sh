#!/bin/bash

# Define the root path for the search
search_root="$HOME/.conan2"

# Use find to locate libffi.a within build-release directories and the corresponding include directory
libffi_a_path=$(find $search_root -type d -name build-release -exec find {} -name libffi.a \; | head -n 1)
include_dir_path=$(find $search_root -type d -name build-release -exec find {} -name ffi.h \; | xargs dirname | head -n 1)

# Check if the paths are found
if [[ -z "$libffi_a_path" || -z "$include_dir_path" ]]; then
    echo "libffi.a or include directory not found in any build-release folder under $search_root."
    exit 1
fi

# Echo found paths for verification
echo "Found libffi.a at: $libffi_a_path"
echo "Found include directory at: $include_dir_path"

# Update CMakeLists.txt with the found paths
sed -i "s|set(LIBFFI_STATIC_LIB .*|set(LIBFFI_STATIC_LIB ${libffi_a_path})|" CMakeLists.txt
sed -i "s|set(LIBFFI_INCLUDE_DIRS .*|set(LIBFFI_INCLUDE_DIRS ${include_dir_path})|" CMakeLists.txt
