adb push /work/build/cliffi /work/build/libcliffi_test.so /data/local/tmp
cd /work/build; ctest
ctest --output-on-failure --parallel || ctest --rerun-failed --output-on-failure --extra-verbose