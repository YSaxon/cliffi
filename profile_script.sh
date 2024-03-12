#!/bin/bash

conan profile detect
cp "$HOME/.conan2/profiles/default" "$HOME/.conan2/profiles/build"

# Define the profile path
PROFILE_PATH="$HOME/.conan2/profiles/build"

# # Check if the CC variable is set
# if [ -z "${CC}" ]; then
#     echo "The CC environment variable is not set."
#     exit 1
# fi

# # Extract the architecture from the CC environment variable
ARCH=$(basename ${CC} | cut -d '-' -f 1)
OS=$(basename ${CC} | rev | cut -d '-' -f 3 | rev)
COMPILER=$(basename ${CC} | rev | cut -d '-' -f 1 | rev)


# sed in place the arch, os, and compiler
#if powerpc is in arch, replace with ppc

#capitalize the first letter of the os

#if no commas in CC, then use the CMAKE_TOOLCHAIN_FILE
if [[ $CC != *"-"* ]]; then
    ARCH=$(cat $CMAKE_TOOLCHAIN_FILE | grep -oP 'CMAKE_SYSTEM_PROCESSOR \K\w+')
    OS=$(cat $CMAKE_TOOLCHAIN_FILE | grep -oP 'CMAKE_SYSTEM_NAME \K\w+')
else
    ARCH=$(basename ${CC} | cut -d '-' -f 1)
    OS=$(basename ${CC} | rev | cut -d '-' -f 3 | rev)
    OS=$(echo ${OS} | sed -e 's/\b\(.\)/\u\1/')
    COMPILER=$(basename ${CC} | rev | cut -d '-' -f 1 | rev)
fi

# allowed archs are ['x86', 'x86_64', 'ppc32be', 'ppc32', 'ppc64le', 'ppc64', 'armv4', 'armv4i', 'armv5el', 'armv5hf', 'armv6', 'armv7', 'armv7hf', 'armv7s', 'armv7k', 'armv8', 'armv8_32', 'armv8.3', 'arm64ec', 'sparc', 'sparcv9', 'mips', 'mips64', 'avr', 's390', 's390x', 'asm.js', 'wasm', 'sh4le', 'e2k-v2', 'e2k-v3', 'e2k-v4', 'e2k-v5', 'e2k-v6', 'e2k-v7', 'riscv64', 'riscv32', 'xtensalx6', 'xtensalx106', 'xtensalx7']

ARCH=$(echo ${ARCH} | sed -e 's/powerpc/ppc/' -e 's/i686/x86/' -e 's/i386/x86/' -e 's/aarch64/armv8/' -e 's/extensa/extensalx6/') #not sure about extensa but worth a try


# if there is a var ANDROID_API
if [ -n "${ANDROID_API}" ]; then
    OS="Android"
fi


echo "ARCH: $ARCH"
echo "OS: $OS"
echo "COMPILER: $COMPILER"



# If the architecture extraction fails, fall back to a default or exit
if [ -z "$ARCH" ]; then
    echo "Unable to determine architecture from CC variable."
    exit 1
fi

# Update the architecture in the Conan profile
sed -i "s/arch=.*\$/arch=$ARCH/" "$PROFILE_PATH"
sed -i "s/compiler=.*\$/compiler=$COMPILER/" "$PROFILE_PATH"
sed -i "s/os=.*\$/os=$OS/" "$PROFILE_PATH"

if [ -n "${ANDROID_API}" ]; then
    echo "Setting Android API level to $ANDROID_API"
    echo "os.api_level=$ANDROID_API" >> "$PROFILE_PATH"
elif [ "$OS" == "Android" ]; then
    echo "Setting Android API level to $ANDROID_API (default)"
    echo "os.api_level=21" >> "$PROFILE_PATH"
fi

# Update the [buildenv] section in the Conan profile
{
    echo "[buildenv]"
    echo "CC=${CC}"
    echo "CXX=${CXX:-${CC/gcc/g++}}"
    echo "LD=${LD:-${CC/gcc/ld}}"
} >> "$PROFILE_PATH"

echo "Updated Conan profile at $PROFILE_PATH"

