#!/bin/bash
# dev_android.sh — Local Android build and test helper for cliffi.
#
# Mirrors what the CI does (dockcross cross-compile + ADB push/test) so you
# can iterate without pushing to GitHub.
#
# Prerequisites:
#   docker            — for dockcross cross-compilation
#   adb               — Android Debug Bridge (from Android SDK platform-tools)
#   A running emulator or connected device accessible via adb
#
# Quickstart for x86_64 emulator (easiest, no KVM quirks on Linux):
#   1. Install Android Studio (or sdkmanager), then:
#      sdkmanager "system-images;android-30;google_apis;x86_64"
#      avdmanager create avd -n test -k "system-images;android-30;google_apis;x86_64"
#      emulator -avd test -no-window &
#   2. ./dev_android.sh build          # cross-compile inside dockcross
#   3. ./dev_android.sh push           # adb push cliffi + testlib to device
#   4. ./dev_android.sh test           # run ctest via adb
#   5. ./dev_android.sh all            # build + push + test in one go

set -euo pipefail

# ── Config ───────────────────────────────────────────────────────────────────

# Change this to android-arm64, android-arm, etc. for other targets.
# x86_64 is recommended for local emulator (AVD) use.
ARCH="${CLIFFI_ANDROID_ARCH:-android-x86_64}"

TARGET_DIR=/data/local/tmp
DOCKCROSS_IMAGE="dockcross/${ARCH}"
DOCKCROSS_SCRIPT="./dockcross-${ARCH}"
BUILD_DIR="build-${ARCH}"

# ── Helpers ───────────────────────────────────────────────────────────────────

log()  { echo "[dev_android] $*"; }
die()  { echo "[dev_android] ERROR: $*" >&2; exit 1; }

check_adb() {
    adb devices | grep -v "List of" | grep -q "device$" \
        || die "No Android device/emulator found. Start an emulator or connect a device."
}

# ── Commands ──────────────────────────────────────────────────────────────────

cmd_setup() {
    log "Pulling dockcross image: ${DOCKCROSS_IMAGE}"
    docker run --rm "${DOCKCROSS_IMAGE}" > "${DOCKCROSS_SCRIPT}"
    chmod +x "${DOCKCROSS_SCRIPT}"
    log "Dockcross script written to ${DOCKCROSS_SCRIPT}"
}

cmd_build() {
    if [ ! -f "${DOCKCROSS_SCRIPT}" ]; then
        log "Dockcross script not found — running setup first"
        cmd_setup
    fi

    log "Cross-compiling for ${ARCH} inside dockcross..."
    "${DOCKCROSS_SCRIPT}" bash -c "
        set -e
        ./profile_script.sh
        mkdir -p ${BUILD_DIR}
        rm -rf ${BUILD_DIR}/* CMakeCache.txt CMakeFiles
        conan install . --output-folder=${BUILD_DIR} --build=missing -pr build -g=CMakeDeps
        cd ${BUILD_DIR}
        cmake .. -DUSE_FIND_PACKAGE=ON -DCMAKE_C_FLAGS='-Werror'
        make -j\$(nproc)
    "
    log "Build complete: ${BUILD_DIR}/cliffi"
}

cmd_push() {
    check_adb
    log "Pushing binaries to device at ${TARGET_DIR}"
    adb push "${BUILD_DIR}/cliffi"              "${TARGET_DIR}/cliffi"
    adb push "${BUILD_DIR}/libcliffi_test.so"   "${TARGET_DIR}/libcliffi_test.so"
    adb shell chmod +x "${TARGET_DIR}/cliffi"

    # Unit test binary (may not exist on build-only passes)
    if [ -f "${BUILD_DIR}/cliffi_unit_tests" ]; then
        adb push "${BUILD_DIR}/cliffi_unit_tests" "${TARGET_DIR}/cliffi_unit_tests"
        adb shell chmod +x "${TARGET_DIR}/cliffi_unit_tests"
    fi

    # .cliffi_init if present
    if [ -f "${BUILD_DIR}/.cliffi_init" ]; then
        adb push "${BUILD_DIR}/.cliffi_init" "${TARGET_DIR}/.cliffi_init"
    fi

    log "Push complete."
}

cmd_test() {
    check_adb
    log "Running ctest (redirecting test execution via adb)..."
    # run_on_adb.sh is the per-test wrapper used by ctest's COMMAND
    cd "${BUILD_DIR}"
    ctest --output-on-failure --parallel \
        || ctest --rerun-failed --output-on-failure --extra-verbose
}

cmd_shell() {
    check_adb
    log "Opening adb shell on device"
    adb shell "cd ${TARGET_DIR} && sh"
}

cmd_run() {
    # Pass any extra args directly to cliffi on the device
    check_adb
    shift  # remove 'run' from args
    adb shell "cd ${TARGET_DIR} && ./cliffi $*"
}

cmd_all() {
    cmd_build
    cmd_push
    cmd_test
}

# ── Dispatch ─────────────────────────────────────────────────────────────────

COMMAND="${1:-help}"
case "${COMMAND}" in
    setup)  cmd_setup  ;;
    build)  cmd_build  ;;
    push)   cmd_push   ;;
    test)   cmd_test   ;;
    shell)  cmd_shell  ;;
    run)    cmd_run "$@" ;;
    all)    cmd_all    ;;
    help|*)
        cat <<'EOF'
Usage: ./dev_android.sh <command> [args]

Commands:
  setup     Pull dockcross image and write helper script
  build     Cross-compile cliffi for Android inside dockcross
  push      adb-push built binaries to running emulator/device
  test      Run ctest suite on device via adb
  shell     Open an interactive shell on the device at /data/local/tmp
  run ...   Run cliffi on device with given args
  all       build + push + test

Environment:
  CLIFFI_ANDROID_ARCH   dockcross image suffix (default: android-x86_64)
                        Options: android-arm, android-arm64,
                                 android-x86, android-x86_64

Examples:
  CLIFFI_ANDROID_ARCH=android-arm64 ./dev_android.sh build
  ./dev_android.sh all
  ./dev_android.sh run libc.so i strlen hello

Android emulator quickstart (x86_64 on Linux with KVM):
  sdkmanager "system-images;android-30;google_apis;x86_64"
  avdmanager create avd -n cliffi_test -k "system-images;android-30;google_apis;x86_64"
  emulator -avd cliffi_test -no-window -no-audio &
  adb wait-for-device
  ./dev_android.sh all
EOF
        ;;
esac
