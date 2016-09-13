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

cd out_mac
mux_library Release libwebrtc-mac.a
cd ..

