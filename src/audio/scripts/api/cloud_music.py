import requests
import json
import os
import cv2
import cv2
import subprocess
import numpy as np
import pyaudio
import time
import threading
import queue


def check_url(url):
    try:
        response = requests.head(url, timeout=(2,2)) #重定向之后会返回302
        if response.status_code == 200 or response.status_code == 301 or response.status_code == 302:
            #print(f"可以访问: {url}")
            url_location = response.headers.get('Location', '')
            #print("重新定向地址:",url_location)
            if url_location == 'http://music.163.com/404' or url_location == "":
                print("链接无效，返回404网页")
                return False
            else:
                print("链接有效，重定向成功")
                return True
        else:
            print(f"链接无法访问，状态码: {response.status_code}")
            return False

    except requests.exceptions.RequestException as e:
        print(f"请求错误: {e}")
        return False


def play_cloud_music(song_name):
    search_url = "https://music.163.com/api/search/get/web"
    params = {
        "csrf_token": "",
        "hlpretag": "",
        "hlposttag": "",
        "s": song_name,
        "type": 1,
        "offset": 0,
        "total": "true",
        "limit": 10
    }

    print("搜索歌曲:",song_name)
    
    try:
        response = requests.get(search_url, params=params, timeout=(2,2))
        response.raise_for_status()
    except requests.exceptions.RequestException as e:
        print(f"search_url请求错误: {e}")
        return -1

    if response.status_code != 200:
        print("请求失败")
        return -1

    data = response.json()

    if not data.get('result') or not data['result'].get('songs'):
        print("请求错误")
        return -1

    song_ids = [song['id'] for song in data['result']['songs']]

    print("搜索完成:", song_ids)

    for song_id in song_ids:
        song_url = "https://music.163.com/song/media/outer/url?id=%s.mp3" % (song_id)
        print("检查歌曲:", song_id)
        if check_url(song_url) == False:  # 如果不能播放
            continue  # 遍历下一个ID看看能不能播放
        
        print("检查完成，开始播放")
        
        cmd = "ffplay -nodisp \"%s\" >/dev/null 2>&1 &" % (song_url)
        os.system(cmd)
        return 0  # 成功播放返回0

    return -1  # 播放失败返回-1


# https://music.163.com/song/media/outer/url?id=423997333.mp3
# [423997333, 467953710, 2092339903, 1409118269, 1498103330, 2047739094, 2122671513, 2055147655, 2062567757, 2058309266]

if __name__ == "__main__":
    play_cloud_music("小幸运")


