cmake_minimum_required(VERSION 3.0.2)
project(img_encode)

find_package(catkin REQUIRED COMPONENTS
  roscpp
  rospy
  image_transport
  cv_bridge
)

find_package(OpenCV REQUIRED)

catkin_package(
)

include_directories(
  ${catkin_INCLUDE_DIRS}
)

file(GLOB_RECURSE SOURCES src/*.cpp)

add_executable(${PROJECT_NAME}
${SOURCES}
)

message(STATUS "CMAKE_HOST_SYSTEM_PROCESSOR ${CMAKE_HOST_SYSTEM_PROCESSOR}")
if(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "aarch64")
    add_definitions(-DUSE_ARM_LIB=1)
    message(STATUS "define USE_ARM_LIB 1")
    include_directories(
        /usr/include/rga
    )
    target_link_libraries(${PROJECT_NAME}
      ${catkin_LIBRARIES}
      ${OpenCV_LIBS}
      rga
      rknn_api
      rknnrt
      rockchip_mpp
    )
else()
    add_definitions(-DUSE_ARM_LIB=0)
    message(STATUS "define USE_ARM_LIB 0")
    target_link_libraries(${PROJECT_NAME}
       ${catkin_LIBRARIES}
       ${OpenCV_LIBS}
    )
endif()

