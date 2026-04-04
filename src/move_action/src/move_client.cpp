#include <signal.h>
#include <string.h>
#include <cmath>
#include <ros/ros.h>
#include <geometry_msgs/Twist.h>
#include <tf/transform_listener.h>
#include <nav_msgs/Odometry.h>
#include <actionlib/client/simple_action_client.h>
#include <move_base_msgs/MoveBaseAction.h>
#include <actionlib/client/simple_action_client.h>
#include <visualization_msgs/Marker.h>
#include <tf/transform_listener.h>
#include <tf/transform_datatypes.h>
#include <geometry_msgs/PoseStamped.h>
#include <std_msgs/Bool.h>
#include <std_msgs/String.h>
#include "move_action/DoMoveAction.h"

typedef actionlib::SimpleActionClient<move_action::DoMoveAction> Client;


void do_move(Client &client,float angle,float distance)
{
    bool succeeded;

    move_action::DoMoveGoal goal;

    goal.goal_angle = angle*M_PI/180.0;//先旋转这么多角度
    goal.goal_distance = distance;//再走这么多距离
    goal.angular_speed=1.0;//角速度
    goal.linear_speed=0.08;//线速度
    client.sendGoal(goal);
    ROS_INFO("sending goal");

    succeeded= client.waitForResult(ros::Duration(30.0));
    if (succeeded)
    {
        move_action::DoMoveResultConstPtr result = client.getResult();
        ROS_INFO("finished %d %.2f deg %.2f m\n", result->result_flag,result->result_angle*180/M_PI,result->result_distance);
    }
    else
    {
        ROS_INFO("timeout!\n");
    }
}

int main (int argc, char **argv)
{
    setlocale(LC_CTYPE, "zh_CN.utf8");//防止ROS_INFO中文乱码

    ros::init(argc, argv, "move_client");
    ros::NodeHandle n;
    
    if(argc!=1+2)
    {
        ROS_WARN("please input argument: turn_angle distance",argv[0]);
        ros::shutdown();
        return -1;
    }

    float turn_angle = atof(argv[1]);
    float distance = atof(argv[2]);

    // 定义一个客户端
    Client client("do_move", true);
    // 等待服务器端
    ROS_INFO("waiting for move action server to start");
    client.waitForServer();
    ROS_INFO("move action server connect ok");

    do_move(client,turn_angle,distance);

    ros::shutdown();
    return 0;
}
