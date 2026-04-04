#include "wakeup_process.h"

//enable_wakeup订阅回调函数
void AsrProcess::enable_wakeup_callback(const std_msgs::Bool::ConstPtr& msg)
{
  // 在回调函数中处理接收到的消息
  if (msg->data) 
  {
    enable_wakeup = 1;
    ROS_INFO("enable_wakeup true");
  }
  else
  {
    enable_wakeup = 0;
    ROS_INFO("enable_wakeup false");
  }
}

AsrProcess::~AsrProcess()
{

}

AsrProcess::AsrProcess() : nh("~")
{
  string asr_cfg;
  string sub_asr_id_topic;

  nh.param<string>("asr_cfg", asr_cfg, "asr.cfg");

  ROS_INFO("asr_cfg=%s",asr_cfg.c_str());

  parse_config_file(asr_cfg, asr_cmds);

  asr_id_sub = nh.subscribe("/asr_id", 10, &AsrProcess::asr_id_callback, this);
  enable_wakeup_sub = nh.subscribe("/enable_wakeup", 10, &AsrProcess::enable_wakeup_callback, this);

  tts_pub = nh.advertise<std_msgs::String>("/tts",10);//语音命令字符串发布
  action_cmd_pub = nh.advertise<std_msgs::Float32MultiArray>("/action_cmd", 10);
  enable_tracking_pub = nh.advertise<std_msgs::Bool>("/enable_tracking", 10);
  enable_lidar_pub = nh.advertise<std_msgs::Bool>("/enable_lidar", 10);

  // 创建用于调用服务的客户端
  stop_scan_client = nh.serviceClient<std_srvs::Empty>("/stop_scan");
  start_scan_client = nh.serviceClient<std_srvs::Empty>("/start_scan");
}

int AsrProcess::process_asr_cmd(int asr_id,float &turn_angle,float &distance)
{
    int id_index = hex2dec(asr_id)-1;//asr_id是从1开始记的

    if(id_index >= asr_cmds.size())
        return 0;

    string key = asr_cmds[id_index].key;

    if(key=="high_lidar" || key=="medium_lidar" || key=="low_lidar" || key=="open_lidar") //对于M1C1_MINI来说无法调速，所以都一样
    {
        open_lidar(key,enable_lidar_pub,stop_scan_client,start_scan_client);

        return 0;
    }
    else if(key=="off_lidar")//关闭雷达的时候，连接到STM32的串口还要下发关闭协议
    {
        stop_scan(stop_scan_client);
    
        system("gpio mode 20 out && gpio write 20 0");
        std_msgs::Bool msg;
        msg.data = false;//关闭MOS管
        enable_lidar_pub.publish(msg);
        
        return 0;
    }
    else if(key=="reboot")
    {
        //system("sleep 5 && sync && reboot && echo orangepi | sudo -S watchdog_test 1");//执行重启命令，并且打开看门狗，否则看门狗出错可能会让重启很漫长
        system("sleep 5 && sync && reboot");
        return 0;
    }
    else if(key=="tracking_person")//跟着我|跟我走@开始跟着你
    {
        std_msgs::Bool msg;
        msg.data = true;
        enable_tracking_pub.publish(msg);
        return 0;
    }
    else if(key=="cancel_tracking")
    {
        std_msgs::Bool msg;
        msg.data = false;
        enable_tracking_pub.publish(msg);
        return 0;
    }
    else if(key=="stop")
    {
        std_msgs::Bool msg;
        msg.data = false;
        enable_tracking_pub.publish(msg);//停止命令也可以取消跟踪

        turn_angle = 0;
        distance = 0;
        return 1;//1表示执行电机指令
    }
    // else if(key=="dance")
    // {
    //     turn_angle = 0;
    //     distance = 0;
    //     return 2;//2表示跳舞
    // }


    int value = 0;
    // 查找下划线的位置
    size_t underscorePos = key.find("_");
    if (underscorePos != std::string::npos)
    {
        // 解析指令部分
        string command = key.substr(0, underscorePos);
        
        // 解析数字部分
        std::istringstream iss(key.substr(underscorePos + 1));
        iss >> value;
    }

    if(value==0)
        return 0;

    if (key.find("forward") != string::npos)
    {
        turn_angle = 0;
        distance = value*0.01;//厘米转米
        return 1;
    }
    else if (key.find("backward") != string::npos) 
    {
        turn_angle = 0;
        distance = -value*0.01;//厘米转米
        return 1;
    }
    else if (key.find("left") != string::npos) 
    {
        turn_angle = value;
        distance = 0;
        return 1;
    }
    else if (key.find("right") != string::npos) 
    {
        turn_angle = -value;
        distance = 0;
        return 1;
    }
    else
    {
        return 0;
    }
}

void AsrProcess::parse_asr_pub_tts(int asr_id)
{
    //获取回复语
    string reply_str = get_asr_reply(asr_cmds,asr_id);
    std_msgs::String msg;
    msg.data = reply_str;
    tts_pub.publish(msg);//发布tts字符串

    //处理和获取速度命令
    float turn_angle,distance;
    int new_action_mode = process_asr_cmd(asr_id,turn_angle,distance);//解析并执行asr_cmds
    if(new_action_mode)
    {
        //发布数组数据
        vector<float> array = {new_action_mode,turn_angle,distance};
        std_msgs::Float32MultiArray array_msg;
        array_msg.data = array;
        action_cmd_pub.publish(array_msg);
    }
}


void AsrProcess::asr_id_callback(const std_msgs::Int32::ConstPtr& msg)
{
  ROS_INFO("asr_id_callback=%d",msg->data);
  if(enable_wakeup)//enable_wakeup是打开状态才发布tts字符串
    parse_asr_pub_tts(msg->data);
}

int main(int argc,char** argv)
{
    setlocale(LC_CTYPE, "zh_CN.utf8");
    ros::init(argc, argv, "asr_process");

    ROS_INFO("main");

    AsrProcess asr_process;
    ros::spin();
    ros::shutdown();
    return 0;
}
