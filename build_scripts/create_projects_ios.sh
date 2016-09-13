#!/bin/bash
cd ..
export GYP_CROSSCOMPILE=1
export GYP_GENERATORS=ninja

export GYP_GENERATOR_FLAGS="output_dir=out_ios64"
export GYP_DEFINES="use_sysroot=0 include_tests=0 include_examples=1 OS=ios target_arch=arm64 clang_xcode=1 ios_deployment_target='7.0' build_with_libjingle=0 build_with_chromium=0 libjingle_java=0 rtc_use_openmax_dl=0 enable_protobuf==0"
python webrtc/build/gyp_webrtc

export GYP_GENERATOR_FLAGS="output_dir=out_ios"
export GYP_DEFINES="use_sysroot=0 include_tests=0 include_examples=1 OS=ios target_arch=arm clang_xcode=1 ios_deployment_target='7.0' build_with_libjingle=0 build_with_chromium=0 libjingle_java=0 rtc_use_openmax_dl=0 enable_protobuf==0"
python webrtc/build/gyp_webrtc

export GYP_GENERATOR_FLAGS="output_dir=out_ios64_sim"
export GYP_DEFINES="use_sysroot=0 include_tests=0 include_examples=1 OS=ios target_arch=x64 clang_xcode=1 ios_deployment_target='7.0' build_with_libjingle=0 build_with_chromium=0 libjingle_java=0 rtc_use_openmax_dl=0 enable_protobuf==0"
python webrtc/build/gyp_webrtc
