#ifndef RknnRun_H
#define RknnRun_H

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#if(USE_ARM_LIB==1)
    #include "RgaUtils.h"
    #include "im2d.h"
    #include "rga.h"
    #include "rknn_api.h"
#endif


#include <opencv2/opencv.hpp>

#include <condition_variable>
#include <thread>

#include <unistd.h>


#include <ros/ros.h>
#include <cv_bridge/cv_bridge.h>
#include <sensor_msgs/CameraInfo.h>
#include <opencv2/opencv.hpp>
#include <camera_info_manager/camera_info_manager.h>


#include "std_msgs/Header.h"
#include "std_msgs/String.h"
#include "Queue.h"

typedef struct
{
#if(USE_ARM_LIB==1)
    rknn_output outputs[5];//io_num.n_output 注意：这里写小了会崩
#endif
    cv::Mat orig_img;//RGB
    cv::Size mod_size;
    cv::Size img_size;
    std_msgs::Header header;//用于发布和接收一样的时间
}InferData;



class RknnRun
{
public:
    RknnRun();

    ros::NodeHandle nh;

    std::string model_file,yaml_file;
    std::string sub_image_topic,pub_image_topic,pub_det_topic;
    std::string offline_images_path,offline_output_path;
    bool is_offline_image_mode,print_perf_detail,use_multi_npu_core,output_want_float;
    int cls_num;
    double conf_threshold,nms_threshold;
    std::vector<std::string> label_names;

    ros::Subscriber image_sub;
    ros::Publisher image_pub;
    ros::Publisher det_pub;


#if(USE_ARM_LIB==1)
    rknn_context   ctx;
    rknn_input_output_num io_num;
#endif

    std::vector<float>    out_scales;
    std::vector<int32_t>  out_zps;


    void sub_image_callback(const sensor_msgs::ImageConstPtr& msg);

    Queue<sensor_msgs::ImageConstPtr> capture_data_queue;

    int run_infer_thread();

    int run_process_thread();

    void run_check_thread();

    Queue<InferData> process_data_queue;
};

#endif // RknnRun_H
