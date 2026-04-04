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

#include <ros/ros.h>
#include <ros/spinner.h>

#include <sensor_msgs/Imu.h>
#include <sensor_msgs/MagneticField.h>
#include <sensor_msgs/JointState.h>
#include <sensor_msgs/LaserScan.h>

#include <nav_msgs/Odometry.h>
#include <geometry_msgs/Twist.h>
#include <tf/tf.h>
#include <tf/transform_broadcaster.h>
#include <std_msgs/String.h>
#include <std_msgs/Int32.h>
#include <std_msgs/Bool.h>


#include "uart.h"

using namespace std;

#define PKG_LEN_MIN 12
#define PKG_LEN_MAX 60
#define POINT_NUM_MAX_IN_A_PKG 25 //一包最多25个点

#pragma pack(1)

typedef struct
{
  unsigned char head1;//数据头1 0xAA
  unsigned char head2;//数据头2 0x55
  unsigned char points_type;//点云类型1--每圈起始点;0--非一圈起始点
  unsigned char points_num;//点云个数 结构体总长度=10+points_num*2
  unsigned short start_angle;//起始角度
  unsigned short end_angle;//结束角度
  unsigned short check_code;//校验
  unsigned short data[POINT_NUM_MAX_IN_A_PKG];
}LidarData;

#pragma pack()


class Lidar
{
public:
  Lidar();
  ~Lidar();

  void run();
  void enable_lidar_callback(const std_msgs::Bool::ConstPtr& msg);

private:
  ros::NodeHandle nh;

  Uart uart;
  string recv_str;

  LidarData lidar_data;

  ros::Subscriber enable_lidar_sub;
  ros::Publisher laser_pub;
  sensor_msgs::LaserScan laser_scan;

};