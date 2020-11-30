opencv_cuda.o:opencv_cuda.cpp
	g++ -std=c++11 -g -o main.out opencv_cuda.cpp `pkg-config opencv4 --cflags --libs` \
    -I/usr/local/opencv4/include/opencv4/opencv2 \
    -I/usr/local/cuda/include \
    -L/usr/local/cuda/lib64 \
    -I/usr/include/eigen3 \
    -L/usr/lib/x86_64-linux-gnu -lcuda -ldl -lnvcuvid
    
clean:
	rm *.o main.out 

