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
# ARCH=$(basename ${CC} | cut -d '-' -f 1)
# OS=$(basename ${CC} | rev | cut -d '-' -f 3 | rev)
# COMPILER=$(basename ${CC} | rev | cut -d '-' -f 1 | rev)


# # sed in place the arch, os, and compiler
# #if powerpc is in arch, replace with ppc
# ARCH=$(echo ${ARCH} | sed -e 's/powerpc/ppc/' -e 's/i686/x86/' -e 's/i386/x86/')

# #capitalize the first letter of the os
# OS=$(echo ${OS} | sed -e 's/\b\(.\)/\u\1/')


# cat $CMAKE_TOOLCHAIN_FILE | grep -oP 'CMAKE_SYSTEM_NAME \K\w+' | xargs -I {} echo "OS: {}"
# cat $CMAKE_TOOLCHAIN_FILE | grep -oP 'CMAKE_SYSTEM_PROCESSOR \K\w+' | xargs -I {} echo "ARCH: {}"

OS=$(cat $CMAKE_TOOLCHAIN_FILE | grep -oP 'CMAKE_SYSTEM_NAME \K\w+')
ARCH=$(cat $CMAKE_TOOLCHAIN_FILE | grep -oP 'CMAKE_SYSTEM_PROCESSOR \K\w+')
COMPILER=$(basename ${CC} | rev | cut -d '-' -f 1 | rev)

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

# Update the [buildenv] section in the Conan profile
{
    echo "[buildenv]"
    echo "CC=${CC}"
    echo "CXX=${CXX:-${CC/gcc/g++}}"
    echo "LD=${LD:-${CC/gcc/ld}}"
} >> "$PROFILE_PATH"

echo "Updated Conan profile at $PROFILE_PATH"

