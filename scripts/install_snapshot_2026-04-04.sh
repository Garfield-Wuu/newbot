#!/bin/bash
# 快照：与 2026-04-04 时 /home/orangepi/nfs/install.sh 内容一致，便于审计与对比。
# 新部署请优先使用同目录下带变量说明的 install_orangepi_board.sh
# shellcheck disable=SC2148
#1.安装和挂载NFS
sudo apt update
sudo apt install -y nfs-common avahi-daemon
mkdir ~/nfs
#注意事项：这里修改为自己电脑的IP地址和NFS路径
#sudo mount -t nfs -o nolock 192.168.xx.xx:/home/xxxx/xxxx/nfs ~/nfs
sudo mount -t nfs -o nolock 192.168.31.142:/home/legion/orangepi_ws/nfs ~/nfs

#2.配置avahi-daemon服务
#注意事项：一个局域网不能同时有多个orangepi.local设备
sudo sed -i 's/#host-name=foo/host-name=orangepi/' /etc/avahi/avahi-daemon.conf
sudo service avahi-daemon restart

#3.安装和配置ROS
#sudo sh -c 'echo "deb http://packages.ros.org/ros/ubuntu $(lsb_release -sc) main" > /etc/apt/sources.list.d/ros-latest.list' #官方源网速较慢
sudo sh -c 'echo "deb http://mirrors.ustc.edu.cn/ros/ubuntu $(lsb_release -sc) main" > /etc/apt/sources.list.d/ros-latest.list' #中科大源
sudo apt-key adv --keyserver 'hkp://keyserver.ubuntu.com:80' --recv-key C1CF6E31E6BADE8868B172B4F42ED6FBAB17C654
sudo apt update
#安装ros完整版，等待数分钟完成
sudo apt install -y ros-noetic-desktop-full

if ! grep -q "source /opt/ros/noetic/setup.bash" ~/.bashrc; then
	echo "source /opt/ros/noetic/setup.bash" >> ~/.bashrc
fi
if ! grep -q "HOST_IP=\$(hostname -I | awk '{print \$1}')" ~/.bashrc; then
	echo "HOST_IP=\$(hostname -I | awk '{print \$1}')" >> ~/.bashrc
fi
if ! grep -q "export ROS_IP=\$HOST_IP" ~/.bashrc; then
	echo "export ROS_IP=\$HOST_IP" >> ~/.bashrc
fi
if ! grep -q "export ROS_HOSTNAME=\$HOST_IP" ~/.bashrc; then
	echo "export ROS_HOSTNAME=\$HOST_IP" >> ~/.bashrc
fi
if ! grep -q "export ROS_MASTER_URI=http://\$HOST_IP:11311" ~/.bashrc; then
	echo "export ROS_MASTER_URI=http://\$HOST_IP:11311" >> ~/.bashrc
fi
#注意事项：这里如果雷达顶部有两颗螺丝，则写入YDLIDAR；如果雷达顶部有三颗螺丝，则写入M1C1_MINI
#这个配置要在start.sh中也要配置一遍，并且配置要一致
#要么写入YDLIDAR
if ! grep -q "export LIDAR_TYPE=YDLIDAR" ~/.bashrc; then
	echo "export LIDAR_TYPE=YDLIDAR" >> ~/.bashrc
fi
#要么写入M1C1_MINI
#if ! grep -q "export LIDAR_TYPE=M1C1_MINI" ~/.bashrc; then
#	echo "export LIDAR_TYPE=M1C1_MINI" >> ~/.bashrc
#fi

source ~/.bashrc

#4.安装常用的ROS包
sudo apt install -y ros-noetic-teleop-twist-keyboard ros-noetic-move-base-msgs ros-noetic-move-base ros-noetic-map-server ros-noetic-base-local-planner ros-noetic-dwa-local-planner ros-noetic-teb-local-planner ros-noetic-global-planner ros-noetic-gmapping ros-noetic-amcl libudev-dev

#5.安装EAI YDLidar库
#cd ~/nfs/xxx/newbot_ws/src/lidar_sensors/ydlidar/YDLidar-SDK #注意事项：修改为自己的NFS目录
cd ~/nfs/newbot_ws/src/lidar_sensors/ydlidar/YDLidar-SDK
#mkdir build 
cd build
#cmake ..
#make -j #注意：如果已经编译过了不用再编译，编译报错需要删除build文件夹和CMakeCache.txt
sudo make install #编译过了只需安装，如果报错需要重新编译
sync #把所有数据从内存缓冲区同步到硬盘

#6.安装常用的python包
export PATH=$PATH:/home/orangepi/.local/bin #防止安装python包时候的WARNING: The script read_zbar is installed in '/home/orangepi/.local/bin' which is not on PATH.
sudo apt install -y python3-pip python3-websocket python3-pyaudio libsox-fmt-mp3 libatlas-base-dev espeak sox #后面几个是音频相关的依赖包
pip config set global.index-url https://pypi.mirrors.ustc.edu.cn/simple
pip install -U pip

pip install opencv-python #这个报错不用管:ERROR: opencv-python 4.10.0.84 has requirement numpy>=1.19.3; but you'll have numpy 1.17.4 which is incompatible.
pip install sherpa_onnx

pip install pulsectl
pip install baidu-aip
pip install edge_tts
pip install pyttsx3
pip install pyzbar

pip install gpio
pip install python-periphery

pip install sounddevice
pip install httpx
pip install pycryptodome #对应代码:from Crypto.Cipher import AES
pip install pytz

#7.配置音频设备和音量
#这两个命令查看默认设备，前面有星号的代表默认：
pacmd list-sinks | grep -e 'index:' -e 'name:'
pacmd list-sources | grep -e 'index:' -e 'name:'
#如果列表里根本没有USB设备，可以尝试重新安装pulseaudio，可能重装两遍才行
sudo apt remove -y pulseaudio
sudo apt install -y pulseaudio
sudo apt remove -y pulseaudio
sudo apt install -y pulseaudio
pacmd list-sinks | grep -e 'index:' -e 'name:'
pacmd list-sources | grep -e 'index:' -e 'name:'
#这个命令配置在重启后可能会变化，加入启动脚本
pactl set-default-sink "alsa_output.platform-rk809-sound.stereo-fallback"
pactl set-default-source "alsa_input.usb-C-Media_Electronics_Inc._USB_PnP_Sound_Device-00.mono-fallback"
pacmd list-sinks | grep -e 'index:' -e 'name:'
pacmd list-sources | grep -e 'index:' -e 'name:'
#设置喇叭音量
pactl set-sink-volume "alsa_output.platform-rk809-sound.stereo-fallback" 100%
#设置USB麦克风的捕获强度，范围0~16
amixer -c 2 sset Mic 16

#8.拷贝应用程序和启动脚本，解压和编译
#cd ~/nfs/xxx #注意事项：修改为自己的NFS目录
cd ~/nfs/newbot/newbot_ws_v1.1
cp -rv newbot_ws.zip ~ #拷贝newbot_ws.zip压缩包到板子根目录，注意提前做好压缩包放在NFS目录中
sudo cp -rv newbot_ws/src/config/rc.local  /etc
sudo cp -rv newbot_ws/src/config/*v2*      /boot/dtb/rockchip
sync #把所有数据从内存缓冲区同步到硬盘

#解压文件
cd ~
rm -r newbot_ws #删除原有程序，注意备份防止误删
unzip newbot_ws.zip
sync #把所有数据从内存缓冲区同步到硬盘

#如果没有正确编译按照如下命令编译
#cd ~/newbot_ws
#source /opt/ros/noetic/setup.bash
#rosnode kill -a
#killall rosmaster #编译之前要关闭ROS程序，防止内存不够用(c++: fatal error: Killed signal terminated program cc1plus)，如果内存还是不够编译的时候输入catkin_make -j1或catkin_make -j2
#rm devel build #如果已经编译正确则不用删除和编译
#catkin_make --pkg ai_msgs && catkin_make
#catkin_make #Cmakelist.txt里面加了add_dependencies(${PROJECT_NAME} ai_msgs_generate_messages_cpp)之后只需直接输入catkin_make，不用先编译ai_msgs
#cp -rv build/ devel/ ~/nfs/newbot/newbot_ws_v1.1/newbot_ws #把结果复制到电脑一份方便烧录下一个板子 #注意事项：修改为自己的NFS目录
#cp -rv build/ devel/ ~/nfs/newbot_ws
#sync #把所有数据从内存缓冲区同步到硬盘

#9.配置SPI3和UART2,9使能，注意：UART2使能之后，串口调试功能会失效，只能通过网络或屏幕连接
if ! grep -q "overlays=spi3-m0-cs0-spidev uart2-m0 uart9-m2" /boot/orangepiEnv.txt; then
    sudo sh -c 'echo "overlays=spi3-m0-cs0-spidev uart2-m0 uart9-m2" >> /boot/orangepiEnv.txt'
fi
#配置完SPI和UART之后要重启生效
#不要直接拔电，可能造成文件拷贝不完整，要用命令行重启，或者用sync将缓存同步到硬盘
sync #把所有数据从内存缓冲区同步到硬盘
reboot
