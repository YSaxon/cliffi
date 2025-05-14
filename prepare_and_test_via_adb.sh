adb push /work/build/cliffi /work/build/libcliffi_test.so /data/local/tmp
cd /work/build; ctest
ctest --output-on-failure || ctest --rerun-failed --output-on-failure --extra-verbose