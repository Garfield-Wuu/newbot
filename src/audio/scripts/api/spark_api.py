
import _thread as thread
import base64
import datetime
import hashlib
import hmac
import json
from urllib.parse import urlparse
import ssl
from datetime import datetime
from time import mktime
from urllib.parse import urlencode
from wsgiref.handlers import format_date_time

import websocket
from functools import partial

import threading

# x86 使用pip install websocket_client==0.53.0 , must be websocket_client==0.53.0

# 收到websocket错误的处理
def on_error(ws, error):
    print("### error:", error)


# 收到websocket关闭的处理
def on_close(ws,one,two):
    pass


# 收到websocket连接建立的处理
def on_open(ws):
    thread.start_new_thread(run, (ws,))


def run(ws, *args):
    data = json.dumps(gen_params(appid=ws.appid, query= ws.query,domain=ws.domain))
    ws.send(data)


def gen_params(appid, query, domain):
    """
    通过appid和用户的提问来生成请参数
    """
    data = {
        "header": {
            "app_id": appid,
            "uid": "1234",
            # "patch_id": []    #接入微调模型，对应服务发布后的resourceid
        },
        "parameter": {
            "chat": {
                "domain": domain,
                "temperature": 0.5,
                "max_tokens": 1024, #语音聊天最大长度短一点
                "auditing": "default",
            }
        },
        "payload": {
            "message": {
                #"text": [{"role": "user", "content": query}]
                "text": query #在传入之前已经转成了json格式，这里不需要再转换
            }
        }
    }
    return data



class Ws_Param(object):
    # 初始化
    def __init__(self, APPID, APIKey, APISecret, Spark_url):
        self.APPID = APPID
        self.APIKey = APIKey
        self.APISecret = APISecret
        self.host = urlparse(Spark_url).netloc
        self.path = urlparse(Spark_url).path
        self.Spark_url = Spark_url
        self.answer = ""

    # 生成url
    def create_url(self):
        # 生成RFC1123格式的时间戳
        now = datetime.now()
        date = format_date_time(mktime(now.timetuple()))

        # 拼接字符串
        signature_origin = "host: " + self.host + "\n"
        signature_origin += "date: " + date + "\n"
        signature_origin += "GET " + self.path + " HTTP/1.1"

        # 进行hmac-sha256进行加密
        signature_sha = hmac.new(self.APISecret.encode('utf-8'), signature_origin.encode('utf-8'),
                                 digestmod=hashlib.sha256).digest()

        signature_sha_base64 = base64.b64encode(signature_sha).decode(encoding='utf-8')

        authorization_origin = f'api_key="{self.APIKey}", algorithm="hmac-sha256", headers="host date request-line", signature="{signature_sha_base64}"'

        authorization = base64.b64encode(authorization_origin.encode('utf-8')).decode(encoding='utf-8')

        # 将请求的鉴权参数组合为字典
        v = {
            "authorization": authorization,
            "date": date,
            "host": self.host
        }
        # 拼接鉴权参数，生成url
        url = self.Spark_url + '?' + urlencode(v)
        # 此处打印出建立连接时候的url,参考本demo的时候可取消上方打印的注释，比对相同参数时生成的url与自己代码生成的url是否一致
        return url

class WsProcess():
    def __init__(self, wsParam):
        websocket.enableTrace(False)
        wsUrl = wsParam.create_url()
        self.ws = websocket.WebSocketApp(wsUrl, on_message=self.on_message, on_error=on_error, on_close=on_close, on_open=on_open)
        self.answer = ""


    # 收到websocket消息的处理
    def on_message(self, message):
        data = json.loads(message)
        code = data['header']['code']
        if code != 0:
            print(f'请求错误: {code}, {data}')
            try:
                self.ws.close()
            except Exception:
                pass
            #self.answer += data['header']['message']
        else:
            choices = data["payload"]["choices"]
            status = choices["status"]
            content = choices["text"][0]["content"]
            self.answer += content
            if status == 2:
                self.ws.close()

    def run(self, appid, domain, query):
        self.ws.appid = appid
        self.ws.query = query
        self.ws.domain = domain
        self.timeout = False
        self.timer = threading.Timer(5, self.on_timeout) #5秒等待时间，如果大模型回复的内容多，就可能等待时间比较久
        self.timer.start()
        self.ws.run_forever(sslopt={"cert_reqs": ssl.CERT_NONE})
        self.timer.cancel()
        if self.timeout:
            print("大模型网络连接超时")
            return "抱歉，我的网络好像卡住了"
        else:
            return self.answer

    def on_timeout(self):
        print("llm请求超时!")
        try:
            self.timer.cancel()
        except Exception:
            pass
        self.timeout = True
        try:
            if self.ws is not None:
                self.ws.close()
        except Exception:
            pass

def main(appid, api_secret, api_key, Spark_url, domain, query):
    wsParam = Ws_Param(appid, api_key, api_secret, Spark_url)
    ws_process = WsProcess(wsParam)
    return ws_process.run(appid, domain, query)


