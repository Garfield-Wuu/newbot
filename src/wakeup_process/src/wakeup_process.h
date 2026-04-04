#include <iostream>
#include <string>
#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <math.h>
#include <unistd.h>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <sys/time.h>
#include <fstream>
#include <sstream>
#include <mutex>
#include <thread>
#include <std_msgs/String.h>
#include <std_msgs/Int32.h>
#include <std_msgs/Bool.h>
#include <std_msgs/Float32MultiArray.h>
#include <std_msgs/Float64MultiArray.h>

#include "asr.h"

using namespace std;


class AsrProcess
{
public:
    AsrProcess();
    ~AsrProcess();

    void asr_id_callback(const std_msgs::Int32::ConstPtr& msg);
    void parse_asr_pub_tts(int asr_id);
    void enable_wakeup_callback(const std_msgs::Bool::ConstPtr& msg);
    int process_asr_cmd(int asr_id,float &turn_angle,float &distance);

private:
    ros::NodeHandle nh;

    ros::Subscriber asr_id_sub;
    ros::Subscriber enable_wakeup_sub;
    
    ros::Publisher tts_pub;
    ros::Publisher action_cmd_pub;
    ros::Publisher enable_tracking_pub;
    ros::Publisher enable_lidar_pub;

    unsigned char enable_wakeup = 1;//默认开启唤醒

    vector<AsrCmd> asr_cmds;

    ros::ServiceClient stop_scan_client;
    ros::ServiceClient start_scan_client;
};