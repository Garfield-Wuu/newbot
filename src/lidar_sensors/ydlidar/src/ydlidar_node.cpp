//
// The MIT License (MIT)
//
// Copyright (c) 2020 EAIBOT. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#include <ros/ros.h>
#include "sensor_msgs/LaserScan.h"
#include "std_srvs/Empty.h"
#include "src/CYdLidar.h"
#include "ydlidar_config.h"
#include <limits>       // std::numeric_limits

#define SDKROSVerision "1.0.0"

CYdLidar laser;

bool stop_scan(std_srvs::Empty::Request &req,
                               std_srvs::Empty::Response &res)
{
  ROS_INFO("Stop scan");
  return laser.turnOff();
}

bool start_scan(std_srvs::Empty::Request &req,
                               std_srvs::Empty::Response &res)
{
  ROS_INFO("Start scan");
  return laser.turnOn();
}


int main(int argc, char **argv) {
  ros::init(argc, argv, "ydlidar_node");
  ROS_INFO("YDLIDAR ROS Driver Version: %s", SDKROSVerision);
  ros::NodeHandle nh;
  ros::Publisher scan_pub = nh.advertise<sensor_msgs::LaserScan>("scan", 1);//发布队列长度1000

  ros::NodeHandle nh_private("~");
  std::string str_optvalue = "/dev/ydlidar";
  nh_private.param<std::string>("port", str_optvalue, "/dev/ydlidar");
  ///lidar port
  laser.setlidaropt(LidarPropSerialPort, str_optvalue.c_str(), str_optvalue.size());

  ///ignore array
  nh_private.param<std::string>("ignore_array", str_optvalue, "");
  laser.setlidaropt(LidarPropIgnoreArray, str_optvalue.c_str(), str_optvalue.size());

  std::string frame_id;
  nh_private.param<std::string>("frame_id", frame_id, "laser_my_frame");

  printf("frame_id = %s\n",frame_id.c_str());

  //////////////////////int property/////////////////
  /// lidar baudrate
  int optval = 115200;
  nh_private.param<int>("baudrate", optval, 115200);
  laser.setlidaropt(LidarPropSerialBaudrate, &optval, sizeof(int));
  /// tof lidar
  optval = TYPE_TRIANGLE;
  nh_private.param<int>("lidar_type", optval, TYPE_TRIANGLE);
  laser.setlidaropt(LidarPropLidarType, &optval, sizeof(int));
  /// device type
  optval = YDLIDAR_TYPE_SERIAL;
  nh_private.param<int>("device_type", optval, YDLIDAR_TYPE_SERIAL);
  laser.setlidaropt(LidarPropDeviceType, &optval, sizeof(int));
  /// sample rate
  optval = 9;
  nh_private.param<int>("sample_rate", optval, 9);
  laser.setlidaropt(LidarPropSampleRate, &optval, sizeof(int));
  /// abnormal count
  optval = 4;
  nh_private.param<int>("abnormal_check_count", optval, 4);
  laser.setlidaropt(LidarPropAbnormalCheckCount, &optval, sizeof(int));


  //////////////////////bool property/////////////////
  /// fixed angle resolution
  bool b_optvalue = false;
  nh_private.param<bool>("fixed_resolution", b_optvalue, true);
  laser.setlidaropt(LidarPropFixedResolution, &b_optvalue, sizeof(bool));
  /// rotate 180
  nh_private.param<bool>("reversion", b_optvalue, true);
  laser.setlidaropt(LidarPropReversion, &b_optvalue, sizeof(bool));
  /// Counterclockwise
  nh_private.param<bool>("inverted", b_optvalue, true);
  laser.setlidaropt(LidarPropInverted, &b_optvalue, sizeof(bool));
  b_optvalue = true;
  nh_private.param<bool>("auto_reconnect", b_optvalue, true);
  laser.setlidaropt(LidarPropAutoReconnect, &b_optvalue, sizeof(bool));
  /// one-way communication
  b_optvalue = false;
  nh_private.param<bool>("isSingleChannel", b_optvalue, false);
  laser.setlidaropt(LidarPropSingleChannel, &b_optvalue, sizeof(bool));
  /// intensity
  b_optvalue = false;
  nh_private.param<bool>("intensity", b_optvalue, false);
  laser.setlidaropt(LidarPropIntenstiy, &b_optvalue, sizeof(bool));
  /// Motor DTR
  b_optvalue = false;
  nh_private.param<bool>("support_motor_dtr", b_optvalue, false);
  laser.setlidaropt(LidarPropSupportMotorDtrCtrl, &b_optvalue, sizeof(bool));

  //////////////////////float property/////////////////
  /// unit: °
  float f_optvalue = 180.0f;
  nh_private.param<float>("angle_max", f_optvalue, 180.f);
  laser.setlidaropt(LidarPropMaxAngle, &f_optvalue, sizeof(float));
  f_optvalue = -180.0f;
  nh_private.param<float>("angle_min", f_optvalue, -180.f);
  laser.setlidaropt(LidarPropMinAngle, &f_optvalue, sizeof(float));
  /// unit: m
  f_optvalue = 16.f;
  nh_private.param<float>("range_max", f_optvalue, 16.f);
  laser.setlidaropt(LidarPropMaxRange, &f_optvalue, sizeof(float));
  f_optvalue = 0.1f;
  nh_private.param<float>("range_min", f_optvalue, 0.1f);
  laser.setlidaropt(LidarPropMinRange, &f_optvalue, sizeof(float));
  /// unit: Hz
  f_optvalue = 10.f;
  nh_private.param<float>("frequency", f_optvalue, 10.f);
  laser.setlidaropt(LidarPropScanFrequency, &f_optvalue, sizeof(float));

  bool invalid_range_is_inf = false;
  nh_private.param<bool>("invalid_range_is_inf", invalid_range_is_inf, invalid_range_is_inf);

  ros::ServiceServer stop_scan_service = nh.advertiseService("stop_scan", stop_scan);
  ros::ServiceServer start_scan_service = nh.advertiseService("start_scan", start_scan);

  ros::Rate r(30);

  // initialize SDK and LiDAR
  bool ret = laser.initialize();
  if (ret) {//success
    //Start the device scanning routine which runs on a separate thread and enable motor.

    ret = false;
    while(ret==false && ros::ok()) //ret如果等于false,则反复尝试打开雷达
    {
      ret = laser.turnOn();
      //ROS_INFO("Lidar trun on failed"); //如果雷达不接电源就会一直在这个循环里循环
      ros::spinOnce();//处理ctrl c中断
      r.sleep();
    }

  } else {
    ROS_WARN("initialize error: %s\n", laser.DescribeError());

    //等待ctrl c按下退出
    while(ros::ok())
    {
      ros::spinOnce();//处理ctrl c中断
      r.sleep();
    }
  }

  
  while (ret && ros::ok()) {
    LaserScan scan;
    if (laser.doProcessSimple(scan)) {
      sensor_msgs::LaserScan scan_msg;

      ros::Time start_scan_time;
      start_scan_time.sec = scan.stamp/1000000000ul;
      start_scan_time.nsec = scan.stamp%1000000000ul;
      scan_msg.header.stamp = start_scan_time;
      scan_msg.header.frame_id = frame_id;

      scan_msg.angle_min =(scan.config.min_angle);
      scan_msg.angle_max = (scan.config.max_angle);
      scan_msg.angle_increment = (scan.config.angle_increment);
      scan_msg.scan_time = scan.config.scan_time;
      scan_msg.time_increment = scan.config.time_increment;
      scan_msg.range_min = (scan.config.min_range);
      scan_msg.range_max = (scan.config.max_range);


      int size = (scan.config.max_angle - scan.config.min_angle)/ scan.config.angle_increment + 1;
      scan_msg.ranges.resize(size, invalid_range_is_inf ? std::numeric_limits<float>::infinity() : 0.0);
      scan_msg.intensities.resize(size);
      for(size_t i=0; i < scan.points.size(); i++) {
        int index = std::ceil((scan.points[i].angle - scan.config.min_angle)/scan.config.angle_increment);
        if(index >=0 && index < size) {
          if(scan.points[i].range >= scan.config.min_range) {
            scan_msg.ranges[index] = scan.points[i].range;
            scan_msg.intensities[index] = scan.points[i].intensity;
          }
        }

      }
      scan_pub.publish(scan_msg);


    } else {
      //ROS_INFO("Failed to get Lidar Data");//ERROR->INFO  edit
      ;
    }

    ros::spinOnce();//处理ctrl c中断
    r.sleep();
  }

  laser.turnOff();
  ROS_INFO("[YDLIDAR INFO] Now YDLIDAR is stopping .......");
  laser.disconnecting();
  return 0;
}
