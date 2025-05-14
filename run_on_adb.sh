#!/bin/bash
set -e # Exit immediately if a command exits with a non-zero status.

# --- Configuration ---
# FLAG_FILE_DIR: Directory on the host to store the flag file.
# Using CMAKE_BINARY_DIR if available, otherwise /tmp.
# This script would need to know CMAKE_BINARY_DIR (e.g., passed as env var or hardcoded if known).
# For simplicity, let's use a fixed path or one derived from script location.
# Assume script is in project root for this example for a relative path to build dir.
# A more robust way would be to pass CMAKE_BINARY_DIR as an argument or env var.
SCRIPT_DIR=$(dirname "$(realpath "$0")")
FLAG_FILE_DIR="${SCRIPT_DIR}/build" # Adjust if your build dir is elsewhere relative to script
FLAG_FILE_NAME="adb_binaries_pushed.flag"
FLAG_FILE_PATH="${FLAG_FILE_DIR}/${FLAG_FILE_NAME}"
LOCK_DIR_PATH="${FLAG_FILE_DIR}/adb_push.lock"

# TARGET_DIR on device
# This must be consistent. If it includes a timestamp, the "once" logic breaks unless timestamp is also controlled.
# Let's assume a persistent target directory for this model.
TARGET_DIR_ON_DEVICE="/data/local/tmp/"

ADB_CMD="adb"
# --- End Configuration ---

# --- Ensure flag directory exists ---
mkdir -p "${FLAG_FILE_DIR}"

# --- Lock mechanism for atomicity ---
# Try to create the lock directory. If it fails, another instance is likely pushing.
if mkdir "${LOCK_DIR_PATH}" 2>/dev/null; then
    # Acquired the lock

    # Ensure cleanup of lock directory on exit
    trap 'rm -rf "${LOCK_DIR_PATH}"' EXIT

    if [ ! -f "${FLAG_FILE_PATH}" ]; then
        echo "ADB_SCRIPT_LOG: First run or flag not found. Pushing binaries to ${TARGET_DIR_ON_DEVICE}."

        HOST_EXECUTABLE_PATH="$1" # First arg is host executable
        if [ ! -f "${HOST_EXECUTABLE_PATH}" ]; then
            echo "ADB_SCRIPT_ERROR: Host executable '${HOST_EXECUTABLE_PATH}' not found." >&2
            rm -rf "${LOCK_DIR_PATH}" # Release lock
            trap - EXIT # Remove trap
            exit 1
        fi
        EXECUTABLE_NAME=$(basename "${HOST_EXECUTABLE_PATH}")
        TARGET_EXECUTABLE_PATH_ON_DEVICE="${TARGET_DIR_ON_DEVICE}/${EXECUTABLE_NAME}"

        # Create target directory on device
        echo "ADB_SCRIPT_LOG: Creating directory ${TARGET_DIR_ON_DEVICE} on device."
        ${ADB_CMD} shell "mkdir -p ${TARGET_DIR_ON_DEVICE}"

        # Push the main executable
        echo "ADB_SCRIPT_LOG: Pushing ${HOST_EXECUTABLE_PATH} to ${TARGET_EXECUTABLE_PATH_ON_DEVICE}."
        ${ADB_CMD} push "${HOST_EXECUTABLE_PATH}" "${TARGET_EXECUTABLE_PATH_ON_DEVICE}"
        ${ADB_CMD} shell "chmod +x ${TARGET_EXECUTABLE_PATH_ON_DEVICE}"

        # Find and push the test library (assuming it's an argument)
        # This logic needs to be robust, similar to previous scripts
        TEMP_ARGS_FOR_CMD=("$@") # Copy all args
        shift # Remove executable from TEMP_ARGS_FOR_CMD for library search

        FOUND_AND_PUSHED_LIB=false
        for ARG_FOR_LIB_SEARCH in "${TEMP_ARGS_FOR_CMD[@]:1}"; do # Iterate over original args, skipping exe
            if [[ -f "$ARG_FOR_LIB_SEARCH" && "$ARG_FOR_LIB_SEARCH" == *cliffi_test* ]]; then # Heuristic for test lib
                HOST_TESTLIB_PATH="$ARG_FOR_LIB_SEARCH"
                TESTLIB_NAME=$(basename "${HOST_TESTLIB_PATH}")
                TARGET_TESTLIB_PATH_ON_DEVICE="${TARGET_DIR_ON_DEVICE}/${TESTLIB_NAME}"

                echo "ADB_SCRIPT_LOG: Pushing test library ${HOST_TESTLIB_PATH} to ${TARGET_TESTLIB_PATH_ON_DEVICE}."
                ${ADB_CMD} push "${HOST_TESTLIB_PATH}" "${TARGET_TESTLIB_PATH_ON_DEVICE}"
                FOUND_AND_PUSHED_LIB=true
                # Assume only one test library needs this special handling.
                # If multiple, this logic needs to be more general.
                break
            fi
        done

        if ! ${FOUND_AND_PUSHED_LIB}; then
            echo "ADB_SCRIPT_WARNING: Test library not explicitly identified among arguments for pushing."
        fi

        # Create the flag file to indicate binaries are pushed for this session
        echo "Binaries pushed at $(date)" > "${FLAG_FILE_PATH}"
        echo "ADB_SCRIPT_LOG: Flag file ${FLAG_FILE_PATH} created."

        # Release the lock
        rm -rf "${LOCK_DIR_PATH}"
        trap - EXIT # Remove trap for successful push
    else
        echo "ADB_SCRIPT_LOG: Flag file ${FLAG_FILE_PATH} exists. Skipping binary push."
        # Release the lock if we acquired it just to check the flag
        rm -rf "${LOCK_DIR_PATH}"
        trap - EXIT
    fi
else
    # Could not acquire lock, another instance is pushing. Wait a bit.
    echo "ADB_SCRIPT_LOG: Waiting for another instance to finish pushing binaries..."
    WAIT_COUNT=0
    MAX_WAIT=30 # Max 30 seconds
    while ! mkdir "${LOCK_DIR_PATH}" 2>/dev/null && [ "${WAIT_COUNT}" -lt "${MAX_WAIT}" ]; do
        sleep 1
        ((WAIT_COUNT++))
    done
    if [ "${WAIT_COUNT}" -eq "${MAX_WAIT}" ] && [ ! -d "${LOCK_DIR_PATH}" ]; then # Check if lock dir still not creatable by this instance
         # Still locked after timeout, or some other issue. Maybe it was created and removed by another.
         # If flag file exists now, we are good. Otherwise, problem.
        if [ ! -f "${FLAG_FILE_PATH}" ]; then
            echo "ADB_SCRIPT_ERROR: Timed out waiting for lock, and flag file not created. Push might have failed." >&2
            exit 1
        else
            echo "ADB_SCRIPT_LOG: Lock released by another instance, and flag file found. Proceeding."
        fi
    else
         # Acquired lock after waiting OR lock was released and flag should exist
        if [ -d "${LOCK_DIR_PATH}" ]; then # if this instance created the lock after waiting
           rm -rf "${LOCK_DIR_PATH}" # Release it, as the check for flag file will happen again or was done.
        fi
        if [ ! -f "${FLAG_FILE_PATH}" ]; then
           echo "ADB_SCRIPT_ERROR: Waited for lock, but flag file still not created. Push might have failed in other instance." >&2
           exit 1
        fi
        echo "ADB_SCRIPT_LOG: Proceeding after waiting for other instance."
    fi
fi


# --- Prepare arguments for on-device execution ---
# The executable path ($1) needs to be transformed to its on-device path.
# The test library path (if it was an arg) also needs to be transformed.
# All other args remain the same.

ON_DEVICE_EXECUTABLE_PATH="${TARGET_DIR_ON_DEVICE}/$(basename "$1")"
PROCESSED_ARGS_FOR_DEVICE=()
PROCESSED_ARGS_FOR_DEVICE+=("${ON_DEVICE_EXECUTABLE_PATH}") # First arg is now on-device exe path

ORIGINAL_COMMAND_ARGS_NO_EXE=("${@:2}") # All args except the executable itself

for ARG in "${ORIGINAL_COMMAND_ARGS_NO_EXE[@]}"; do
    if [[ -f "$ARG" && "$ARG" == *cliffi_test* ]]; then # Heuristic for test lib
        # This argument was the host path to the test library
        TARGET_TESTLIB_PATH_ON_DEVICE="${TARGET_DIR_ON_DEVICE}/$(basename "$ARG")"
        PROCESSED_ARGS_FOR_DEVICE+=("${TARGET_TESTLIB_PATH_ON_DEVICE}")
    else
        PROCESSED_ARGS_FOR_DEVICE+=("${ARG}") # Keep other args as is
    fi
done

# --- Execute command on device ---
DEVICE_COMMAND_STR=""
for ARG in "${PROCESSED_ARGS_FOR_DEVICE[@]}"; do
    ESCAPED_ARG="${ARG//\"/\\\"}" # Basic quoting
    DEVICE_COMMAND_STR+=" \"${ESCAPED_ARG}\""
done

FULL_SHELL_COMMAND="cd ${TARGET_DIR_ON_DEVICE} && export LD_LIBRARY_PATH=${TARGET_DIR_ON_DEVICE}:\${LD_LIBRARY_PATH} && ${DEVICE_COMMAND_STR}"

echo "ADB_SCRIPT_LOG: Executing on device: ${FULL_SHELL_COMMAND}"
${ADB_CMD} shell "${FULL_SHELL_COMMAND}"
EXIT_CODE=$?

exit ${EXIT_CODE}