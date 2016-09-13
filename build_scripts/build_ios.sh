#!/bin/bash

cd ..

ninja -C out_ios64/Release-iphoneos
ninja -C out_ios/Release-iphoneos
ninja -C out_ios64_sim/Release-iphonesimulator

