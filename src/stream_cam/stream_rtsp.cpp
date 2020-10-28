//#include <iostream>
#include <sys/stat.h>
#include <sys/types.h>
//#include <string>
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
//#include <pthread.h>
#include <stdlib.h>

#define _1080_P_
       
using namespace std;
using namespace cv;

#ifdef _720_P_
int cameraWidth = 1280;
int cameraHeight = 720;
#endif
#ifdef _1080_P_
int cameraWidth = 1920;
int cameraHeight = 1080;
#endif
static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER; //mutex per la mutua esclusione
GMainLoop *loop; //loop di gstreamer
//typedef struct _App App;

//struttura da usare per il thread di gstreamer:
/*
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
App s_app;*/

typedef struct
{
  gboolean white;
  GstClockTime timestamp;
} MyContext;

//int counter = 0;

//frame condiviso tra i due thread:
 
//cv::Mat frameimage (2,2,CV_8UC1,cv::Scalar(255));   
//cv::Mat dst (320,240,CV_8UC1,cv::Scalar(255));  
//cv::Mat dii3;
//cv::Mat dst_roi;
//cv::Mat win;
    
//Mat frameimage;    
 Mat frameimage = cv::Mat(cv::Size(cameraWidth, cameraHeight),
                   CV_8UC3)   ;
/*
 cb_need_data: funzione che inserisce nel buffer il frame ricevuto dal thread 
 OpenCVthread e lo passa all'elemento appsrc di gstreamer
    @app: Ã¨ un puntatore alla struttura App (che contiene gli elementi di gstreamer)
    @return: ritorna un valore booleano per indicare se c'Ã¨ stato qualche errore 
*/

static void need_data (GstElement * appsrc, guint unused, MyContext * ctx){  

    //static GstClockTime timestamp = 0;
    GstBuffer *buffer;
    guint buffersize;
    GstFlowReturn ret;
    GstMapInfo info;

    //m.lock();
    pthread_mutex_lock( &m );
                
    
        buffersize = frameimage.cols * frameimage.rows * frameimage.channels();
         
        buffer = gst_buffer_new_and_alloc(buffersize);

        uchar *  IMG_data = frameimage.data;
    
    pthread_mutex_unlock( &m );    
    //m.unlock();

        if (gst_buffer_map (buffer, &info, (GstMapFlags)GST_MAP_WRITE)) {
            memcpy(info.data, IMG_data, buffersize);
            gst_buffer_unmap (buffer, &info);
        }
        else g_print("OPS! ERROR.");
    
   
    ctx->white = !ctx->white;

    GST_BUFFER_PTS (buffer) = ctx->timestamp;
    GST_BUFFER_DURATION (buffer) = gst_util_uint64_scale_int (15, GST_MSECOND, 1);
    ctx->timestamp += GST_BUFFER_DURATION (buffer);

    //segnalo che ho abbastanza dati da fornire ad appsrc:
    g_signal_emit_by_name (appsrc, "push-buffer", buffer, &ret);  

    //GST_DEBUG ("everything allright in cb_need_data");
    
    //nel caso di errore esce dal loop:
    if (ret != GST_FLOW_OK) {
        g_print("ops\n");
        GST_DEBUG ("something wrong in cb_need_data");
        g_main_loop_quit (loop);
    }
    //g_print("end gstreamer \n");

     gst_buffer_unref (buffer);
   
    //delete img;
    
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
  gst_util_set_object_arg (G_OBJECT (appsrc), "format", "time");

  g_object_set (G_OBJECT (appsrc), "caps",
      gst_caps_new_simple ("video/x-raw",
          "format", G_TYPE_STRING, "RGB",
          "width", G_TYPE_INT, cameraWidth,
          "height", G_TYPE_INT, cameraHeight,
          "framerate", GST_TYPE_FRACTION, 15, 1,
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


/*
 start_feed:richiama la funzione cb_need_data in modo continuo ogni ms
    @app: Ã¨ un puntatore alla struttura App (che contiene gli elementi di gstreamer)
    @pipeline: la pipeline di gstreamer
    @size: ...
    @return: void
 */




/*****
 thread2: thread per la gestione della pipeline di gstreamer
 */


//bool start = false;


void *thread2new(void *arg){

    //App * app = &s_app; //struttura dati di gstramer
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

    mounts = gst_rtsp_server_get_mount_points (server);

    factory = gst_rtsp_media_factory_new ();

#ifdef _720_P_
    //jpegenc, width và height trong capsfilter = cameraWidth và cameraHeight
    gst_rtsp_media_factory_set_launch (factory,
      "( appsrc name=mysrc ! videoconvert ! capsfilter caps=video/x-raw,format=I420,width=1280,height=720,framerate=15/1,pixel-aspect-ratio=1/1 ! jpegenc tune=zerolatency pass=qual ! rtpjpegpay name=pay0 pt=96 )");
#endif
#ifdef _1080_P_
    //jpegenc, width và height trong capsfilter = cameraWidth và cameraHeight
    gst_rtsp_media_factory_set_launch (factory,
      "( appsrc name=mysrc ! videoconvert ! capsfilter caps=video/x-raw,format=I420,width=1920,height=1080,framerate=15/1,pixel-aspect-ratio=1/1 ! jpegenc tune=zerolatency pass=qual ! rtpjpegpay name=pay0 pt=96 )");
#endif

    //x264enc, width và height trong capsfilter = cameraWidth và cameraHeight
/*
    gst_rtsp_media_factory_set_launch (factory,
      "( appsrc name=mysrc ! videoconvert ! capsfilter caps=video/x-raw,format=I420,width=640,height=480,framerate=15/1,pixel-aspect-ratio=1/1 ! x264enc bitrate=256 noise-reduction=10000 tune=zerolatency pass=qual ! rtph264pay config-interval=1 name=pay0 pt=96 )");
*/

    g_signal_connect (factory, "media-configure", (GCallback) media_configure, NULL);
    //g_signal_connect (app->videosrc, "need-data", G_CALLBACK (start_feed), app);
    //g_signal_connect (app->videosrc, "enough-data", G_CALLBACK (stop_feed),app);
   
    
    /* attach the test factory to the /test url */
    gst_rtsp_mount_points_add_factory (mounts, "/test", factory);

    /* don't need the ref to the mounts anymore */
    g_object_unref (mounts);

    /* attach the server to the default maincontext */
    gst_rtsp_server_attach (server, NULL);

    /* start serving */
    g_print ("stream ready at rtsp://127.0.0.1:8554/test\n");
    g_main_loop_run (loop);

    pthread_exit(NULL);
}



void *thread1(void *arg){
    VideoCapture cap(1);
    Mat tempframe, result;
   // FireDetection FD;

    if (!cap.isOpened()) {
        throw "Error when reading steam_avi";
    }
	cap.set(CV_CAP_PROP_FRAME_WIDTH, cameraWidth);
	cap.set(CV_CAP_PROP_FRAME_HEIGHT, cameraHeight);


    while (1) {
   
        cap.read(tempframe);
	//tempframe = imread("2.jpg");
	//if(! tempframe.data ){ //nel caso in cui non la trova
      	//	cout <<  "Could not open or find image" << std::endl ;
        //}
	//FD.fireDetection(tempframe,result);   

        pthread_mutex_lock( &m ); 

                //frameimage = tempframe;
		//frameimage = result;
resize(tempframe,frameimage,Size(cameraWidth, cameraHeight), 0, 0, INTER_CUBIC);
                cv::cvtColor(frameimage, frameimage,CV_BGR2RGB);
 		//cout << frameimage.cols;
                
        pthread_mutex_unlock( &m );
	//imshow("frame",tempframe);
	//waitKey(20);
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
