//compile: g++ -std=c++11 udp_jpegenc.cpp -o udp -lpthread -lgstapp-1.0 `pkg-config --libs --cflags opencv gstreamer-1.0`
//view: gst-launch-1.0 udpsrc port=5000 ! application/x-rtp, encoding-name=JPEG, payload=26 ! rtpjpegdepay ! jpegdec ! videoconvert ! autovideosink

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
#include <glib.h>
#include <pthread.h>
#include <stdlib.h>
//#include "firedetection.h"

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

static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER; //mutex per la mutua esclusione
GMainLoop *loop; //loop di gstreamer
typedef struct _App App;

//struttura da usare per il thread di gstreamer:
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
GstElement *queue1;
GstElement *timeoverlay;
GstElement *tee;
GstElement *queue2;
GstElement *videosink;
GTimer *timer;
};
App s_app;

int counter = 0;

//frame condiviso tra i due thread:
 
//cv::Mat frameimage (2,2,CV_8UC1,cv::Scalar(255));   
//cv::Mat dst (320,240,CV_8UC1,cv::Scalar(255));  
//cv::Mat dii3;
//cv::Mat dst_roi;
//cv::Mat win;
    
Mat frameimage;    
    
/*
 cb_need_data: funzione che inserisce nel buffer il frame ricevuto dal thread 
 OpenCVthread e lo passa all'elemento appsrc di gstreamer
    @app: Ã¨ un puntatore alla struttura App (che contiene gli elementi di gstreamer)
    @return: ritorna un valore booleano per indicare se c'Ã¨ stato qualche errore 
*/

static gboolean cb_need_data (App * app){  

    //cvNamedWindow( "iplimage", CV_WINDOW_AUTOSIZE );
    static GstClockTime timestamp = 0;
    GstBuffer *buffer;
    guint buffersize;
    GstFlowReturn ret;
    GstMapInfo info;

    counter++;
    //m.lock();
    pthread_mutex_lock( &m );
                /*
                //logo dii
                dii3 = imread("/home/style/groovy_workspace/sandbox/maris_hmi/resources/diilogo.jpg");
                if(! dii3.data ){
                    cout <<  "Could not open or find image" << std::endl ;
                }
                dst_roi = frameimage(Rect(5,5, dii3.cols, dii3.rows));
                dii3.copyTo(dst_roi);
                */
                /*
                //finestra:
                //win = imread("/home/style/groovy_workspace/sandbox/maris_hmi/resources/window17_contorno.png");
                if(! win.data ){
                    cout <<  "Could not open or find image" << std::endl ;
                }
                overlayImage(frameimage,win,dst,cv::Point(0,0));
                */
    
        buffersize = frameimage.cols * frameimage.rows * frameimage.channels();
         
        buffer = gst_buffer_new_and_alloc(buffersize);

        uchar *  IMG_data = frameimage.data;
    //m.unlock();
    pthread_mutex_unlock( &m );    

        if (gst_buffer_map (buffer, &info, (GstMapFlags)GST_MAP_WRITE)) {
            memcpy(info.data, IMG_data, buffersize);
            gst_buffer_unmap (buffer, &info);
        }
        else g_print("OPS! ERROR.");


    //segnalo che ho abbastanza dati da fornire ad appsrc:
    g_signal_emit_by_name (app->videosrc, "push-buffer", buffer, &ret);  

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
    //return TRUE;
    
}

/*
 start_feed:richiama la funzione cb_need_data in modo continuo ogni ms
    @app: Ã¨ un puntatore alla struttura App (che contiene gli elementi di gstreamer)
    @pipeline: la pipeline di gstreamer
    @size: ...
    @return: void
 */


static void start_feed (GstElement * pipeline, guint size, App * app){
   if (app->sourceid == 0){
    //GST_DEBUG ("start feeding");
    //esegue all'infinito cb_need_data (si ferma qui):
    app->sourceid = g_timeout_add (67, (GSourceFunc) cb_need_data, app); //67ms 
    //app->sourceid = g_timeout_add (1, (GSourceFunc) cb_need_data, app);
     
    //app->sourceid = g_idle_add ((GSourceFunc) cb_need_data, app);
   }
}

/*
 stop_feed: ferma il flusso (vedi start_feed)
    @app: Ã¨ un puntatore alla struttura App (che contiene gli elementi di gstreamer)
    @pipeline: la pipeline di gstreamer
    @return: void
 */


static void stop_feed (GstElement * pipeline, App * app){
  if (app->sourceid != 0) {
    //GST_DEBUG ("stop feeding");
    g_source_remove (app->sourceid);
    app->sourceid = 0;
  }
}
/*
 bus_call: funzione che gestisce gli eventi relativi al loop di gstreamer
    @bus: Ã¨ un puntatore al bus di gstreamer
 *  @message: Ã¨ il messaggio di errore da mostrare
 *  @data: Ã¨ la struttura che viene passata (a volte si vuole mostrare l'errore
 *         relativo ad uno specifico elemento della pipeline)
    @return: ritorna un valore booleano per indicare se c'Ã¨ stato qualche errore 
 */

/*
static gboolean bus_call (GstBus *bus, GstMessage *message, gpointer data) {
    GError *err = NULL;
    gchar *dbg_info = NULL;
    GST_DEBUG ("got message %s",gst_message_type_get_name (GST_MESSAGE_TYPE (message)));
 
    switch (GST_MESSAGE_TYPE (message)) {
     case GST_MESSAGE_ERROR: {   
        gst_message_parse_error (message, &err, &dbg_info);
        g_printerr ("ERROR from element %s: %s\n",
        GST_OBJECT_NAME (message->src), err->message);
        g_printerr ("Debugging info: %s\n", (dbg_info) ? dbg_info : "none");
        g_error_free (err);
        g_free (dbg_info);
        g_main_loop_quit (loop);
        break;
     }
     case GST_MESSAGE_EOS:
        g_main_loop_quit (loop);
        break;
     default:
        break;
    }
    return TRUE;
}*/

/*
 check_log_handler: funzione per il debug
    @log_domain: ..
    @message: ..
    @user_data: struttura dati dell'utente (in questo caso sarebbe App)
    @return: void
 */

/*
static void check_log_handler (const gchar * const log_domain,
const GLogLevelFlags log_level, const gchar * const message,
gpointer const user_data){
  GstDebugLevel level;
  switch (log_level & G_LOG_LEVEL_MASK) {
    case G_LOG_LEVEL_ERROR:
        level = GST_LEVEL_ERROR;
        break;
    case G_LOG_LEVEL_CRITICAL:
        level = GST_LEVEL_WARNING;
        break;
    case G_LOG_LEVEL_WARNING:
        level = GST_LEVEL_WARNING;
        break;
    case G_LOG_LEVEL_MESSAGE:
        level = GST_LEVEL_INFO;
        break;
    case G_LOG_LEVEL_INFO:
        level = GST_LEVEL_INFO;
        break;
    case G_LOG_LEVEL_DEBUG:
        level = GST_LEVEL_DEBUG;
        break;
    default:
        level = GST_LEVEL_LOG;
        break;
    }
    gst_debug_log (GST_CAT_DEFAULT, level, "?", "?", 0, NULL, "%s", message);
}*/

/*****
 thread2: thread per la gestione della pipeline di gstreamer
 */


bool start = false;
void *thread2(void *arg){

    App * app = &s_app; //struttura dati di gstramer
    GstCaps * caps2;  
    GstCaps * caps3;
    GstFlowReturn ret;
    //GstBus *bus;
    GstElement *pipeline;
	GstPad * tee_app, * tee_udp;
	GstPad * queue_app, * queue_udp;

    gst_init (NULL,NULL);

    loop = g_main_loop_new (NULL, FALSE);

    //creazione della pipeline:
    pipeline = gst_pipeline_new ("gstreamer-encoder");   
    if( ! pipeline ) {
        g_print("Error creating Pipeline, exiting...");
    }
    
    //creazione elemento appsrc:
    app-> videosrc = gst_element_factory_make ("appsrc", "videosrc");
    if( !  app->videosrc ) {
            g_print( "Error creating source element, exiting...");       
    }
    
    //creazione elemento queue:
    app-> queue1 = gst_element_factory_make ("queue", "queue1");
    if( !  app->queue1 ) {
            g_print( "Error creating queue element, exiting...");       
    }

	app-> queue2 = gst_element_factory_make ("queue", "queue2");
    	if( !  app->queue2 ) {
            	g_print( "Error creating queue element, exiting...");       
    	}
    
    //creazione elemento filter:
    app->filter = gst_element_factory_make ("capsfilter", "filter");
    if( ! app->filter ) {
            g_print( "Error creating filter, exiting...");
    } 
    
    //creazione elemento videoscale (ridimensiona il video):
    app->videoscale = gst_element_factory_make ("videoscale", "videoscale");
    if( ! app->videoscale ) {
            g_print( "Error creating videoscale, exiting...");
    } 
    
    //creazione elemento videoenc (per la codifica h264)
   /* 
    app->videoenc = gst_element_factory_make ("x264enc", "videoenc");   
    if( !app->videoenc ) {
            std::cout << "Error creating encoder, exiting...";     
    }*/

	app->videoenc = gst_element_factory_make ("jpegenc", "videoenc");   
    	if( !app->videoenc ) {
            std::cout << "Error creating encoder, exiting...";     
    	}
    
    //creazione elemento rtppay (flusso streaming real-time)
  /*  
    app->rtppay = gst_element_factory_make ("rtph264pay", "rtppay");   
    if( !app->rtppay ) {
            std::cout << "Error creating rtppay, exiting...";        
    }*/

	app->rtppay = gst_element_factory_make ("rtpjpegpay", "rtppay");   
    	if( !app->rtppay ) {
            std::cout << "Error creating rtppay, exiting...";        
    	}
    
    //creazione elemento gdppay (payload di buffer ed eventi usando il protocollo dati di Gstreamer)
    /*
    app->gdppay = gst_element_factory_make ("gdppay", "gdppay");   
    if( !app->gdppay) {
            std::cout << "Error creating gdppay, exiting...";           
    }*/
    
    //creazione elemento videoconvert (in gstreamer-0.10 si chiamava ffmpegcolorspace)
    app-> ffmpeg = gst_element_factory_make( "videoconvert", "ffmpeg" );  //!!!!!! ffenc_mpeg2video
    if( ! app-> ffmpeg ) {
            g_print( "Error creating ffmpegcolorspace, exiting...");
    }
    /*  
    app-> ffmpeg2 = gst_element_factory_make( "videoconvert", "ffmpeg2" );  //!!!!!! ffenc_mpeg2video
    if( ! app-> ffmpeg ) {
            g_print( "Error creating ffmpegcolorspace, exiting...");
    }*/  
    //creazione elemento sink:
    /*
    app-> sink = gst_element_factory_make ("tcpserversink", "sink"); 
    if( !  app-> sink) {
            g_print( "Error creating sink, exiting...");           
    }*/
	app-> sink = gst_element_factory_make ("udpsink", "sink"); 
    	if( !  app-> sink) {
            g_print( "Error creating sink, exiting...");           
    	}


	app-> videosink = gst_element_factory_make ("autovideosink", "videosink"); 
    	if( !  app-> videosink) {
            g_print( "Error creating sink, exiting...");           
    	}

	app-> timeoverlay = gst_element_factory_make ("timeoverlay", "timeoverlay"); 
    	if( !  app-> timeoverlay) {
            g_print( "Error creating sink, exiting...");           
    	}

	app-> tee = gst_element_factory_make ("tee", "tee"); 
    	if( !  app-> tee) {
            g_print( "Error creating sink, exiting...");           
    	}

    g_print ("Elements are created\n");




    //Config elements

    //impostazione delle proprietÃ  dei vari elementi:
    g_object_set (G_OBJECT (app->sink), "host" , "127.0.0.1" ,  NULL); //
    g_object_set (G_OBJECT (app->sink), "port" , 5000 ,  NULL);
    g_object_set (G_OBJECT (app->sink), "sync" , FALSE ,  NULL); 
    
      
    g_object_set (G_OBJECT (app->videoenc), "bitrate", 256,  NULL);   
    g_object_set (G_OBJECT (app->videoenc), "noise-reduction", 10000,  NULL);
    gst_util_set_object_arg (G_OBJECT (app->videoenc), "tune", "zerolatency");
    gst_util_set_object_arg (G_OBJECT (app->videoenc), "pass" , "qual");
    g_object_set (G_OBJECT (app->rtppay), "config-interval", 1, NULL);

	
	
    
 
    //proprietÃ  da associare al filtro della pipeline(bisogna convertire in yuv):
    //caps3 -> capsfilter
    caps3 = gst_caps_new_simple("video/x-raw",
        "format", G_TYPE_STRING, "I420",
        "width", G_TYPE_INT, 640,
        "height", G_TYPE_INT, 480,
        //"width", G_TYPE_INT, 225,
        //"height", G_TYPE_INT, 225,
        "framerate", GST_TYPE_FRACTION, 15, 1,
        "pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1,
    NULL);
    g_object_set (G_OBJECT (app->filter), "caps", caps3, NULL);

    //caps2 -> appsrc
    caps2 = gst_caps_new_simple("video/x-raw",
        "format", G_TYPE_STRING, "RGB",
        "width", G_TYPE_INT, 640,
        "height", G_TYPE_INT, 480,
        //"width", G_TYPE_INT, 800,
        //"height", G_TYPE_INT, 600,
        "framerate", GST_TYPE_FRACTION, 15, 1,
        "pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1,
    NULL);    
    gst_app_src_set_caps(GST_APP_SRC( app->videosrc), caps2);
//	g_object_set (G_OBJECT (app->videosrc), "is-live" , TRUE ,  NULL);
//  g_object_set (G_OBJECT (app->videosrc), "min-latency" , 200000000 ,  NULL);
    g_object_set (G_OBJECT (  app->videosrc),"stream-type", 0,"format", GST_FORMAT_TIME, NULL);

	//gestisco il flusso di immisione dei buffer nell'elemento sorgente (appsrc):
    g_signal_connect (app->videosrc, "need-data", G_CALLBACK (start_feed), app);
    g_signal_connect (app->videosrc, "enough-data", G_CALLBACK (stop_feed),app);

    g_print ("end of settings\n");



    
    //associo il bus alla pipeline:
    //bus = gst_pipeline_get_bus (GST_PIPELINE ( pipeline));
    //g_assert(bus);
    //gst_bus_add_watch ( bus, (GstBusFunc) bus_call, app);  
    
    //metto i vari elementi nella pipeline
    // appsrc: opencv frames
    //gst_bin_add_many (GST_BIN ( pipeline), app-> videosrc, app->ffmpeg, app-> filter, app->videoenc,  app->rtppay, app->gdppay, app-> sink, NULL);
    gst_bin_add_many (GST_BIN ( pipeline), app-> videosrc, app->ffmpeg, app-> filter, app->tee, app->queue1,  app->videoenc,  app->rtppay, app-> sink, app->queue2, app->videosink, NULL);
	//gst_bin_add_many (GST_BIN ( pipeline), app-> videosrc, app->ffmpeg, app-> filter, app-> videosink, NULL);
	//gst_bin_add_many (GST_BIN ( pipeline), app-> videosrc, app->ffmpeg, app-> filter, app-> videoenc, app->rtppay, app->sink, NULL);
    g_print ("Added all the Elements into the pipeline\n");
    
    //collego i vari elementi fra loro:
    //int ok = false;
    //ok = gst_element_link_many ( app->videosrc, app->ffmpeg, app->filter, app->videoenc,  app->rtppay, app->sink, NULL);
	//ok = gst_element_link_many ( app-> videosrc, app->ffmpeg, app-> filter, app-> videosink, NULL);

    //if(ok)g_print ("Linked all the Elements together\n");
   //else g_print("*** Linking error ***\n");


	if(gst_element_link_many(app->videosrc, app->ffmpeg, app->filter, app->tee, NULL) != TRUE ||
	   gst_element_link_many(app->queue1, app->videoenc, app->rtppay, app->sink, NULL) != TRUE ||
	   gst_element_link_many(app->queue2, app->videosink, NULL) != TRUE ) {
		g_printerr("Elements could not be linked.\n");
		gst_object_unref(pipeline);
		pthread_exit(NULL);
	}


    //g_assert(app->videosrc);
    //g_assert(GST_IS_APP_SRC(app->videosrc));

	tee_udp = gst_element_get_request_pad (app->tee, "src_%u");
	g_print("Obtained request pad %s for branch 1. \n",gst_pad_get_name(tee_udp));
	queue_udp = gst_element_get_static_pad(app->queue1, "sink");

	tee_app = gst_element_get_request_pad (app->tee, "src_%u");
	g_print("Obtained request pad %s for branch 2. \n",gst_pad_get_name(tee_app));
	queue_app = gst_element_get_static_pad(app->queue2, "sink");

	if(gst_pad_link (tee_udp, queue_udp) != GST_PAD_LINK_OK || gst_pad_link (tee_app, queue_app) != GST_PAD_LINK_OK){
		g_printerr("Tee could not be linked\n");
		gst_object_unref(pipeline);
		pthread_exit(NULL);
	}

	gst_object_unref(queue_udp);
	gst_object_unref(queue_app);
   
   
    //metto lo stato della pipeline a playing:
    g_print ("Playing the video\n");
    gst_element_set_state (pipeline, GST_STATE_PLAYING);
   
    g_print ("Running...\n");
    g_main_loop_run ( loop); 
	
	gst_element_release_request_pad(app->tee, tee_udp);
	gst_element_release_request_pad(app->tee, tee_app);
	gst_object_unref(tee_udp);
	gst_object_unref(tee_app);
  
    g_print ("Returned, stopping playback\n");
    gst_element_set_state (pipeline, GST_STATE_NULL);
    //gst_object_unref ( bus);
    //g_main_loop_unref (loop); 
    g_print ("Deleting pipeline\n");
    gst_object_unref (pipeline); 
    pthread_exit(NULL);
}


void *thread1(void *arg){
    VideoCapture cap(0);
    Mat tempframe, result;
    //FireDetection FD;

    if (!cap.isOpened()) {
        throw "Error when reading steam_avi";
    }

    while (1) {
   
        cap.read(tempframe);
	//tempframe = imread("2.jpg");
	//if(! tempframe.data ){ //nel caso in cui non la trova
      	//	cout <<  "Could not open or find image" << std::endl ;
        //}
	//FD.fireDetection(tempframe,result);   

        pthread_mutex_lock( &m ); 

                frameimage = tempframe;
		//frameimage = result;

                cv::cvtColor(frameimage, frameimage,CV_BGR2RGB);
 
                
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
  if( (rc2 = pthread_create(&StreamThread, NULL, thread2, NULL)) )
	cout << "Thread creation failed: " << rc2 << endl;
  
  pthread_join( CaptureImageThread, NULL );
  pthread_join( StreamThread, NULL );
  return 0;
};
