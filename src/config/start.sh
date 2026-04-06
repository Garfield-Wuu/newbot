# 检查是否保存有WIFI连接
wifi_ssid=$(nmcli connection show | grep wifi | awk '{print $1}')
if [ -z "$wifi_ssid" ]; then
    echo "系统没有存储任何WIFI密码"
    #host_ip=$(hostname -I | awk '{print $1}')
    host_ip=$(hostname -I | awk '{print $1}' | grep -E '^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+') #只采用IPv4地址
else
    echo "系统中保存有WIFI密码"
    # 尝试获取IP地址15秒
    for cnt in $(seq 1 15); do
        #host_ip=$(hostname -I | awk '{print $1}')
        host_ip=$(hostname -I | awk '{print $1}' | grep -E '^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+') #只采用IPv4地址
        if [ -z "$host_ip" ]; then
            echo "尝试第$cnt次获取IP地址失败，等待1秒后重试"  
            sleep 1
        else
            echo "IP地址已成功获取: $host_ip"
            play /home/orangepi/newbot_ws/src/audio/scripts/sound/sta.mp3
            break
        fi
    done
fi

# 如果IP地址为空，则开启AP模式
if [ -z "$host_ip" ]; then
    echo "开启AP模式..."
    echo "orangepi" | sudo -S create_ap --no-virt -m nat wlan0 eth0 orangepi orangepi &
    host_ip="192.168.12.1"  #AP模式的默认IP地址是192.168.12.1  
    echo "AP模式已开启，IP地址: $host_ip"
    play /home/orangepi/newbot_ws/src/audio/scripts/sound/ap.mp3
fi


export ROS_IP=$host_ip
export ROS_HOSTNAME=$host_ip
export ROS_MASTER_URI=http://$host_ip:11311
#注意事项：这里如果雷达顶部有两颗螺丝，则设置为YDLIDAR；如果雷达顶部有三颗螺丝，则设置为M1C1_MINI；如果雷达接到USB串口板，则写入M1C1_MINI_TTYUSB
#如果雷达顶部有小红点，则设置为LD14
#这个配置要在.bashrc中也要配置一遍，并且配置要一致
export LIDAR_TYPE=YDLIDAR
#export LIDAR_TYPE=M1C1_MINI_TTYUSB
#export LIDAR_TYPE=M1C1_MINI
#export LIDAR_TYPE=LD14

# 加载 API 密钥（独立文件，不受 .bashrc 交互式守卫影响）
[ -f /home/orangepi/.robot_env ] && source /home/orangepi/.robot_env

source /opt/ros/noetic/setup.sh
source /home/orangepi/newbot_ws/devel/setup.sh
sleep 3 #等待3秒，防止网络不稳定引起的ROS启动错误

#为解决重启后声卡可能丢失的问题，重新打开一下pulseaudio
systemctl --user stop pulseaudio.socket
systemctl --user stop pulseaudio.service
systemctl --user start pulseaudio.socket
systemctl --user start pulseaudio.service

#设置默认的麦克风设备为USB麦克风
pactl set-default-source "alsa_input.usb-C-Media_Electronics_Inc._USB_PnP_Sound_Device-00.mono-fallback"
#麦克风接收增益调到100%，范围0~16
amixer -c 2 sset Mic 16

#启动all.launch
play /home/orangepi/newbot_ws/src/audio/scripts/sound/launch.mp3
roslaunch pkg_launch all.launch #如果启动失败，请查看~/.ros/log/latest/*.log，~/.ros/start.log等log文件








