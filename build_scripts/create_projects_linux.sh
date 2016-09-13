#!/bin/bash

export GYP_GENERATORS=ninja
export GYP_GENERATOR_FLAGS="output_dir=out_linux64"
export GYP_DEFINES="use_sysroot=0 include_tests=0 include_examples=0 OS=linux target_arch=x64 clang=0 host_clang=0 build_with_libjingle=0 build_with_chromium=0 libjingle_java=0 rtc_use_openmax_dl=0 enable_protobuf==0"
cd ..
python webrtc/build/gyp_webrtc

