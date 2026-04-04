cmake_minimum_required(VERSION 3.0.2)
project(move_action)

find_package(catkin REQUIRED COMPONENTS
  tf
  actionlib
  actionlib_msgs
  roscpp
  std_msgs
  nav_msgs
  rostime
  sensor_msgs 
)

find_package(catkin REQUIRED genmsg actionlib_msgs actionlib tf)
add_action_files(DIRECTORY action FILES DoMove.action)
generate_messages(DEPENDENCIES actionlib_msgs)

catkin_package(
)

catkin_package(
LIBRARIES learning_tf
)

include_directories(
  ${catkin_INCLUDE_DIRS}
)

add_executable(move_client src/move_client.cpp)
target_link_libraries(move_client ${catkin_LIBRARIES})
add_dependencies(move_client ${${PROJECT_NAME}_EXPORTED_TARGETS})

add_executable(move_client_cmd src/move_client_cmd.cpp)
target_link_libraries(move_client_cmd ${catkin_LIBRARIES})
add_dependencies(move_client_cmd ${${PROJECT_NAME}_EXPORTED_TARGETS})

add_executable(move_client_find_charger src/move_client_find_charger.cpp)
target_link_libraries( move_client_find_charger ${catkin_LIBRARIES})
add_dependencies(move_client_find_charger ${${PROJECT_NAME}_EXPORTED_TARGETS})

add_executable(move_server src/move_server.cpp)
target_link_libraries( move_server ${catkin_LIBRARIES})
add_dependencies(move_server ${${PROJECT_NAME}_EXPORTED_TARGETS})

