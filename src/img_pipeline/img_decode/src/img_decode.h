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

    /** 已弃用：由 use_rga_fd_experimental 替代，保留仅用于兼容旧 launch */
    bool use_rga;

    /** 阶段2实验：先 copyTo(heap)，再尝试 VA-RGA；失败降级 OpenCV */
    bool use_rga_va_fallback;

    /** 阶段3实验：使用 get_dmabuf_fd + rga_resize_fd；失败冷却重试，最终降级 */
    bool use_rga_fd_experimental;

    void run_check_thread();

#if(USE_ARM_LIB==1)
    MppDecode mpp_decode;
#endif

};