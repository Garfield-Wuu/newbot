#include "m1c1_mini.h"

Lidar::~Lidar()
{

}

Lidar::Lidar() : nh("~")
{
  string dev;
  int buad;

  nh.param<string>("dev", dev, "/dev/ttyUSB0");
  nh.param<int>("buad", buad, 115200);

  // 初始化发布器
  laser_pub = nh.advertise<sensor_msgs::LaserScan>("/scan", 10);
  enable_lidar_sub = nh.subscribe("/enable_lidar", 10, &Lidar::enable_lidar_callback, this);//打开或关闭雷达订阅

  //串口初始化
  int ret = uart.init(dev,buad);
  if(ret<0)
      return;

}

void Lidar::enable_lidar_callback(const std_msgs::Bool::ConstPtr& msg)
{
  if (msg->data) 
  {
    ROS_INFO("m1c1 enable_lidar true");
    //发送开启命令A5 F0，雷达开始转动
    unsigned char start_cmd[2] = {0xA5,0xF0};
    uart.send_data(start_cmd,2);
  }
  else
  {
    ROS_INFO("m1c1 enable_lidar false");
    //发送停止命令A5 F5，雷达停止转动
    unsigned char stop_cmd[2] = {0xA5,0xF5};
    uart.send_data(stop_cmd,2);
  }
}


void Lidar::run()
{
  int ret;
  int start_cnt=0;

  float laser_min_range = 0.1;  //雷达最小量程
  float laser_max_range = 8.0; //雷达最大量程

  int points_size = 800;//修正和插值到800点(本身390个点左右)
  laser_scan.header.frame_id = "laser_link";
  laser_scan.angle_min = 0;
  laser_scan.angle_max = 2*M_PI;
  laser_scan.angle_increment = (laser_scan.angle_max - laser_scan.angle_min) / points_size;
  laser_scan.range_min = laser_min_range;
  laser_scan.range_max = laser_max_range;
  laser_scan.ranges.resize(points_size);
  std::fill(laser_scan.ranges.begin(), laser_scan.ranges.end(), std::numeric_limits<float>::infinity());//全部填充为无限远
  laser_scan.intensities.resize(points_size, 127); // 假设强度值为127

  while(ros::ok())
  {
    ros::spinOnce();//处理回调函数

    ret = uart.read_lidar_data(recv_str);
    if(ret < 0)//报错，则重新读取
    {
      if(ret!=-1)//雷达断电后会返回-1，雷达断电不用打印报错
        ROS_WARN("read_lidar_data ret=%d",ret);
      continue;
    }
    
    // printf("recv_str(%d): ",recv_str.size());
    // for(int i=0;i<recv_str.size();i++)
    //   printf("%02x ",(unsigned char)recv_str.data()[i]);
    // printf("\n");

    if(recv_str.size()<PKG_LEN_MIN || recv_str.size()>PKG_LEN_MAX)
    {
      ROS_WARN("recv_str size %d error!",recv_str.size());
      continue;
    }

    //拷贝进数据包结构体
    memcpy(&lidar_data,recv_str.data(),recv_str.size());

    if(lidar_data.points_type==1)//每圈起始点，发布上一圈的数据
    {
      if(start_cnt<10)//前10圈数据丢弃
      {
        start_cnt++;
        continue;
      }
    
      laser_scan.header.stamp = ros::Time::now();
      // 发布LaserScan消息
      laser_pub.publish(laser_scan);
      ROS_INFO_ONCE("Published LaserScan OK");

      //必须给每个点刷入无效数据，防止下一圈产生重影
      std::fill(laser_scan.ranges.begin(), laser_scan.ranges.end(), std::numeric_limits<float>::infinity());//全部填充为无限远
    }


    //开始解析数据包结构体中的数据
    float start_angle0 = (lidar_data.start_angle>>1)/64.0;
    float end_angle0 = (lidar_data.end_angle>>1)/64.0;
    // float start_angle = start_angle0-ang_corr_1;
    // float end_angle = end_angle0-ang_corr_n;

    float ang_corr_i,distance_mm_i,angle_i;
    for(int i=0;i<lidar_data.points_num;i++)
    {
      //解析距离：单位mm
      distance_mm_i = lidar_data.data[i]>>2;
      
      //根据距离来求修正角度
      ang_corr_i = (distance_mm_i==0) ? 0 : atan(19.16*(distance_mm_i-90.15)/(90.15*distance_mm_i)) * 180.0 / M_PI;

      if(lidar_data.points_num!=1)//分母不能为0
        angle_i = start_angle0 + (end_angle0-start_angle0)/(lidar_data.points_num-1)*i - ang_corr_i;
      else
        angle_i = start_angle0 - ang_corr_i;

      angle_i = 180.0 - (angle_i + 12.0);//12度是雷达手册中写的固有偏差，180是和雷达安装朝向有关

      //角度限制在[0,360]度之间
      angle_i = fmod(angle_i, 360.0);  // 使用fmod函数计算角度对360的余数  
      angle_i = (angle_i < 0)   ? (angle_i+360.0) : angle_i; // 如果结果是负数，则加上360使其变为正数 

      //printf("%.2f(%.2f)\n",distance_mm_i,angle_i);

      //考虑到角度修正之后填充，并且插值到800点
      int new_i = angle_i / 360.0 * (points_size-1) + 0.5;//索引最大不能到points_size，而是points_size-1
      laser_scan.ranges[new_i] = distance_mm_i/1000.0;//填充到LaserScan消息中

    }
  }
}


int main(int argc,char** argv)
{
  setlocale(LC_CTYPE, "zh_CN.utf8");
  ros::init(argc, argv, "m1c1_mini");

  Lidar lidar;
  lidar.run();

  ROS_INFO("m1c1_mini exit");
  ros::shutdown();
  return 0;
}
