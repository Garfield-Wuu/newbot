#include "img_decode.h"

#include <thread>

//如果没有破浪线nh.param后面的参数要加上节点名字，否则获取不到launch中的参数，所以最好加上波浪线
ImgDecode::ImgDecode() : nh("~") 
{
    string pub_raw_image_topic,camera_info_topic;
    string camera_name,camera_info_url;
    int width,height;

    nh.param<string>("sub_jpeg_image_topic", sub_jpeg_image_topic, "/image_raw/compressed");
    nh.param<string>("pub_raw_image_topic", pub_raw_image_topic, "/camera/image_raw");
    nh.param<int>("fps_div", fps_div, 2);
    nh.param<double>("scale", scale, 1.0);

    nh.param<int>("width", width, 1280);
    nh.param<int>("height", height, 720);

    raw_image_pub = nh.advertise<sensor_msgs::Image>(pub_raw_image_topic, 10);
    

#if(USE_ARM_LIB==1)
    mpp_decode.init(width,height);
#endif

}

void ImgDecode::compressed_image_callback(const sensor_msgs::CompressedImageConstPtr& msg)
{

    frame_cnt++;

    if(frame_cnt%fps_div!=0)//分频 减少后续处理负担
    {
        return;
    }


//auto t1 = std::chrono::system_clock::now();

    
#if(USE_ARM_LIB==1)
    if(msg->data.size() <= 4096) //一般情况下，JPEG图像不能小于4KB
    {
        ROS_WARN("jpeg data size error! size = %d\n",msg->data.size());
        return;
    }

    //硬解码JPEG->RGB
    int ret = mpp_decode.decode((unsigned char*)msg->data.data(), msg->data.size(), image);//msg-->image
    if(ret < 0)
    {
        ROS_WARN("jpeg decode error! size = %d\n",msg->data.size());
        return;
    }
#else
    //软解码JPEG->BGR->RGB
    image = cv::imdecode(cv::Mat(msg->data), cv::IMREAD_COLOR);
    cv::cvtColor(image, image, cv::COLOR_BGR2RGB);
    if (image.empty())
    {
        ROS_WARN("Failed to decode compressed image");
        return;
    }
#endif

//auto t2 = std::chrono::system_clock::now();
    

    msg_pub.header = msg->header;//使用原有时间戳
    msg_pub.height = image.rows * scale;
    msg_pub.width =  image.cols * scale;
    msg_pub.encoding = "rgb8";
    msg_pub.step = msg_pub.width * 3;
    msg_pub.data.resize(msg_pub.height * msg_pub.width * 3);

    //避免图像多次拷贝，直接将缩放后的数据写到msg_pub中

#if(USE_ARM_LIB==1)
    //硬缩放RGB->小RGB
    rga_resize(image, msg_pub.data.data(), scale);//image-->msg_pub.data
#else
    //软缩放RGB->小RGB
    cv::resize(image, image, cv::Size(), scale, scale);
    memcpy(msg_pub.data.data(), image.data, msg_pub.height * msg_pub.width * 3);
#endif
    
//auto t3 = std::chrono::system_clock::now();

    raw_image_pub.publish(msg_pub);//发布图像

// auto t4 = std::chrono::system_clock::now();

// ROS_WARN_THROTTLE(1,"decode=%d resize=%d pub=%d ms",
// std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count(),
// std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2).count(),
// std::chrono::duration_cast<std::chrono::milliseconds>(t4 - t3).count());
        
}

void ImgDecode::run_check_thread()
{
    int last_subscribers = 0;
    while(ros::ok())
    {
        int subscribers = raw_image_pub.getNumSubscribers();
        if(subscribers==0 && last_subscribers>0)
        {
            ROS_INFO("decode image subscribers = 0, src sub shutdown");
            compressed_image_sub.shutdown();
        }
        else if(subscribers>0 && last_subscribers==0)
        {
            ROS_INFO("decode image subscribers > 0, src sub start");
            compressed_image_sub = nh.subscribe(sub_jpeg_image_topic, 10, &ImgDecode::compressed_image_callback,this);
        }

        last_subscribers = subscribers;

        usleep(100*1000);//100ms
    }
}



int main(int argc, char** argv)
{
    ros::init(argc, argv, "img_decode");

    ImgDecode img_decode;

    std::thread check_thread(&ImgDecode::run_check_thread, &img_decode);

    ros::spin();


    
    return 0;
}




