SET GYP_GENERATORS=ninja
SET DEPOT_TOOLS_WIN_TOOLCHAIN=0
SET GYP_GENERATOR_FLAGS=output_dir=out_win
SET GYP_DEFINES='include_tests=0 use_sysroot=0 include_examples=0 clang=0 host_clang=0 build_with_libjingle=0 build_with_chromium=0 libjingle_java=0 rtc_use_openmax_dl=0 enable_protobuf=0'
cd ..
python webrtc/build/gyp_webrtc
cd build_scripts

