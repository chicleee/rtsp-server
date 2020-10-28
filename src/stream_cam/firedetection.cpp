#include "firedetection.h"

void calcDensity(const Mat& mask, Mat& density, int ksize){
    int r = (ksize - 1) / 2;
    if (r <= 0) {
        return;
    }

    density = Mat::zeros(mask.size(), CV_32SC1);

    int rowBound = density.rows - r, colBound = density.cols - r;

    Rect rect(0, 0, ksize, ksize);
    density.at<int>(r, r) = cvRound(sum(mask(rect))[0] / 255);

    for (int j = r + 1; j < colBound; j++) {
        int col1 = j - r - 1, col2 = j + r;
        int delta = 0;
        for (int k = 0; k < ksize; k++) {
            delta += mask.at<uchar>(k, col2) - mask.at<uchar>(k, col1);
        }
        density.at<int>(r, j) = density.at<int>(r, j - 1) + delta / 255;
    }

    for (int i = r + 1; i < rowBound; i++) {
        int row1 = i - r - 1, row2 = i + r;
        int delta = 0;
        for (int k = 0; k < ksize; k++) {
            delta += mask.at<uchar>(row2, k) - mask.at<uchar>(row1, k);
        }
        density.at<int>(i, r) = density.at<int>(i - 1, r) + delta / 255;
    }

    for (int i = r + 1; i < rowBound; i++) {
        for (int j = r + 1; j < colBound; j++) {
            int delta = (mask.at<uchar>(i + r, j + r) - mask.at<uchar>(i - r - 1, j + r) -
                mask.at<uchar>(i + r, j - r - 1) + mask.at<uchar>(i - r - 1, j - r - 1)) / 255;
            density.at<int>(i, j) = density.at<int>(i - 1, j) + density.at<int>(i, j - 1) -
                density.at<int>(i - 1, j - 1) + delta;
        }
    }
}

// this function is also an alternative to the old one which is implemented with 'moments'
void getMassCenter(const Mat& mask, Point& center){
    int sumX = 0, sumY = 0, count = 0;
    for (int i = 0; i < mask.rows; i++) {
        for (int j = 0; j < mask.cols; j++) {
            if (mask.at<uchar>(i, j) == 255) {
                sumX += j;
                sumY += i;
                count++;
            }
        }
    }
    center.x = sumX / count;
    center.y = sumY / count;
}
Rectangle::Rectangle(){
}

Rectangle::Rectangle(const Rect& r)
: Rect(r)
{
}

inline bool Rectangle::near(const Rectangle& r){
    return abs((x + width / 2.0) - (r.x + r.width / 2.0)) - (width + r.width) / 2.0 <
                max(width, r.width) * 0.2 &&
           abs((y + height / 2.0) - (r.y + r.height / 2.0)) - (height + r.height) / 2.0 <
                max(height, r.height) * 0.2;
}

inline void Rectangle::merge(const Rectangle& r){
    int tx = min(x, r.x);
    int ty = min(y, r.y);
    width = max(x + width, r.x + r.width) - tx;
    height = max(y + height, r.y + r.height) - ty;
    x = tx;
    y = ty;
}

Rectangle::~Rectangle(){

}

/**************** Region ****************/

Region::Region(){
}

Region::Region(ContourInfo* contour, const Rectangle& rect)
: contours(1, contour)
, rect(rect)
{
}

Region::Region(const vector<ContourInfo*>& contours, const Rectangle& rect)
: contours(contours)
, rect(rect)
{
}

inline bool Region::near(const Region& r){
    return rect.near(r.rect);
}

void Region::merge(const Region& r){
    rect.merge(r.rect);
    for (vector<ContourInfo*>::const_iterator it = r.contours.begin(); it != r.contours.end(); it++) {
        contours.push_back(*it);
    }
}

Region::~Region(){

}
/**************** TargetExtractor ****************/

TargetExtractor::TargetExtractor(){
    mMOG.set("detectShadows", false);
}

void TargetExtractor::movementDetect(double learningRate){
    mMOG(mFrame, mMask, learningRate);
    //namedWindow("x");
    //imshow("x", mMask);
    //mMOG.getBackgroundImage(mBackground);
}

void TargetExtractor::colorDetect(int redThreshold, double saturationThreshold, Mat temp, int x, int y, int w, int h){
    //Mat temp;
    //GaussianBlur(mFrame, temp, Size(3, 3), 0);

    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            if (mMask.at<uchar>(i+y, j+x) == 255) {
                Vec3b& v = temp.at<Vec3b>(i, j);
                double s = 1 - 3.0 * min(v[0], min(v[1], v[2])) / (v[0] + v[1] + v[2]);
                if (!(v[2] > redThreshold && v[2] >= v[1] && v[1] > v[0] &&
                    s >= ((255 - v[2]) * saturationThreshold / redThreshold))) {
                    mMask.at<uchar>(i+y, j+x) = 0;
                }
            }
        }
    }
    //temp.release();
}

void TargetExtractor::denoise(int ksize, int threshold){
    int r = (ksize - 1) / 2;
    if (r <= 0) {
        return;
    }

    Mat density;
    calcDensity(mMask, density, ksize);

    for (int i = r; i < mMask.rows - r; i++) {
        for (int j = r; j < mMask.cols - r; j++) {
            int count = density.at<int>(i, j);
            if (count < threshold) {
                mMask.at<uchar>(i, j) = 0;
            }
        }
    }
    density.release();
}

void TargetExtractor::fill(int ksize, int threshold){
    int r = (ksize - 1) / 2;
    if (r <= 0) {
        return;
    }

    Mat density;
    calcDensity(mMask, density, ksize);

    double half = ksize / 2.0, dist = ksize / 5.0;
    int max = ksize * ksize * 9 / 10;

    for (int i = r; i < mMask.rows - r; i++) {
        for (int j = r; j < mMask.cols - r; j++) {
            int count = density.at<int>(i, j);
            if (count > max) {
                mMask.at<uchar>(i, j) = 255;
            } else if (count >= threshold) {
                // TODO: further optimize the mass-center calculation
                Point center;
                Rect rect(j - r, i - r, ksize, ksize);
                getMassCenter(mMask(rect), center);
                if (abs(center.x - half) < dist && abs(center.y - half) < dist) {
                    mMask.at<uchar>(i, j) = 255;
                }
            }
        }
    }
    density.release();
}

void TargetExtractor::regionGrow(int threshold){
    Mat gray;
    cvtColor(mFrame, gray, CV_BGR2GRAY);

    Mat temp;
    mMask.copyTo(temp);

    vector<vector<Point> > contours;
    findContours(temp, contours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE);

    int maxQueueSize = mFrame.rows * mFrame.cols / 4;
    static int direction[8][2] = {
        { 0, 1 }, { 1, 1 }, { 1, 0 }, { 1, -1 },
        { 0, -1 }, { -1, -1 }, { -1, 0 }, { -1, 1 }
    };

    for (int i = 0; i < contours.size(); i++) {
        Rect rect = boundingRect(Mat(contours[i]));
        Mat mask = Mat::zeros(gray.size(), CV_8U);
        drawContours(mask, contours, i, Scalar::all(255), CV_FILLED);
        int size = sum(mask(rect))[0] / 255;
        Scalar m, s;
        meanStdDev(gray(rect), m, s, mask(rect));
        double mean = m[0], stdDev = s[0];

        Mat temp;
        mMask.copyTo(temp);
        int origSize = size;

        queue<Point> pointQueue;
        for (int j = 0; j < contours[i].size(); j++) {
            uchar pixel = gray.at<uchar>(contours[i][j]);
            if (abs(pixel - mean) < 1.0 * stdDev) {
                pointQueue.push(contours[i][j]);
            }
        }

        Point cur, pop;
        while (!pointQueue.empty() && pointQueue.size() < maxQueueSize) {

            pop = pointQueue.front();
            pointQueue.pop();
            uchar pixel = gray.at<uchar>(pop);

            for (int k = 0; k < 8; k++) {
                cur.x = pop.x + direction[k][0];
                cur.y = pop.y + direction[k][1];

                if (cur.x < 0 || cur.x > gray.cols - 1 || cur.y < 0 || cur.y > gray.rows - 1) {
                    continue;
                }

                if (temp.at<uchar>(cur) != 255) {
                    uchar curPixel = gray.at<uchar>(cur);

                    if (abs(curPixel - pixel) < threshold &&
                        abs(curPixel - mean) < 1.0 * stdDev) {

                        temp.at<uchar>(cur) = 255;

                        double diff = curPixel - mean;
                        double learningRate = 1.0 / (++size);
                        mean = (1 - learningRate) * mean + learningRate * curPixel;
                        stdDev = sqrt((1 - learningRate) * stdDev * stdDev + learningRate * diff * diff);

                        pointQueue.push(cur);
                    }
                }
            }
        }

        if (pointQueue.empty()) {
            int incSize = size - origSize;
            if (incSize < mFrame.rows * mFrame.cols / 6 && incSize / origSize < 5) {
                mMask = temp;
            }
        }
    }
}

extern vector<ContourInfo*> xContours;

extern vector<ContourInfo*> saveContours;

Mat elemenDilate = getStructuringElement( MORPH_ELLIPSE,
                                     Size( 2*DILATION_SIZE + 1, 2*DILATION_SIZE+1 ),
                                     Point( DILATION_SIZE, DILATION_SIZE ) );
Mat elementErode = getStructuringElement( MORPH_ELLIPSE,
                                    Size( 2*EROSION_SIZE + 1, 2*EROSION_SIZE+1 ),
                                    Point( EROSION_SIZE, EROSION_SIZE ) );

void TargetExtractor::smallAreaFilter(int threshold, int keep, int countFrame){
    bool Existed=false;
    bool Existed1=false;
    vector<vector<Point> > contours;
    // this will change mMask, but it doesn't matter
    findContours(mMask, contours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE);

    vector<int> indexes;
    vector<double> areas;
    vector<Rect> boundRects;
    vector<int> label;

    for (int i = 0; i < contours.size(); i++) {
        double area = contourArea(contours[i]);
        if (area < threshold) {
            continue;
        }

        Rect rect = boundingRect(Mat(contours[i]));
        if (rect.width < 0.01 * mMask.cols && rect.height < 0.01 * mMask.rows) {
            continue;
        }
        rect.x=rect.x*2;
        rect.y=rect.y*2;
        rect.width=rect.width*2;
        rect.height=rect.height*2;

        indexes.push_back(i);
        areas.push_back(area);
        boundRects.push_back(rect);
        label.push_back((countFrame-SKIP_FRAME_COUNT)%CHECK_CONTINUES);
    }

    mMask = Mat::zeros(mMask.size(), mMask.type());
    vector<ContourInfo>().swap(mContours); //xóa các phần tử trong vector, nếu không loại bỏ thì các biên sẽ rơi vào vòng lặp

    //mContours.clear();
    //x = Mat::zeros(mMask.size(), mMask.type());

    if (areas.size() == 0) {
        return;
    }

    while (keep > 0) {
        vector<double>::iterator it = max_element(areas.begin(), areas.end());
        if (*it == 0) {
            break;
        }

        vector<double>::difference_type offset = it - areas.begin();    //difference_tye: xác định khoảng cách giữa các vòng lặp
        int index = indexes[offset];

        //Dùng label giải quyết điều kiện tọa độ contours nếu trong cùng 1 frame => cần tối ưu hóa
        if(saveContours.size()==0){
            //vector<ContourInfo>::size_type size1 = saveContours->size();
            //saveContours->resize(size1 + 1);
            ContourInfo *temp=new ContourInfo;
            temp->contour=contours[index];
            temp->area = areas[offset];
            temp->boundRect = boundRects[offset];
            temp->count = 1;
            //cout<<size1<<saveContours[size1]->count<<endl;
            temp->label=label[offset];
            saveContours.push_back(temp);
            //delete temp;
            Existed1=true;
        }
        else{
            for(int i=0;i<saveContours.size();i++){
                if((boundRects[offset].x <= saveContours[i]->boundRect.x + MAX_X && boundRects[offset].x >=  saveContours[i]->boundRect.x - MAX_X)||
                    (boundRects[offset].y <=  saveContours[i]->boundRect.y + MAX_Y && boundRects[offset].y >= saveContours[i]->boundRect.y - MAX_Y)){
                    if(saveContours[i]->label==label[offset])
                       break;
                    //cout<<"XXXXXX"<<endl;
                    saveContours[i]->contour=contours[index];
                    saveContours[i]->area = areas[offset];
                    saveContours[i]->boundRect = boundRects[offset];
                    saveContours[i]->count++;
                    saveContours[i]->label=label[offset];
                    //cout<<i<<": "<<saveContours[i]->count<<endl;
                    //delete saveContours[i];
                    if(saveContours[i]->count==APPERANCE){
                        //vector<ContourInfo>::size_type size2 = xContours->size();
                        //xContours->resize(size2 + 1);
                        ContourInfo *temp=new ContourInfo;
                        temp->contour=contours[index];
                        temp->area = areas[offset];
                        temp->boundRect = boundRects[offset];
                        temp->label=label[offset];
                        xContours.push_back(temp);
                        //delete temp;
                        //cout<<"size2: "<<size2<<endl;
                        //drawContours(x, contours, index, Scalar::all(255), CV_FILLED);
                        Existed=true;
                        //saveContours.erase(saveContours.begin()+i);
                        break;
                    }
                    else if(saveContours[i]->count>APPERANCE){
                        //drawContours(x, contours, index, Scalar::all(255), CV_FILLED);
                        Existed=true;
                        break;
                    }
                    else{
                        Existed=true;
                        break;
                    }
                }
            }
        }

        if(Existed==false && Existed1==false){
            //vector<ContourInfo>::size_type size3 = saveContours->size();
            //saveContours->resize(size3 + 1);
            ContourInfo *temp=new ContourInfo;
            temp->contour=contours[index];
            temp->area = areas[offset];
            temp->boundRect = boundRects[offset];
            temp->count = 1;
            temp->label=label[offset];
            saveContours.push_back(temp);
            //delete temp;
            //cout<<size3<<" :"<<(*saveContours)[size3].count<<endl;
        }
        Existed=false;
        Existed1=false;

        drawContours(mMask, contours, index, Scalar::all(255), CV_FILLED);

        // use 'resize' and 'swap' to avoid copy of contours
        vector<ContourInfo>::size_type size = mContours.size();
        mContours.resize(size + 1);
        mContours[size].contour.swap(contours[index]);
        mContours[size].area = areas[offset];
        mContours[size].boundRect = boundRects[offset];
        *it = 0;
        keep--;
    }
}

void TargetExtractor::blobTrack(map<int, Target>& targets, vector<ContourInfo> temp){
    list<Region> regions;
    for (vector<ContourInfo>::iterator it = temp.begin(); it != temp.end(); it++) {
        regions.push_back(Region(&(*it), it->boundRect));
    }

    list<Region>::size_type lastRegionsSize;
    do {
        lastRegionsSize = regions.size();
        for (list<Region>::iterator it1 = regions.begin(); it1 != regions.end(); it1++) {
            list<Region>::iterator it2 = it1;
            for (it2++; it2 != regions.end(); ) {
                if (it1->near(*it2)) {
                    it1->merge(*it2);
                    regions.erase(it2++);
                } else {
                    it2++;
                }
            }
        }
    } while (regions.size() != lastRegionsSize);

    srand((unsigned)clock());

    map<int, Target>().swap(targets);

    if (targets.empty()) {
        int id;
        for (list<Region>::iterator it = regions.begin(); it != regions.end(); it++) {
            while (id = rand(), targets.find(id) != targets.end());
            targets[id] = Target();
            targets[id].type = Target::TARGET_NEW;
            targets[id].region = *it;
            targets[id].times++;
        }
        return;
    }

    list<Rectangle> rects;
    map<int, Rectangle> targetRects;
    for (list<Region>::iterator it = regions.begin(); it != regions.end(); it++) {
        rects.push_back(it->rect);
    }
    for (map<int, Target>::iterator it = targets.begin(); it != targets.end(); it++) {
        rects.push_back(it->second.region.rect);
        targetRects[it->first] = it->second.region.rect;
    }

    list<Rectangle>::size_type lastRectsSize;
    do {
        lastRectsSize = rects.size();
        for (list<Rectangle>::iterator it1 = rects.begin(); it1 != rects.end(); it1++) {
            list<Rectangle>::iterator it2 = it1;
            for (it2++; it2 != rects.end(); ) {
                if (it1->near(*it2)) {
                    it1->merge(*it2);
                    rects.erase(it2++);
                } else {
                    it2++;
                }
            }
        }
    } while (rects.size() != lastRectsSize);

    for (list<Rectangle>::iterator it1 = rects.begin(); it1 != rects.end(); it1++) {
        vector<int> vi;
        vector<list<Region>::iterator> vlit;
        for (map<int, Rectangle>::iterator it2 = targetRects.begin(); it2 != targetRects.end(); it2++) {
            if (it1->contains(it2->second.tl())) {
                vi.push_back(it2->first);
            }
        }
        for (list<Region>::iterator it2 = regions.begin(); it2 != regions.end(); it2++) {
            if (it1->contains(it2->rect.tl())) {
                vlit.push_back(it2);
            }
        }
        int id;
        if (vlit.size() == 0) {
            assert(vi.size() == 1);
            id = vi[0];
            targets[id].type = Target::TARGET_LOST;
            targets[id].lostTimes++;
        } else if (vi.size() == 0) {
            assert(vlit.size() == 1);
            while (id = rand(), targets.find(id) != targets.end());
            targets[id] = Target();
            targets[id].type = Target::TARGET_NEW;
            targets[id].region = *(vlit[0]);
            targets[id].times++;
        } else {
            Region r(*(vlit[0]));
            vector<list<Region>::iterator>::iterator it3 = vlit.begin();
            for (it3++; it3 != vlit.end(); it3++) {
                r.merge(**it3);
            }
            if (vi.size() == 1) {
                id = vi[0];
                targets[id].type = Target::TARGET_EXISTING;
                targets[id].region = r;
                targets[id].times++;
            } else {
                while (id = rand(), targets.find(id) != targets.end());
                targets[id] = Target();
                targets[id].type = Target::TARGET_MERGED;
                targets[id].region = r;
                int times = 0;
                for (vector<int>::iterator it4 = vi.begin(); it4 != vi.end(); it4++) {
                    targets[id].mergeSrc.push_back(*it4);
                    if (targets[*it4].times > times) {
                        times = targets[*it4].times;
                    }
                }
                targets[id].times = times;
            }
        }
    }
}

void TargetExtractor::extract(const Mat& frame, map<int, Target>& targets, bool track, int countFrame){
    Mat temp = Mat::zeros(Size(frame.cols/4, frame.rows/4), frame.type());
    pyrDown(frame, temp);   //Scale down hình ảnh

    temp.copyTo(mFrame);

    movementDetect(LEARNING_RATE); //Phát hiện chuyển động

    if(countFrame < SKIP_FRAME_COUNT){
        temp.release();
        return;
    }
    //Thực hiện xử lý trong 6 frames liên tiếp
    else if((countFrame-SKIP_FRAME_COUNT)%CHECK_CONTINUES==0 || (countFrame-SKIP_FRAME_COUNT)%CHECK_CONTINUES==1||(countFrame-SKIP_FRAME_COUNT)%CHECK_CONTINUES==2
            ||(countFrame-SKIP_FRAME_COUNT)%CHECK_CONTINUES==3||(countFrame-SKIP_FRAME_COUNT)%CHECK_CONTINUES==4||(countFrame-SKIP_FRAME_COUNT)%CHECK_CONTINUES==5
            ||(countFrame-SKIP_FRAME_COUNT)%CHECK_CONTINUES==6){


        /* for 2.avi:
        *     movement:   0.008;
        *     color:      120, 0.2;
        *     regionGrow: enable;
        * for 6.avi:
        *     movement:   0.012;
        *     color:      150, 0.4;
        *     regionGrow: disable;
        */

        erode(mMask, mMask, elementErode);
        dilate(mMask, mMask, elemenDilate );

        //Tìm những contour sau khi thực hiện background subtraction
        vector<vector<Point> > contours;
        findContours(mMask, contours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE);

        if (contours.size()==0)
            return;

        //Thực hiện giải thuật phát hiện màu lửa trên từng contours
        for (int i = 0; i < contours.size(); i++) {
            Rect rect = boundingRect(Mat(contours[i]));
            drawContours(mMask, contours, i, Scalar::all(255), CV_FILLED);
            mFrame(rect).copyTo(temp);
            GaussianBlur(temp, temp, Size(3,3),0);
            colorDetect(RED_THRESHOLD, SATURATION_THRESHOLD, temp, (int)rect.x, (int)rect.y, (int)rect.width, (int)rect.height);
        }

        //Hiển thị sau khi thực hiện giải thuật phát hiện màu
        //namedWindow("Detect Color");
        //imshow("Detect Color", mMask);

        //denoise(KSIZE, THRESHOLD);
        //fill(KSIZE, THRESHOLD);
        //medianBlur(mMask, mMask, 3);

        // TODO: make use of accumulate result

        //regionGrow();
        //fill(7, 6);
        //medianBlur(mMask, mMask, 3);

        smallAreaFilter(AREA_THRESHOLD, KEEP_CONTOURS, countFrame);

        //Hiển thị sau khi lọc diện tích
        //namedWindow("Area Filter");
        //imshow("Area Filter", mMask);

        //Khoanh vùng lửa sau 6 frames và giải phóng bộ nhớ
        if (track && (countFrame-SKIP_FRAME_COUNT)%CHECK_CONTINUES==6) {
            //x = Mat::zeros(mMask.size(), mMask.type());
            vector<vector<Point> > contours;
            vector<ContourInfo> temp;
            for(int i=0;i<xContours.size();i++){
                vector<ContourInfo>::size_type size = temp.size();
                temp.resize(size + 1);
                temp[size].contour=xContours[i]->contour;
                temp[size].area=xContours[i]->area;
                temp[size].label=xContours[i]->label;
                temp[size].boundRect=xContours[i]->boundRect;
                contours.push_back(xContours[i]->contour);
            }
            //for(int i=0;i<contours.size();i++)
                //drawContours(x, contours, i, Scalar::all(255), CV_FILLED);

            //Hiển thị những contours là lửa
            //namedWindow("Flame");
            //imshow("Flame", x);
            //x.release();

            blobTrack(targets,temp);

            vector<ContourInfo>().swap(temp);

            /******************XÓA xContours và saveContours************/
            for(int i=0;i<xContours.size();i++)
                delete xContours[i];

            for(int i=0;i<saveContours.size();i++)
                delete saveContours[i];

            vector<ContourInfo*>().swap(saveContours);
            vector<ContourInfo*>().swap(xContours);
            /***********************************************************/
        }
        temp.release();
    }
}


TargetExtractor::~TargetExtractor(){

}

FeatureAnalyzer::FeatureAnalyzer(){

}

void FeatureAnalyzer::targetUpdate(map<int, Target>& targets){
    for (map<int, Target>::iterator it = targets.begin(); it != targets.end(); ) {
        Target& target = it->second;

        if (target.type == Target::TARGET_LOST) {
            int maxTimes = min(target.times * 2, 10);
            if (target.lostTimes >= maxTimes) {
                targets.erase(it++);
                continue;
            }
        } else {
            if (target.lostTimes != 0) {
                target.lostTimes = 0;
            }
            if (target.type == Target::TARGET_MERGED) {
                vector<int>& keys = target.mergeSrc;
                //featureMerge(target, targets, keys);
                for (vector<int>::const_iterator it2 = keys.begin(); it2 != keys.end(); it2++) {
                    targets.erase(targets.find(*it2));
                }
                vector<int>().swap(keys);
            }
        }
        it++;
    }
}

void FeatureAnalyzer::analyze(const Mat& frame, Mat& result, map<int, Target>& targets){
    mFrame = frame;

    targetUpdate(targets);

    mFrame.copyTo(result);
    for (map<int, Target>::iterator it = targets.begin(); it != targets.end(); it++) {
        rectangle(result, it->second.region.rect, Scalar(0, 255, 0));
    }
}

FeatureAnalyzer::~FeatureAnalyzer(){

}

FlameDetector::FlameDetector()
: mFrameCount(0)
, mFlameCount(0)
, mTrack(false)
{
}

void FlameDetector::detect(const Mat& frame, Mat &result){
    mTrack=false;
    mFrame = frame;

    clock_t start, finish;
    if(++mFrameCount >= SKIP_FRAME_COUNT) {
        mTrack = true;
        start = clock();
    }

    mExtractor.extract(mFrame, mTargetMap, mTrack, mFrameCount);
    if (mTrack) {
        if((mFrameCount-SKIP_FRAME_COUNT)%CHECK_CONTINUES==6)
            mAnalyzer.analyze(mFrame, result, mTargetMap);
        else
            mFrame.copyTo(result);
        finish = clock();
        cout << "duration: " << 1.0 * (finish - start) / CLOCKS_PER_SEC << endl;
        cout << "frame: " << (mFrameCount - SKIP_FRAME_COUNT) << ", flame: " << mFlameCount << endl;
    }
    else
        mFrame.copyTo(result);
}

FlameDetector::~FlameDetector(){

}

FireDetection::FireDetection()
:mVideoFPS(0){
    if (mCapture.isOpened()) {
        mVideoFPS = mCapture.get(CV_CAP_PROP_FPS);
        assert(mVideoFPS != 0);
    }
}

void FireDetection::fireDetection(const Mat& src, Mat &result){
    mDetector.detect(src,result);
}

FireDetection::~FireDetection(){

}
