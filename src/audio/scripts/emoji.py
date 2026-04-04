import os
import time
import cv2
import random
from random import randint
import signal
import numpy as np
import rospy
import music

import platform
if platform.machine()=="x86_64":
    ARM_SYS = False
else:
    ARM_SYS = True

if ARM_SYS:
    from lcd import  Lcd
    

is_shutdown = False

def handler(signum, frame):
    print("receive a signal %d"%signum)
    global is_shutdown
    is_shutdown = True
    rospy.signal_shutdown("ctrl c shutdown")

signal.signal(signal.SIGINT, handler)

class Emobj:
    def __init__(self):
        self.cmd_str = None
        self.bgr_list = (255, 255, 255)

    def read_img_dict(self,root_path):
        emoji_states = \
            [
                "眨眼",
                #"左上看",
                #"右上看",
                "兴奋",
                "微笑",
                # "惊恐",
                # "不屑",
                # "愤怒",
                # "难过",
                "正常",
                "睡觉",
                "苏醒",
            ]
        imgs_dict = dict()
        for emoji_state in emoji_states:
            if emoji_state == "眨眼":
                paths = [
                    os.path.join(root_path, emoji_state, "单次眨眼偶发"),
                    os.path.join(root_path, emoji_state, "快速双眨眼偶发")
                ]
            elif emoji_state == "正常" or emoji_state == "睡觉" or emoji_state == "苏醒" or emoji_state == "微笑":
                paths = [
                    os.path.join(root_path, emoji_state)
                ]
            else:
                paths = [
                    os.path.join(root_path, emoji_state, emoji_state + "_1进入姿势"),
                    os.path.join(root_path, emoji_state, emoji_state + "_2可循环动作"),
                    os.path.join(root_path, emoji_state, emoji_state + "_3回正")
                ]

            for path in paths:
                print("path=",path)
                key = os.path.basename(path)  # 把文件夹名字作为key
                imgs_dict[key] = []

                for i in range(1, 200):
                    img_name = os.path.join(path, "%d.jpg" % (i))
                    if not os.path.exists(img_name):
                        continue

                    img = cv2.imread(img_name)
                    imgs_dict[key].append(img)

        # for key,val in imgs_dict.items():
        #     print(key,val)

        return imgs_dict


    def do_an_emoji(self,emoji_state):
        keys = []
        if emoji_state == "眨眼":
            keys.append((0, 0, "单次眨眼偶发"))
            keys.append((0, 0, "快速双眨眼偶发"))
            keys = [random.choice(keys)]  # 随机只挑一个
        elif emoji_state == "正常" or emoji_state == "睡觉" or emoji_state == "苏醒" or emoji_state == "微笑":
            keys.append((0, 0, emoji_state))
        else:
            #1.进入姿势
            if emoji_state == "左上看":
                keys.append((-2000, 2000, emoji_state + "_1进入姿势")) #前两个数字表示轮胎动作
            elif emoji_state == "右上看":
                keys.append((2000, -2000, emoji_state + "_1进入姿势"))
            else:
                keys.append((0, 0, emoji_state + "_1进入姿势"))

            #2.进入姿势后的循环动作
            loop_num = 1
            for l in range(loop_num):
                # if  emoji_state=="兴奋":
                #     if l%2==0:
                #         keys.append((1500, 1500, emoji_state+"_2可循环动作"))
                #     elif l%2==1:
                #         keys.append((-1500, -1500, emoji_state+"_2可循环动作"))
                # else:
                keys.append((0, 0, emoji_state + "_2可循环动作"))

            #3.循环动作后的回正
            if emoji_state == "左上看":
                keys.append((2000, -2000, emoji_state + "_3回正"))
            elif emoji_state == "右上看":
                keys.append((-2000, 2000, emoji_state + "_3回正"))
            else:
                keys.append((0, 0, emoji_state + "_3回正"))

        if emoji_state == "兴奋":
            action_div = 8
        elif emoji_state == "睡觉" or emoji_state == "苏醒":
            action_div = 1
        else:
            action_div = 3

        #开始执行动作
        all_cnt = 0
        for pwm1, pwm2, key in keys:  # 遍历所有key(进入，循环，回正)
            imgs = self.imgs_dict[key]

            # uart.set_pwm(pwm1,pwm2) #设置轮胎pwm

            for img in imgs: #遍历每个动作的图片
                if all_cnt % action_div == 0:  # 为加速播放，跳过动画中一部分的图像
                    start = time.time()

                    img_show = img.copy()

                    for i in range(3):
                        if self.bgr_list[i]!=255:
                            img_show[:, :, i] &= self.bgr_list[i]

                    if ARM_SYS:
                        if self.lcd:
                            self.lcd.display(img_show)  # 从字典读取表情并显示
                    else:
                        self.imshow_in_pc_screen(img_show)
                        

                    interval = time.time() - start

                    if (0.050 - interval) >= 0:
                        time.sleep(0.050 - interval) #控制时间间隔刚好为50ms
                    # else:
                    #     print("显示时间:%f ms, 超过了50ms!"%(interval*1000))
                all_cnt += 1

        # uart.set_pwm(0, 0)


    def set_display_cmd(self,cmd_str):
        self.cmd_str = cmd_str

    def set_display_picture(self,image):
        self.cmd_str = "show_pic"
        self.image = image #BGR

    def set_eye_color(self,bgr_list):
        self.bgr_list = bgr_list

    def get_eye_color(self):
        return self.bgr_list

    def imshow_in_pc_screen(self,image):
        # # 获取图像尺寸
        # image_height, image_width = image.shape[:2]

        # # 计算缩放比例
        # scale = min(self.screen_width / image_width, self.screen_height / image_height)

        # # 缩放图像
        # scaled_width = int(image_width * scale)
        # scaled_height = int(image_height * scale)
        # scaled_image = cv2.resize(image, (scaled_width, scaled_height))

        # # 创建一个黑色背景
        # full_image = np.zeros((self.screen_height, self.screen_width, 3), dtype=np.uint8)
        
        # # 将图像居中放置在黑色背景上
        # x_offset = (self.screen_width - scaled_width) // 2
        # y_offset = (self.screen_height - scaled_height) // 2
        # full_image[y_offset:y_offset+scaled_height, x_offset:x_offset+scaled_width] = scaled_image

        scaled_image = cv2.resize(image, dsize=(0,0), fx=3, fy=3)

        cv2.imshow('image', scaled_image)
        key = cv2.waitKey(1) & 0xFF  # 获取按下的键的ASCII码
        if key == 27 or key == ord('q'):  # 按下esc键或者q键
            rospy.signal_shutdown("shutdown")

    def open_lcd(self):
        if ARM_SYS:
            self.lcd = Lcd()
        else:
            import tkinter as tk
            root = tk.Tk()
            self.screen_width = root.winfo_screenwidth()
            self.screen_height = root.winfo_screenheight()
            root.destroy()

    def close_lcd(self):
        if ARM_SYS:
            del self.lcd
            self.lcd=None #防止报错：'Emobj' object has no attribute 'lcd'

    def loop(self):
        self.open_lcd()

        self.current_dir = os.path.dirname(os.path.realpath(__file__))  # 获取当前文件夹
        self.imgs_dict = self.read_img_dict(os.path.join(self.current_dir, "image")) #读取图片耗时较长！

        # x86系统创建一个全屏窗口
        # if not ARM_SYS:
        #     cv2.namedWindow('fullscreen', cv2.WND_PROP_FULLSCREEN)
        #     cv2.setWindowProperty('fullscreen', cv2.WND_PROP_FULLSCREEN, cv2.WINDOW_FULLSCREEN)

        while not rospy.is_shutdown():
            num = random.uniform(10, 30)  # 10*0.1s~30*0.1s,也就是休眠1~3s
            for i in range(int(num)):
                time.sleep(0.1)  # 0.1s
                if self.cmd_str != None: #收到外部新的命令，则退出等待
                    break
                if is_shutdown:
                    return

            #print("cmd_str=",self.cmd_str)
            if self.cmd_str == "open_lcd":
                self.cmd_str = None  # 接收到命令立即清空命令状态为None
                self.open_lcd()
                self.do_an_emoji("正常")

            elif self.cmd_str == "close_lcd":
                self.cmd_str = None  # 接收到命令立即清空命令状态为None
                self.close_lcd()

            elif self.cmd_str == "smile":
                self.cmd_str = None  # 接收到命令立即清空命令状态为None
                self.do_an_emoji("微笑")
                while not rospy.is_shutdown(): #永远微笑
                    time.sleep(0.1)  # 0.1s
                    if self.cmd_str != None:  # 收到外部新的命令，则退出等待
                        break
                    if is_shutdown:
                        return
                #self.do_an_emoji("兴奋")
                self.do_an_emoji("正常") # 眼睛恢复正常

            elif self.cmd_str == "good_night":  # 一直持续睡觉，不用清空状态和恢复
                self.cmd_str = None  # 接收到命令立即清空命令状态为None
                self.do_an_emoji("睡觉")
                while not rospy.is_shutdown(): #永远睡觉
                    time.sleep(0.1)  # 0.1s
                    if self.cmd_str != None:  # 收到外部新的命令，则退出等待
                        break
                    if is_shutdown:
                        return
                self.do_an_emoji("苏醒")

            elif self.cmd_str == "show_pic":
                while not rospy.is_shutdown():
                    self.cmd_str = None  # 接收到命令立即清空命令状态为None
                    if ARM_SYS:
                        if self.lcd:
                            self.lcd.display(self.image) #显示图片
                    else:
                        self.imshow_in_pc_screen(self.image)

                    while not rospy.is_shutdown():  # 永远保持画画的画面，直到接收到新的命令
                        if self.cmd_str != None:  # 收到外部新的命令，则退出等待
                            break
                        if is_shutdown:
                            return
                        time.sleep(0.01)  #10ms，只等待10ms是为了循环显示图片更加流畅
                        
                    #如果切换成了其他命令而不再显示图像，则跳出显示图像的大循环
                    if self.cmd_str!="show_pic":
                        break

            elif self.cmd_str == None:  # None进行眨眼
                # emoji_state = random.choice(["眨眼","左上看","右上看","兴奋"])
                #music.play_music(os.path.join(self.current_dir, "sound", "du.wav"),background_playback=True)
                self.do_an_emoji("眨眼")

            else: #其他状态下，显示正常
                self.cmd_str = None  # 接收到命令立即清空命令状态为None
                #music.play_music(os.path.join(self.current_dir, "sound", "du.wav"), background_playback=True)
                self.do_an_emoji("正常")

            if is_shutdown:
                return

if __name__ == "__main__":
    emobj = Emobj()
    emobj.loop()





