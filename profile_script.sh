#!/bin/bash
set -e # Keep this for error checking

# --- Conan Home Symlink ---
CONAN_HOME_SYMLINK_TARGET="/work/.conan2"
CONAN_HOME_SYMLINK_LINK="${HOME}/.conan2"
mkdir -p "$CONAN_HOME_SYMLINK_TARGET"
if ! ([ -L "${CONAN_HOME_SYMLINK_LINK}" ] && [ "$(readlink "${CONAN_HOME_SYMLINK_LINK}")" = "${CONAN_HOME_SYMLINK_TARGET}" ]); then
    echo "Setting up Conan home symlink..."
    rm -rf "${CONAN_HOME_SYMLINK_LINK}"
    ln -s "$CONAN_HOME_SYMLINK_TARGET" "${CONAN_HOME_SYMLINK_LINK}"
else
    echo "Conan home symlink already correct."
fi

# --- Conan Profiles ---
DEFAULT_PROFILE_PATH="$HOME/.conan2/profiles/default"
BUILD_PROFILE_PATH="$HOME/.conan2/profiles/build"
echo "Ensuring default Conan profile exists..."
if ! conan profile list | grep -q -w "default"; then # Check if 'default' profile exists
    conan profile detect > /dev/null 2>&1 # Create it if not
fi
if [ ! -f "$DEFAULT_PROFILE_PATH" ]; then
    echo "ERROR: Default profile '$DEFAULT_PROFILE_PATH' not found even after 'conan profile detect'."
    exit 1
fi
echo "Copying '$DEFAULT_PROFILE_PATH' to '$BUILD_PROFILE_PATH'..."
cp "$DEFAULT_PROFILE_PATH" "$BUILD_PROFILE_PATH"
PROFILE_PATH="$BUILD_PROFILE_PATH"

# --- Helper to clean extracted values ---
clean_extracted_value() {
    echo "$1" | sed 's/[^a-zA-Z0-9_.-].*$//' | xargs
}

# --- Parameter Detection (ARCH, OS, COMPILER) ---
if [ -z "${CC}" ]; then echo "ERROR: CC environment variable is not set."; exit 1; fi
CC_BASENAME=$(basename "${CC}")
ARCH_RAW="" OS_RAW="" COMPILER_RAW=""

if [[ $CC_BASENAME != *"-"*"-"* ]] && [ -n "$CMAKE_TOOLCHAIN_FILE" ] && [ -f "$CMAKE_TOOLCHAIN_FILE" ]; then
    echo "Using CMAKE_TOOLCHAIN_FILE method from: $CMAKE_TOOLCHAIN_FILE"
    ARCH_ABI_RAW=$(grep -oP 'CMAKE_ANDROID_ARCH_ABI\s+\K\S+' "$CMAKE_TOOLCHAIN_FILE" || true)
    if [ -n "$ARCH_ABI_RAW" ]; then ARCH_RAW="$ARCH_ABI_RAW";
    else ARCH_RAW=$(grep -oP 'CMAKE_SYSTEM_PROCESSOR\s+\K\S+' "$CMAKE_TOOLCHAIN_FILE" || true); fi
    OS_RAW=$(grep -oP 'CMAKE_SYSTEM_NAME\s+\K\S+' "$CMAKE_TOOLCHAIN_FILE" || true)
else
    echo "Using CC method from CC_BASENAME: $CC_BASENAME"
    ARCH_RAW=$(echo "${CC_BASENAME}" | cut -d '-' -f 1)
    OS_FROM_CC_REV=$(echo "${CC_BASENAME}" | rev | cut -d '-' -f 3 | rev)
    if [[ "$OS_FROM_CC_REV" != "$ARCH_RAW" ]] && [[ "$OS_FROM_CC_REV" != "$(echo ${CC_BASENAME} | rev | cut -d '-' -f 1 | rev)" ]]; then
      OS_RAW=$(echo "${OS_FROM_CC_REV}" | sed -e 's/\b\(.\)/\u\1/')
    elif [[ "${CC_BASENAME}" == *"android"* ]]; then OS_RAW="Android";
    elif [[ "${CC_BASENAME}" == *"linux"* ]]; then OS_RAW="Linux"; fi
fi
COMPILER_RAW=$(echo "${CC_BASENAME}" | rev | cut -d '-' -f 1 | rev)

ARCH_CLEANED=$(clean_extracted_value "$ARCH_RAW"); OS_CLEANED=$(clean_extracted_value "$OS_RAW")
COMPILER_CLEANED=$(clean_extracted_value "$COMPILER_RAW" | sed 's/[0-9.-]*$//')


# Goal is to map ARCH to Conan values for settings.arch:
# Possible values are ['x86', 'x86_64', 'ppc32be', 'ppc32', 'ppc64le', 'ppc64', 'armv4', 'armv4i', 'armv5el', 'armv5hf', 'armv6', 'armv7', 'armv7hf', 'armv7s', 'armv7k', 'armv8', 'armv8_32', 'armv8.3', 'arm64ec', 'sparc', 'sparcv9', 'mips', 'mips64', 'avr', 's390', 's390x', 'asm.js', 'wasm', 'sh4le', 'e2k-v2', 'e2k-v3', 'e2k-v4', 'e2k-v5', 'e2k-v6', 'e2k-v7', 'riscv64', 'riscv32', 'xtensalx6', 'xtensalx106', 'xtensalx7', 'tc131', 'tc16', 'tc161', 'tc162', 'tc18']
ARCH=$(echo "${ARCH_CLEANED}" | tr '[:upper:]' '[:lower:]' | sed \
    -e 's/powerpc/ppc/' -e 's/i[36]86/x86/' -e 's/i386/x86/' \
    -e 's/aarch64/armv8/' -e 's/arm64.*/armv8/' \
    -e 's/armeabi-v7a/armv7/' -e 's/armv7a/armv7/' \
    -e 's/xtensa$/xtensalx7/' -e 's/armv5$/armv5el/')
OS_NORMALIZED=$(echo "${OS_CLEANED}" | sed -e 's/Darwin/Macos/' -e 's/W64/Windows/')
OS="$OS_NORMALIZED"; COMPILER="$COMPILER_CLEANED"
if [ -n "${ANDROID_API}" ]; then OS="Android"; fi

echo "Determined ARCH: '$ARCH'"; echo "Determined OS: '$OS'"; echo "Determined COMPILER: '$COMPILER'"
if [ -z "$ARCH" ] || [ -z "$OS" ] || [ -z "$COMPILER" ]; then
    echo "ERROR: Failed to determine ARCH ('$ARCH'), OS ('$OS'), or COMPILER ('$COMPILER'). Exiting."
    exit 1
fi

# Update settings in the profile. Assumes 'default' profile has these lines.
sed -i "s|^arch\s*=.*|arch=$ARCH|" "$PROFILE_PATH"
sed -i "s|^compiler\s*=.*|compiler=$COMPILER|" "$PROFILE_PATH"
sed -i "s|^os\s*=.*|os=$OS|" "$PROFILE_PATH" # This ensures the 'os=' line is present and set.
echo "Updated arch, compiler, os in profile."

# --- Correctly add os.api_level AFTER the 'os=' line ---
if [ "$OS" == "Android" ]; then
    TARGET_API_LEVEL="${ANDROID_API}"
    if [ -z "$TARGET_API_LEVEL" ]; then
        echo "Setting Android API level to 21 (default as ANDROID_API env var is empty)"
        TARGET_API_LEVEL="21"
    else
        echo "Setting Android API level to $TARGET_API_LEVEL (from ANDROID_API env var)"
    fi

    if [ -n "$TARGET_API_LEVEL" ]; then
        # Remove any existing os.api_level line first (wherever it might be)
        sed -i '/^os\.api_level\s*=/d' "$PROFILE_PATH"
        # Use awk to insert os.api_level specifically AFTER the line starting with "os="
        awk -v api_level="$TARGET_API_LEVEL" '
        { print }  # Print the current line
        /^os\s*=/ { # If the current line is the "os=" line
            print "os.api_level=" api_level  # Print the os.api_level line immediately after
        }
        ' "$PROFILE_PATH" > "${PROFILE_PATH}.tmp" && mv "${PROFILE_PATH}.tmp" "$PROFILE_PATH"
        echo "Set os.api_level=$TARGET_API_LEVEL in profile (after os line)."
    fi
fi

# --- Correctly manage [conf] section ---
if [ "$OS" == "Android" ] && [ -n "${ANDROID_NDK}" ]; then
    echo "Setting Android NDK path to $ANDROID_NDK"
    TEMP_PROFILE_CONF_CLEAN=$(mktemp)
    awk 'BEGIN{in_section=0} /^\[conf\]/{in_section=1; next} /^\[.*\]/{in_section=0} !in_section{print}' "$PROFILE_PATH" > "$TEMP_PROFILE_CONF_CLEAN"
    mv "$TEMP_PROFILE_CONF_CLEAN" "$PROFILE_PATH"
    echo "" >> "$PROFILE_PATH"; echo "[conf]" >> "$PROFILE_PATH"; echo "tools.android:ndk_path=$ANDROID_NDK" >> "$PROFILE_PATH"

    ACTUAL_TOOLCHAIN_FILE_ORIGINAL_SCRIPT="$ANDROID_NDK/Toolchain.cmake"
    SYMLINK_LOCATION="$ANDROID_NDK/build/cmake/android.toolchain.cmake"; SYMLINK_TARGET_DIR=$(dirname "$SYMLINK_LOCATION")
    if [ -f "$ACTUAL_TOOLCHAIN_FILE_ORIGINAL_SCRIPT" ]; then
        SUDO_CMD=""; if ! (mkdir -p "$SYMLINK_TARGET_DIR" && touch "$SYMLINK_TARGET_DIR/.canwrite_test" && rm "$SYMLINK_TARGET_DIR/.canwrite_test") >/dev/null 2>&1; then SUDO_CMD="sudo "; fi
        echo "Creating NDK toolchain symlink: $SYMLINK_LOCATION -> $ACTUAL_TOOLCHAIN_FILE_ORIGINAL_SCRIPT"
        ${SUDO_CMD}mkdir -p "$SYMLINK_TARGET_DIR"; ${SUDO_CMD}rm -f "$SYMLINK_LOCATION"; ${SUDO_CMD}ln -s "$ACTUAL_TOOLCHAIN_FILE_ORIGINAL_SCRIPT" "$SYMLINK_LOCATION"
    else echo "Warning: Source NDK toolchain file '$ACTUAL_TOOLCHAIN_FILE_ORIGINAL_SCRIPT' not found for symlinking."; fi
fi

# --- Update the [buildenv] section (CORE FIX) ---
TOOL_DIR=$(dirname "${CC}"); echo "Tool directory for [buildenv] is: $TOOL_DIR"
COMPILER_TYPE_FOR_TOOLS="unknown"
if [[ "$(basename "${CC}")" == *"clang"* ]]; then COMPILER_TYPE_FOR_TOOLS="clang";
elif [[ "$(basename "${CC}")" == *"gcc"* ]]; then COMPILER_TYPE_FOR_TOOLS="gcc"; fi

_CC="${CC}"; _CXX="${CXX:-${CC/gcc/g++}}"; _LD="${LD:-${CC/gcc/ld}}"
_AR_DEFAULT=""; _RANLIB_DEFAULT=""; _STRIP_DEFAULT=""; _NM_DEFAULT=""
if [ "$COMPILER_TYPE_FOR_TOOLS" == "clang" ]; then
    _AR_DEFAULT="${TOOL_DIR}/llvm-ar"; _RANLIB_DEFAULT="${TOOL_DIR}/llvm-ranlib"; _STRIP_DEFAULT="${TOOL_DIR}/llvm-strip"; _NM_DEFAULT="${TOOL_DIR}/llvm-nm"
elif [ "$COMPILER_TYPE_FOR_TOOLS" == "gcc" ]; then
    GCC_BASENAME_NO_COMPILER=$(basename "${CC}" | sed -e "s/-${COMPILER_TYPE_FOR_TOOLS}$//" -e "s/${COMPILER_TYPE_FOR_TOOLS}$//")
    GCC_PREFIX=""; if [ "$GCC_BASENAME_NO_COMPILER" != "$(basename "${CC}")" ]; then GCC_PREFIX="${GCC_BASENAME_NO_COMPILER}-"; fi
    _AR_DEFAULT="${TOOL_DIR}/${GCC_PREFIX}ar"; _RANLIB_DEFAULT="${TOOL_DIR}/${GCC_PREFIX}ranlib"; _STRIP_DEFAULT="${TOOL_DIR}/${GCC_PREFIX}strip"; _NM_DEFAULT="${TOOL_DIR}/${GCC_PREFIX}nm"
else _AR_DEFAULT="${TOOL_DIR}/ar"; _RANLIB_DEFAULT="${TOOL_DIR}/ranlib"; _STRIP_DEFAULT="${TOOL_DIR}/strip"; _NM_DEFAULT="${TOOL_DIR}/nm"; fi
_AR="${AR:-$_AR_DEFAULT}"; _RANLIB="${RANLIB:-$_RANLIB_DEFAULT}"; _STRIP="${STRIP:-$_STRIP_DEFAULT}"; _NM="${NM:-$_NM_DEFAULT}"

TEMP_PROFILE_BUILDENV_CLEAN=$(mktemp)
awk 'BEGIN{in_section=0} /^\[buildenv\]/{in_section=1; next} /^\[.*\]/{in_section=0} !in_section{print}' "$PROFILE_PATH" > "$TEMP_PROFILE_BUILDENV_CLEAN"
mv "$TEMP_PROFILE_BUILDENV_CLEAN" "$PROFILE_PATH"
{ echo ""; echo "[buildenv]"; echo "CC=${_CC}"; echo "CXX=${_CXX}"; echo "LD=${_LD}"; echo "AR=${_AR}"; echo "RANLIB=${_RANLIB}"; echo "STRIP=${_STRIP}"; echo "NM=${_NM}"; } >> "$PROFILE_PATH"
echo "[buildenv] section updated."

# --- Original GCC Version Capping & Windows readline removal ---
COMPILER_VERSION_FROM_DEFAULT=$(grep -oP 'compiler.version=\K[^ ]+' "$HOME/.conan2/profiles/default" || true)
if [ "$COMPILER" == "gcc" ] && [ -n "$COMPILER_VERSION_FROM_DEFAULT" ]; then
    if command -v bc >/dev/null && [[ "$COMPILER_VERSION_FROM_DEFAULT" =~ ^[0-9]+(\.[0-9]+)*$ ]]; then
        if (( $(echo "$COMPILER_VERSION_FROM_DEFAULT > 13.2" | bc -l) )); then
            echo "GCC version ($COMPILER_VERSION_FROM_DEFAULT) > 13.2. Capping to 13.2 in $PROFILE_PATH."
            sed -i "s/compiler.version=.*\$/compiler.version=13.2/" "$PROFILE_PATH"
        fi
    fi
fi
if [ "$OS" == "Windows" ] && [ -f "conanfile.txt" ]; then echo "Removing readline from conanfile.txt."; sed -i '/readline/d' "conanfile.txt"; fi

echo "Updated Conan profile at $PROFILE_PATH"
echo "--- Final Profile Content ($PROFILE_PATH) ---"; cat "$PROFILE_PATH"; echo "--- End Profile Content ---"