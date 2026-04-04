#!/usr/bin/env python3

import sys
sys.path.append("/opt/ros/melodic/lib/python2.7/dist-packages")
sys.path.append("/opt/ros/noetic/lib/python3/dist-packages")
sys.path.append("/home/orangepi/.local/lib/python3.8/site-packages")

import numpy as np
import cv2

import rospy
from geometry_msgs.msg import Twist
from sensor_msgs.msg import Image,CompressedImage

from PyQt5 import QtCore
from PyQt5 import QtGui
from PyQt5.QtCore import QThread,Qt,pyqtSignal,QTimer
from PyQt5.QtGui import QImage
from PyQt5.QtWidgets import QApplication, QWidget, QHBoxLayout,QVBoxLayout, QLabel,QLayout,QGridLayout,QSizePolicy
from PyQt5.QtGui import QPixmap,QPalette,QBrush,QFont,QColor
from PyQt5.QtWidgets import QPushButton

import platform
if platform.machine()=="x86_64":
    ARM_SYS = False
else:
    ARM_SYS = True
print("ARM_SYS:",ARM_SYS)

class PyqtControl(QWidget):
    def __init__(self):
        super(PyqtControl, self).__init__()
        self.ui_init()

        rospy.init_node('pyqt_control', anonymous=True)
        self.pub = rospy.Publisher('/cmd_vel', Twist, queue_size=10)
        if ARM_SYS:
            self.image_sub = rospy.Subscriber('/camera/image_raw', Image, self.raw_image_callback_and_display) #本地用原图
        else:
            self.image_sub = rospy.Subscriber('/image_raw/compressed', CompressedImage,self.compressed_image_callback_and_display) #远程用压缩图
            #self.image_sub = rospy.Subscriber('/camera/image_det_track/compressed', CompressedImage,self.compressed_image_callback_and_display)
        self.cmd_vel = Twist()


    def frame_display(self,frame):
        W, H = self.label_img.width(), self.label_img.height()
        w_scale = W / frame.shape[1]
        h_scale = H / frame.shape[0]
        scale = w_scale if w_scale < h_scale else h_scale
        frame = cv2.resize(frame, (0, 0), fx=scale, fy=scale)
        h, w, c = frame.shape

        image = np.zeros((H, W, 3), np.uint8)
        image[(H - h) // 2:(H - h) // 2 + h, (W - w) // 2:(W - w) // 2 + w, :] = frame

        height, width, channel = image.shape
        pixmap = QPixmap.fromImage(QImage(image.data, width, height, channel * width, QImage.Format_RGB888))

        self.palette.setBrush(self.label_img.backgroundRole(), QBrush(pixmap))
        self.label_img.setPalette(self.palette)

    def compressed_image_callback_and_display(self,msg):
        frame = cv2.imdecode(np.frombuffer(msg.data, np.uint8), -1)
        frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        self.frame_display(frame)

    def raw_image_callback_and_display(self,msg):
        # 将ROS中的Image消息转换为numpy数组
        img_array = np.frombuffer(msg.data, np.uint8)
        frame = np.reshape(img_array, (msg.height, msg.width, -1))
        self.frame_display(frame)

    def ui_init(self):
        self.setWindowTitle('Pyqt Control')

        self.palette = QPalette()

        # 按钮设置
        self.forward_button = QPushButton('Forward')
        self.backward_button = QPushButton('Backward')
        self.left_button = QPushButton('Left')
        self.right_button = QPushButton('Right')
        self.stop_button = QPushButton('Stop')

        # 连接信号
        self.forward_button.clicked.connect(self.move_forward)
        self.backward_button.clicked.connect(self.move_backward)
        self.left_button.clicked.connect(self.move_left)
        self.right_button.clicked.connect(self.move_right)
        self.stop_button.clicked.connect(self.stop)

        buttons = [
            QLabel(),self.forward_button,QLabel(),
            self.left_button,self.stop_button,self.right_button,
            QLabel(), self.backward_button, QLabel()
        ]

        # 添加按钮到布局
        self.grid_layout = QGridLayout()
        for row in range(3):
           for col in range(3):
                buttons[row*3+col].setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding) # 必须设置大小策略
                self.grid_layout.addWidget(buttons[row*3+col], row, col)

        self.label_img = QLabel(self)
        self.label_img.setAutoFillBackground(True)
        self.label_img.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)

        self.vbox = QVBoxLayout()
        self.vbox.addWidget(self.label_img)
        self.vbox.addLayout(self.grid_layout)
        self.vbox.setStretch(0, 7)
        self.vbox.setStretch(1, 3)

        self.setLayout(self.vbox)
        self.resize(640, 480)
        self.show()

    def move_forward(self):
        self.cmd_vel.linear.x = 0.08
        self.cmd_vel.angular.z = 0.0
        self.pub.publish(self.cmd_vel)

    def move_backward(self):
        self.cmd_vel.linear.x = -0.08
        self.cmd_vel.angular.z = 0.0
        self.pub.publish(self.cmd_vel)

    def move_left(self):
        self.cmd_vel.linear.x = 0.0
        self.cmd_vel.angular.z = 1.0
        self.pub.publish(self.cmd_vel)

    def move_right(self):
        self.cmd_vel.linear.x = 0.0
        self.cmd_vel.angular.z = -1.0
        self.pub.publish(self.cmd_vel)

    def stop(self):
        self.cmd_vel.linear.x = 0.0
        self.cmd_vel.angular.z = 0.0
        self.pub.publish(self.cmd_vel)

if __name__ == '__main__':
    app = QApplication(sys.argv)
    pyqt_control = PyqtControl()
    sys.exit(app.exec_())
