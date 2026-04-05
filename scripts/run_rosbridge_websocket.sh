#!/usr/bin/env bash
# 供 systemd 调用：先加载 ROS 与工作空间，再启动 rosbridge（Foxglove 选 Rosbridge → ws://本机IP:9090）
set -euo pipefail
WS_ROOT="${WS_ROOT:-/home/orangepi/newbot_ws}"
ROS_DISTRO="${ROS_DISTRO:-noetic}"
export ROS_MASTER_URI="${ROS_MASTER_URI:-http://127.0.0.1:11311}"
export ROS_HOSTNAME="${ROS_HOSTNAME:-127.0.0.1}"
source "/opt/ros/${ROS_DISTRO}/setup.bash"
source "${WS_ROOT}/devel/setup.bash"
exec roslaunch rosbridge_server rosbridge_websocket.launch port:=9090 max_message_size:=20000000
