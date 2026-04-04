# -*- coding: utf-8 -*-
import base64
import json
# import re
import time
# import uuid
# from contextlib import suppress
# from hashlib import md5
# from threading import Lock

from api.online_tts.base_translate import BaseTranslate

from typing import Literal
from Crypto.Cipher import AES

def aes_encrypt(
        text: str,
        key: str,
        iv: str = None,
        mode: Literal[1, 2, 3, 5, 6] = AES.MODE_CBC,
        padding: Literal['PKCS7', 'ZeroPadding', 'ISO10126', 'AnsiX923', 'NoPadding'] = 'PKCS7'
):
    """AES加密"""
    # 根据填充模式计算填充字符，并与原文进行拼接
    text = text.encode('utf-8')
    pad_num = AES.block_size - len(text) % AES.block_size
    if padding == 'PKCS7':
        pad_text = chr(pad_num) * pad_num
    elif padding == 'ZeroPadding':
        pad_text = chr(0) * pad_num
    elif padding == 'ISO10126':
        pad_text = ''.join([chr(random.randint(0, 9)) for _ in range(pad_num - 1)]) + chr(pad_num)
    elif padding == 'AnsiX923':
        pad_text = chr(0) * (pad_num - 1) + chr(pad_num)
    else:
        pad_text = ''
    pad_text = pad_text.encode('utf-8')
    text += pad_text
    # AES加密
    key = key.encode('utf-8')
    if mode in [AES.MODE_ECB, AES.MODE_CTR]:
        cipher = AES.new(key, mode)
    else:
        iv = iv.encode('utf-8')
        cipher = AES.new(key, mode, iv)
    encrypted_bytes = cipher.encrypt(text)
    # Base64编码
    base64_bytes = base64.b64encode(encrypted_bytes)
    base64_str = base64_bytes.decode('utf-8')
    return base64_str

#注意：搜狗不支持特别长的文本
class SougouTranslate(BaseTranslate):
    def __init__(self):
        super(SougouTranslate, self).__init__()
        # 搜狗翻译主页
        self.home = 'https://fanyi.sogou.com/'
        # 获取secretCode，同时请求一下首页以更新客户端的cookies
        # 发送翻译请求时，构建请求表单中的s参数值
        #response = self._get()

    def get_tts(self, text, lan, url=None, *args, **kwargs):
        """ 获取发音
        :param text: 源文本
        :param lan: 文本语言
        :param url: 单词发音直链
        :return: 文本语音
        """
        if url:
            response = self.session.get(url)
        else:
            params = {
                'S-AppId': 102356845,
                'S-Param': aes_encrypt(
                    json.dumps({
                        'curTime': int(time.time() * 1000),
                        'text': text,
                        'spokenDialect': lan,
                        'rate': '0.8'
                    }),
                    '76350b1840ff9832eb6244ac6d444366',
                    base64.b64decode('AAAAAAAAAAAAAAAAAAAAAA==').decode()
                )
            }
            path = 'openapi/external/getWebTTS'
            response = self._get(path, params)
        if response==None:
            return None

        if len(response.content)==0:
            return None

        return response.content


if __name__ == '__main__':
    st = SougouTranslate()
    tts = st.get_tts("喵~好的呀，让我来给你讲一个有趣的猫和老鼠的故事吧！从前有只聪明的老鼠，它总能想出各种方法躲过猫的追捕。但是有一天，它不小心被猫抓住了。就在猫准备吃掉老鼠的时候，老鼠突然开口说话了：猫大哥，您看我这么可爱，不如您放了我吧。我可以做您的好朋友，一起玩乐呢！","zh-CHS")
    print("tts",len(tts))
    mp3_data = tts
    if mp3_data != None:
        with open("tts.mp3", "wb") as file:
            file.write(mp3_data)
        import os
        os.system("play tts.mp3")
        
        
