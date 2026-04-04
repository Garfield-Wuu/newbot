#!/bin/bash
#1.挂载NFS
#注意事项：这里修改为自己电脑的IP地址和NFS路径
#sudo mount -t nfs -o nolock 192.168.xx.xx:/home/xxxx/xxxx/nfs ~/nfs
sudo mount -t nfs -o nolock 192.168.5.119:/home/ubuntu/workspace/nfs ~/nfs

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
#sync #把所有数据从内存缓冲区同步到硬盘


