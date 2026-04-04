#include <ros/ros.h>
#include <ros/package.h>
#include <cv_bridge/cv_bridge.h>
#include <sensor_msgs/CameraInfo.h>
#include <opencv2/opencv.hpp>
#include <camera_info_manager/camera_info_manager.h>
#include <image_transport/image_transport.h>

#include <iostream>
#include <sys/time.h>
#include <math.h>

#if(USE_ARM_LIB==1)
    #include "mpp_encode.h"
    #include "rga_cvtcolor.h"
#endif

using namespace std;
using namespace cv;

class ImgEncode
{
public:
    ImgEncode();


    void image_callback(const sensor_msgs::ImageConstPtr& msg);

    ros::NodeHandle nh;
    ros::Subscriber image_sub;
    ros::Publisher jpeg_pub;
    sensor_msgs::CompressedImage msg_pub;
    int jpeg_quality;

    string sub_image_topic;
    
    void run_check_thread();

#if(USE_ARM_LIB==1)
    MppEncode mpp_encode;
#endif
};