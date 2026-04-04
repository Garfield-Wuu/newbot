import socket
import os
import time
import fcntl
import struct
import subprocess




def get_host_ip(interface):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        ip_address = socket.inet_ntoa(fcntl.ioctl(
            sock.fileno(),
            0x8915,  # SIOCGIFADDR
            struct.pack('256s', bytes(interface[:15], 'utf-8'))
        )[20:24])
        return ip_address
    except IOError:
        return "空"

def get_wifi_ssid():  
    # 尝试使用iwconfig获取SSID  
    result = subprocess.run(['iwconfig'], capture_output=True, text=True)  
    ssid = None
    for line in result.stdout.split('\n'):  
        if 'ESSID' in line:  
            ssid = line.split('"')[1]  
            break
    
    if ssid==None:
        ssid = "空"

    return ssid


def forget_all_wifi():
    cmd = "nmcli connection show | grep wifi | awk '{print $1}' | xargs -n 1 nmcli connection delete"
    print("执行命令:",cmd)
    os.system(cmd)


def connect_wifi(text_in): #WIFI:S:ChinaNet-3611;T:WPA;P:66668888;H:false;;
    SSID = None
    PWD = None

    if text_in[:5] == "WIFI:": #去除开头的WIFI:字符串
        text_in = text_in[5:]

    for txt in text_in.split(";"):
        if txt[:2]=="S:":
            SSID = txt[2:]
        elif txt[:2]=="P:":
            PWD = txt[2:]
            
    if SSID != None and PWD != None:
        print("WIFI信息:",SSID,PWD)
        
        forget_all_wifi() #先忘记所有WIFI再连接

        cmd= "echo orangepi | sudo -S create_ap --fix-unmanaged && sleep 5 && nmcli device wifi connect '%s' password '%s' && sync && sleep 5 && reboot && echo orangepi | sudo -S watchdog_test 1 &"%(SSID,PWD)
        #cmd="echo orangepi | sudo -S create_ap --fix-unmanaged && sleep 5 && nmcli device wifi connect '%s' password '%s' && sync && sleep 5 && reboot &"%(SSID,PWD)
        print("执行命令:",cmd)
        os.system(cmd)

        text_out = "已连接到网络："+SSID+"，开始重启"
    else:
        text_out = "二维码无效"
    return text_out





