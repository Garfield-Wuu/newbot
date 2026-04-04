cmake_minimum_required(VERSION 3.0.2)
project(wakeup_process)

find_package(catkin REQUIRED COMPONENTS
  roscpp
  rospy
  image_transport
  cv_bridge
  tf
  message_filters
  ai_msgs
)

find_package(OpenCV REQUIRED)

catkin_package(
)

include_directories(
  ${catkin_INCLUDE_DIRS}
  src
)

set(SOURCES
  src/wakeup_process.cpp
  src/asr.cpp
)

add_executable(${PROJECT_NAME}
  ${SOURCES}
)

target_link_libraries(${PROJECT_NAME}
   ${catkin_LIBRARIES}
   ${OpenCV_LIBS}
)


