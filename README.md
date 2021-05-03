# RTSP流媒体服务器

1.	利用opencv videoCapture实现RTSP解码；
2.	实现对解码出的cv::Mat帧进行处理，如区域选择；
3.	实现对解码出的cv::Mat帧进行本地存储；
4.	基于Gstreamer的gst-rtsp-server套件实现对Mat帧进行H.264编码、组流、推流至服务器指指定url（端口、index）；
5.	利用互斥线程锁将解码、处理、编码、组流等步骤封装成管线，每一路管线对应一路RTSP流；
6.	利用多进程管理实现同时处理多路流；
7.	交叉编译opencv和cuda，使用nvidia video codec sdk加速RTSP解码，解放CPU资源、降低延迟；
8.	通过init配置文件启动系统，其中包括rtsp路数以及每一路流各自的参数；

# 系统数据流图如下

![系统数据流图](https://github.com/chicleee/rtsp-server/blob/master/doc/DataFlowDiagram.png)

# 每一路RTSP流的处理管线如下

![管线](https://github.com/chicleee/rtsp-server/blob/master/doc/Pipeline.png)

# 结果

本RTSP流媒体服务器实现了预期的系统功能，但由于cv::Ptr<cv::cudacodec::VideoReader>解码一路4K的RTSP视频流需要占用580M的显存，解码一路1080P的视频流需要约200M的显存，故在11GB显存的1080Ti显卡上，最多只能同时处理18路4K流、或者50路1080P流。

# 源码请见src
# 详细文档请见doc
