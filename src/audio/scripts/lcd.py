# coding=utf-8
import sys
sys.path.append("/home/orangepi/.local/lib/python3.8/site-packages/")
import os
import time


import gpio
from periphery import SPI #pip3 install python-periphery
import cv2
import numpy as np

'''
屏幕接线：
         +------+-----+----------+--------+---+   PI3B   +---+--------+----------+-----+------+
         | GPIO | wPi |   Name   |  Mode  | V | Physical | V |  Mode  | Name     | wPi | GPIO |
         +------+-----+----------+--------+---+----++----+---+--------+----------+-----+------+
         |      |     |     3.3V |        |   |  1 || 2  |   |        | 5V       |     |      |
         |  140 |   0 |    SDA.2 |     IN | 1 |  3 || 4  |   |        | 5V       |     |      |
         |  141 |   1 |    SCL.2 |     IN | 1 |  5 || 6  |   |        | GND      |     |      |
         |  147 |   2 |    PWM15 |     IN | 0 |  7 || 8  | 1 | ALT1   | RXD.2    | 3   | 25   |
         |      |     |      GND |        |   |  9 || 10 | 1 | ALT1   | TXD.2    | 4   | 24   |
         |  118 |   5 | GPIO3_C6 |     IN | 0 | 11 || 12 | 0 | IN     | GPIO3_C7 | 6   | 119  |
D/C---   |  128 |   7 | GPIO4_A0 |     IN | 0 | 13 || 14 |   |        | GND      |     |      |
RES---   |  130 |   8 |    TXD.7 |     IN | 0 | 15 || 16 | 0 | IN     | RXD.7    | 9   | 131  |
VCC---   |      |     |     3.3V |        |   | 17 || 18 | 0 | IN     | GPIO4_A1 | 10  | 129  |
MOSI--   |  138 |  11 | SPI3_TXD |   ALT4 | 0 | 19 || 20 |   |        | GND      |     |      |
NULL--   |  136 |  12 | SPI3_RXD |   ALT4 | 1 | 21 || 22 | 0 | IN     | TXD.9    | 13  | 132  |
CLK---   |  139 |  14 | SPI3_CLK |   ALT4 | 0 | 23 || 24 | 1 | ALT4   | SPI3_CS1 | 15  | 134  |
GND---   |      |     |      GND |        |   | 25 || 26 | 1 | ALT4   | GPIO4_A7 | 16  | 135  |
         |   32 |  17 |    SDA.3 |     IN | 1 | 27 || 28 | 1 | IN     | SCL.3    | 18  | 33   |
         |  133 |  19 |    RXD.9 |     IN | 0 | 29 || 30 |   |        | GND      |     |      |
         |  124 |  20 | GPIO3_D4 |    OUT | 0 | 31 || 32 | 0 | IN     | PWM11    | 21  | 144  |
         |  127 |  22 | GPIO3_D7 |     IN | 0 | 33 || 34 |   |        | GND      |     |      |
         |  120 |  23 | GPIO3_D0 |     IN | 0 | 35 || 36 | 0 | IN     | GPIO3_D5 | 24  | 125  |
         |  123 |  25 | GPIO3_D3 |     IN | 0 | 37 || 38 | 0 | IN     | GPIO3_D2 | 26  | 122  |
         |      |     |      GND |        |   | 39 || 40 | 0 | IN     | GPIO3_D1 | 27  | 121  |
         +------+-----+----------+--------+---+----++----+---+--------+----------+-----+------+
         | GPIO | wPi |   Name   |  Mode  | V | Physical | V |  Mode  | Name     | wPi | GPIO |
         +------+-----+----------+--------+---+   PI3B   +---+--------+----------+-----+------+
 '''

DC =128 #写GPIO号，而不是wPi号
RES=130

X_MAX_PIXEL = 240
Y_MAX_PIXEL = 240

def delay_ms(t):
    time.sleep(t / 1000)

class Lcd:
    def __init__(self):
        # 把以下命令放在开机脚本里
        # os.system("sudo chmod 777 /sys/class/gpio/export")
        # os.system("sudo chmod 777 /dev/spidev3.0")
        # os.system("sudo chmod 777 /sys/class/gpio/export")
        #
        # if not os.path.exists("/sys/class/gpio/gpio%d"%RES):
        #     os.system("echo %d > /sys/class/gpio/export"%RES)
        #
        # if not os.path.exists("/sys/class/gpio/gpio%d"%DC):
        #     os.system("echo %d > /sys/class/gpio/export"%DC)
        #
        # os.system("sudo chmod 777 /sys/class/gpio/gpio%d/value"%RES)
        # os.system("sudo chmod 777 /sys/class/gpio/gpio%d/direction"%RES)
        # os.system("sudo chmod 777 /sys/class/gpio/gpio%d/value"%DC)
        # os.system("sudo chmod 777 /sys/class/gpio/gpio%d/direction"%DC)

        spi_mode = 0
        #注意：这里SPI频率设置为50MHz及其以上会在扬声器产生噪音，所以必须设置到50MHz以下
        #spi_hz = 1000 * 1000 * 49  #* 1000 #实际只有几十M,写再大速度并不会无限制变快
        spi_hz = 1000 * 1000 * 40
        self.spi = SPI("/dev/spidev3.0", spi_mode, spi_hz)
        print("spi mode=", spi_mode, "freq=", spi_hz / 1000000,"MHz")
        self.LCD_GPIO_Init()  # gpio初始化
        self.Lcd_Reset()  # RES硬件复位
        self.lcd_init()
        print("lcd init ok")

    def __del__(self):
        self.lcd_clear()
        self.spi.close()
        print("spi close")


    def gen_image_bytes(self,image):
        #image = cv2.flip(image,1) #水平镜像
        image = cv2.cvtColor(image,cv2.COLOR_BGR2BGR565)
        #高8位和低8位互换
        temp = image[:, :, 0].copy()
        image[:, :, 0] = image[:, :, 1]
        image[:, :, 1] = temp

        image_bytes = bytes(image.flatten())
        return image_bytes

    def display(self,image):
        image_bytes = self.gen_image_bytes(image)
        h,w,c = image.shape
        #居中显示
        self.SPI_WriteArray((X_MAX_PIXEL - w) // 2, (Y_MAX_PIXEL - h) // 2, w, h, image_bytes)

    def lcd_clear(self):
        zero_list = [0x00 for i in range(X_MAX_PIXEL * Y_MAX_PIXEL * 2)]
        self.SPI_WriteArray(0, 0, X_MAX_PIXEL, Y_MAX_PIXEL, zero_list)

    def SPI_WriteData(self,data): # 传输单个Byte
        self.spi.transfer([data])   #参数类型必须为bytes, bytearray, or list

    def SPI_WriteArray(self,x_start, y_start, w, h, data_array): #传输多个Byte

        self.Lcd_SetRegion(x_start, y_start, x_start + w - 1, y_start + h - 1)

        length = 4096  # 最大一次发送4096字节,继续加大会报错
        num = w * h * 2 // length
        for i in range(num): #分包发送 240*240*2/4096=28.125包
            self.spi.transfer(data_array[length * i: length * (i + 1)])

        self.spi.transfer(data_array[length * num: w * h * 2])

    def LCD_GPIO_Init(self):
        gpio.setup(DC, "out")
        gpio.setup(RES, "out")
        gpio.set(DC, 1)
        gpio.set(RES, 1) # 正常工作时，RES必须输出1

    def Lcd_WriteIndex(self,Index):
        gpio.set(DC, 0)  # DC = 0 表示写寄存器
        self.SPI_WriteData(Index)
        gpio.set(DC, 1)  # DC = 1 表示写数据

    def Lcd_WriteData(self,Data):
        self.SPI_WriteData(Data)

    def Lcd_SetRegion(self,x_start, y_start, x_end, y_end):
        self.Lcd_WriteIndex(0x2a) # 列地址设置
        self.Lcd_WriteData(x_start >> 8)  # 先传高8位
        self.Lcd_WriteData(x_start & 0xff)  # 再传低8位
        self.Lcd_WriteData(x_end >> 8)
        self.Lcd_WriteData(x_end & 0xff)

        self.Lcd_WriteIndex(0x2b) # 行地址设置
        self.Lcd_WriteData(y_start >> 8)
        self.Lcd_WriteData(y_start & 0xff)
        self.Lcd_WriteData(y_end >> 8)
        self.Lcd_WriteData(y_end & 0xff)

        self.Lcd_WriteIndex(0x2C) #储存器写

    def Lcd_Reset(self):
        gpio.set(RES, 0)
        delay_ms(50)
        gpio.set(RES, 1)
        delay_ms(50)

    def lcd_init(self):
        self.Lcd_WriteIndex(0xEF)
        self.Lcd_WriteIndex(0xEB)
        self.Lcd_WriteData(0x14) 

        self.Lcd_WriteIndex(0xFE)			 
        self.Lcd_WriteIndex(0xEF) 

        self.Lcd_WriteIndex(0xEB)	
        self.Lcd_WriteData(0x14) 

        self.Lcd_WriteIndex(0x84)			
        self.Lcd_WriteData(0x40) 

        self.Lcd_WriteIndex(0x85)			
        self.Lcd_WriteData(0xFF) 

        self.Lcd_WriteIndex(0x86)			
        self.Lcd_WriteData(0xFF) 

        self.Lcd_WriteIndex(0x87)			
        self.Lcd_WriteData(0xFF)

        self.Lcd_WriteIndex(0x88)			
        self.Lcd_WriteData(0x0A)

        self.Lcd_WriteIndex(0x89)			
        self.Lcd_WriteData(0x21) 

        self.Lcd_WriteIndex(0x8A)			
        self.Lcd_WriteData(0x00) 

        self.Lcd_WriteIndex(0x8B)			
        self.Lcd_WriteData(0x80) 

        self.Lcd_WriteIndex(0x8C)			
        self.Lcd_WriteData(0x01) 

        self.Lcd_WriteIndex(0x8D)			
        self.Lcd_WriteData(0x01) 

        self.Lcd_WriteIndex(0x8E)			
        self.Lcd_WriteData(0xFF) 

        self.Lcd_WriteIndex(0x8F)			
        self.Lcd_WriteData(0xFF) 


        self.Lcd_WriteIndex(0xB6)
        self.Lcd_WriteData(0x00)
        self.Lcd_WriteData(0x20)

        self.Lcd_WriteIndex(0x36)

        USE_HORIZONTAL = 2
        if(USE_HORIZONTAL==0): self.Lcd_WriteData(0x08)
        elif(USE_HORIZONTAL==1): self.Lcd_WriteData(0xC8)
        elif(USE_HORIZONTAL==2): self.Lcd_WriteData(0x68)
        else:  self.Lcd_WriteData(0xA8)

        self.Lcd_WriteIndex(0x3A)			
        self.Lcd_WriteData(0x05) 

        self.Lcd_WriteIndex(0x90)			
        self.Lcd_WriteData(0x08)
        self.Lcd_WriteData(0x08)
        self.Lcd_WriteData(0x08)
        self.Lcd_WriteData(0x08) 

        self.Lcd_WriteIndex(0xBD)			
        self.Lcd_WriteData(0x06)

        self.Lcd_WriteIndex(0xBC)			
        self.Lcd_WriteData(0x00)	

        self.Lcd_WriteIndex(0xFF)			
        self.Lcd_WriteData(0x60)
        self.Lcd_WriteData(0x01)
        self.Lcd_WriteData(0x04)

        self.Lcd_WriteIndex(0xC3)			
        self.Lcd_WriteData(0x13)
        self.Lcd_WriteIndex(0xC4)			
        self.Lcd_WriteData(0x13)

        self.Lcd_WriteIndex(0xC9)			
        self.Lcd_WriteData(0x22)

        self.Lcd_WriteIndex(0xBE)			
        self.Lcd_WriteData(0x11) 

        self.Lcd_WriteIndex(0xE1)			
        self.Lcd_WriteData(0x10)
        self.Lcd_WriteData(0x0E)

        self.Lcd_WriteIndex(0xDF)			
        self.Lcd_WriteData(0x21)
        self.Lcd_WriteData(0x0c)
        self.Lcd_WriteData(0x02)

        self.Lcd_WriteIndex(0xF0)   
        self.Lcd_WriteData(0x45)
        self.Lcd_WriteData(0x09)
        self.Lcd_WriteData(0x08)
        self.Lcd_WriteData(0x08)
        self.Lcd_WriteData(0x26)
        self.Lcd_WriteData(0x2A)

        self.Lcd_WriteIndex(0xF1)    
        self.Lcd_WriteData(0x43)
        self.Lcd_WriteData(0x70)
        self.Lcd_WriteData(0x72)
        self.Lcd_WriteData(0x36)
        self.Lcd_WriteData(0x37)  
        self.Lcd_WriteData(0x6F)


        self.Lcd_WriteIndex(0xF2)   
        self.Lcd_WriteData(0x45)
        self.Lcd_WriteData(0x09)
        self.Lcd_WriteData(0x08)
        self.Lcd_WriteData(0x08)
        self.Lcd_WriteData(0x26)
        self.Lcd_WriteData(0x2A)

        self.Lcd_WriteIndex(0xF3)   
        self.Lcd_WriteData(0x43)
        self.Lcd_WriteData(0x70)
        self.Lcd_WriteData(0x72)
        self.Lcd_WriteData(0x36)
        self.Lcd_WriteData(0x37) 
        self.Lcd_WriteData(0x6F)

        self.Lcd_WriteIndex(0xED)	
        self.Lcd_WriteData(0x1B) 
        self.Lcd_WriteData(0x0B) 

        self.Lcd_WriteIndex(0xAE)			
        self.Lcd_WriteData(0x77)

        self.Lcd_WriteIndex(0xCD)			
        self.Lcd_WriteData(0x63)		


        self.Lcd_WriteIndex(0x70)			
        self.Lcd_WriteData(0x07)
        self.Lcd_WriteData(0x07)
        self.Lcd_WriteData(0x04)
        self.Lcd_WriteData(0x0E) 
        self.Lcd_WriteData(0x0F) 
        self.Lcd_WriteData(0x09)
        self.Lcd_WriteData(0x07)
        self.Lcd_WriteData(0x08)
        self.Lcd_WriteData(0x03)

        self.Lcd_WriteIndex(0xE8)			
        self.Lcd_WriteData(0x34)

        self.Lcd_WriteIndex(0x62)			
        self.Lcd_WriteData(0x18)
        self.Lcd_WriteData(0x0D)
        self.Lcd_WriteData(0x71)
        self.Lcd_WriteData(0xED)
        self.Lcd_WriteData(0x70) 
        self.Lcd_WriteData(0x70)
        self.Lcd_WriteData(0x18)
        self.Lcd_WriteData(0x0F)
        self.Lcd_WriteData(0x71)
        self.Lcd_WriteData(0xEF)
        self.Lcd_WriteData(0x70) 
        self.Lcd_WriteData(0x70)

        self.Lcd_WriteIndex(0x63)			
        self.Lcd_WriteData(0x18)
        self.Lcd_WriteData(0x11)
        self.Lcd_WriteData(0x71)
        self.Lcd_WriteData(0xF1)
        self.Lcd_WriteData(0x70) 
        self.Lcd_WriteData(0x70)
        self.Lcd_WriteData(0x18)
        self.Lcd_WriteData(0x13)
        self.Lcd_WriteData(0x71)
        self.Lcd_WriteData(0xF3)
        self.Lcd_WriteData(0x70) 
        self.Lcd_WriteData(0x70)

        self.Lcd_WriteIndex(0x64)			
        self.Lcd_WriteData(0x28)
        self.Lcd_WriteData(0x29)
        self.Lcd_WriteData(0xF1)
        self.Lcd_WriteData(0x01)
        self.Lcd_WriteData(0xF1)
        self.Lcd_WriteData(0x00)
        self.Lcd_WriteData(0x07)

        self.Lcd_WriteIndex(0x66)			
        self.Lcd_WriteData(0x3C)
        self.Lcd_WriteData(0x00)
        self.Lcd_WriteData(0xCD)
        self.Lcd_WriteData(0x67)
        self.Lcd_WriteData(0x45)
        self.Lcd_WriteData(0x45)
        self.Lcd_WriteData(0x10)
        self.Lcd_WriteData(0x00)
        self.Lcd_WriteData(0x00)
        self.Lcd_WriteData(0x00)

        self.Lcd_WriteIndex(0x67)			
        self.Lcd_WriteData(0x00)
        self.Lcd_WriteData(0x3C)
        self.Lcd_WriteData(0x00)
        self.Lcd_WriteData(0x00)
        self.Lcd_WriteData(0x00)
        self.Lcd_WriteData(0x01)
        self.Lcd_WriteData(0x54)
        self.Lcd_WriteData(0x10)
        self.Lcd_WriteData(0x32)
        self.Lcd_WriteData(0x98)

        self.Lcd_WriteIndex(0x74)			
        self.Lcd_WriteData(0x10)	
        self.Lcd_WriteData(0x85)	
        self.Lcd_WriteData(0x80)
        self.Lcd_WriteData(0x00) 
        self.Lcd_WriteData(0x00) 
        self.Lcd_WriteData(0x4E)
        self.Lcd_WriteData(0x00)					

        self.Lcd_WriteIndex(0x98)			
        self.Lcd_WriteData(0x3e)
        self.Lcd_WriteData(0x07)

        self.Lcd_WriteIndex(0x35)	
        self.Lcd_WriteIndex(0x21)

        self.Lcd_WriteIndex(0x11)
        self.Lcd_WriteIndex(0x29)


if __name__=="__main__":
    lcd = Lcd()

    t_cnt = 0
    for i in range(100):
        display_image = np.zeros((240, 240, 3), dtype=np.uint8)  # h,w,c 240x240x3
        cv2.putText(display_image, "%d"%(i+1), (120,120), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2)
        cv2.circle(display_image,(120,120),119,(0,0,255),1)

        start = time.time()
        lcd.display(display_image)
        t = time.time() - start
        t_ms = t*1000
        t_cnt += t

        if (30-t_ms) > 0:
            delay_ms(30-t_ms)
        print("t=%.1f ms %.1f Hz"%( t_ms, 1/t ) )

    print("mean t=%.1f ms %.1f Hz"%( t_cnt*1000 / 100, 100/t_cnt ) )
