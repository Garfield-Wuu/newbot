import cv2
from pyzbar import pyzbar
import os

def scan_qrcode(frame):
    data = pyzbar.decode(frame)
    return data[0].data.decode('utf-8')

def get_qr_code_from_dev_video(cap_device=0):
    cap = cv2.VideoCapture(cap_device)
    while True:
        ret, frame = cap.read()
        if not ret:
            print("cap read error")
            cap.release()
            return False,"cap read error"
            
        # 解析二维码
        text = None
        try:
            text = scan_qrcode(frame)
        except Exception as e: #没有二维码的情况会进入异常
            continue
            
        if text:
            print("text=",text)
            cap.release()
            return True,text
        else:
            print("text is None")

if __name__ == "__main__":
    get_qr_code_from_dev_video()
            
