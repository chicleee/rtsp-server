#ifndef FIREDETECTION_H
#define FIREDETECTION_H

#include <iostream>
#include <cstdlib>
#include <fstream>
#include <string>
#include <stack>
#include <queue>
#include <vector>
#include <list>
#include <map>
#include <cstdlib>
#include <unistd.h>
#include "Para.h"

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/video/video.hpp>
#include <opencv2/ml/ml.hpp>

using namespace std;
using namespace cv;

struct ContourInfo {
    vector<Point> contour;
    double area;
    Rect boundRect;
    int count;
    int label;
};

void calcDensity(const Mat& mask, Mat& density, int ksize = 7);
void getMassCenter(const Mat& mask, Point& center);


class Rectangle : public Rect {
public:
    Rectangle();
    Rectangle(const Rect& r);

    bool near(const Rectangle& r);
    void merge(const Rectangle& r);
    ~Rectangle();
};


class Region {
public:
    vector<ContourInfo*> contours;
    Rectangle rect;

    Region();
    Region(ContourInfo* contour, const Rectangle& rect);
    Region(const vector<ContourInfo*>& contours, const Rectangle& rect);
    bool near(const Region& r);
    void merge(const Region& r);
    ~Region();
};

struct Target {
    static const int TARGET_EXISTING = 0;
    static const int TARGET_NEW = 1;
    static const int TARGET_LOST = 2;
    static const int TARGET_MERGED = 3;

    int type;
    int times;
    int lostTimes;
    vector<int> mergeSrc;
    Region region;
    bool isFlame;
};

class TargetExtractor {
private:
    //static const int MAX_MASK_QUEUE_SIZE = 10;

    Mat x;
    Mat mFrame;
    Mat mMask;
    queue<Mat> mMaskQueue;
    Mat mMaskSum;
    vector<ContourInfo> mContours;

    Mat mBackground;
    BackgroundSubtractorMOG2 mMOG;

    void movementDetect(double learningRate = -1);
    void colorDetect(int redThreshold, double saturationThreshold, Mat temp, int x, int y, int w, int h);
    void denoise(int ksize = 7, int threshold = 6);
    void fill(int ksize = 7, int threshold = 6);
    void regionGrow(int threshold = 20);
    void smallAreaFilter(int threshold, int keep, int countFrame);
    void blobTrack(map<int, Target>& targets, vector<ContourInfo> temp);

public:
    TargetExtractor();
    void extract(const Mat& frame, map<int, Target>& targets, bool track, int countFrame);
    ~TargetExtractor();
};

class FeatureAnalyzer {
private:
    Mat mFrame;
    void targetUpdate(map<int, Target>& targets);

public:
    FeatureAnalyzer();
    void analyze(const Mat& frame, Mat& result, map<int, Target>& targets);
    ~FeatureAnalyzer();
};


class FlameDetector {
private:
    //static const int SKIP_FRAME_COUNT = 20;

    Mat mFrame;
    TargetExtractor mExtractor;
    FeatureAnalyzer mAnalyzer;
    map<int, Target> mTargetMap;
    int mFrameCount;
    int mFlameCount;
    bool mTrack;

public:
    FlameDetector();
    void detect(const Mat& frame, Mat &result);
    ~FlameDetector();
};

class FireDetection{
private:
    VideoCapture mCapture;
    FlameDetector mDetector;
    double mVideoFPS;
public:
    FireDetection();
    void fireDetection(const Mat &src, Mat &result);
    ~FireDetection();
};

#endif // FIREDETECTION_H
