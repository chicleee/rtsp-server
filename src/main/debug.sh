#!/bin/bash
#--------------------------------------------------------------------------------------------
echo "--- START RTSP SERVER  ---"
g++ -std=c++11 rtsp_new.cpp -o rtsp_new -lpthread -lgstapp-1.0 `pkg-config --libs --cflags opencv gstreamer-1.0 gstreamer-rtsp-server-1.0` ;

./debug_viewer.sh & 
clipid=$!

./rtsp_new & bash 
kill $clipid
