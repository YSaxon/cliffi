#!/bin/bash
# run_on_adb_simplified.sh
set -e # Exit immediately if a command exits with a non-zero status.

if [ -z "$1" ]; then
    echo "Error: No executable path provided to emulator script." >&2
    exit 1
fi

# The first argument ($1) is the absolute path to the executable ON THE DEVICE.
# e.g., /data/local/tmp/cliffi_tests_persistent/cliffi
ON_DEVICE_EXECUTABLE_PATH="$1"

# Infer the base directory for LD_LIBRARY_PATH from the executable's path.
# This assumes the executable and its libraries are in the same directory on device.
ANDROID_DEVICE_TEST_DIR=$(dirname "${ON_DEVICE_EXECUTABLE_PATH}")

# Construct the command string for adb shell using all passed arguments ($@).
# The arguments already contain on-device paths.
DEVICE_COMMAND_STR=""
for ARG in "$@"; do
    # Basic quoting: escape existing double quotes and wrap with new ones.
    ESCAPED_ARG="${ARG//\"/\\\"}"
    DEVICE_COMMAND_STR+=" \"${ESCAPED_ARG}\""
done

# Prepend environment setup.
# The `cd ${ANDROID_DEVICE_TEST_DIR}` is good practice, though not strictly needed if all paths are absolute.
FULL_SHELL_COMMAND="cd ${ANDROID_DEVICE_TEST_DIR} && export LD_LIBRARY_PATH=${ANDROID_DEVICE_TEST_DIR}:\${LD_LIBRARY_PATH} && ${DEVICE_COMMAND_STR}"

echo "ADB (Optimized): Executing on device: ${FULL_SHELL_COMMAND}"
# Execute the command and capture its exit code
adb shell "${FULL_SHELL_COMMAND}"
EXIT_CODE=$?

# Return the exit code of the test command.
exit ${EXIT_CODE}