#!/bin/bash
cd build
cmake .. -DUSE_FIND_PACKAGE=ON -DCMAKE_C_FLAGS="-Werror"
make