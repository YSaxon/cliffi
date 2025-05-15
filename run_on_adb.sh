#!/bin/bash
set -e # Exit immediately if a command exits with a non-zero status.

TARGET_DIR_ON_DEVICE="/data/local/tmp/"

ADB_CMD="adb"
# # --- End Configuration ---

# # --- Prepare arguments for on-device execution ---
# # The executable path ($1) needs to be transformed to its on-device path.
# # The test library path (if it was an arg) also needs to be transformed.
# # All other args remain the same.

ON_DEVICE_EXECUTABLE_PATH="${TARGET_DIR_ON_DEVICE}/$(basename "$1")"
PROCESSED_ARGS_FOR_DEVICE=()
PROCESSED_ARGS_FOR_DEVICE+=("${ON_DEVICE_EXECUTABLE_PATH}") # First arg is now on-device exe path

ORIGINAL_COMMAND_ARGS_NO_EXE=("${@:2}") # All args except the executable itself

for ARG in "${ORIGINAL_COMMAND_ARGS_NO_EXE[@]}"; do
        PROCESSED_ARGS_FOR_DEVICE+=("${ARG}") # Keep other args as is
done

# --- Execute command on device ---
DEVICE_COMMAND_STR=""
for ARG in "${PROCESSED_ARGS_FOR_DEVICE[@]}"; do
    ESCAPED_ARG="${ARG//\"/\\\"}" # Basic quoting
    DEVICE_COMMAND_STR+=" \"${ESCAPED_ARG}\""
done

FULL_SHELL_COMMAND="cd ${TARGET_DIR_ON_DEVICE}  && ${DEVICE_COMMAND_STR}"

${ADB_CMD} shell "${FULL_SHELL_COMMAND}; echo \$? > /data/local/tmp/exit_code.txt"
DEVICE_EXIT_CODE=$(${ADB_CMD} exec-out cat /data/local/tmp/exit_code.txt | tr -d '[:space:]')
exit ${DEVICE_EXIT_CODE}