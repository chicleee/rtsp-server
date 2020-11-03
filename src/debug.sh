#!/bin/bash
#--------------------------------------------------------------------------------------------
echo "--- START RTSP SERVER  ---"
g++ -std=c++11 cmdinput.cpp -o cmdinput -lpthread -lgstapp-1.0 `pkg-config --libs --cflags opencv gstreamer-1.0 gstreamer-rtsp-server-1.0` ;

./debug_viewer.sh & 
clipid=$!

./cmdinput & bash 
kill $clipid
