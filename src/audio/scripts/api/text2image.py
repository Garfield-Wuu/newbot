# encoding: UTF-8
import time

import numpy as np
import requests
from datetime import datetime
from wsgiref.handlers import format_date_time
from time import mktime
import hashlib
import base64
import hmac
from urllib.parse import urlencode
import json
from PIL import Image
from io import BytesIO
import os



class AssembleHeaderException(Exception):
    def __init__(self, msg):
        self.message = msg

class Url:
    def __init__(this, host, path, schema):
        this.host = host
        this.path = path
        this.schema = schema
        pass


# calculate sha256 and encode to base64
def sha256base64(data):
    sha256 = hashlib.sha256()
    sha256.update(data)
    digest = base64.b64encode(sha256.digest()).decode(encoding='utf-8')
    return digest


def parse_url(requset_url):
    stidx = requset_url.index("://")
    host = requset_url[stidx + 3:]
    schema = requset_url[:stidx + 3]
    edidx = host.index("/")
    if edidx <= 0:
        raise AssembleHeaderException("invalid request url:" + requset_url)
    path = host[edidx:]
    host = host[:edidx]
    u = Url(host, path, schema)
    return u


# 生成鉴权url
def assemble_ws_auth_url(requset_url, method="GET", api_key="", api_secret=""):
    u = parse_url(requset_url)
    host = u.host
    path = u.path
    now = datetime.now()
    date = format_date_time(mktime(now.timetuple()))
    # print(date)
    # date = "Thu, 12 Dec 2019 01:57:27 GMT"
    signature_origin = "host: {}\ndate: {}\n{} {} HTTP/1.1".format(host, date, method, path)
    # print(signature_origin)
    signature_sha = hmac.new(api_secret.encode('utf-8'), signature_origin.encode('utf-8'),
                             digestmod=hashlib.sha256).digest()
    signature_sha = base64.b64encode(signature_sha).decode(encoding='utf-8')
    authorization_origin = "api_key=\"%s\", algorithm=\"%s\", headers=\"%s\", signature=\"%s\"" % (
        api_key, "hmac-sha256", "host date request-line", signature_sha)
    authorization = base64.b64encode(authorization_origin.encode('utf-8')).decode(encoding='utf-8')
    # print(authorization_origin)
    values = {
        "host": host,
        "date": date,
        "authorization": authorization
    }

    return requset_url + "?" + urlencode(values)

# 生成请求body体
def getBody(appid,text):
    body= {
        "header": {
            "app_id": appid,
            "uid":"123456789"
        },
        "parameter": {
            "chat": {
                "domain": "general",
                "temperature":0.5,
                "max_tokens":4096
            }
        },
        "payload": {
            "message":{
                "text":[
                    {
                        "role":"user",
                        "content":text
                    }
                ]
            }
        }
    }
    return body

# 发起请求并返回结果
def main(text,appid,apikey,apisecret):
    host = 'http://spark-api.cn-huabei-1.xf-yun.com/v2.1/tti'
    url = assemble_ws_auth_url(host,method='POST',api_key=apikey,api_secret=apisecret)
    content = getBody(appid,text)
    #print(time.time())
    response = requests.post(url,json=content,headers={'content-type': "application/json"}).text
    #print(time.time())
    return response


def get_imagedata(message):
    data = json.loads(message)
    # print("data" + str(message))
    code = data['header']['code']
    if code != 0:
        print(f'请求错误: {code}, {data}')
        return ""
    else:
        text = data["payload"]["choices"]["text"]
        imageContent = text[0]
        # if('image' == imageContent["content_type"]):
        imageBase = imageContent["content"]
        #imageName = data['header']['sid']
        #savePath = f"{imageName}.jpg"
        img_data = base64.b64decode(imageBase)
        return img_data


class XunfeiTTI():
    def __init__(self):
        pass

    def text2image(self,text_desc):
        # 运行前请配置以下鉴权三要素，获取途径：https://console.xfyun.cn/services/tti
        appid = os.environ.get('XUNFEI_APPID')     #填写控制台中获取的 APPID 信息
        api_key = os.environ.get('XUNFEI_APIKEY')    #填写控制台中获取的 APIKey 信息
        api_secret = os.environ.get('XUNFEI_APISECRET')   #填写控制台中获取的 APISecret 信息
        if appid==None or api_key==None or api_secret==None:
            print("tti please set environment variables: XUNFEI_APPID,XUNFEI_APIKEY,XUNFEI_APISECRET!!!")
            return None


        result = main(text_desc,appid=appid,apikey=api_key,apisecret=api_secret)
        img_data = get_imagedata(result)
        return img_data


def create_tti(type):
    if type == "xunfei":
        tti_client = XunfeiTTI()
    else:
        print("llm type error!")
        exit(-1)

    return tti_client



# import cv2
# from lcd import  Lcd
# lcd = Lcd()
# if __name__ == '__main__':
#     tti = XunfeiTTI()
#
#     t1 = time.time()
#     img_data = tti.text2image("画一幅蓝天白云")
#     t2 = time.time()
#     print("画画耗时 %.1f 秒"%(t2-t1))
#
#     print(type(img_data),len(img_data))
#     if len(img_data) != 0:
#         image = cv2.imdecode(np.frombuffer(img_data,dtype=np.uint8), cv2.IMREAD_COLOR)
#         resize_image = cv2.resize(image, (240, 240))
#         lcd.display(resize_image)
    # if len(img_data) != 0:
    #     with open("ai.png","wb") as f:
    #         f.write(img_data)

