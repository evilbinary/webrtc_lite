#!/bin/bash

function mux_library
{
	filelist=`ls $1/*.a `
    for file in $filelist 
    do
        echo expanding $file
        ar -x $file 
    done
    libtool -static -o $2 *.o
    rm *.o
}

cd ..

cd out_linux
find . -name "*.a" | xargs -i cp {} ./
mux_library Release libwebrtc-linux.aa
rm *.a 
mv libwebrtc-linux.aa libwebrtc-linux.a
cd ..

