#!/bin/bash

function mux_library
{
	filelist=`ls $1/*.a `
    for file in $filelist 
    do
        echo expanding $file
        ar -x $file 
    done
    echo merge object files to $2 .......
    libtool -static -o $2 *.o
    rm *.o
}

cd ..

cd out_ios64
mux_library Release-iphoneos libwebrtc-ios.a
cd ..

cd out_ios
mux_library Release-iphoneos libwebrtc-ios.a
cd ..

cd out_ios64_sim
mux_library Release-iphonesimulator libwebrtc-ios.a
cd ..

echo lipo muitli platform librarys to signle library use lipo ......
mkdir ios_lipo_lib
lipo -create out_ios64/libwebrtc-ios.a out_ios/libwebrtc-ios.a out_ios64_sim/libwebrtc-ios.a -output ios_lipo_lib/libwebrtc-ios.a

