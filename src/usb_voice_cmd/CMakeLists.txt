cmake_minimum_required(VERSION 3.0.2)
project(usb_voice_cmd)

find_package(catkin REQUIRED COMPONENTS rospy std_msgs)

catkin_package()

catkin_install_python(PROGRAMS
  scripts/usb_voice_cmd_node.py
  scripts/download_vosk_model_cn.py
  DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
)

install(DIRECTORY launch doc
  DESTINATION ${CATKIN_PACKAGE_SHARE_DESTINATION}
)
