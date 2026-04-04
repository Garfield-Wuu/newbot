#!/usr/bin/env python3
#coding=utf-8
import os
import music
# python一开始运行则会播放提示音
current_dir = os.path.dirname(os.path.realpath(__file__))
started_file_name = os.path.join(current_dir, "sound", "start.wav")
music.play_music(started_file_name)

import sys
sys.path.append("/opt/ros/melodic/lib/python2.7/dist-packages")
sys.path.append("/opt/ros/noetic/lib/python3/dist-packages")
sys.path.append("/home/orangepi/.local/lib/python3.8/site-packages")
import signal
import re

import rospy
from std_msgs.msg import String
from std_msgs.msg import Bool
from std_msgs.msg import Int16MultiArray
from geometry_msgs.msg import Twist
from sensor_msgs.msg import CompressedImage
from sensor_msgs.msg import Image

import numpy as np
import random
import time
import queue
import cv2
from PIL import Image as PILImage #改为PILImage防止和ros Image名字冲突
from PIL import ImageDraw, ImageFont
import threading

from api import asr
from api import llm
from api import tts
from api import text2image
from api import image2text
from api import mail
from api import qr_code
from api import cloud_music
from api import weather
import emoji
import record
import network
import yaml_cfg

app_version = "1.1.0" #定义软件版本


class Logger(object):
    def __init__(self, filename="log.txt"):
        filename = os.path.join(os.path.expanduser('~'), ".ros", filename) #log放到~/.ros目录下
        self.terminal = sys.stdout
        self.log_file = open(filename, "w+") #覆盖上次开机的log,防止log文件过大

    def write(self, message):
        self.terminal.write(message)
        self.log_file.write(message)
        self.flush() #调用flush立即写入，相当于printf(flush=True)，默认是False

    def flush(self):
        self.terminal.flush()
        self.log_file.flush()

def decode_encode_jpeg(jpeg_data, calibration_file):
    # 读取JPEG数据
    image = cv2.imdecode(np.frombuffer(jpeg_data, np.uint8), -1)
    
    # 创建FileStorage对象
    fs = cv2.FileStorage(calibration_file, cv2.FILE_STORAGE_READ)
    
    # 读取相机内参和畸变系数
    camera_matrix = fs.getNode("CameraMat").mat()
    dist_coeffs = fs.getNode("DistCoeff").mat()
    
    # 关闭FileStorage对象
    fs.release()
    
    h,w,c = image.shape
    # 鱼眼矫正
    map1, map2 = cv2.fisheye.initUndistortRectifyMap(camera_matrix, dist_coeffs, np.eye(3), camera_matrix, (w,h), cv2.CV_16SC2)
    undistorted_image = cv2.remap(image, map1, map2, interpolation=cv2.INTER_CUBIC, borderMode=cv2.BORDER_CONSTANT)

    undistorted_image = undistorted_image[0:0+720, 160:160+960]

    # 将OpenCV图像编码为JPEG数据
    success, encoded_image = cv2.imencode('.jpg', undistorted_image, [cv2.IMWRITE_JPEG_QUALITY, 100])
    jpeg_data = encoded_image.tobytes()
    
    return jpeg_data


def get_cpu_temperature():
    temperature_file = "/sys/class/thermal/thermal_zone0/temp"
    if os.path.exists(temperature_file):
        temp=0
        with open(temperature_file, 'r') as file:
            temp = int(file.read().strip()) / 1000.0  # 通常读到的是微摄氏度，转换为摄氏度
        return temp
    else:
        return 0
        


class Audio:
    def __init__(self):
        self.tts_str_queue = queue.Queue(2)
        self.new_tts_flag = False
        self.image_sub = None
        self.frame_cnt = 0
        self.jpeg_data = b""
        self.jpeg_cnt = 0

        self.current_dir = os.path.dirname(os.path.realpath(__file__))  # 获取当前文件夹
        self.emoji = emoji.Emobj()

        rospy.init_node("audio")
        self.tts_sub = rospy.Subscriber("/tts", String, self.sub_tts_callback, queue_size=2) #语音识别指令订阅
        
        self.cmd_vel_pub = rospy.Publisher('/cmd_vel', Twist, queue_size=10) #车轮速度发布
        self.enable_wakeup_pub = rospy.Publisher('/enable_wakeup', Bool, queue_size=10)  #是否使能唤醒发布
        self.enable_wakeup_flag = True

        self.cfg_dict = dict()
        self.tts_local_client = None
        self.asr_client = None

        
    def do_dance(self,check_music_ended):
        speed = Twist()

        zxt_list = [(-1.0, 0, 0.5), (0, 0, 0.1),
                (1.0, 0, 0.5), (0, 0, 0.1),

                (0, 0.04, 0.5), (0, 0, 0.1),
                (0, -0.04, 0.5), (0, 0, 0.1),

                (1.0, 0, 0.5), (0, 0, 0.1),
                (-1.0, 0, 0.5), (0, 0, 0.1),

                (0, -0.04, 0.5), (0, 0, 0.1),
                (0, 0.04, 0.5), (0, 0, 0.1),
                ]

        while not rospy.is_shutdown():

            for z, x, t in zxt_list:
                speed.angular.z = z
                speed.linear.x = x
                self.cmd_vel_pub.publish(speed)

                for i in range(int(t / 0.1)):  # 0.5/0.1=5
                    time.sleep(0.1)  # 0.1s

                if self.new_tts_flag:  # 如果收到新的命令，则退出
                    # 关闭播放音乐
                    os.system("killall play >/dev/null 2>&1")
                    break

            if check_music_ended:
                if music.check_music_playing() == False:
                    print("音乐播放已结束，停止舞蹈")
                    break

            if self.new_tts_flag:  # 如果收到新的命令，则退出
                # 关闭播放音乐
                os.system("killall play >/dev/null 2>&1")
                break

        #最终要停止
        speed.angular.z = 0
        speed.linear.x = 0
        self.cmd_vel_pub.publish(speed)


    def sub_qr_code(self):
        self.frame_cnt = 0
        if self.image_sub != None:
            self.image_sub.unregister()  # 取消订阅
        # 订阅消息
        self.image_sub = rospy.Subscriber('/camera/image_raw', Image, self.raw_image_callback)

    def sub_a_photo(self):
        self.jpeg_cnt = 0
        self.jpeg_data = b"" #清空图像

        if self.image_sub != None:
            self.image_sub.unregister()  #取消订阅

        # 订阅消息
        self.image_sub = rospy.Subscriber('/image_raw/compressed', CompressedImage, self.a_compressed_image_callback)

    def sub_photo_and_display(self):
        if self.image_sub != None:
            self.image_sub.unregister()  #取消订阅

        # 订阅消息
        self.image_sub = rospy.Subscriber('/camera/image_raw', Image, self.raw_image_callback_and_display)

    def sub_robot_state_callback(self,msg):
        if len(msg.data) != 8:
            print("robot_state data len error")
            return
        encoder1,encoder2,vbat_mv,charging,full_charged,pwm1,pwm2,enable_lidar = msg.data
        if charging==1 and full_charged==0:
            charge_state="充电中"
        elif charging==0 and full_charged==1:
            charge_state="充电器断开"
        elif charging==1 and full_charged==1:
            charge_state="已充满"
        else:
            charge_state="未知"

        temp = get_cpu_temperature()
        text_in = "CPU温度 %.2f°C\n电池电压 %.2fV\n充电状态 %s\n编码器 %d %d\nPWM %d %d\n雷达 %s\n麦克风 %d\n软件版本 %s"%(
            temp,vbat_mv/1000,charge_state,encoder1,encoder2,pwm1,pwm2,"开启" if enable_lidar else "关闭",
            record.get_mic_value(),app_version)
        self.emoji.set_display_picture(self.gen_img_with_text(text_in,remove_wrap=False))

        if self.frame_cnt==0:
            tts_str = "温度%.2f°C\n电压%.2fV#noshow"%(temp,vbat_mv/1000)
            self.tts_str_queue.put(tts_str)
            self.new_tts_flag = True
            
        self.frame_cnt += 1


    def a_robot_state_callback(self,msg):
        self.image_sub.unregister()  # 收到一次立即取消订阅
        if len(msg.data) != 8:
            print("robot_state data len error")
            return
        encoder1,encoder2,vbat_mv,charging,full_charged,pwm1,pwm2,enable_lidar = msg.data
        tts_str = "当前电池电压%.2fV"%(vbat_mv/1000)
        self.tts_str_queue.put(tts_str)
        self.new_tts_flag = True

    def sub_state_and_display(self):
        if self.image_sub != None:
            self.image_sub.unregister()  #取消订阅
        # 订阅底盘状态消息
        self.frame_cnt = 0
        self.image_sub = rospy.Subscriber("/robot_state", Int16MultiArray,self.sub_robot_state_callback)

    def sub_a_bat_state(self):
        if self.image_sub != None:
            self.image_sub.unregister()  #取消订阅
        # 订阅底盘状态消息
        self.image_sub = rospy.Subscriber("/robot_state", Int16MultiArray,self.a_robot_state_callback)

    def unsub_and_remove_display(self):
        if self.image_sub != None:
            self.image_sub.unregister()  #取消订阅

        self.emoji.set_display_cmd("normal") #下发正常命令，退出图像显示 


    def a_compressed_image_callback(self,msg):

        self.jpeg_cnt+=1
        if self.jpeg_cnt<10: #多获取几帧，防止刚开始打开摄像头是黑的
            return

        self.jpeg_cnt = 0

        self.image_sub.unregister()  # 取消订阅

        # with open("photo.jpg", 'wb') as f:
        #     f.write(msg.data)
        do_rectify = False

        #获取图像
        if do_rectify:
            calibration_file = os.path.join(self.current_dir, "cfg", "fisheye.yml")
            self.jpeg_data = decode_encode_jpeg(msg.data, calibration_file)
        else:
            image = cv2.imdecode(np.frombuffer(msg.data, np.uint8), -1)
            success, encoded_image = cv2.imencode('.jpg', image, [cv2.IMWRITE_JPEG_QUALITY, 100])
            self.jpeg_data = encoded_image.tobytes()


    def raw_image_callback(self,msg):

        self.frame_cnt += 1

        # 将ROS中的Image消息转换为numpy数组
        img_array = np.frombuffer(msg.data, np.uint8)
        image = np.reshape(img_array, (msg.height, msg.width, -1))


        # 计算较小的边长作为正方形边长
        size = min( msg.width, msg.height)
        # 计算剪切区域的起始位置
        start_x = ( msg.width - size) // 2
        start_y = (msg.height - size) // 2

        # 使用切片操作剪切出正方形图像
        square_image = image[start_y:start_y+size, start_x:start_x+size]

        resize_image = cv2.resize(square_image, (240, 240))
        resize_image_bgr = cv2.cvtColor(resize_image,cv2.COLOR_RGB2BGR)
        self.emoji.set_display_picture( cv2.flip(resize_image_bgr, 1) ) #镜像显示

        if self.frame_cnt%10!=0: #解析二维码比较慢，所以每10帧只处理一帧
            return

        # 解析二维码（速度比较慢，会让图像显示产生延迟）
        try:
            text = qr_code.scan_qrcode(image) #解析的是原始图像
        except Exception as e:  # 没有二维码的情况会进入异常
            text = ""

        if text != "" or self.frame_cnt > 1000:  # 0.033s * 1000 = 33s
            self.frame_cnt = 0
            self.image_sub.unregister()  # 取消订阅

            self.emoji.set_display_cmd("normal") #下发正常命令，退出图像显示 

            msg = String()
            if text == "":
                text = "未能识别到二维码"
                self.tts_str_queue.put(text)
                self.new_tts_flag = True
            else:
                self.current_dir = os.path.dirname(os.path.realpath(__file__))
                deng_file_name = os.path.join(self.current_dir, "sound", "cut_deng.wav")
                music.play_music(deng_file_name)
                print("二维码信息:",text)


                self.tts_str_queue.put("开始连接网络，请稍等……")
                self.new_tts_flag = True

                text =network.connect_wifi(text) #连接WIFI
                self.tts_str_queue.put(text)
                self.new_tts_flag = True




    def raw_image_callback_and_display(self,msg):
        # 将ROS中的Image消息转换为numpy数组
        img_array = np.frombuffer(msg.data, np.uint8)
        image = np.reshape(img_array, (msg.height, msg.width, -1))

        # 计算较小的边长作为正方形边长
        size = min( msg.width, msg.height)
        # 计算剪切区域的起始位置
        start_x = ( msg.width - size) // 2
        start_y = (msg.height - size) // 2

        # 使用切片操作剪切出正方形图像
        square_image = image[start_y:start_y+size, start_x:start_x+size]

        resize_image = cv2.resize(square_image, (240, 240))
        resize_image_bgr = cv2.cvtColor(resize_image,cv2.COLOR_RGB2BGR)
        self.emoji.set_display_picture( cv2.flip(resize_image_bgr, 1) ) #镜像显示

    def sub_tts_callback(self,msg):
        #print("audio sub_tts_callback:", msg.data)
        if self.enable_wakeup_flag==False:
            return

        tts_str_list = msg.data.split("#")
        if len(tts_str_list) == 1:
            tts_str = tts_str_list[0]
            key_str = None
        else:
            tts_str = tts_str_list[0]
            key_str = tts_str_list[1]

        #打断之前的播放，但是音量调节和离线唤醒退出的时候不打断
        if key_str != "voice_up" and key_str != "voice_down" and key_str != "wakeup_exit":
            os.system("killall play >/dev/null 2>&1")
            os.system("killall ffplay >/dev/null 2>&1")
            # 已经重新唤醒，关闭图像显示
            self.unsub_and_remove_display()

        self.tts_str_queue.put(msg.data)
        self.new_tts_flag = True



    # 播放命令的回复词，可能回复词会和命令词一样，所以要关闭唤醒，防止自己唤醒自己
    def tts_play(self, tts_str, turn_off_wakeup =True,use_database=True):

        #kaldi tts不会返回mp3数据
        mp3_data = self.tts_client.text_to_speech(tts_str,use_database=use_database)

        if turn_off_wakeup:
            msg = Bool()
            msg.data = False  # 获取到语音命令，自己开始发声音，关闭唤醒
            self.enable_wakeup_pub.publish(msg)
            self.enable_wakeup_flag = msg.data
        
        if mp3_data != None:
            tts_file_name = os.path.join(os.path.expanduser('~'),".ros/tts.mp3")
            with open(tts_file_name, "wb") as file: #默认存放在.ros里
                file.write(mp3_data)

            if music.check_ffplay_playing(): #如果在线歌曲正在播放，为了听清楚提示语，将整体音量缩小
                current_volume = music.get_current_volume()
                music.set_volume(current_volume*0.7) #机器音量缩小
                music.play_music(tts_file_name,volume_factor=5.0) #但大声播放提示语
                music.set_volume(current_volume) #恢复音量
            else:
                music.play_music(tts_file_name)

        else:  # edge tts调用失败，则用本地TTS播放
            if self.tts_local_client!=None:
                self.tts_local_client.text_to_speech(tts_str)
            else:
                tts_str = "离线语音合成模型未完成加载，请稍等..."
                os.system('espeak -v zh "%s"' % (tts_str))

        time.sleep(0.5) #阅读完等待0.5秒再开启唤醒，防止离线芯片重复唤醒

        if turn_off_wakeup:
            msg = Bool()
            msg.data = True  # 完成语音命令，打开唤醒
            self.enable_wakeup_pub.publish(msg)
            self.enable_wakeup_flag = msg.data


    def gen_img_with_text(self, text, remove_wrap=True):
        
        if remove_wrap:
            text = text.replace("\r","").replace("\n","") #去除换行符

        # Create a black image
        img = PILImage.new('RGB', (240, 240), color='black')
        draw = ImageDraw.Draw(img)

        # Draw a white circle in the center
        center = (img.width // 2, img.height // 2)
        radius = img.width // 2-1
        #draw.ellipse([(center[0]-radius, center[1]-radius), (center[0]+radius, center[1]+radius)], fill=None, outline=(255, 255, 255), width=2) #GBR

        if remove_wrap:
            max_size = 60
            if len(text) > max_size:
                text = text[:max_size-2] + "……"
            line_size = 10
            wrapped_lines = [text[i:i+line_size] for i in range(0, len(text), line_size)]
        else:
            wrapped_lines = text.split('\n')

        # Get the size of the text
        total_height = sum(self.font.getsize(line)[1] for line in wrapped_lines)
        current_height = center[1] - total_height // 2

        # Draw the wrapped text
        for line in wrapped_lines:
            line_width, line_height = self.font.getsize(line)
            line_x = center[0] - line_width // 2
            draw.text((line_x, current_height), line, font=self.font, fill=(50, 205, 50)) #GBR
            current_height += line_height

        return np.array(img)  # Convert PIL Image back to numpy array


    
    def chat_with_llm(self, text):
        if text == "":
            print("没有从语音中识别到文字")
            answer = "我好像没有听清你在说什么"
        elif "画" in text:
            try:
                t1 = time.time()
                img_data = self.tti_client.text2image(text)
                t2 = time.time()
                print("已完成画画耗时%.1f秒" % (t2 - t1))
            
                image = cv2.imdecode(np.frombuffer(img_data, dtype=np.uint8), cv2.IMREAD_COLOR) #TypeError: a bytes-like object is required, not 'str
                resize_image = cv2.resize(image, (240, 240))
                self.emoji.set_display_picture(resize_image) #发送图片显示命令，异步显示图片
                answer = "已完成画画"
            except:
                print("文生图错误!")
                answer = "画画错误"
        elif  "看" in text or "面前" in text or "前面" in text:
            self.sub_photo_and_display()
            text_input = "你是我的宠物机器人，刚刚用你的摄像头拍下了你眼前的画面，请回答问题：" + text
            #music.play_music(os.path.join(self.current_dir, "sound", "focus.wav"))
            self.sub_a_photo()

            #等待3秒拍照完成
            for i in range(30):
                if len(self.jpeg_data)!=0:
                    break
                time.sleep(0.1)

            self.unsub_and_remove_display()
            #处理拍照结果
            if len(self.jpeg_data)!=0:
                music.play_music(os.path.join(self.current_dir, "sound", "beng.mp3"))
                answer = self.itt_client.image2text(self.jpeg_data,text_input)
                if answer == "":
                    print("图生文模型结果为空")
                    answer = "图生文模型结果为空"
                mail.send_mail(self.jpeg_data, answer)
                self.jpeg_data = b"" #清空
            else:
                answer = "拍照故障，请检查摄像头连接"
        elif "眼睛变成" in text:
            if "红" in text:
                rgb_list = [255,99,71]
            elif "绿" in text:
                rgb_list = [0,255,0]
            elif "蓝" in text:
                rgb_list = [65,105,225]
            elif "黄" in text:
                rgb_list = [255,165,0]
            elif "青" in text:
                rgb_list = [0,206,209]
            elif "紫" in text:
                rgb_list = [138,43,226]
            elif "白" in text:
                rgb_list = [255,255,255]
            elif "随机" in text: #Cannot cast ufunc 'multiply' output from dtype('float64') to dtype('uint8') with casting rule 'same_kind
                rgb_list = [random.randint(0, 255),random.randint(0, 255),random.randint(0, 255)]
            else:
                return "未知的眼睛颜色"

            self.emoji.set_eye_color(bgr_list=(rgb_list[2],rgb_list[1],rgb_list[0]))

            #写入配置
            self.cfg_dict["eye_color_rgb"] = rgb_list
            yaml_cfg.write_yaml_cfg(self.cfg_dict)

            answer = text.replace("眼睛变成","眼睛已变成")
        elif "唱" in text or "播放" in text or "放首" in text or "放一首" in text  or "换首" in text  or "换一首" in text:
            #唱歌之前关闭后台播放的歌曲
            os.system("killall play >/dev/null 2>&1")
            os.system("killall ffplay >/dev/null 2>&1")
            #text_input = "你是一个智能点歌台，请从这个问题中得到最有可能的歌曲名，将歌曲名两边加上书名号作为回复内容，" + text
            text_input = "你是我的宠物机器人，你的名字叫小白，对话字数必须小于50字，对话中必须出现歌手名，歌曲名，和这首歌的介绍，你对话中的歌曲名必须加书名号方便程序调用API播放，现在我聊天的内容是，" + text
            answer = self.llm_client.chat(text_input)
            #print("大模型回复结果:",answer)
            # 使用正则表达式提取双引号《》中的歌名
            song_list = re.findall(r'《(.*?)》', answer)
            result = False
            if len(song_list)==0:
                result = False
            else:
                #song_name = random.choice(song_list)
                song_name = song_list[0]
                if len(song_name)==0:
                    result = False
                else:
                    if cloud_music.play_cloud_music(song_name)<0: #后台播放云端音乐
                        result = False
                    else:
                        result = True
            
            # if result == False: #如果AI推荐的歌播放失败，直接从问题手动提取歌名
            #     song_name = text.replace("放一首","").replace("放首","").replace("播放","").replace("唱","").replace("换首","").replace("换一首","")
            #     if cloud_music.play_cloud_music(song_name)<0: #后台播放云端音乐
            #         result = False
            #     else:
            #         result = True

            if result == False:
                answer = "抱歉，音乐库中暂未搜索到相关歌曲"
        else:
            #text_input = text
            #发现直接把提示词加在对话前面貌似效果更好
            text_input = "你是我的宠物机器人，你的名字叫小白，你要用一只软萌机器人的语气和我聊天，回答字数必须小于50字，现在我聊天的内容是，" + text
            answer = self.llm_client.chat(text_input)
            if answer == "":
                print("大模型聊天回复为空")  # 可能问了不合法的问题，不合法的问题有可能返回的是空
                answer = "这个问题我还不知道呢"

        return answer

    def backend_load_tts_thread(self):
        print("正在后台加载离线语音合成模型，请稍等……")
        self.tts_local_client = tts.create_tts("kaldi") #离线语音合成 espeak或kaldi 都不输出mp3文件数据 耗费时间10秒

        print("正在后台加载离线语音识别模型，请稍等……")
        self.asr_client = asr.create_asr("kaldi") #耗费时间20秒

        print("离线语音识别模型加载完成")

    def main_loop(self):

        #读取配置
        self.cfg_dict = yaml_cfg.read_yaml_cfg()
        if "eye_color_rgb" in self.cfg_dict.keys():
            rgb_list = self.cfg_dict["eye_color_rgb"]
            self.emoji.set_eye_color(bgr_list=(rgb_list[2], rgb_list[1], rgb_list[0]))

        self.llm_client = llm.create_llm("xunfei")
        self.tti_client = text2image.create_tti("xunfei")
        self.itt_client = image2text.create_itt("xunfei")

        # 读取字体文件
        font_path = os.path.join(self.current_dir, "font", "wqy-microhei.ttf")
        font_size = 20
        self.font = ImageFont.truetype(font_path, font_size)

        detected_file_name = os.path.join(self.current_dir, "sound", "cut_deng.wav")
        finished_file_name = os.path.join(self.current_dir, "sound", "dong.wav")
        send_file_name = os.path.join(self.current_dir, "sound", "send.wav")
        files = os.listdir(os.path.join(self.current_dir, "sound", "music"))
        music_file_names = []
        for file in files:
            music_file_names.append(os.path.join(self.current_dir, "sound", "music", file))

        self.tts_client = tts.create_tts("edge")  # 在线语音合成 edge,youdao,baidu或sougou 都会输出mp3文件数据

        threading.Thread(target=self.backend_load_tts_thread).start()  # 后台加载TTS线程，因为耗时较长

        tts_str = "主人您好，请说小白你好唤醒我，请说小白小白和我对话"
        self.emoji.set_display_picture(self.gen_img_with_text(tts_str))  # 发送图片显示命令，异步显示图片,刚开机可能显示不出来
        self.tts_play(tts_str)
        self.emoji.set_display_cmd("normal")  # 下发正常命令，退出图像显示

        #self.sub_a_bat_state()  # 开机报告电量

        rate = rospy.Rate(10)  # 10Hz 0.1s
        time.sleep(1.0) #等待1秒清空队列再读取唤醒词 
        self.tts_str_queue.queue.clear() #初始化结束后将队列清空
        while not rospy.is_shutdown():
            try:
                tts_string = self.tts_str_queue.get(timeout=1)  # 会阻塞  #等待1秒，如果还是不能取到数据，则会抛出异常
            except:
                rate.sleep()  # 0.1s
                continue

            self.new_tts_flag = False  # 清空标志

            print("TTS:", tts_string)

            tts_str_list = tts_string.split("#")
            if len(tts_str_list) == 1:
                tts_str = tts_str_list[0]
                key_str = None
            else:
                tts_str = tts_str_list[0]
                key_str = tts_str_list[1]

            if key_str == "noshow" or key_str == "wakeup_exit":
                self.tts_play(tts_str)  # 播放机器人的回复语音
                continue  # 跳过后面的代码，不做屏幕显示，只播放语音

            self.emoji.set_display_cmd(key_str)  # 发送表情显示命令，异步显示表情

            if key_str == "ip_address":
                wifi_ssid = network.get_wifi_ssid()
                wlan0_addr = network.get_host_ip("wlan0")
                eth0_addr = network.get_host_ip("eth0")
                tts_str = "WIFI名称：%s 地址：%s" % (wifi_ssid, wlan0_addr)
                if eth0_addr != "空":
                    tts_str += " 以太网地址：" + eth0_addr

            elif key_str == "scan":
                self.sub_qr_code()  # 扫码连接WIFI
                tts_str = "请打开手机设置中的分享WIFI二维码界面，将手机屏幕对着摄像头扫码"

            elif key_str == "forget_wifi_con":
                network.forget_all_wifi()
                tts_str = "已删除所有WIFI，开始重启进入AP模式"  # 配合开机脚本中在没有IP的情况下就打开AP模式
                os.system("sync && sleep 5 && reboot && echo orangepi | sudo -S watchdog_test 1 &") #重启命令
                #os.system("sync && sleep 5 && reboot &")  # 重启命令

            elif key_str == "voice_up":
                music.change_volume(10, 0, 150)  # 用代码接口设置音量，最大好像只能到153%
                tts_str = "音量调大到%d" % (music.get_current_volume()) + "%"

            elif key_str == "voice_down":
                music.change_volume(-10, 0, 150)  # 用代码接口设置音量，最大好像只能到153%
                tts_str = "音量调小到%d" % (music.get_current_volume()) + "%"

            elif key_str == "photo":
                self.sub_photo_and_display()
                self.tts_play("三、二、一")
                music.play_music(os.path.join(self.current_dir, "sound", "focus.wav"))
                self.sub_a_photo()

                # 等待3秒拍照完成
                for i in range(30):
                    if len(self.jpeg_data) != 0:
                        break
                    time.sleep(0.1)

                self.unsub_and_remove_display()
                if len(self.jpeg_data) != 0:
                    music.play_music(os.path.join(self.current_dir, "sound", "capture.wav"))
                    res = mail.send_mail(self.jpeg_data, "您好，我是小白机器人，我为你拍了一张照片。")
                    if res == 0:
                        tts_str = "拍照成功，发送邮件成功"
                    elif res == -1:
                        tts_str = "拍照成功"  # 请在环境变量中设置邮箱地址接收照片
                    else:
                        tts_str = "拍照成功，发送邮件失败"
                else:
                    tts_str = "拍照故障，请检查摄像头连接"


            elif key_str == "sing":  # 唱歌模式
                music.play_music(random.choice(music_file_names), background_playback=True)

            elif key_str == "dance":  # 跳舞模式
                music.play_music(random.choice(music_file_names), background_playback=True)
                self.do_dance(check_music_ended=True)  # 阻塞在这个函数里
                tts_str = ""

            elif key_str == "mirror_mode":  # 镜子模式
                self.sub_photo_and_display()

            elif key_str == "dis_state_mode":  # 显示状态模式
                self.sub_state_and_display()

            elif key_str == "cur_time":  # 查询时间
                tts_str = weather.get_current_time()

            elif key_str == "weather":  # 查询天气
                music.play_music(send_file_name)
                city = weather.get_city_name() #查询天气比较耗时，在之前放个提示音
                tts_str = weather.get_weather_info(city)

            elif key_str == "sound":  # 打开声音
                # 写入声音配置，会保存到Linux系统
                music.set_volume(100) #设置音量为100

            elif key_str == "mute":  # 关闭声音
                # 写入静音配置，会保存到Linux系统
                music.set_volume(0)  # 设置音量为0

            elif key_str == "chatgpt":  # 聊天模式
                # music.play_music(detected_file_name,background_playback=True) #叮的一声后台播放，录音进去，防止说话太早丢字
                music.play_music(detected_file_name, background_playback=False)  # 不把叮录进去，如果没有说话，会引起kaldi识别错误

                msg = Bool()
                msg.data = False  # 关闭唤醒
                self.enable_wakeup_pub.publish(msg)
                self.enable_wakeup_flag = msg.data

                cur_eye_color = self.emoji.get_eye_color()
                self.emoji.set_eye_color((50, 205, 50))
                self.emoji.set_display_cmd("normal")  # 下发正常表情命令，显示绿色表示正在录音

                audio_file_name = record.record_audio()  # 录音过程中禁止唤醒

                self.emoji.set_eye_color(cur_eye_color)
                self.emoji.set_display_cmd("normal")  # 下发正常表情命令，恢复眼睛颜色

                msg = Bool()
                msg.data = True  # 打开唤醒
                self.enable_wakeup_pub.publish(msg)
                self.enable_wakeup_flag = msg.data

                if self.new_tts_flag:
                    continue

                music.play_music(finished_file_name)

                if audio_file_name == None:
                    print("没有检测到语音")
                    tts_str = "我什么也没有听到，请大声和我对话"
                else:
                    print('录音完成')
                    # wav_data = asr.get_file_content(audio_file_name)
                    if self.asr_client != None:
                        text = self.asr_client.audio_file_to_text(audio_file_name)
                        print("User: %s" % text)
                        self.emoji.set_display_picture(self.gen_img_with_text(text))  # 发送图片显示命令，异步显示图片
                        tts_str = self.chat_with_llm(text)  # 大模型对话处理
                    else:
                        text = "离线语音识别模型未完成加载，请稍等..."
                        print("User: %s" % text)
                        self.emoji.set_display_picture(self.gen_img_with_text(text))  # 发送图片显示命令，异步显示图片
                        tts_str = text


                print("AI: %s" % tts_str)

                if self.new_tts_flag:
                    continue

            if tts_str != "":
                if tts_str=="已完成画画" or key_str=="smile" or key_str=="good_night" or key_str=="dis_state_mode" \
                or key_str=="open_lcd" or key_str=="close_lcd":  #这些情况不显示字幕
                    self.tts_play(tts_str)  # 只播放机器人的回复语音
                else:
                    self.emoji.set_display_picture(self.gen_img_with_text(tts_str))  # 发送图片显示命令，异步显示图片
                    self.tts_play(tts_str)  # 播放机器人的回复语音
                    self.emoji.set_display_cmd("normal")  # 下发正常命令，退出图像显示


            rate.sleep()  # 0.1s



def handler(signum, frame):
    print("receive a signal %d"%signum)
    global is_shutdown
    is_shutdown = True
    rospy.signal_shutdown("ctrl c shutdown")


if __name__ == "__main__":
    # 讯飞密钥须从环境变量读取（如 ~/.bashrc 或 systemd/roslaunch 中 export），勿在此硬编码覆盖
    #sys.stdout = Logger() #将print打印的内容写入文件
    
    os.system("pactl set-default-source \"alsa_input.usb-C-Media_Electronics_Inc._USB_PnP_Sound_Device-00.mono-fallback\"")
    os.system("amixer -c 2 sset Mic 16") #麦克风接收增益调到100%，范围0～16
    signal.signal(signal.SIGINT, handler)
    audio = Audio()
    threading.Thread(target=audio.emoji.loop).start() #表情显示线程
    audio.main_loop() #主线程处理TTS和命令




