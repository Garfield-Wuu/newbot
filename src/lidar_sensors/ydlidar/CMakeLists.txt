cmake_minimum_required(VERSION 2.8.3)
project(ydlidar)

add_definitions(-std=c++11) # Use C++11

find_package(catkin REQUIRED COMPONENTS
  rosconsole
  roscpp
  sensor_msgs
)

#add_subdirectory(sdk)

#find ydlidar_sdk package
find_package(ydlidar_sdk REQUIRED)



catkin_package()


#link library directories
link_directories(${YDLIDAR_SDK_LIBRARY_DIRS})

include_directories(
  ${catkin_INCLUDE_DIRS}
  ${PROJECT_SOURCE_DIR}/src
  ${YDLIDAR_SDK_INCLUDE_DIRS}

)

add_executable(ydlidar_node src/ydlidar_node.cpp  )
add_executable(ydlidar_client src/ydlidar_client.cpp)

target_link_libraries(ydlidar_node
   ${catkin_LIBRARIES} 
   ${YDLIDAR_SDK_LIBRARIES}
 )
target_link_libraries(ydlidar_client
   ${catkin_LIBRARIES} 
   ${YDLIDAR_SDK_LIBRARIES}
 )

install(TARGETS ydlidar_node ydlidar_client
  ARCHIVE DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
  LIBRARY DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
  RUNTIME DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
)


install(DIRECTORY launch startup
  DESTINATION ${CATKIN_PACKAGE_SHARE_DESTINATION}
  USE_SOURCE_PERMISSIONS
)


