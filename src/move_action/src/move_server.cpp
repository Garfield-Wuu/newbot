#include <ros/ros.h>
#include <actionlib/server/simple_action_server.h>
#include "move_action/DoMoveAction.h"

#include <signal.h>
#include <geometry_msgs/Twist.h>
#include <tf/transform_listener.h>
#include <nav_msgs/Odometry.h>
#include <string.h>

#include <move_base_msgs/MoveBaseAction.h>
#include <actionlib/client/simple_action_client.h>
#include <visualization_msgs/Marker.h>
#include <tf/transform_datatypes.h>
#include <geometry_msgs/PoseStamped.h>
#include <std_msgs/Bool.h>

#include <angles/angles.h>

#include <cmath>

typedef actionlib::SimpleActionServer<move_action::DoMoveAction> Server;

ros::Publisher cmd_vel_pub;




// 收到action的goal后调用的回调函数
void execute(const move_action::DoMoveGoalConstPtr& goal, Server* as)
{

    ros::Rate rate(50);//50Hz 20ms
    move_action::DoMoveFeedback feedback;
    move_action::DoMoveResult result;

    tf::TransformListener listener;
    tf::StampedTransform transform;
    
    std::string odom_frame = "/odom";
    std::string base_frame = "/base_footprint";;
    try
    {
        listener.waitForTransform(odom_frame, base_frame, ros::Time(), ros::Duration(2.0) );
        listener.lookupTransform( odom_frame, base_frame, ros::Time(), transform);
    }
    catch (tf::TransformException &ex)
    {
        ROS_ERROR("%s",ex.what());

        cmd_vel_pub.publish(geometry_msgs::Twist());
        ros::Duration(0.5).sleep();
        ROS_INFO("robot stoped");

        result.result_flag = -1;
        result.result_angle = 0;
        result.result_distance = 0;
        as->setSucceeded(result);//成功标志

        return;
    }

    if(goal->angular_speed<=0 || goal->linear_speed<=0)
    {
        ROS_INFO("input speed error, robot stoped");

        result.result_flag = -1;
        result.result_angle = 0;
        result.result_distance = 0;
        as->setSucceeded(result);//成功标志

        return;
    }

    ROS_INFO("goal_angle:%.2f  goal_distance:%.2fm angular_speed:%.2frad/s linear_speed:%.2fm/s",
                goal->goal_angle*180/M_PI,
                goal->goal_distance,
                goal->angular_speed,
                goal->linear_speed);

    float angular_tolerance = 1.0*M_PI/180;//0.5*M_PI/180;//0.5度*pi/180 //角度转换成弧度
    float linear_tolerance = 0.01;//0.01m=1cm

    geometry_msgs::Twist speed;

    int ok_cnt=0;

    float error_angle, angle, last_angle, turn_angle=0, last_error_angle;
    listener.lookupTransform(odom_frame, base_frame, ros::Time(), transform);
    
    last_angle = tf::getYaw(transform.getRotation());//记录初始yaw角度
    last_error_angle = goal->goal_angle - 0;

    while(goal->goal_angle !=0 && ros::ok() )//目标角度不为0才会进入循环，否则跳过循环
    {
        //读取角度
        listener.lookupTransform(odom_frame, base_frame, ros::Time(0), transform);
        angle = tf::getYaw(transform.getRotation());

        turn_angle += angles::shortest_angular_distance(last_angle,angle);//last_angel->angle累加
        
        //输出速度 = KP*(目标角度 - 当前角度)
        error_angle = goal->goal_angle - turn_angle;
        float kp=5.0;
        speed.angular.z = kp*error_angle;  //+ 2.0*(error_angle-last_error_angle);
        //调试经验:KP太大，会超过调，然后往回转一下
        //KP太小，没到位就停了，然后往前转一点，此时可能还会往回转一下
        //判断了KP是太大或者太小之后，然后再反向做调整

        last_angle = angle;
        last_error_angle = error_angle;

        //输出限幅，不能太大
        if(speed.angular.z > goal->angular_speed)//angular_speed永远是正的
            speed.angular.z = goal->angular_speed; // 设置角速度，正为左转，负为右转
        else if(speed.angular.z < -goal->angular_speed)
            speed.angular.z = -goal->angular_speed; // 设置角速度，正为左转，负为右转

        //输出限幅，不能太小，太小直接等于0.5
        float min_angular_speed = 0.5;
        if(speed.angular.z>0 && speed.angular.z<min_angular_speed)
            speed.angular.z = min_angular_speed;
        else if(speed.angular.z<0 && speed.angular.z>-min_angular_speed)
            speed.angular.z = -min_angular_speed;

        if(fabs(error_angle)<angular_tolerance)
        {
            ok_cnt++;
            if(ok_cnt>=5) //20ms*5 = 100ms 连续5次误差都满足要求
            {
                ok_cnt=0;
                ROS_INFO("error angle < tolerance %.2f deg: %.2f",angular_tolerance*180/M_PI,error_angle*180/M_PI);
                break;
            }

            speed.angular.z = 0;
        }
        else
        {
            ok_cnt = 0;//要连续的5次误差都满足要求
        }

        cmd_vel_pub.publish(speed);

        if(as->isPreemptRequested())
        {
            ROS_INFO("cancelled!\n");
            cmd_vel_pub.publish(geometry_msgs::Twist());//停车
            result.result_flag = 0;
            result.result_angle = feedback.current_angle;
            result.result_distance = feedback.current_distance;
            as->setPreempted(result);
            return;
        }

        //发布反馈数据
        feedback.current_angle = turn_angle;
        as->publishFeedback(feedback);

        rate.sleep();//20ms
    }

    //旋转完停顿0.1s秒
    cmd_vel_pub.publish(geometry_msgs::Twist());
    ros::Duration(0.1).sleep();

    //再次检查角度变化
    //读取角度
    listener.lookupTransform(odom_frame, base_frame, ros::Time(0), transform);
    angle = tf::getYaw(transform.getRotation());
    turn_angle += angles::shortest_angular_distance(last_angle,angle);//last_angel->angle累加

    //发布反馈数据
    feedback.current_angle = turn_angle;
    as->publishFeedback(feedback);


    //如果goal_distance为0，则直接向客户端返回结果
    if(goal->goal_distance==0 || goal->linear_speed<=0)
    {
        ROS_INFO("finished goal, angle %.2f deg distance %.2f m\n",feedback.current_angle*180/M_PI,feedback.current_distance);
        result.result_flag = 1;
        result.result_angle = feedback.current_angle;
        result.result_distance = feedback.current_distance;
        as->setSucceeded(result);//成功标志

        return;
    }
    
    //开始直走
    if(goal->goal_distance > 0)
    {
        ROS_INFO("go front %.2f m",goal->goal_distance);
        speed.linear.x = goal->linear_speed; // 设置线速度，正为前进，负为后退
    }
    else
    {
        ROS_INFO("go back %.2f m",goal->goal_distance);
        speed.linear.x = -goal->linear_speed; // 设置线速度，正为前进，负为后退
    }

    speed.angular.z = 0;//角速度清0

    listener.lookupTransform(odom_frame, base_frame, ros::Time(0), transform);
    float x_start = transform.getOrigin().x();
    float y_start = transform.getOrigin().y();

    float distance = 0;
    float x,y;
    while( (distance + linear_tolerance < fabs(goal->goal_distance)) && ros::ok() )
    {
        //直走
        cmd_vel_pub.publish(speed);
        rate.sleep();

        //读取位置信息
        listener.lookupTransform(odom_frame, base_frame, ros::Time(0), transform);
        x = transform.getOrigin().x();
        y = transform.getOrigin().y();

        //计算已走距离 计算出来距离肯定是正数
        distance = sqrt( pow(x-x_start,2) +  pow(y-y_start,2) );

        if(as->isPreemptRequested())
        {
            ROS_INFO("cancelled!\n");//停车
            cmd_vel_pub.publish(geometry_msgs::Twist());
            result.result_flag = 0;
            result.result_angle = feedback.current_angle;
            result.result_distance = feedback.current_distance;
            as->setPreempted(result);
            return;
        }

        //发布反馈数据
        feedback.current_distance = distance;
        as->publishFeedback(feedback);
    }

    //直走完停顿0.5秒
    cmd_vel_pub.publish(geometry_msgs::Twist());
    ros::Duration(0.5).sleep();

    //再次读取位置信息
    listener.lookupTransform(odom_frame, base_frame, ros::Time(0), transform);
    x = transform.getOrigin().x();
    y = transform.getOrigin().y();

    //计算已走距离 计算出来距离肯定是正数
    distance = sqrt( pow(x-x_start,2) +  pow(y-y_start,2) );

    //发布反馈数据
    feedback.current_distance = distance;
    as->publishFeedback(feedback);
    
    //当action完成后，向客户端返回结果
    ROS_INFO("all finished goal, angle %.2f deg distance %.2f m\n",feedback.current_angle*180/M_PI,feedback.current_distance);
    result.result_flag = 1;
    result.result_angle = feedback.current_angle;
    result.result_distance = feedback.current_distance;
    as->setSucceeded(result);//成功标志
}


int main(int argc, char** argv)
{

    setlocale(LC_CTYPE, "zh_CN.utf8");

    ros::init(argc, argv, "move_server");
    ros::NodeHandle n;
    
    cmd_vel_pub = n.advertise<geometry_msgs::Twist>("/cmd_vel", 1);

    // 定义一个服务器
    Server server(n, "do_move", boost::bind(&execute, _1, &server), false);

    // 服务器开始运行
    server.start();

    ROS_INFO("move action start ok");

    ros::spin();

    return 0;
}
