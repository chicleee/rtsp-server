其中有以下重要文件，

1)Start.cpp是只使用CPU的版本

编译命令：
g++ -std=c++11 start.cpp -o start -lboost_system -lboost_filesystem -lpthread -lgstapp-1.0 `pkg-config --libs --cflags opencv gstreamer-1.0 gstreamer-rtsp-server-1.0`

运行命令：
./start

2)start_gpu.cpp是GPU版本

编译命令：
g++ -std=c++11 start_gpu.cpp -g -o start_gpu -lboost_system -lboost_filesystem -lpthread -lgstapp-1.0 `pkg-config --libs --cflags opencv4 gstreamer-1.0 gstreamer-rtsp-server-1.0` \
-I/usr/local/opencv4/include/opencv4/opencv2 \
-I/usr/local/cuda/include \
-L/usr/local/cuda/lib64 \
-I/usr/include/eigen3 \
-L/usr/lib/x86_64-linux-gnu -lcuda -ldl -lnvcuvid

运行命令：
./start_gpu

3)Config.ini是系统的配置文件
