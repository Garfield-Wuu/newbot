#include <ros/ros.h>

#include "postprocess.h"
#include "rknn_run.h"

int main(int argc, char** argv)
{
    ros::init(argc, argv, "rknn_yolov6");

    RknnRun rknn_run;
    std::thread infer_thread(&RknnRun::run_infer_thread, &rknn_run);
    std::thread process_thread(&RknnRun::run_process_thread, &rknn_run);
    std::thread check_thread(&RknnRun::run_check_thread, &rknn_run);
    
    ros::spin();

    return 0;
}



