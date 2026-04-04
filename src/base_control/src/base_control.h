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
//#include <sensor_msgs/BatteryState.h>
#include "BatteryState.h"
#include <sensor_msgs/Imu.h>
#include <sensor_msgs/MagneticField.h>
#include <sensor_msgs/JointState.h>

#include <nav_msgs/Odometry.h>
#include <geometry_msgs/Twist.h>
#include <tf/tf.h>
#include <tf/transform_broadcaster.h>
#include <std_msgs/String.h>
#include <std_msgs/Int32.h>
#include <std_msgs/Bool.h>

#include <dynamic_reconfigure/server.h>
#include <base_control/BaseConfig.h>

#include <std_msgs/Float32MultiArray.h>
#include <std_msgs/Float64MultiArray.h>

#include <std_msgs/Int16MultiArray.h>

#include "uart.h"
#include "pid.h"


using namespace std;

enum {
  POWER_SUPPLY_STATUS_UNKNOWN = 0u,
  POWER_SUPPLY_STATUS_CHARGING = 1u,
  POWER_SUPPLY_STATUS_DISCHARGING = 2u,
  POWER_SUPPLY_STATUS_NOT_CHARGING = 3u,
  POWER_SUPPLY_STATUS_FULL = 4u,
  POWER_SUPPLY_HEALTH_UNKNOWN = 0u,
  POWER_SUPPLY_HEALTH_GOOD = 1u,
  POWER_SUPPLY_HEALTH_OVERHEAT = 2u,
  POWER_SUPPLY_HEALTH_DEAD = 3u,
  POWER_SUPPLY_HEALTH_OVERVOLTAGE = 4u,
  POWER_SUPPLY_HEALTH_UNSPEC_FAILURE = 5u,
  POWER_SUPPLY_HEALTH_COLD = 6u,
  POWER_SUPPLY_HEALTH_WATCHDOG_TIMER_EXPIRE = 7u,
  POWER_SUPPLY_HEALTH_SAFETY_TIMER_EXPIRE = 8u,
  POWER_SUPPLY_TECHNOLOGY_UNKNOWN = 0u,
  POWER_SUPPLY_TECHNOLOGY_NIMH = 1u,
  POWER_SUPPLY_TECHNOLOGY_LION = 2u,
  POWER_SUPPLY_TECHNOLOGY_LIPO = 3u,
  POWER_SUPPLY_TECHNOLOGY_LIFE = 4u,
  POWER_SUPPLY_TECHNOLOGY_NICD = 5u,
  POWER_SUPPLY_TECHNOLOGY_LIMN = 6u,
};


#pragma pack(1)

typedef struct
{
unsigned char head1;//数据头1 'D'
unsigned char head2;//数据头2 'A'
unsigned char struct_size;//结构体长度

short encoder1;//编码器当前值1
short encoder2;//编码器当前值2

short vbat_mv;//电池电压 mV
unsigned char charger_connected;//是否连接充电器
unsigned char fully_charged;//是否充满电

unsigned char asr_id;//语音命令ID

unsigned char end1;//数据尾1 'T'
unsigned char end2;//数据尾2 'A'
unsigned char end3;//数据尾3 '\r' 0x0d
unsigned char end4;//数据尾4 '\n' 0x0a
}McuData;


typedef struct
{
unsigned char head1;//数据头1 'D'
unsigned char head2;//数据头2 'A'
unsigned char struct_size;//结构体长度

short pwm1;//油门PWM1
short pwm2;//油门PWM2
unsigned char enable_power;//开启5V雷达电源

unsigned char end1;//数据尾1 'T'
unsigned char end2;//数据尾2 'A'
unsigned char end3;//数据尾3 '\r' 0x0d
unsigned char end4;//数据尾4 '\n' 0x0a
}CmdData;

#pragma pack()




typedef struct
{
    int wheel_circumference_mm;
    int pulses_per_wheel_turn_around;
    int pulses_per_robot_turn_around;

    int max_pwm;

    int pid_p;
    int pid_i;
    int pid_d;
}SettingData;


class BaseControl
{
public:
    BaseControl();
    ~BaseControl();
    void run();

private:
    void dynamic_reconfigure_callback(base_control::BaseConfig &config, uint32_t level);
    void cmd_vel_callback(const geometry_msgs::Twist::ConstPtr& msg);
    void enable_lidar_callback(const std_msgs::Bool::ConstPtr& msg);
    void reset_callback(const std_msgs::Bool::ConstPtr& msg);
    void pub_battery_msg();
    void pub_joint_msg(double l_angle,double r_angle);
    void control_robot(int target1,int target2);
    void pub_tf_and_odom(ros::Time ros_time_now,double delta_m_s,double delta_rad_s);
    void pub_plot(vector<float> array);


    CmdData cmd_data = {'D','A',sizeof(CmdData),0,0,0,'T','A','\r','\n'};

    double target_m_s=0;//目标线速度m/s
    double target_rad_s=0;//目标角速度rad/s

    ros::NodeHandle nh;

    string odom_frame_id,base_frame_id;
    sensor_msgs::BatteryState battery_msg;

    ros::Subscriber command_sub;
    ros::Subscriber enable_lidar_sub,reset_sub;
    ros::Publisher odom_pub, battery_pub, tts_pub;
    ros::Publisher joint_pub;
    ros::Publisher plot_pub;
    ros::Publisher asr_id_pub;
    ros::Publisher robot_state_pub;

    Uart uart;


    McuData mcu_data;
    unsigned int cnt=0;

    SettingData setting_data;

    double current_time=0,previous_time=0;
    double dt=0,pose_yaw=0;
    double pose_x=0,pose_y=0;

    tf::TransformBroadcaster odom_broadcaster;
    nav_msgs::Odometry odom_msg;
    geometry_msgs::TransformStamped odom_trans;
    geometry_msgs::Quaternion odom_quat;


    string recv_str;
    
    short last_charge_state=0;
    int low_power_cnt=0;
    
    long long all_encoder1=0;
    long long all_encoder2=0;
    
    dynamic_reconfigure::Server<base_control::BaseConfig> server;
    dynamic_reconfigure::Server<base_control::BaseConfig>::CallbackType dynamic_config;

    PidController left_pid;
    PidController right_pid;

    int last_target1=0;
    int last_target2=0;

    double l_angle=0;
    double r_angle=0;

    double pluses_m = 0;
    double wheel_distance_m = 0;

    unsigned char enable_lidar = 0;//默认关闭雷达电源

    bool ignore_serial_asr_id_ = false;
};


