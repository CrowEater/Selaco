#!/bin/bash
mkdir -p build
cd build

ZMUSICCONFIG=-DZMUSIC_INCLUDE_DIR=../bin/osx/zmusic/include\ -DZMUSIC_LIBRARIES=../bin/osx/zmusic/lib/libzmusic.dylib
GZDOOMCONFIG="-DCMAKE_BUILD_TYPE=Release -DOSX_COCOA_BACKEND=OFF -DCMAKE_OSX_ARCHITECTURES=arm64;x86_64"
OSX_DYNAMIC_LIB=-DOSX_DYNAMIC_LIB=/opt/local/lib/libsndfile.1.dylib\;/opt/local/lib/libopenal.1.dylib\;/opt/local/lib/libMoltenVK.dylib
cmake $GZDOOMCONFIG $ZMUSICCONFIG $OSX_DYNAMIC_LIB ..
make -j 8
