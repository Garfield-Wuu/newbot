cmake_minimum_required(VERSION 3.0.2)
project(object_track)

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
CATKIN_DEPENDS ai_msgs  # 依赖的消息包
)

include_directories(
  ${catkin_INCLUDE_DIRS}
  src
)

file(GLOB_RECURSE SOURCES src/*.cpp)


add_executable(${PROJECT_NAME}
${SOURCES}
)

add_dependencies(${PROJECT_NAME} ai_msgs_generate_messages_cpp)

target_link_libraries(${PROJECT_NAME}
   ${catkin_LIBRARIES}
   ${OpenCV_LIBS}
)


