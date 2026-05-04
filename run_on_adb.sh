#!/bin/bash
set -e # Exit immediately if a command exits with a non-zero status.

TARGET_DIR_ON_DEVICE="/data/local/tmp/"
ADB_CMD="adb"

 # --- Prepare arguments for on-device execution ---
 # The executable path ($1) needs to be transformed to its on-device path.
 # The rest of the arguments are escaped properly with printf

ON_DEVICE_EXECUTABLE_PATH="${TARGET_DIR_ON_DEVICE}/$(basename "$1")"
FULL_SHELL_COMMAND="cd $TARGET_DIR_ON_DEVICE && $ON_DEVICE_EXECUTABLE_PATH "$(printf "'%s' "  "${@:2}")

${ADB_CMD} shell "${FULL_SHELL_COMMAND}; echo \$? > /data/local/tmp/exit_code.txt"
DEVICE_EXIT_CODE=$(${ADB_CMD} exec-out cat /data/local/tmp/exit_code.txt | tr -d '[:space:]')
exit ${DEVICE_EXIT_CODE}