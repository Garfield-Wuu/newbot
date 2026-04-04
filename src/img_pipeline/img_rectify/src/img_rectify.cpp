#include "img_rectify.h"

void read_transform_from_file(int i,string &calibration_file,tf::Transform &transform,sensor_msgs::CameraInfo &camera_info_msg,cv::Mat &map1,cv::Mat &map2)
{

    ROS_INFO("open file: %s",calibration_file.c_str());
    
	cv::FileStorage fs(calibration_file, cv::FileStorage::READ);
	
	if (!fs.isOpened())
	{
		ROS_ERROR("Cannot open file calibration file '%s'", calibration_file.c_str());
		ros::shutdown();
		return;
	}
    
    cv::Mat CameraMat,CameraExtrinsicMat,DistCoeff;
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


    // tf::Matrix3x3 rotation(
    //     CameraExtrinsicMat.at<double>(0, 0), CameraExtrinsicMat.at<double>(0, 1), CameraExtrinsicMat.at<double>(0, 2),
    //     CameraExtrinsicMat.at<double>(1, 0), CameraExtrinsicMat.at<double>(1, 1), CameraExtrinsicMat.at<double>(1, 2),
    //     CameraExtrinsicMat.at<double>(2, 0), CameraExtrinsicMat.at<double>(2, 1), CameraExtrinsicMat.at<double>(2, 2)
    // );

    // double roll, pitch, yaw;
    // rotation.getRPY(roll, pitch, yaw);

    // ROS_INFO("CameraExtrinsicMat %d roll=%lf, pitch=%lf, yaw=%lf",i, roll*180.0/M_PI, pitch*180.0/M_PI, yaw*180.0/M_PI);

    // tf::Vector3 translation(
    //     CameraExtrinsicMat.at<double>(0, 3), CameraExtrinsicMat.at<double>(1, 3), CameraExtrinsicMat.at<double>(2, 3)
    // );

    // ROS_INFO("CameraExtrinsicMat %d x=%lf, y=%lf, z=%lf\n",i, CameraExtrinsicMat.at<double>(0, 3), CameraExtrinsicMat.at<double>(1, 3), CameraExtrinsicMat.at<double>(2, 3));

    // transform = tf::Transform(rotation, translation);


    //填写内参K
    memcpy(camera_info_msg.K.data(), CameraMat.data, CameraMat.total()*sizeof(double));

    //填写畸变D
    vector<double> D((double *)DistCoeff.data,(double *)DistCoeff.data+DistCoeff.total());
    camera_info_msg.D = D;

    //填写图像尺寸
    camera_info_msg.height = ImageSize.height;
    camera_info_msg.width = ImageSize.width;

    //填写旋转矩阵R(只有双目才需要有旋转)
    cv::Mat R = cv::Mat::eye(3, 3, CameraMat.type());
    memcpy(camera_info_msg.R.data(), R.data, R.total()*sizeof(double));

    //计算鱼眼模型的新K,用于投影,最后一个参数可以控制图像保留多少，即新内参的焦距大小
    //https://zhuanlan.zhihu.com/p/641968268
    cv::Mat new_K;
    cv::fisheye::estimateNewCameraMatrixForUndistortRectify(CameraMat, DistCoeff, ImageSize, cv::Mat::eye(3, 3, CV_32F), new_K, 0.5);

    //填写投影矩阵P
    cv::Mat P;
    cv::Mat zeros = cv::Mat::zeros(3, 1, CameraMat.type());
    cv::hconcat(new_K, zeros, P);//水平拼接mat和zeros，得到一个3行4列的新矩阵
    memcpy(camera_info_msg.P.data(), P.data, P.total()*sizeof(double));

    //填写畸变模型
    camera_info_msg.distortion_model = "equidistant";//equidistant就是fisheye Kannala-Brandt

    //计算map1,map2
    fisheye::initUndistortRectifyMap(CameraMat, DistCoeff, Mat::eye(3, 3, CV_32F), new_K, ImageSize, CV_16SC2, map1, map2);
}

//畸变模型的选择：
//https://github.com/ros-perception/image_pipeline/blob/noetic/camera_calibration/src/camera_calibration/calibrator.py
// def _get_dist_model(dist_params, cam_model):
//     # Select dist model
//     if CAMERA_MODEL.PINHOLE == cam_model:
//         if dist_params.size > 5:
//             dist_model = "rational_polynomial"
//         else:
//             dist_model = "plumb_bob"
//     elif CAMERA_MODEL.FISHEYE == cam_model:
//         dist_model = "equidistant"
//     else:
//         dist_model = "unknown"
//     return dist_model



ImgPub::ImgPub():nh("~")
{
    string sub_image_topic,pub_image_topic;
    string calibration_file[CAM_NUM];

    nh.param<string>("camera_name", camera_name[0], "camera0");

    nh.param<string>("sub_image_topic", sub_image_topic, "/camera/image_raw");

    nh.param<string>("pub_image_topic", pub_image_topic, "/camera/image_rect");

    nh.param<string>("calibration_file0", calibration_file[0], "fisheye.yaml");

    nh.param<bool>("pub_camerainfo", pub_camerainfo, "true");
    
    nh.param<bool>("pub_image_rect", pub_image_rect, "false");
    
    
    for(int i=0;i<CAM_NUM;i++)
    {
        //从文件读取矫正信息
        read_transform_from_file(i,calibration_file[i],transform[i],camera_info_msg[i],map1,map2);
        camera_info_pub[i] = nh.advertise<sensor_msgs::CameraInfo>("/"+camera_name[i]+"/camera_info", 10, true);
    }

    image_sub = nh.subscribe(sub_image_topic, 10, &ImgPub::sub_image_callback, this);

    if(pub_image_rect)
        undistort_pub = nh.advertise<sensor_msgs::Image>(pub_image_topic, 10);
}

void ImgPub::sub_image_callback(const sensor_msgs::Image &msg)
{
    if(pub_image_rect)
    {
        cv::Mat image(msg.height, msg.width, CV_8UC3, (char *)msg.data.data());

        Mat undistorted_image;
        remap(image, undistorted_image, map1, map2, INTER_LINEAR, BORDER_CONSTANT);

        memcpy((char *)msg.data.data(), undistorted_image.data, msg.data.size());
        undistort_pub.publish(msg);
    }
    
    //相机坐标(子)到雷达坐标(父)的转换 例如相机坐标z=0 转换到雷达中坐标为z=-0.06
    for(int i=0;i<CAM_NUM;i++)
    {
        //if(pub_tf)
	    //broadcaster[i].sendTransform(tf::StampedTransform(transform[i], msg.header.stamp, target_frame, camera_name[i]));//target是父坐标 camera是子坐标
        
        //填写header信息，时间戳会动态变化
        camera_info_msg[i].header = msg.header;

        if(pub_camerainfo)
            camera_info_pub[i].publish(camera_info_msg[i]);
    }

    ROS_INFO_ONCE("camera info publish ok");

}

int main(int argc, char** argv)
{
    ros::init(argc, argv, "info_pub");

    ImgPub img_rect;

    ros::spin();
    ros::shutdown();

    return 0;
}
