cmake_minimum_required(VERSION 3.0.2)
project(img_rectify)

set(CMAKE_CXX_STANDARD 11)
## Compile as C++11, supported in ROS Kinetic and newer
#add_compile_options(-std=c++11)

## Find catkin macros and libraries
## if COMPONENTS list like find_package(catkin REQUIRED COMPONENTS xyz)
## is used, also find other catkin packages

#link_directories(/usr/lib/aarch64-linux-gnu)

#SET(OpenCV_DIR /usr/local/lib/cmake/opencv4/OpenCVConfig.cmake)
#SET(OpenCV_DIR /usr/share/OpenCV/OpenCVConfig.cmake)
#SET(OpenCV_DIR /usr/lib/aarch64-linux-gnu/cmake/opencv3/OpenCV3Config.cmake)
find_package(OpenCV REQUIRED)


#set(OpenCV_LIBS opencv_core;opencv_highgui;opencv_imgcodecs;opencv_imgproc;opencv_videoio)
#set(OpenCV_INCLUDE_DIRS /usr/include/opencv)

find_package(catkin REQUIRED COMPONENTS
  roscpp
  std_msgs
  image_transport
  #cv_bridge #有这行之后调用opencv读取yaml文件，程序就会崩溃！！！必须去掉这一行才能调用cv::FileStorage。。。！！！
  camera_info_manager
  tf
)


catkin_package(

)


include_directories(
  ${catkin_INCLUDE_DIRS}
  ${OpenCV_INCLUDE_DIRS}
)


add_executable(${PROJECT_NAME}
src/img_rectify.cpp
)


target_link_libraries(${PROJECT_NAME}
  ${catkin_LIBRARIES}
  ${OpenCV_LIBS}
)



