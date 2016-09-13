#!/bin/bash
webrtc_root=/Users/apple/solutions/webrtc_ios/src

mkdir -p ../third_party/boringssl
mkdir -p ../third_party/expat
mkdir -p ../third_party/gflags
mkdir -p ../third_party/jsoncpp
mkdir -p ../third_party/libjpeg_turbo
mkdir -p ../third_party/libsrtp
mkdir -p ../third_party/libvpx
mkdir -p ../third_party/libyuv
mkdir -p ../third_party/ocmock
mkdir -p ../third_party/openmax_dl
mkdir -p ../third_party/opus
mkdir -p ../third_party/protobuf
mkdir -p ../third_party/usrsctp
mkdir -p ../third_party/yasm

#from windows and ios
mkdir -p ../third_party/winsdk_samples
mkdir -p ../third_party/llvm-build

mkdir ../build
mkdir ../webrtc
mkdir ../talk
mkdir ../buildtools
mkdir ../tools
mkdir ../testing

       
cp -rf $webrtc_root/third_party/boringssl/*			  ../third_party/boringssl/
cp -rf $webrtc_root/third_party/expat/*               ../third_party/expat/
cp -rf $webrtc_root/third_party/gflags/*              ../third_party/gflags/
cp -rf $webrtc_root/third_party/jsoncpp/*             ../third_party/jsoncpp/
cp -rf $webrtc_root/third_party/libjpeg_turbo/*       ../third_party/libjpeg_turbo/
cp -rf $webrtc_root/third_party/libsrtp/*             ../third_party/libsrtp/
cp -rf $webrtc_root/third_party/libvpx/*              ../third_party/libvpx/
cp -rf $webrtc_root/third_party/libyuv/*              ../third_party/libyuv/
cp -rf $webrtc_root/third_party/ocmock/*              ../third_party/ocmock/
cp -rf $webrtc_root/third_party/openmax_dl/*          ../third_party/openmax_dl/
cp -rf $webrtc_root/third_party/opus/*                ../third_party/opus/
cp -rf $webrtc_root/third_party/protobuf/*            ../third_party/protobuf/
cp -rf $webrtc_root/third_party/usrsctp/*             ../third_party/usrsctp/
cp -rf $webrtc_root/third_party/yasm/*                ../third_party/yasm/

#from windows and ios
#cp -rf $webrtc_root/third_party/llvm-build/*          ../third_party/llvm-build/
cp -rf $webrtc_root/third_party/winsdk_samples/*      ../third_party/winsdk_samples/

cp -rf $webrtc_root/build/* 		../build/
cp -rf $webrtc_root/webrtc/* 		../webrtc
cp -rf $webrtc_root/talk/* 			../talk
cp -rf $webrtc_root/buildtools/* 	../buildtools
cp -rf $webrtc_root/tools/* 		../tools
cp -rf $webrtc_root/testing/* 		../testing

cp 	   $webrtc_root/* ../