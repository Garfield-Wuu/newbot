# -*- coding: utf-8 -*-
# import json
# import math
# import re
import os

from api.online_tts.base_translate import BaseTranslate

class BaiduTranslate(BaseTranslate):
    def __init__(self):
        super(BaiduTranslate, self).__init__()
        # 百度翻译主页
        self.home = 'https://fanyi.baidu.com/'
        # 请求一下首页以更新客户端的cookies
        #self._get()
        # 获取token（必须先更新客户端的cookies，再次请求首页时才会有token）
        # 发送翻译请求时需携带此token
        #response = self._get()

    def get_tts(self, text, lan, *args, **kwargs):
        """ 获取发音

        :param text: 源文本
        :param lan: 文本语言
        :return: 文本语音
        """
        spd = 5 if lan == 'zh' else 3
        path = 'gettts'
        params = {'lan': lan, 'text': text, 'spd': spd, 'source': 'web'}
        response = self._get(path, params)
        if response==None:
            return None

        if len(response.content)==0:
            return None

        return response.content


if __name__ == '__main__':
    bt = BaiduTranslate()

    tts = bt.get_tts("喵~好的呀，让我来给你讲一个有趣的猫和老鼠的故事吧！从前有只聪明的老鼠，它总能想出各种方法躲过猫的追捕。但是有一天，它不小心被猫抓住了。就在猫准备吃掉老鼠的时候，老鼠突然开口说话了：“猫大哥，您看我这么可爱，不如您放了我吧。我可以做您的好朋友，一起玩乐呢！”猫听了之后，被老鼠的机智和可爱所吸引，于是就放过了它。从此以后，猫和老鼠成为了最好的朋友，一起度过了许多快乐的时光。","zh") #(*explanations[0]['symbols'][0][1])
    print("tts",len(tts))
    mp3_data = tts
    if mp3_data != None:
        with open("tts.mp3", "wb") as file:
            file.write(mp3_data)
        os.system("play tts.mp3")

