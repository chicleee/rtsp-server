//compile: g++ camera.cpp -o camera `pkg-config --libs --cflags opencv`
#include <opencv2/opencv.hpp>
#include <opencv2/video/video.hpp>
#include <iostream>

using namespace std;
using namespace cv;

int main(){
	VideoCapture cap(0);
	Mat frame;
	cap.set(CV_CAP_PROP_FRAME_WIDTH,300);
	cap.set(CV_CAP_PROP_FRAME_HEIGHT,300);
	while(1){
		cap.read(frame);
		cout << "Width: " << frame.cols << endl;
		cout << "Height: " << frame.rows << endl;
		imshow("cameraframe",frame);
		waitKey(20);
	}
	return 0;
}
