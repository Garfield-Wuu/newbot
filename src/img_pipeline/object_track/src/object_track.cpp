#include <ros/ros.h>
#include <std_msgs/String.h>
#include <geometry_msgs/Twist.h>
#include <sensor_msgs/LaserScan.h>
#include <sensor_msgs/Image.h>
#include <sensor_msgs/CameraInfo.h>
#include <tf/transform_listener.h>
#include <opencv2/opencv.hpp>
#include <json.hpp>
#include <cv_bridge/cv_bridge.h>

#include <message_filters/subscriber.h>
#include <message_filters/synchronizer.h>
#include <message_filters/time_synchronizer.h>
#include <message_filters/sync_policies/exact_time.h>
#include <sensor_msgs/Image.h>
#include <sensor_msgs/CameraInfo.h>
#include <message_filters/sync_policies/approximate_time.h>
#include <std_msgs/Bool.h>


#include "ai_msgs/Dets.h"

#include <thread>

using namespace nlohmann;

void read_camera_info_from_file(std::string &calibration_file,cv::Mat &CameraMat,cv::Mat &DistCoeff)
{

    ROS_INFO("open file: %s",calibration_file.c_str());
    
	cv::FileStorage fs(calibration_file, cv::FileStorage::READ);
	
	if (!fs.isOpened())
	{
		ROS_ERROR("Cannot open file calibration file '%s'", calibration_file.c_str());
		ros::shutdown();
		return;
	}
    
    cv::Size ImageSize_raw;
    
    //fs["CameraExtrinsicMat"] >> CameraExtrinsicMat;
    fs["CameraMat"] >> CameraMat;
    fs["DistCoeff"] >> DistCoeff;
    fs["ImageSize"] >> ImageSize_raw;

    fs.release();

    //缩放尺寸
    cv::Size ImageSize(ImageSize_raw.width/2,ImageSize_raw.height/2);

    CameraMat = CameraMat/2;//缩放内参
    CameraMat.at<double>(2,2) = 1;//右下角不缩放
}


class ObjectTrack 
{
public:
    //如果没有破浪线nh.param后面的参数要加上节点名字，否则获取不到launch中的参数，所以最好加上波浪线
    ObjectTrack() : nh("~")
    {
        nh.param<std::string>("calibration_file", calibration_file, "fisheye.yaml");
        nh.param<std::string>("camera_frame", camera_frame, "camera_link");
        nh.param<std::string>("laser_frame", laser_frame, "laser_link");
        nh.param<std::string>("pub_image_topic", pub_image_topic, "/camera/image_det_track");

        nh.param<std::string>("sub_laser_topic", sub_laser_topic, "/scan");
        //nh.param<std::string>("sub_camera_info_topic", sub_camera_info_topic, "/camera/camera_info");
        nh.param<std::string>("sub_image_topic", sub_image_topic, "/camera/image_det");
        nh.param<std::string>("sub_det_topic", sub_det_topic,  "/ai_msg_det");
        nh.param<std::string>("tracking_class_name", tracking_class_name,  "person");

        nh.param<int>("width", width,  640);
        nh.param<int>("height",height, 360);
        nh.param<int>("object_area_threshold",object_area_threshold, 20);

        nh.param<double>("kp_angular",  kp_angular,   0.01);
        nh.param<double>("kp_linear",   kp_linear,  0.5);
        nh.param<double>("max_angular", max_angular,1.5);
        nh.param<double>("max_linear",  max_linear, 0.1);
        nh.param<double>("keep_distance", keep_distance, 0.3);
        nh.param<double>("valid_distance_min",  valid_distance_min, 0.05);
        nh.param<double>("valid_distance_max",  valid_distance_max, 5);

        nh.param<bool>("control_linear",  control_linear, true);

        read_camera_info_from_file(calibration_file, K, D);

        ROS_INFO("waitForTransform laser_link");
        // 获取camera_link和laser_link之间的tf变换换（只读一次，因为是固定的）
        try
        {
            listener.waitForTransform(camera_frame, laser_frame, ros::Time(0), ros::Duration(10.0));
            listener.lookupTransform(camera_frame,  laser_frame, ros::Time(0), laser_transform);
        }
        catch (tf::TransformException ex) 
        {
            ROS_ERROR("%s", ex.what());
        }
        ROS_INFO("waitForTransform ok");

        cmd_vel_pub = nh.advertise<geometry_msgs::Twist>("/cmd_vel", 10);
        image_pub = nh.advertise<sensor_msgs::Image>(pub_image_topic, 10);

        
        //camera_info_sub = nh.subscribe(sub_camera_info_topic, 10, &ObjectTrack::camera_info_callback, this);

        enable_tracking_sub = nh.subscribe("/enable_tracking", 10, &ObjectTrack::enable_tracking_callback, this);
        enable_lidar_sub = nh.subscribe("/enable_lidar", 10, &ObjectTrack::enable_lidar_callback, this);
    }

    //使能跟踪订阅回调函数
    void enable_tracking_callback(const std_msgs::Bool::ConstPtr& msg)
    {
        enable_tracking = msg->data;

        int subscribers = image_pub.getNumSubscribers();//获取目标图像的订阅者个数
        if(subscribers==0)//如果没有人订阅，说明没有客户端查看，此时才对源话题的状态做修改
        {
            if (msg->data)
            {
                if(image_sub)
                    image_sub->subscribe(); // 开启图像话题的订阅
                if(det_sub)
                    det_sub->subscribe();   // 开启检测结果话题的订阅
            }
            else
            {
                if(image_sub)
                    image_sub->unsubscribe();  // 停止图像话题的订阅
                if(det_sub)
                    det_sub->unsubscribe();    // 停止检测结果话题的订阅
            }
        }

        //跟踪状态切换后，速度清0
        cmd_vel_pub.publish(geometry_msgs::Twist());

        //雷达点云和距离清0
        pixel_points.clear();
        object_distance = 0;
    }

    void enable_lidar_callback(const std_msgs::Bool::ConstPtr& msg)
    {
        if (msg->data) 
        {
            scan_sub = nh.subscribe(sub_laser_topic, 10, &ObjectTrack::scan_callback, this);
            ROS_INFO("enable_lidar true");
        }
        else
        {
            scan_sub.shutdown();

            //雷达点云和距离清0
            pixel_points.clear();
            object_distance = 0;

            ROS_INFO("enable_lidar false");
        }
    }
            
    void sync_callback(const sensor_msgs::ImageConstPtr &image_msg,const ai_msgs::DetsConstPtr &dets_msg)//const std_msgs::StringPtr &msg)
    {
        cv::Mat image(image_msg->height, image_msg->width, CV_8UC3, (char *)image_msg->data.data());

        // 找到面积最大且类别为"person"的目标框
        int max_area = object_area_threshold;//必须大于20像素
        int object_index = -1;
        
        for (int i = 0; i < dets_msg->dets.size(); i++) 
        {
            ai_msgs::Det det = dets_msg->dets[i];
            int x1 = det.x1;
            int y1 = det.y1;
            int x2 = det.x2;
            int y2 = det.y2;
            std::string cls_name = det.cls_name;
            int area = (x2 - x1) * (y2 - y1);
            if (cls_name == tracking_class_name && area > max_area) 
            {
                max_area = area;
                object_index = i;
                object_det = det;
            }
        }

        // 如果找到了目标框，进行跟踪
        if (object_index != -1)
        {
            int x1 = object_det.x1;
            int y1 = object_det.y1;
            int x2 = object_det.x2;
            int y2 = object_det.y2;

            int object_x = (x1 + x2) / 2;
            int object_y = (y1 + y2) / 2;

            //ROS_INFO("object_x,y=%d,%d",object_x,object_y);

            // 如果目标偏左，则机器人需要向左转动
            // 假设traget_x = 200，则 0.01*(320-200)=1.2 rad/s
            
            cmd_vel.angular.z = kp_angular * (width/2 - object_x);//0.01 * (320-最极端0) = 最大3.2

            //角速度限幅
            if(cmd_vel.angular.z>max_angular)
                cmd_vel.angular.z = max_angular;
            else if(cmd_vel.angular.z<-max_angular)
                cmd_vel.angular.z = -max_angular;


            // 根据目标距离控制机器人前后运动
            // 假设目标距离为0.6米,则 0.5*(0.6-0.5)=0.05 m/s
            if(object_distance>=valid_distance_min && object_distance<=valid_distance_max)//5cm~5米之间认为识别正常
                cmd_vel.linear.x = kp_linear * (object_distance - keep_distance);//0.5 * (0.5-0.3) = 0.1
            else//否则线速度给0
                cmd_vel.linear.x = 0;

            //线速度限幅
            if(cmd_vel.linear.x>max_linear)
                cmd_vel.linear.x = max_linear;
            else if(cmd_vel.linear.x<-max_linear)
                cmd_vel.linear.x = -max_linear;

            //如果不控制线速度，则清0
            if(control_linear==false)
                cmd_vel.linear.x = 0;

            //ROS_INFO("distance: %.2f m control: %d linear.x: %.2f m/s",object_distance,control_linear,cmd_vel.linear.x);

            
            rectangle(image, cv::Point(x1, y1), cv::Point(x2, y2), cv::Scalar(0,255,0), 3);

            putText(image, cv::format("[px:%d,dis:%.2f] [vx:%.2f,vz:%.2f]",object_x,object_distance,cmd_vel.linear.x,cmd_vel.angular.z), cv::Point(20,20), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0,255,0), 2);
        }
        else
        {
            //停止
            cmd_vel.linear.x = 0;
            cmd_vel.angular.z = 0;

            putText(image, "[No target found]", cv::Point(20,20), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0,255,0), 2);
        }

        if(enable_tracking)//如果使能跟踪打开
            cmd_vel_pub.publish(cmd_vel);

        for(int i=0; i < pixel_points.size(); i++)
        {
            cv::circle(image, cv::Point(pixel_points[i].x, pixel_points[i].y),
                2.0/pixel_points[i].z, cv::Scalar(0, 255, 0), -1);//绘制投影点云
        }

        //cv::imshow("image",image);
        //cv::waitKey(1);

        memcpy((char *)image_msg->data.data(), image.data, image_msg->data.size());
        image_pub.publish(image_msg);
    }

    void scan_callback(const sensor_msgs::LaserScanConstPtr& msg)
    {
        float min_distance = msg->range_max;//最小距离初始值
        std::vector<cv::Point3d> camera_points;
        for (int i = 0; i < msg->ranges.size(); i++)
        {
            float range = msg->ranges[i];//读取距离
            if (std::isnan(range) || range >= msg->range_max)
                continue;

            float angle = msg->angle_min + i * msg->angle_increment;//读取角度
            tf::Vector3 point(range * cos(angle), range * sin(angle), 0.0);//得到点云坐标
            point = laser_transform * point;//转换到相机坐标系下
            if(point.z() <= 0) //z值必须是正数，在相机的前面
                continue;

            cv::Point3d camera_point(point.x(), point.y(), point.z());
            camera_points.push_back(camera_point);//相机坐标系下的点坐标
        }

        // if(camera_info_ok==false)
        //     return;

        std::vector<cv::Point2d> pixels;
        cv::fisheye::projectPoints(camera_points, pixels, cv::Vec3d(0, 0, 0), K, D);
        
        //读取目标框
        int x1 = object_det.x1;
        int y1 = object_det.y1;
        int x2 = object_det.x2;
        int y2 = object_det.y2;
        std::vector<cv::Point3d> points;
        for(int i=0; i < pixels.size(); i++)
        {
            int u=pixels[i].x;
            int v=pixels[i].y;
            cv::Point3d camera_point = camera_points[i];
            if (u >= 0 && u < width && v >= 0 && v < height)//如果点云落在图像内
            {
                //if (u > x1 && u < x2 && v > y1 && v < y2)//点云落在检测框内
                if (u > x1 && u < x2)//点云落在检测框的x范围内，不考察高度y，因为路面崎岖的话高度方向会抖动
                {
                    float distance = sqrt(camera_point.x * camera_point.x + camera_point.y * camera_point.y + camera_point.z * camera_point.z);//相机坐标系下的距离
                    if (distance < min_distance)
                    {
                        min_distance = distance;
                    }
                }

                camera_point.x = pixels[i].x;
                camera_point.y = pixels[i].y;
                points.push_back(camera_point);
            }
        }

        pixel_points = points;
        object_distance = min_distance;

        //ROS_INFO("object_distance: %.2f m",object_distance);

    }

    void run_check_thread(message_filters::Subscriber<sensor_msgs::Image> *image_sub, message_filters::Subscriber<ai_msgs::Dets> *det_sub)
    {
        this->image_sub = image_sub;
        this->det_sub = det_sub;
        int last_subscribers = 0;
        while(ros::ok())
        {
            int subscribers = image_pub.getNumSubscribers();
            if(subscribers==0 && last_subscribers>0)
            {
                ROS_INFO("track image subscribers = 0, src sub shutdown");
                image_sub->unsubscribe();  // 停止图像话题的订阅
                det_sub->unsubscribe();    // 停止检测结果话题的订阅
            }
            else if(subscribers>0 && last_subscribers==0)
            {
                ROS_INFO("track image subscribers > 0, src sub start");
                image_sub->subscribe(); // 开启图像话题的订阅
                det_sub->subscribe();   // 开启检测结果话题的订阅
            }

            last_subscribers = subscribers;

            usleep(100*1000);//100ms
        }
    }

    // void camera_info_callback(const sensor_msgs::CameraInfoConstPtr& msg)
    // {
    //     //校准雷达点并投影到像素坐标
    //     memcpy(K.data,(char *)msg->K.data(),3*3*sizeof(double));
    //     memcpy(D.data,(char *)msg->D.data(),1*4*sizeof(double));
    //     camera_info_ok = true;

    //     camera_info_sub.shutdown();//订阅到一次camera info之后，取消订阅camera info
    // }
    
    ros::NodeHandle nh;
    std::string camera_frame,laser_frame,pub_image_topic,calibration_file;
    std::string sub_laser_topic,sub_image_topic,sub_det_topic,tracking_class_name;
    int width,height,object_area_threshold;
    double kp_angular,kp_linear,max_angular,max_linear,keep_distance,valid_distance_min,valid_distance_max;
    bool control_linear;


    ros::Subscriber scan_sub,enable_tracking_sub,enable_lidar_sub;
    //ros::Subscriber camera_info_sub;

    ros::Publisher cmd_vel_pub;
    ros::Publisher image_pub;

    tf::TransformListener listener;
    tf::StampedTransform laser_transform;

    //sensor_msgs::CameraInfo camera_info;

    cv::Mat K,D;
    //bool camera_info_ok=false;

    ai_msgs::Det object_det;
    std::vector<cv::Point3d> pixel_points;
    float object_distance=0;

    geometry_msgs::Twist cmd_vel;
    bool enable_tracking = false;

    message_filters::Subscriber<sensor_msgs::Image> *image_sub;
    message_filters::Subscriber<ai_msgs::Dets> *det_sub;
};




int main(int argc, char** argv) 
{
    ros::init(argc, argv, "object_track");

    ObjectTrack object_track;

    message_filters::Subscriber<sensor_msgs::Image> image_sub(object_track.nh, object_track.sub_image_topic, 1);
    message_filters::Subscriber<ai_msgs::Dets> det_sub(object_track.nh,  object_track.sub_det_topic, 1);

    typedef message_filters::sync_policies::ExactTime<sensor_msgs::Image, ai_msgs::Dets> SyncPolicy;
    message_filters::Synchronizer<SyncPolicy> sync(SyncPolicy(10), image_sub, det_sub);
    sync.registerCallback(boost::bind(&ObjectTrack::sync_callback,&object_track, _1, _2));
    image_sub.unsubscribe();  // 停止图像话题的订阅
    det_sub.unsubscribe();    // 停止检测结果话题的订阅


    std::thread check_thread(&ObjectTrack::run_check_thread, &object_track, &image_sub, &det_sub);
    ros::spin();

    return 0;
}
