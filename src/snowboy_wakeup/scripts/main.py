#!/usr/bin/env python3
#coding=utf-8

import sys
sys.path.append("/opt/ros/melodic/lib/python2.7/dist-packages")
sys.path.append("/opt/ros/noetic/lib/python3/dist-packages")
sys.path.append("/home/orangepi/.local/lib/python3.8/site-packages")

import platform
import os

import rospy
from std_msgs.msg import String
from std_msgs.msg import Int32

current_dir = os.path.dirname(os.path.realpath(__file__))

link_file_name  = os.path.join(current_dir, "_snowboydetect.so")
x86_file_name  = os.path.join(current_dir,"snowboy", "_snowboydetect_x86.so")
arm_file_name  = os.path.join(current_dir,"snowboy", "_snowboydetect_arm.so")
if os.path.exists(link_file_name):
    os.remove(link_file_name)
if platform.machine() == "x86_64":
    os.system("ln -s %s %s"%(x86_file_name,link_file_name))
else:
    os.system("ln -s %s %s"%(arm_file_name,link_file_name))

from snowboy import snowboydecoder
import signal

interrupted = False
asr_id_pub = None

def signal_handler(signal, frame):
    global interrupted
    exit(0)
    interrupted = True

# capture SIGINT signal, e.g., Ctrl+C
signal.signal(signal.SIGINT, signal_handler)

#Returns True if the main loop needs to stop.
def interrupt_callback():
    global interrupted
    if interrupted == True:
        exit(0)
    return interrupted



def detectedCallback():
    print("已检测到关键词")
    msg = Int32()
    msg.data = 2
    asr_id_pub.publish(msg)



if __name__ == "__main__":
    print('Listening... Press Ctrl+C to exit')

    rospy.init_node("audio")
    asr_id_pub = rospy.Publisher('/asr_id',Int32, queue_size=10) #ASR ID发布

    pmdl_file_name  = os.path.join(current_dir,"snowboy","resources", "alexa.umdl")
    models = [
    pmdl_file_name
    ]
    detector = snowboydecoder.HotwordDetector(models, sensitivity=0.5) #灵敏度

    detectedCallbacks = [
        detectedCallback,
    ]

    # main loop
    detector.start(detected_callback=detectedCallbacks,
                   audio_recorder_callback=None, #唤醒后的录音函数
                   interrupt_check=interrupt_callback,
                   sleep_time=0.03, #0.03s=30ms检查一次pyaudio录制是否OK
                   silent_count_threshold=5, #10 沉默等待时间
                   recording_timeout=60)

    detector.terminate()

