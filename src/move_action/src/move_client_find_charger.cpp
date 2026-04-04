#include <actionlib/client/simple_action_client.h>
#include "move_action/DoMoveAction.h"

#include <signal.h>

#include <ros/ros.h>
#include <signal.h>
#include <geometry_msgs/Twist.h>
#include <tf/transform_listener.h>
#include <nav_msgs/Odometry.h>
#include <string.h>

#include <ros/ros.h>
#include <move_base_msgs/MoveBaseAction.h>
#include <actionlib/client/simple_action_client.h>
#include <visualization_msgs/Marker.h>
#include <tf/transform_listener.h>
#include <tf/transform_datatypes.h>
#include <geometry_msgs/PoseStamped.h>
#include <std_msgs/Bool.h>
#include <std_msgs/String.h>
#include <cmath>

typedef actionlib::SimpleActionClient<move_action::DoMoveAction> Client;



void do_move(Client &client,float angle,float distance)
{
    bool succeeded;

    // 创建一个action的goal
    move_action::DoMoveGoal goal;

    goal.goal_angle = angle*M_PI/180.0;//先旋转这么多角度
    goal.goal_distance = distance;//再走这么多距离
    goal.angular_speed=1.0;//旋转速度 只能为正数
    goal.linear_speed=0.08;//直走速度 只能为正数
    client.sendGoal(goal); //, Client::SimpleDoneCallback(), Client::SimpleActiveCallback(), Client::SimplFeedbackCallback());
    ROS_INFO("sending goal.");

    succeeded= client.waitForResult(ros::Duration(10.0));
    if (succeeded)
    {
        move_action::DoMoveResultConstPtr result = client.getResult();
        ROS_INFO("Yay! go finished %d\n", result->result_flag);
    }
    else
    {
        ROS_INFO("timeout!!!\n");
    }
}




typedef actionlib::SimpleActionClient<move_base_msgs::MoveBaseAction>  MoveBaseClient;
move_base_msgs::MoveBaseGoal goal;
geometry_msgs::PoseStamped ar_pose_in_camera_link,ar_pose_in_map;


bool new_ar_marker_flag = false;

//AR二维码接收回调函数
void marker_sub_callback(const visualization_msgs::Marker &marker_tmp)
{
    if(marker_tmp.id!=11)//必须是11号Marker
        return;
    
    ar_pose_in_camera_link.header=marker_tmp.header;
    ar_pose_in_camera_link.pose.position=marker_tmp.pose.position;
    ar_pose_in_camera_link.pose.orientation=marker_tmp.pose.orientation;
    new_ar_marker_flag = true;
}


int main (int argc, char **argv)
{
    ros::init(argc, argv, "move_to_target");
    ros::NodeHandle n;

    MoveBaseClient ac("move_base", true);
    tf::TransformListener listener;

    ros::Subscriber marker_sub=n.subscribe("/visualization_marker",10,marker_sub_callback);

    ros::Publisher odom_point_pub = n.advertise<geometry_msgs::PoseStamped>("/ar_pose",10);

    ros::Publisher tts_pub = n.advertise<std_msgs::String>("/tts",10);
    std_msgs::String str_msg;







    ROS_INFO("waiting for the move_base server...");
    ac.waitForServer(ros::Duration(60));
    ROS_INFO("move_base server connect ok");


    ros::Rate loop_rate(1);//loop sleep=1s


    double last_goal_x=0,last_goal_y=0,last_goal_yaw=0;
    double yaw_error=0;


    // 定义一个客户端
    Client client("do_move", true);
    // 等待服务器端
    ROS_INFO("Waiting for action server to start.");
    client.waitForServer();


    while(1)
    {
        int step=1;
        float keep_distance_m;
        while(ros::ok())
        {
            ros::spinOnce();//必须要有spinOnce，否则收不到消息
            loop_rate.sleep();


            if(new_ar_marker_flag == false)
            {
                ROS_INFO("do not find ar marker");

                do_move(client,90,0);


                str_msg.data = "未发现目标，左转90度搜寻目标";
                tts_pub.publish(str_msg);


                ros::Duration(1).sleep();//等待1s
                continue;
            }
            else
            {


                ros::Duration(1).sleep();//等待1s
                new_ar_marker_flag = false;//清0
            }




            //把ar位置(相对于/camera_link)转换成相对于/map的位置
            try
            {
                listener.transformPose("map", ar_pose_in_camera_link,ar_pose_in_map);
            }
            catch( tf::TransformException ex)
            {
                ROS_INFO("transfrom exception : %s",ex.what());
                continue;
            }

            //对ar位置做安全距离设置
            tf::Quaternion quat;
            double roll, pitch, yaw;

            tf::quaternionMsgToTF(ar_pose_in_map.pose.orientation, quat);//geometry_msgs/Quaternion转tf::Quaternion四元数
            tf::Matrix3x3(quat).getRPY(roll, pitch, yaw);//四元数转欧拉角

            yaw += 1.5708;//旋转90度


            if(step==1)
            {
                keep_distance_m = 0.30;//先大概到充电座附近
                str_msg.data = "发现目标，和目标保持30厘米";
                tts_pub.publish(str_msg);
            }
            else if(step==2)
            {
                keep_distance_m = 0.20;//在20cm处认为识别识别已经很准了
                str_msg.data = "发现目标，和目标保持20厘米";
                tts_pub.publish(str_msg);
            }
            else if(step==3)
            {
                keep_distance_m = 0.12;//到15cm处转向，到这里之后并没有做任何识别确认了。。。
                str_msg.data = "发现目标，和目标保持12厘米";
                tts_pub.publish(str_msg);
            }

            ar_pose_in_map.pose.position.x -= keep_distance_m*cos(yaw);//keep_distance_m表示移动的目标点距离标签的垂直距离，即将当前的x值进行一下偏移
            ar_pose_in_map.pose.position.y -= keep_distance_m*sin(yaw);
            ar_pose_in_map.pose.position.z = 0;
            ar_pose_in_map.pose.orientation = tf::createQuaternionMsgFromYaw(yaw);//将rpy表示转换为四元数表示

            odom_point_pub.publish(ar_pose_in_map);//发布一个可视化的目标坐标点

            //赋值给导航目标
            goal.target_pose.header.frame_id = "map";
            goal.target_pose.header.stamp = ros::Time::now();
            goal.target_pose.pose.position = ar_pose_in_map.pose.position;
            goal.target_pose.pose.orientation = ar_pose_in_map.pose.orientation;


            ROS_INFO("send goal to move_base, waiting for result...");
            ac.sendGoal(goal);

            ac.waitForResult();

            if (ac.getState() == actionlib::SimpleClientGoalState::SUCCEEDED)
            {
                ROS_INFO("move to goal ok");

                if(step==1)
                {
                    step=2;
                    ROS_INFO("Step 2");
                    continue;
                }
                else if(step==2)
                {
                    step=3;
                    ROS_INFO("Step 3");
                    continue;
                }

                //两次设定的目标误差在1cm和1度范围内，说明识别已经稳定，不再运动
    //            yaw_error = abs(yaw-last_goal_yaw);
    //            if(yaw_error>180)
    //            {
    //                yaw_error = 360-yaw_error;
    //            }

    //            if( abs(ar_pose_in_map.pose.position.x - last_goal_x)<0.01*5 //1cm
    //             && abs(ar_pose_in_map.pose.position.y - last_goal_y)<0.01*5 //1cm
    //             && yaw_error < 0.017*3)//1度
    //            {


                    ros::Duration(1).sleep();//等待1s

                    str_msg.data = "右转180度，后退5厘米，对接充电座";
                    tts_pub.publish(str_msg);

                    do_move(client,-180,-0.05);


                    //str_msg.data = "位置微调";
                    //tts_pub.publish(str_msg);
                    //do_move(client,3,0.0);

                    //do_move(client,-3,0.0);




                    ROS_INFO("finally, move to target success ^_^");

                    sleep(60);//1min
                    
                    do_move(client,0,0.30);//前进0.3m
                    do_move(client,90,0.20);//转45度前进0.2m
                    break;
                    //exit(0);
                //}




    //            last_goal_x=ar_pose_in_map.pose.position.x;
    //            last_goal_y=ar_pose_in_map.pose.position.y;
    //            last_goal_yaw=yaw;

            }
            else
            {
                ROS_INFO("move to goal failed!");
            }
        }
    }

    return 0;
}
