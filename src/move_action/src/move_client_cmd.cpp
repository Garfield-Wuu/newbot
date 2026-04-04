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
#include <std_msgs/Float32MultiArray.h>
#include "move_action/DoMoveAction.h"

typedef actionlib::SimpleActionClient<move_action::DoMoveAction> Client;

float cmd_turn_angle = 0;
float cmd_distance = 0;
int new_cmd_mode = 0;

void action_cmd_callback(const std_msgs::Float32MultiArray::ConstPtr& array) 
{
    if (array->data.size() == 3) 
    {
        new_cmd_mode   = array->data[0];
        cmd_turn_angle = array->data[1];
        cmd_distance   = array->data[2];
        
        ROS_INFO("Received Array: [%.0f %.2f, %.2f]",new_cmd_mode,cmd_turn_angle, cmd_distance);
    }
    else
    {
        ROS_ERROR("Array size is not 3!");
    }
}

void do_move_action(Client &client,float angle,float distance)
{
    bool succeeded;

    move_action::DoMoveGoal goal;

    goal.goal_angle = angle*M_PI/180.0;//先旋转这么多角度
    goal.goal_distance = distance;//再走这么多距离
    goal.angular_speed=1.0;//角速度
    goal.linear_speed=0.08;//线速度
    client.sendGoal(goal);
    ROS_INFO("sending goal");

    // 等待 Action 完成或取消
    int cnt = 0;
    while( !client.waitForResult(ros::Duration(0.1)) )//等待0.1s
    {
        ros::spinOnce();//必须要有spinOnce，否则收不到消息

        cnt++;
        if(cnt>=300)//300*0.1s = 30s 超时不再等待，取消目标
        {
            ROS_INFO("timeout 30s!");
            client.cancelGoal();
            break;
        }

        if(new_cmd_mode) //如果收到新的命令，则取消目标
        {
            ROS_INFO("received new cmd, cancelGoal!");
            client.cancelGoal();
            break;
        }
    }

    // 检查 Action 的最终状态
    if (client.getState() == actionlib::SimpleClientGoalState::SUCCEEDED) 
    {
        move_action::DoMoveResultConstPtr result = client.getResult();
        ROS_INFO("goal finished %d %.2f deg %.2f m\n", result->result_flag,result->result_angle*180/M_PI,result->result_distance);
    }
    else
    {
        ROS_INFO("action failed or canceled.\n");
    }
}


// void do_dance_action(ros::Publisher &cmd_vel_pub)
// {
//      geometry_msgs::Twist speed;

//     float zs[8]={-1.0, 1.0, 0,    0,    1.0, -1.0, -1.0,  1.0};
//     float xs[8]={ 0,   0,   0.1,  -0.1, -0.1, 0.1, 0.1,   -0.1};
//     for(int i=0;i<8;i++)
//     {
//         speed.angular.z = zs[i];
//         speed.linear.x = xs[i];
//         cmd_vel_pub.publish(speed);

//         //等待完成或取消
//         int cnt = 0;
//         for(int t=0;t<2;t++)//2*100ms=200ms
//         {
//             ros::spinOnce();//必须要有spinOnce，否则收不到消息

//             if(new_cmd_mode) //如果收到新的命令，则取消目标
//             {
//                 ROS_INFO("received new cmd, quit!");
//                 break;
//             }

//             usleep(100*1000);//100ms
//         }

//         if(new_cmd_mode) //如果收到新的命令，则退出目标
//         {
//             break;
//         }
//     }

//     //停止
//     cmd_vel_pub.publish(geometry_msgs::Twist());
// }


int main (int argc, char **argv)
{
    setlocale(LC_CTYPE, "zh_CN.utf8");//防止ROS_INFO中文乱码

    ros::init(argc, argv, "move_client_cmd");
    ros::NodeHandle nh;
    
    // 定义一个客户端
    Client client("do_move", true);

    ros::Subscriber action_cmd_sub = nh.subscribe("/action_cmd", 10, action_cmd_callback);
    //ros::Publisher cmd_vel_pub = nh.advertise<geometry_msgs::Twist>("/cmd_vel", 10);
    
    // 等待服务器端
    ROS_INFO("waiting for move action server to start");
    client.waitForServer();
    ROS_INFO("move action server connect ok");

    ros::Rate loop_rate(10);//10hz 0.1s
    while(ros::ok())
    {
        if(new_cmd_mode==1)//移动
        {
            new_cmd_mode = 0;
            do_move_action(client, cmd_turn_angle, cmd_distance);
        }
        // else if(new_cmd_mode==2)//跳舞
        // {
        //     new_cmd_mode = 0;
        //     do_dance_action(cmd_vel_pub);
        // }
        else
        {
            ros::spinOnce();//必须要有spinOnce，否则收不到消息
            loop_rate.sleep();//0.1s
        }
    }

    ros::shutdown();
    return 0;
}
