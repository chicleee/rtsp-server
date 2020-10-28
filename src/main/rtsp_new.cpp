//compile: g++ -std=c++11 rtsp_new.cpp -o rtsp_new -lpthread -lgstapp-1.0 `pkg-config --libs --cflags opencv gstreamer-1.0 gstreamer-rtsp-server-1.0`
//view: gst-launch-1.0 rtspsrc location=rtsp://127.0.0.1:8554/test latency=10 ! rtph264depay ! avdec_h264 ! videoconvert ! autovideosink
//vlc rtsp://127.0.0.1:8554/test

#include <iostream>
#include <sys/stat.h>
#include <sys/types.h>
#include <string>
#include <time.h>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc/types_c.h>
#include <gstreamer-1.0/gst/gstelement.h>
#include <gstreamer-1.0/gst/gstpipeline.h>
#include <gstreamer-1.0/gst/gstutils.h>
#include <gstreamer-1.0/gst/app/gstappsrc.h>
#include <gstreamer-1.0/gst/base/gstbasesrc.h>
#include <gstreamer-1.0/gst/video/video.h>
#include <gstreamer-1.0/gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>
#include <glib.h>
#include <pthread.h>
#include <stdlib.h>

//vector<ContourInfo*> xContours;
//vector<ContourInfo*> saveContours;

//debug:
/*
#define GST_CAT_DEFAULT appsrc_pipeline_debug
GST_DEBUG_CATEGORY (appsrc_pipeline_debug);
//GST_DEBUG_CATEGORY (pipeline_debug);
*/
       
using namespace std;
using namespace cv;


//paramates
#define cameraWidth 1280
#define cameraHeight 720
#define outFps 5
#define inputRtsp "rtsp://admin:kuangping108@192.168.1.64/h264/ch1/main/av_stream"
#define RTSP_PORT "8554"
#define index "0"


static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER; //创建互斥进程
GMainLoop *loop;
typedef struct _App App;

//用于gstreamer线程的结构：:
struct _App{
GstElement *ffmpeg;
//GstElement *ffmpeg2;
//GstElement *rtppay, *gdppay;
GstElement *rtppay;
GstElement *videoenc; 
GstElement *videosrc;
GstElement *sink;
GstElement *videoscale;
GstElement *filter;
guint sourceid;
GstElement *queue;
GTimer *timer;
};
App s_app;

typedef struct
{
  gboolean white;
  GstClockTime timestamp;
} MyContext;

int counter = 0;

//两个线程之间共享的帧：  
Mat frameimage;    

void processing( Mat frame )
{
    Rect rect(200, 200, 300, 300);//左上坐标（x,y）和矩形的长(x)宽(y)
    cv::rectangle(frame, rect, Scalar(255, 0, 0),1, 1, 0);
    cv::putText(frame, "example of image-processing", cv::Point(200, 200), CV_FONT_HERSHEY_PLAIN, 1, cv::Scalar(255, 255, 255));
}

/*
 将从线程接收到的帧插入缓冲区的函数
 OpenCVthread并将其传递给gstreamer appsrc元素
    @app: 它是指向app结构（包含gstreamer元素）的指针)
    @return: 返回一个布尔值以指示是否有任何错误 
*/

static void need_data (GstElement * appsrc, guint unused, MyContext * ctx){  

    //cvNamedWindow( "iplimage", CV_WINDOW_AUTOSIZE );
    //static GstClockTime timestamp = 0;
    GstBuffer *buffer;
    guint buffersize;
    GstFlowReturn ret;
    GstMapInfo info;

    counter++;
    //m.lock();
    pthread_mutex_lock( &m ); 
        /* allocate buffer */ 
        buffersize = frameimage.cols * frameimage.rows * frameimage.channels();
        //cout<<"frameimage.cols:"<<frameimage.cols <<"  frameimage.rows:"<<frameimage.rows << "  frameimage.channels:" << frameimage.channels()<<endl;
        buffer = gst_buffer_new_and_alloc(buffersize);
        uchar *  IMG_data = frameimage.data;
    //m.unlock();
    pthread_mutex_unlock( &m );    

        if (gst_buffer_map (buffer, &info, (GstMapFlags)GST_MAP_WRITE)) {
            memcpy(info.data, IMG_data, buffersize);
            gst_buffer_unmap (buffer, &info);
        }
        else g_print("OPS! ERROR.");
    
   
    ctx->white = !ctx->white;

    //increment the timestamp every {duration = 1/outFps} second
    GST_BUFFER_PTS (buffer) = ctx->timestamp;
    std::cout<<"Gctx->timestamp:"<< ctx->timestamp <<std::endl;
    GST_BUFFER_DURATION (buffer) = gst_util_uint64_scale_int (1, GST_SECOND, outFps);
    ctx->timestamp += GST_BUFFER_DURATION (buffer);

    //std::cout<<"GST_BUFFER_DURATION (buffer):"<<GST_BUFFER_DURATION (buffer)<<std::endl;

    //有足够的数据提供给appsrc：
    g_signal_emit_by_name (appsrc, "push-buffer", buffer, &ret);     
    
    if (ret != GST_FLOW_OK) {
        g_print("ops\n");
        GST_DEBUG ("something wrong in cb_need_data");
        g_main_loop_quit (loop);
    }
    gst_buffer_unref (buffer);   
    std::cout<<"Frame:"<<counter<<std::endl;
}



static void
media_configure (GstRTSPMediaFactory * factory, GstRTSPMedia * media,
    gpointer user_data )
{
  GstElement *element, *appsrc;
  MyContext *ctx;

  /* get the element used for providing the streams of the media */
  element = gst_rtsp_media_get_element (media);

  /* get our appsrc, we named it 'mysrc' with the name property */
  appsrc = gst_bin_get_by_name_recurse_up (GST_BIN (element), "mysrc");

  /* this instructs appsrc that we will be dealing with timed buffer */
  // g_object_set (G_OBJECT (appsrc), "is-live" , TRUE ,  NULL);
  // g_object_set (G_OBJECT (appsrc), "min-latency" , 67000000 ,  NULL);  
  g_object_set (G_OBJECT (appsrc), 
    "stream-type" , 0 , //rtsp
    "format" , GST_FORMAT_TIME , NULL);
  

  g_object_set (G_OBJECT (appsrc), "caps",
      gst_caps_new_simple ("video/x-raw",
          "format", G_TYPE_STRING, "BGR",
          "width", G_TYPE_INT, cameraWidth,
          "height", G_TYPE_INT, cameraHeight,
          "framerate", GST_TYPE_FRACTION, outFps, 1,
  "pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1, NULL), NULL);
/*
  GstCaps *caps = gst_caps_new_simple("video/x-raw",
        "format", G_TYPE_STRING, "RGB",
        "width", G_TYPE_INT, 640,
        "height", G_TYPE_INT, 480,
        //"width", G_TYPE_INT, 800,
        //"height", G_TYPE_INT, 600,
        "framerate", GST_TYPE_FRACTION, 30, 1,
        "pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1,
    NULL);    
    gst_app_src_set_caps(GST_APP_SRC(appsrc), caps);
    g_object_set (G_OBJECT (appsrc),"stream-type", 0,"format", GST_FORMAT_TIME, NULL);*/

  /* configure the caps of the video 
  g_object_set (G_OBJECT (appsrc), "caps",
      gst_caps_new_simple ("video/x-raw",
          "format", G_TYPE_STRING, "RGB16",
          "width", G_TYPE_INT, 384,
          "height", G_TYPE_INT, 288,
          "framerate", GST_TYPE_FRACTION, 0, 1, NULL), NULL);*/

  ctx = g_new0 (MyContext, 1);
  ctx->white = FALSE;
  ctx->timestamp = 0;
  /* make sure ther datais freed when the media is gone */
  g_object_set_data_full (G_OBJECT (media), "my-extra-data", ctx,
      (GDestroyNotify) g_free);

  /* install the callback that will be called when a buffer is needed */
  g_signal_connect (appsrc, "need-data", (GCallback) need_data, ctx);
  //g_signal_connect (appsrc, "need-data", G_CALLBACK (start_feed), );
  //g_signal_connect (appsrc, "enough-data", G_CALLBACK (stop_feed), );
  gst_object_unref (appsrc);
  gst_object_unref (element);
}


/*****
 thread2: push stream
 */
void *thread2new(void *arg){

    App * app = &s_app;
    GstCaps * caps2;  
    GstCaps * caps3;
    GstFlowReturn ret;
    //GstBus *bus;
    GstElement *pipeline;

    GstRTSPServer *server;
    GstRTSPMountPoints *mounts;
    GstRTSPMediaFactory *factory;

    gst_init (NULL,NULL);
    loop = g_main_loop_new (NULL, FALSE);
    server = gst_rtsp_server_new ();
    g_object_set (server, "service", RTSP_PORT, NULL);
    mounts = gst_rtsp_server_get_mount_points (server);
    factory = gst_rtsp_media_factory_new ();

/*    gst_rtsp_media_factory_set_launch (factory,
      "( appsrc name=mysrc ! videoconvert ! capsfilter caps=video/x-raw,format=I420,width=640,height=480,framerate=15/1,pixel-aspect-ratio=1/1 ! x264enc noise-reduction=10000 tune=zerolatency ! rtph264pay config-interval=1 name=pay0 pt=96 )");
*/
/*
  gst_rtsp_media_factory_set_launch (factory,
      "( appsrc name=mysrc ! videoconvert ! capsfilter caps=video/x-raw,format=I420,width=640,height=480,framerate=15/1,pixel-aspect-ratio=1/1 ! jpegenc tune=zerolatency ! rtpjpegpay name=pay0 pt=96 )");*/

/*    gst_rtsp_media_factory_set_launch (factory,
      "( appsrc name=mysrc ! videoconvert ! capsfilter caps=video/x-raw,format=RGB,width=1920,height=1080,framerate=25/1,pixel-aspect-ratio=1/1 ! tee name=\"local\" ! queue ! ximagesink local. ! queue ! x264enc noise-reduction=10000 tune=zerolatency ! rtph264pay config-interval=1 name=pay0 pt=96 )");
*/
/*  gst_rtsp_media_factory_set_launch (factory,
      "( v4l2src ! video/x-raw,width=640,height=480 ! timeoverlay ! tee name=\"local\" ! queue ! autovideosink local. ! queue ! jpegenc ! rtpjpegpay name=pay0 pt=96 )");*/
    char *outAppsrc = new char[200];
    sprintf(outAppsrc, "( appsrc name=mysrc is-live=true block=true format=GST_FORMAT_TIME caps=video/x-raw,format=BGR,width=%d,height=%d,framerate=%d/1 ! videoconvert ! video/x-raw,format=I420 ! x264enc speed-preset=ultrafast tune=zerolatency ! rtph264pay config-interval=1 name=pay0 pt=96 )", 
      cameraWidth, cameraHeight, outFps);
    gst_rtsp_media_factory_set_launch (factory, outAppsrc);

    g_signal_connect (factory, "media-configure", (GCallback) media_configure, NULL);
    //g_signal_connect (app->videosrc, "need-data", G_CALLBACK (start_feed), app);
    //g_signal_connect (app->videosrc, "enough-data", G_CALLBACK (stop_feed),app);
   
    char index_url[16] = {0};
    /* attach the test factory to the /test url */
    sprintf(index_url, "/index/%s", index);
    gst_rtsp_mount_points_add_factory (mounts, index_url, factory);

    /* don't need the ref to the mounts anymore */
    g_object_unref (mounts);

    /* attach the server to the default maincontext */
    gst_rtsp_server_attach (server, NULL);

    /* start serving */
    g_print ("stream ready at rtsp://127.0.0.1:%s%s\n",RTSP_PORT,index_url);
    g_main_loop_run (loop);

    pthread_exit(NULL);
}

/*****
 thread1: fetch stream
 */
void *thread1(void *arg){
    VideoCapture cap(inputRtsp);
    Mat tempframe, result;

    if (!cap.isOpened()) {
        throw "Error when reading steam from camera";
    }
    cap.set(CV_CAP_PROP_FRAME_WIDTH, cameraWidth);
    cap.set(CV_CAP_PROP_FRAME_HEIGHT, cameraHeight);


    while (1) {
        cap.read(tempframe);
        pthread_mutex_lock( &m ); 
                frameimage = tempframe;
                //resize frames. Speed comparison:INTER_NEAREST、INTER_LINEAR>INTER_CUBIC>INTER_AREA
                //缩小图像，避免出现波纹现象通常使用#INTER_AREA；放大图像通常使用INTER_CUBIC(速度较慢，但效果最好)，或INTER_LINEAR(速度较快，效果还可以)。INTER_NEAREST，一般不推荐
                resize(frameimage,frameimage,Size(cameraWidth, cameraHeight),0,0,INTER_AREA);

                /* frame image processing */
                processing( frameimage );

        pthread_mutex_unlock( &m );
    }
    pthread_exit(NULL);    
}

int main(int argc, char** argv){
  int rc1, rc2;
  pthread_t CaptureImageThread, StreamThread;

  if( (rc1 = pthread_create(&CaptureImageThread, NULL, thread1, NULL)) )
    cout << "Thread creation failed: " << rc1 << endl;
  if( (rc2 = pthread_create(&StreamThread, NULL, thread2new, NULL)) )
  cout << "Thread creation failed: " << rc2 << endl;
  
  pthread_join( CaptureImageThread, NULL );
  pthread_join( StreamThread, NULL );
  return 0;
};