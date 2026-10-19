#!/bin/sh
set -eu
cd "$(dirname "$0")"

SDK=$(xcrun --sdk macosx --show-sdk-path)
KH="$SDK/System/Library/Frameworks/Kernel.framework/Headers"
LILU_DIR="${LILU_DIR:-../lilu-src/Lilu}"
OUT="build/PolarisVCEOff.kext"

FLAGS="-arch x86_64 -mmacosx-version-min=11.0 -isysroot $SDK -I$KH -I$LILU_DIR -mkernel -nostdlib -fno-builtin -fno-common -O2 -Wall -Wno-sign-conversion -Wno-shadow -DKERNEL -DKERNEL_PRIVATE -DDRIVER_PRIVATE -DAPPLE -DNeXT -D__ACIDANTHERA_MAC_SDK=1 -DPRODUCT_NAME=PolarisVCEOff -DMODULE_VERSION=1.0.0"

rm -rf build
mkdir -p "$OUT/Contents/MacOS"

echo "Compiling kmod_info.c..."
xcrun clang $FLAGS -c kmod_info.c -o build/kmod_info.o

echo "Compiling PolarisVCEOff.cpp..."
xcrun clang++ $FLAGS -std=c++17 -fapple-kext -fno-exceptions -fno-rtti -c PolarisVCEOff.cpp -o build/PolarisVCEOff.o

echo "Compiling plugin_start.cpp..."
xcrun clang++ $FLAGS -std=c++17 -fapple-kext -fno-exceptions -fno-rtti -c "$LILU_DIR/Library/plugin_start.cpp" -o build/plugin_start.o

CCKEXT="$(xcrun clang -print-resource-dir)/lib/darwin/libclang_rt.cc_kext.a"

echo "Linking PolarisVCEOff..."
xcrun clang++ $FLAGS -Xlinker -kext -Xlinker -undefined -Xlinker dynamic_lookup -o "$OUT/Contents/MacOS/PolarisVCEOff" \
	build/kmod_info.o build/PolarisVCEOff.o build/plugin_start.o -lkmod -lkmodc++ "$CCKEXT"

cp Info.plist "$OUT/Contents/Info.plist"
plutil -lint "$OUT/Contents/Info.plist"
file "$OUT/Contents/MacOS/PolarisVCEOff"
echo "Build succeeded: $OUT"
