#!/bin/bash
# newbot 香橙派（ROS Noetic）一键部署脚本 — 整理版
# 基于 2026-04-04 的 nfs/install.sh，改为顶部集中配置；细节仍以官方文档为准。
# 官方开发文档: https://newbot.readthedocs.io/zh-cn/latest/3_how_to_develop.html
#
# 用法: 编辑下方「配置区」后执行
#   bash ~/newbot_ws/scripts/install_orangepi_board.sh
#
# 说明:
# - LIDAR_TYPE 与 src/config/start.sh、~/.bashrc 须一致（两颗螺丝 YDLIDAR / 三颗 M1C1_MINI 等）。
# - 离线 ASR 模型目录 src/audio/scripts/model/ 体积大，本仓库 .gitignore 已排除；请从网盘/NFS/备份拷回。
# - 讯飞大模型: 在 ~/.bashrc 或 systemd 环境中 export XUNFEI_APPID / XUNFEI_APIKEY / XUNFEI_APISECRET；
#   勿在 audio/main.py 中硬编码（已移除旧逻辑）。

set -u

# ========================= 配置区（按你的电脑/NFS 修改）=========================
# NFS 服务端：开发机 IP + export 路径（示例来自原 nfs/install.sh，请改成你的）
NFS_HOST="192.168.31.142"
NFS_EXPORT="/home/legion/orangepi_ws/nfs"
NFS_MOUNT="$HOME/nfs"

# YDLidar SDK：在 NFS 上已存在的源码树中的 build 目录（先在本机 nfs 路径下编译好 SDK，再 sudo make install）
YDLIDAR_SDK_BUILD="${NFS_MOUNT}/newbot_ws/src/lidar_sensors/ydlidar/YDLidar-SDK/build"

# 含 newbot_ws.zip 与解压用 newbot_ws 子目录的包路径（原脚本为 ~/nfs/newbot/newbot_ws_v1.1）
NEWBOT_RELEASE_DIR="${NFS_MOUNT}/newbot/newbot_ws_v1.1"

# 雷达类型三选一（与官方文档一致）: YDLIDAR | M1C1_MINI | M1C1_MINI_TTYUSB
LIDAR_TYPE="YDLIDAR"

# 可选：安装 Rosbridge，便于电脑 Foxglove Studio 选「Rosbridge」连接 ws://板子IP:9090
INSTALL_ROSBRIDGE_SUITE="${INSTALL_ROSBRIDGE_SUITE:-1}"
# =============================================================================

echo "[1/9] NFS 客户端与挂载..."
sudo apt update
sudo apt install -y nfs-common avahi-daemon
mkdir -p "${NFS_MOUNT}"
# shellcheck disable=SC2024
if ! mountpoint -q "${NFS_MOUNT}"; then
  sudo mount -t nfs -o nolock "${NFS_HOST}:${NFS_EXPORT}" "${NFS_MOUNT}"
else
  echo "  ${NFS_MOUNT} 已挂载，跳过 mount"
fi

echo "[2/9] avahi-daemon（勿多机同名 orangepi.local）..."
sudo sed -i 's/#host-name=foo/host-name=orangepi/' /etc/avahi/avahi-daemon.conf
sudo service avahi-daemon restart

echo "[3/9] ROS Noetic..."
sudo sh -c 'echo "deb http://mirrors.ustc.edu.cn/ros/ubuntu $(lsb_release -sc) main" > /etc/apt/sources.list.d/ros-latest.list'
sudo apt-key adv --keyserver 'hkp://keyserver.ubuntu.com:80' --recv-key C1CF6E31E6BADE8868B172B4F42ED6FBAB17C654
sudo apt update
sudo apt install -y ros-noetic-desktop-full

append_bashrc_line() {
  local line="$1"
  grep -qxF "${line}" ~/.bashrc 2>/dev/null || echo "${line}" >> ~/.bashrc
}

append_bashrc_line "source /opt/ros/noetic/setup.bash"
append_bashrc_line "HOST_IP=\$(hostname -I | awk '{print \$1}')"
append_bashrc_line "export ROS_IP=\$HOST_IP"
append_bashrc_line "export ROS_HOSTNAME=\$HOST_IP"
append_bashrc_line "export ROS_MASTER_URI=http://\$HOST_IP:11311"

# 仅写入选定的一种 LIDAR_TYPE
if [[ "${LIDAR_TYPE}" == "YDLIDAR" ]]; then
  append_bashrc_line "export LIDAR_TYPE=YDLIDAR"
elif [[ "${LIDAR_TYPE}" == "M1C1_MINI" ]]; then
  append_bashrc_line "export LIDAR_TYPE=M1C1_MINI"
elif [[ "${LIDAR_TYPE}" == "M1C1_MINI_TTYUSB" ]]; then
  append_bashrc_line "export LIDAR_TYPE=M1C1_MINI_TTYUSB"
else
  echo "错误: 未知 LIDAR_TYPE=${LIDAR_TYPE}" >&2
  exit 1
fi

# shellcheck source=/dev/null
source ~/.bashrc

echo "[4/9] 常用 ROS 包..."
sudo apt install -y \
  ros-noetic-teleop-twist-keyboard ros-noetic-move-base-msgs ros-noetic-move-base \
  ros-noetic-map-server ros-noetic-base-local-planner ros-noetic-dwa-local-planner \
  ros-noetic-teb-local-planner ros-noetic-global-planner ros-noetic-gmapping \
  ros-noetic-amcl libudev-dev

if [[ "${INSTALL_ROSBRIDGE_SUITE}" == "1" ]]; then
  sudo apt install -y ros-noetic-rosbridge-suite || true
fi

echo "[5/9] YDLidar SDK install（需已存在 build）..."
if [[ -d "${YDLIDAR_SDK_BUILD}" ]]; then
  ( cd "${YDLIDAR_SDK_BUILD}" && sudo make install )
else
  echo "  警告: 未找到 ${YDLIDAR_SDK_BUILD}，请改配置或先在 NFS 上编译 SDK"
fi
sync

echo "[6/9] Python 依赖..."
export PATH="$PATH:/home/orangepi/.local/bin"
sudo apt install -y python3-pip python3-websocket python3-pyaudio libsox-fmt-mp3 libatlas-base-dev espeak sox
pip config set global.index-url https://pypi.mirrors.ustc.edu.cn/simple
pip install -U pip
pip install opencv-python sherpa_onnx pulsectl baidu-aip edge_tts pyttsx3 pyzbar \
  gpio python-periphery sounddevice httpx pycryptodome pytz

echo "[7/9] PulseAudio 默认设备（与 main.py/start 中 USB 麦一致，可按 pacmd 列表改）..."
sudo apt remove -y pulseaudio || true
sudo apt install -y pulseaudio || true
sudo apt remove -y pulseaudio || true
sudo apt install -y pulseaudio
pactl set-default-sink "alsa_output.platform-rk809-sound.stereo-fallback" || true
pactl set-default-source "alsa_input.usb-C-Media_Electronics_Inc._USB_PnP_Sound_Device-00.mono-fallback" || true
pactl set-sink-volume "alsa_output.platform-rk809-sound.stereo-fallback" 100% || true
amixer -c 2 sset Mic 16 || true

echo "[8/9] 拷贝工程 zip、rc.local、dtb..."
if [[ ! -d "${NEWBOT_RELEASE_DIR}" ]]; then
  echo "错误: 未找到 ${NEWBOT_RELEASE_DIR}" >&2
  exit 1
fi
cd "${NEWBOT_RELEASE_DIR}" || exit 1
cp -rv newbot_ws.zip "${HOME}/"
sudo cp -rv newbot_ws/src/config/rc.local /etc/
sudo cp -rv newbot_ws/src/config/*v2* /boot/dtb/rockchip/
sync

cd "${HOME}" || exit 1
if [[ -d newbot_ws ]]; then
  echo "  将删除 ~/newbot_ws ，请确认已备份！10 秒内 Ctrl+C 取消"
  sleep 10
  rm -rf newbot_ws
fi
unzip -o newbot_ws.zip
sync

echo "[9/9] 设备树 overlay（UART2 使能后串口调试可能不可用）..."
if ! grep -q "overlays=spi3-m0-cs0-spidev uart2-m0 uart9-m2" /boot/orangepiEnv.txt; then
  sudo sh -c 'echo "overlays=spi3-m0-cs0-spidev uart2-m0 uart9-m2" >> /boot/orangepiEnv.txt'
fi

echo ""
echo "完成。请在本机执行编译（关闭 ROS 后再编）:"
echo "  killall rosmaster 2>/dev/null || true"
echo "  cd ~/newbot_ws && source /opt/ros/noetic/setup.bash && catkin_make -j2"
echo "将 ASR 模型拷回: src/audio/scripts/model/（若使用离线识别）"
echo "配置讯飞: ~/.bashrc 增加 XUNFEI_APPID / XUNFEI_APIKEY / XUNFEI_APISECRET"
echo "同步 start.sh 与 ~/.bashrc 中的 LIDAR_TYPE=${LIDAR_TYPE}"
sync
echo "即将 reboot ..."
sleep 3
sudo reboot
