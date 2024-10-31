#!/bin/bash -e
# build-mac.sh

BUILD_DIR=./build-nogui
DATA_SYS_PATH="./Data/Sys"

CMAKE_FLAGS="-DCMAKE_POLICY_VERSION_MINIMUM=3.5"
CMAKE_FLAGS+=' -DLINUX_LOCAL_DEV=true -DENABLE_HEADLESS=true -DENABLE_QT=false'

# For some reason the system xxhash library doesn't get properly linked,
# at least on my M1. The clang command gets -lxxhash, but probably needs
# -L/opt/homebrew/lib/ to actually find the library.
if [[ $(arch) == 'arm64' ]]; then
  CMAKE_FLAGS+=" -DUSE_SYSTEM_XXHASH=OFF"
fi
export LIBRARY_PATH=$LIBRARY_PATH:/usr/local/lib:/usr/lib/

# Build type
if [ "$1" == "playback" ]
then
        echo "Using Playback build config"
        CMAKE_FLAGS+=" -DSLIPPI_PLAYBACK=true"
        BUILD_DIR+="-playback"
else
        echo "Using Netplay build config"
        CMAKE_FLAGS+=" -DSLIPPI_PLAYBACK=false"
fi

if [ "$CI" == "true" ]
then
        CMAKE_FLAGS+=" -DMACOS_CODE_SIGNING=OFF"
fi

# Move into the build directory, run CMake, and compile the project
mkdir -p $BUILD_DIR
pushd $BUILD_DIR
cmake ${CMAKE_FLAGS} ..
cmake --build . --target dolphin-nogui -- -j$(sysctl -n hw.ncpu)
popd

BINARY_PATH="${BUILD_DIR}/Binaries"
SYS_PATH="${BINARY_PATH}/Sys"

rm -rf ${SYS_PATH}
cp -r ${DATA_SYS_PATH} ${SYS_PATH}

# Copy Playback codes.
if [ "$1" == "playback" ]
then
        cp Data/Sys/GameSettings/Playback/* ${SYS_PATH}/GameSettings/
fi
