#!/bin/bash
cd ..
ninja -C out_android_v7a/Release
./third_party/android_tools/ndk/toolchains/arm-linux-androideabi-4.9/prebuilt/linux-x86_64/bin/arm-linux-androideabi-strip out_android_v7a/Release/lib/*.so
