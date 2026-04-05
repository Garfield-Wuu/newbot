#include <ros/ros.h>
#include <cv_bridge/cv_bridge.h>
#include <sensor_msgs/CameraInfo.h>
#include <opencv2/opencv.hpp>
#include <camera_info_manager/camera_info_manager.h>

#include <iostream>
#include <sys/time.h>
#include <math.h>

#if(USE_ARM_LIB==1)
    #include "mpp_decode.h"
    #include "rga_resize.h"
#endif

using namespace std;
using namespace cv;

class ImgDecode
{
public:
    ImgDecode();


    ros::NodeHandle nh;

    ros::Subscriber compressed_image_sub;
    ros::Publisher raw_image_pub;
    sensor_msgs::Image msg_pub;
    cv::Mat image;

    int fps_div;
    double scale;
    unsigned int frame_cnt=0;

    void compressed_image_callback(const sensor_msgs::CompressedImageConstPtr& msg);

    string sub_jpeg_image_topic;

    /** false：启动即订阅 JPEG（推荐，避免无人订 /camera/image_raw 时断流）；true：仅在有订阅者时再订阅压缩图 */
    bool lazy_compressed_subscribe;

    /** 是否尝试 RGA 缩放；若为 false 则始终 OpenCV（省 ioctl / 内核日志） */
    bool use_rga;

    void run_check_thread();

#if(USE_ARM_LIB==1)
    MppDecode mpp_decode;
#endif

};