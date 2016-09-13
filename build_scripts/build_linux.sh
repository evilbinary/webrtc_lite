#!/bin/bash
cd ..
ninja -C out_linux64/Release
sudo cp out_linux64/Release/lib/*.so /usr/local/lib
