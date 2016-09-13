#!/bin/bash
ANDROID_NDK=/vagrant/webrtc_android/src/chromium/src/third_party/android_tools/ndk
ANDROID_SDK=/vagrant/webrtc_android/src/chromium/src/third_party/android_tools/sdk

mkdir -p ../third_party/android_tools
rm -f ../third_party/android_tools/ndk
rm -f ../third_party/android_tools/sdk
ln -s $ANDROID_NDK ../third_party/android_tools/ndk
ln -s $ANDROID_SDK ../third_party/android_tools/sdk
#must ln -s ndk and sdk to third_party/android_tools
export GYP_GENERATORS=ninja
export GYP_GENERATOR_FLAGS="output_dir=out_android_v7a"
export GYP_DEFINES="use_sysroot=0 include_tests=0 include_examples=1 use_experimental_allocator_shim=0 OS=android target_arch=arm arm_version=7 clang=0 host_clang=0 build_with_libjingle=0 build_with_chromium=0 libjingle_java=0 rtc_use_openmax_dl=0 enable_protobuf==0"
cd ..
python webrtc/build/gyp_webrtc

export GYP_GENERATORS=ninja
export GYP_GENERATOR_FLAGS="output_dir=out_android_arm64"
export GYP_DEFINES="use_sysroot=0 include_tests=0 include_examples=1 use_experimental_allocator_shim=0 OS=android target_arch=arm64 clang=0 host_clang=0 build_with_libjingle=0 build_with_chromium=0 libjingle_java=0 rtc_use_openmax_dl=0 enable_protobuf==0"
python webrtc/build/gyp_webrtc
