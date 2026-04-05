#!/usr/bin/env bash
# 供 systemd 调用：先加载 ROS 与工作空间，再启动 rosbridge（Foxglove 选 Rosbridge → ws://本机IP:9090）
set -euo pipefail
WS_ROOT="${WS_ROOT:-/home/orangepi/newbot_ws}"
ROS_DISTRO="${ROS_DISTRO:-noetic}"
# 可选：sudo tee /etc/default/rosbridge <<'EOF'
# ROS_MASTER_URI=http://192.168.x.x:11311
# ROS_IP=192.168.x.x
# EOF
if [[ -f /etc/default/rosbridge ]]; then
  # shellcheck source=/dev/null
  source /etc/default/rosbridge
fi
export ROS_MASTER_URI="${ROS_MASTER_URI:-http://127.0.0.1:11311}"
if [[ -z "${ROS_IP:-}" ]]; then
  ROS_IP="$(hostname -I 2>/dev/null | tr ' ' '\n' | grep -E '^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$' | head -1 || true)"
fi
if [[ -n "${ROS_IP:-}" ]]; then
  export ROS_IP
  export ROS_HOSTNAME="${ROS_HOSTNAME:-$ROS_IP}"
fi
source "/opt/ros/${ROS_DISTRO}/setup.bash"
source "${WS_ROOT}/devel/setup.bash"
# 勿抢在 rc.local/start.sh 之前自起 roscore：等已有 master（与 all.launch 一致）
echo "rosbridge: waiting for roscore at ${ROS_MASTER_URI} ..."
for _ in $(seq 1 90); do
  if rostopic list >/dev/null 2>&1; then
    echo "rosbridge: roscore ok"
    exec roslaunch rosbridge_server rosbridge_websocket.launch port:=9090 max_message_size:=20000000
  fi
  sleep 2
done
echo "rosbridge: timeout, no roscore at ${ROS_MASTER_URI}" >&2
exit 1
