cmake_minimum_required(VERSION 3.0.2)
project(m1c1_mini)

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

file(GLOB_RECURSE SOURCES src/*.cpp)


add_executable(${PROJECT_NAME}
${SOURCES}
)

target_link_libraries(${PROJECT_NAME}
   ${catkin_LIBRARIES}
   ${OpenCV_LIBS}
)


