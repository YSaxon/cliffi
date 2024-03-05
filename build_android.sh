if [ -f "CMakeLists.txt" ]; then
    mkdir -p build_cmake_android && cd build_cmake_android
    export NDK=/opt/homebrew/Caskroom/android-ndk/25b.upgrading/AndroidNDK8937393.app/Contents/NDK
    export ABI=armeabi-v7a
    export MINSDKVERSION=21
    cmake \
    -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
    -DANDROID_ABI=$ABI \
    -DANDROID_PLATFORM=android-$MINSDKVERSION \
    $OTHER_ARGS .. -DCMAKE_BUILD_TYPE=Debug
    make VERBOSE=1
    cd ..
else
    echo "Please run this from the project root directory."
fi



