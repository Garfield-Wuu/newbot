
#include <signal.h>
#include <iostream>
#include <sys/time.h>
#include <math.h>
#include <dirent.h>
#include <sys/stat.h>
#include "json.hpp"

#include "rknn_run.h"

#include "postprocess.h"
#include "utils.h"

#include "ai_msgs/Dets.h"

#include <ros/package.h>


#define printf ROS_INFO



//输入形式：话题(配合其他摄像头、RTSP等采集节点使用)，或图片文件夹(离线测试)
//输出形式：话题，或图片文件（图片文件用于测试）
RknnRun::RknnRun():nh("~")
{
    nh.param<std::string>("model_file", model_file, ""); //$(find rknn_yolo)/config/xxx.rknn
    nh.param<std::string>("yaml_file", yaml_file, "");
    nh.param<std::string>("offline_images_path", offline_images_path, "");
    nh.param<std::string>("offline_output_path", offline_output_path, "");

    nh.param<std::string>("sub_image_topic", sub_image_topic, "/camera/image_raw");
    nh.param<std::string>("pub_image_topic", pub_image_topic, "/camera/image_det");
    nh.param<std::string>("pub_det_topic", pub_det_topic, "/ai_msg_det");

    nh.param<bool>("is_offline_image_mode", is_offline_image_mode, false);//离线用于图片测试

    nh.param<bool>("print_perf_detail", print_perf_detail, false);
    nh.param<bool>("use_multi_npu_core", use_multi_npu_core, false);
    nh.param<bool>("output_want_float", output_want_float, false);

    nh.param<double>("conf_threshold", conf_threshold, 0.25);
    nh.param<double>("nms_threshold", nms_threshold, 0.45);
    

    //读取classes配置
    cv::FileStorage fs(yaml_file, cv::FileStorage::READ);
    if(!fs.isOpened())
    {
        ROS_WARN("Failed to open file %s\n", yaml_file.c_str());
        ros::shutdown();
        return;
    }

    fs["nc"] >> cls_num;
    fs["label_names"] >> label_names;

    printf("cls_num (nc) =%d label_names len=%d\n",cls_num,label_names.size());

    image_pub = nh.advertise<sensor_msgs::Image>(pub_image_topic, 10);

    //det_pub = nh.advertise<std_msgs::String>(pub_det_topic, 10);
    det_pub = nh.advertise<ai_msgs::Dets>(pub_det_topic, 10);

    //image_sub = nh.subscribe(sub_image_topic, 10, &RknnRun::sub_image_callback,this);
}

void RknnRun::sub_image_callback(const sensor_msgs::ImageConstPtr& msg)
{
    //ROS_WARN_STREAM("frame_id="<<msg->header.frame_id);
    capture_data_queue.push(msg);
}

void sig_handler(int sig)
{
    if (sig == SIGINT)
    {
        printf("Ctrl C pressed, shutdown\n");
        ros::shutdown();
        //exit(0);//在ROS里不要调用exit(),会卡住
    }
}

int RknnRun::run_infer_thread()
{

    signal(SIGINT, sig_handler); // SIGINT 信号由 InterruptKey 产生，通常是 CTRL +C 或者 DELETE

    int img_width;
    int img_height;
    int ret;

    InferData infer_data;

    

    unsigned char* model_data=nullptr;
    int model_width   = 0;
    int model_height  = 0;

#if(USE_ARM_LIB==1)
    if(access(model_file.c_str(), 0)!=0)//模型文件不存在，则退出
    {
        ROS_WARN("%s model file is not exist!!!",model_file.c_str());
        ros::shutdown();
    }

    ret = rknn_load(ctx,model_file.c_str(),model_data,io_num,print_perf_detail,use_multi_npu_core);
    if(ret<0)
    {
      ROS_WARN("rknn_load error, shutdown\n");
      ros::shutdown();
    }

    rknn_input inputs[io_num.n_input];

    ret = rknn_config(ctx,io_num,model_width,model_height,inputs,infer_data.outputs,out_scales,out_zps,output_want_float);
    if(ret<0)
    {
      ROS_WARN("rknn_config error, shutdown\n");
      ros::shutdown();
    }
#endif

    cv::Mat img_resize(model_height,model_width,CV_8UC3);
    std::vector<cv::String> image_files;
    int image_id=0;
    if(is_offline_image_mode)
    {

        cv::glob(offline_images_path, image_files,false);//三个参数分别为要遍历的文件夹地址；结果的存储引用；是否递归查找，默认为false
        if (image_files.size() == 0)
        {
            ROS_WARN_STREAM("offline_images_path read image files!!! : " <<offline_images_path);
            ros::shutdown();
        }
        else
        {
            for (int i = 0; i< image_files.size(); i++)
            {
                ROS_INFO_STREAM(  "offline_images: " <<image_files[i]  );
            }
        }


        if(access(offline_output_path.c_str(), 0)!=0)
        {
            // if this folder not exist, create a new one.
            if(mkdir(offline_output_path.c_str(),0777)!=0)
            {
                ROS_INFO_STREAM( "offline_output_path mkdir fail!!! : " <<offline_output_path  );
                ros::shutdown();
            }
        }
    }

    ROS_INFO("rknn init finished");


    while(1)
    {
        if(is_offline_image_mode)
        {
            if(image_id>=image_files.size())
            {
                printf("image read finished\n");
                return 0;
            }

            infer_data.orig_img = cv::imread(image_files[image_id]);
            cv::cvtColor(infer_data.orig_img, infer_data.orig_img, cv::COLOR_BGR2RGB);//转为RGB用于推理

            infer_data.header.seq = image_id;
            infer_data.header.stamp = ros::Time::now();//离线图片时间戳
            infer_data.header.frame_id = "image";

            image_id++;

            usleep(30*1000);//30ms
        }
        else
        {
            sensor_msgs::ImageConstPtr msg;
            capture_data_queue.wait_and_pop(msg);

            cv_bridge::CvImageConstPtr cv_ptr = cv_bridge::toCvShare(msg,"rgb8");//ROS消息转OPENCV
            if(cv_ptr->image.empty())
            {
                ROS_WARN("cv_ptr->image.empty() !!!");
                continue;
            }

            infer_data.orig_img = cv_ptr->image.clone();//接收的ROS消息已经是RGB格式了，不需要再转换为RGB用于推理
            infer_data.header = msg->header;
        }


        img_width  = infer_data.orig_img.cols;
        img_height = infer_data.orig_img.rows;

        infer_data.mod_size = cv::Size(model_width,model_height);
        infer_data.img_size = infer_data.orig_img.size();

#if(USE_ARM_LIB==1)

        //ROS_INFO("start infer");

//auto t1 = std::chrono::system_clock::now();
        //缩放
        if(img_width != model_width || img_height != model_height)
        {
            rga_resize(infer_data.orig_img,img_resize,infer_data.mod_size);
            inputs[0].buf = (void*)img_resize.data;
        }
        else
        {
            inputs[0].buf = (void*)infer_data.orig_img.data;
        }

        // cv::imshow("rga_resize",img_resize);
        // cv::waitKey(1);

//auto t2 = std::chrono::system_clock::now();
        //1输入数据
        rknn_inputs_set(ctx, io_num.n_input, inputs);

        //2运行推理
        ret = rknn_run(ctx, NULL);

        if(print_perf_detail)//是否打印每层运行时间
        {
            rknn_perf_detail perf_detail;
            ret = rknn_query(ctx, RKNN_QUERY_PERF_DETAIL, &perf_detail,sizeof(perf_detail));
            printf("perf_detail: %s\n",perf_detail.perf_data);
        }


        //3读取结果
        ret = rknn_outputs_get(ctx, io_num.n_output, infer_data.outputs, NULL);

        //ROS_INFO("end infer");

// auto t3 = std::chrono::system_clock::now();

// auto time1 = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
// auto time2 = std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2).count();

//         ROS_WARN_THROTTLE(1,"rga_resize=%d ms rknn_run=%d ms infer_fps=%.1f",time1,time2,1000.0/(time1+time2));
#endif

        process_data_queue.push(infer_data);


    }

#if(USE_ARM_LIB==1)
    ret = rknn_destroy(ctx);

    if(model_data)
    {
        free(model_data);
    }
#endif

}



int RknnRun::run_process_thread()
{

    signal(SIGINT, sig_handler); // SIGINT 信号由 InterruptKey 产生，通常是 CTRL +C 或者 DELETE

    printf("post process config: conf_threshold = %.2f, nms_threshold = %.2f\n",
         conf_threshold, nms_threshold);

    int ret;
    char text[256];

    //cv::Mat seg_mask1,seg_mask2;

    unsigned int frame_cnt=0;
    double t=0,last_t=0;
    double fps;
    struct timeval tv;

    cv::Scalar color_list[12] = {
        cv::Scalar(0, 0 ,255),
        cv::Scalar(0, 255 ,0),
        cv::Scalar(255, 0 ,0),

        cv::Scalar(0, 255 ,255),
        cv::Scalar(255, 0 ,255),
        cv::Scalar(255,255 ,0),

        cv::Scalar(0, 128 ,255),
        cv::Scalar(0, 255 ,128),

        cv::Scalar(128, 0 ,128),
        cv::Scalar(255, 0 ,128),

        cv::Scalar(128, 255 ,0),
        cv::Scalar(255, 128 ,0)
    };

    sensor_msgs::ImagePtr image_msg;

#if(USE_ARM_LIB==0)


    std::string package_path = ros::package::getPath("rknn_yolov6");

    // 加载级联分类器文件，这个文件通常包含在OpenCV的data目录下
    cv::CascadeClassifier face_cascade;
    std::string xml_file = package_path+"/config/haarcascade_frontalface_default.xml";
    if (!face_cascade.load(xml_file))
    {
        cout << "Error loading face cascade: " << xml_file << endl;
        return -1;
    }

#endif

    while(1)
    {

        InferData infer_data;
        process_data_queue.wait_and_pop(infer_data);

        cv::Mat infer_data_orig_img = infer_data.orig_img.clone();//拷贝一份绘制，否则会影响到原始图片data

#if(USE_ARM_LIB==1)
        rknn_output *outputs = infer_data.outputs;//获取结构体的指针
#endif
        cv::Size mod_size = infer_data.mod_size;
        cv::Size img_size = infer_data.img_size;

        float scale_w = (float)img_size.width / mod_size.width;
        float scale_h = (float)img_size.height / mod_size.height;
        
        std::vector<int> out_index={0,1,2};
        std::vector<Det> dets;

//auto t1 = std::chrono::system_clock::now();

#if(USE_ARM_LIB==1)
        //4后处理
        post_process(outputs[out_index[0]].want_float,
                   outputs[out_index[0]].buf,outputs[out_index[1]].buf,outputs[out_index[2]].buf, mod_size.height, mod_size.width,
                   conf_threshold, nms_threshold, scale_w, scale_h,
                   out_zps, out_scales, out_index,label_names,cls_num,dets);
#else
        // 转为灰度图
        cv::Mat gray;
        cv::cvtColor(infer_data_orig_img, gray, cv::COLOR_RGB2GRAY);

        // 进行人脸检测
        vector<cv::Rect> faces;
        face_cascade.detectMultiScale( gray, faces,
        1.1, 2, 0|cv::CASCADE_SCALE_IMAGE,
        cv::Size(30, 30) );
        
        //Ubuntu18.04 ros-melodic opencv版本冲突问题解决:https://www.cnblogs.com/long5683/p/16060461.html

        Det det;
        for (size_t i = 0; i < faces.size(); i++)
        {
            det.x1 = faces[i].x;
            det.y1 = faces[i].y;
            det.x2 = det.x1+faces[i].width;
            det.y2 = det.y1+faces[i].height;
            det.conf = 1.0;
            det.cls_name = "person";
            det.cls_id = 0;
            det.obj_id = 0;
            dets.push_back(det);
        }
#endif

//auto t2 = std::chrono::system_clock::now();

        ai_msgs::Dets dets_msg;
        //nlohmann::json json_dets;
        for(int i = 0; i < dets.size(); i++)
        {
            sprintf(text, "%s%.0f%%",dets[i].cls_name.c_str(), dets[i].conf * 100);
            //sprintf(text, "%.1f%%",dets[i].cls_name.c_str(), dets[i].conf * 100);
            int x1 = dets[i].x1;
            int y1 = dets[i].y1;
            int x2 = dets[i].x2;
            int y2 = dets[i].y2;
            rectangle(infer_data_orig_img, cv::Point(x1, y1), cv::Point(x2, y2), color_list[dets[i].cls_id%12], 2);
            putText(infer_data_orig_img, text, cv::Point(x1, y1 - 6), cv::FONT_HERSHEY_SIMPLEX, 0.5, color_list[dets[i].cls_id%12],2);


            // nlohmann::json det;
            // det["x1"] = dets[i].x1;
            // det["y1"] = dets[i].y1;
            // det["x2"] = dets[i].x2;
            // det["y2"] = dets[i].y2;
            // det["conf"] = int(dets[i].conf*100);
            // det["cls_name"] = dets[i].cls_name;
            // det["cls_id"] = dets[i].cls_id;
            // det["obj_id"] = dets[i].obj_id;

            // // 将单个目标框信息添加到JSON数组中
            // json_dets.push_back(det);

            ai_msgs::Det det;
            det.x1 = dets[i].x1;
            det.y1 = dets[i].y1;
            det.x2 = dets[i].x2;
            det.y2 = dets[i].y2;
            det.conf = dets[i].conf;
            det.cls_name = dets[i].cls_name;
            det.cls_id = dets[i].cls_id;
            det.obj_id = dets[i].obj_id;
            dets_msg.dets.push_back(det);
        }

        // 将JSON数组转换为字符串
        //std::string json_str = json_dets.dump();


        //ROS_WARN("dets size=%d",dets.size());

        //cv::imshow("img",infer_data_orig_img);
        //cv::waitKey(1);

#if(USE_ARM_LIB==1)
        //5释放输出outputs 必须释放，不然代码运行一会儿会崩溃 因此，后处理时间一定要比推理时间要短，否则队列会有丢弃的可能，会出现outputs没有释放
        ret = rknn_outputs_release(ctx, io_num.n_output, outputs);
#endif

// auto t3 = std::chrono::system_clock::now();

// auto time1 = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
// auto time2 = std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2).count();

//         ROS_WARN_THROTTLE(1,"post=%d ms draw=%d ms process_fps=%.1f",time1,time2,1000.0/(time1+time2));
        
        frame_cnt++;
        if(frame_cnt % 30 == 0)
        {
           gettimeofday(&tv, NULL);
           t = tv.tv_sec + tv.tv_usec/1000000.0;

           fps = 30.0/(t-last_t);
           //ROS_WARN("det publish_fps=%.1f (%.1f ms)",fps,1000.0/fps);
           last_t = t;
        }

        image_msg = cv_bridge::CvImage(infer_data.header, "rgb8", infer_data_orig_img).toImageMsg();//opencv-->ros
        image_pub.publish(image_msg);//发布绘制了检测框的图像

        // std_msgs::String json_msg;
        // json_msg.data = json_str;
        // det_pub.publish(json_msg);//发布json字符串

        dets_msg.header = infer_data.header;//时间戳赋值用于同步订阅
        det_pub.publish(dets_msg);

        //ROS_INFO_THROTTLE(1,"%s",json_str.c_str());

        if(is_offline_image_mode)//离线模式会保存图片
        {
            std::string output_file = offline_output_path + "/" + std::to_string(infer_data.header.seq) + ".jpg";
            cv::imwrite(output_file,infer_data_orig_img);

            ROS_INFO_STREAM( "saved: " << output_file );
        }
    }
}


void RknnRun::run_check_thread()
{
    int last_subscribers = 0;
    while(ros::ok())
    {
        int subscribers = image_pub.getNumSubscribers();
        if(subscribers==0 && last_subscribers>0)
        {
            ROS_INFO("det image subscribers = 0, src sub shutdown");
            image_sub.shutdown();
        }
        else if(subscribers>0 && last_subscribers==0)
        {
            ROS_INFO("det image subscribers > 0, src sub start");
            image_sub = nh.subscribe(sub_image_topic, 10, &RknnRun::sub_image_callback,this);
        }

        last_subscribers = subscribers;

        usleep(100*1000);//100ms
    }
}

