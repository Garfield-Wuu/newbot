#include <ros/ros.h>
#include <tf/transform_broadcaster.h>
#include <sensor_msgs/Image.h>
#include <sensor_msgs/CompressedImage.h>
#include <sensor_msgs/CameraInfo.h>
#include <opencv2/opencv.hpp>

using namespace std;
using namespace cv;

#define CAM_NUM 1

class ImgPub
{
public:
    ImgPub();
    
private:
    //string target_frame;

    string camera_name[CAM_NUM];

    tf::Transform transform[CAM_NUM];

    ros::NodeHandle nh;
    
    //tf::TransformBroadcaster broadcaster[CAM_NUM];

    ros::Subscriber image_sub;

    sensor_msgs::CameraInfo camera_info_msg[CAM_NUM];

    ros::Publisher camera_info_pub[CAM_NUM];

    ros::Publisher undistort_pub;

    void sub_image_callback(const sensor_msgs::Image &msg);
    //void sub_image_callback(const sensor_msgs::CompressedImage &image_msg);

    bool pub_camerainfo,pub_image_rect;

    cv::Mat map1,map2;
};
