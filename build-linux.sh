#!/bin/bash -e
# build-linux.sh

CMAKE_FLAGS='-DLINUX_LOCAL_DEV=true -DENABLE_HEADLESS=true -DENABLE_QT=false'

PLAYBACK_CODES_PATH="./Data/PlaybackGeckoCodes/"

DATA_SYS_PATH="./Data/Sys/"
BUILD_DIR="build-exi-ai"
BINARY_PATH="./$BUILD_DIR/Binaries/"

# Build type
if [ "$1" == "playback" ]
    then
        CMAKE_FLAGS+=" -DSLIPPI_PLAYBACK=true"
        echo "Using Playback build config"
else
        # TODO: move this around, playback should be the secondary build
        CMAKE_FLAGS+=" -DSLIPPI_PLAYBACK=false"
        echo "Using Netplay build config"
fi

# Move into the build directory, run CMake, and compile the project
mkdir -p $BUILD_DIR
pushd $BUILD_DIR
cmake ${CMAKE_FLAGS} ../
cmake --build . --target dolphin-nogui -- -j$(nproc)
popd

# Rename executable for compatibility with the AppImage build script.
mv $BINARY_PATH/dolphin-emu-nogui $BINARY_PATH/dolphin-emu

# Copy the Sys folder in
rm -rf ${BINARY_PATH}/Sys
cp -r ${DATA_SYS_PATH} ${BINARY_PATH}

touch ./$BINARY_PATH/portable.txt

# Copy playback specific codes if needed
if [ "$1" == "playback" ]
    then
        # Update Sys dir with playback codes
        echo "Copying Playback gecko codes"
		rm -rf "${BINARY_PATH}/Sys/GameSettings" # Delete netplay codes
		cp -r "${PLAYBACK_CODES_PATH}/." "${BINARY_PATH}/Sys/GameSettings/"
fi
