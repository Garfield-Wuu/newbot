# -*- coding: utf-8 -*-
# from hashlib import md5
# from threading import Lock
# from urllib.parse import parse_qs

from api.online_tts.base_translate import BaseTranslate

#有道音色最好
class YoudaoTranslate(BaseTranslate):
    def __init__(self):
        super(YoudaoTranslate, self).__init__()
        # 有道词典主页
        self.home = 'https://dict.youdao.com/'
        # 请求一下首页以更新客户端的cookies
        #self._get()
 
 
    def get_tts(self, text, lan='', type_=2, *args, **kwargs):
        """ 获取发音
        :param text: 源文本
        :param lan: 文本语言
        :param type_: 发音类型
        :return: 文本语音
        """
        path = 'dictvoice'
        params = {'audio': text, 'le': lan, 'type': type_}
        response = self._get(path, params)
        if response==None:
            return None

        if len(response.content)==0:
            return None

        return response.content


if __name__ == '__main__':
    yt = YoudaoTranslate()
    tts = yt.get_tts("十四届全国人大二次会议将于3月11日下午3时在北京人民大会堂举行闭幕会。届时，中央广播电视总台所属中央电视台综合频道、新闻频道、中文国际频道、中国国际电视台各外语频道、4K超高清频道，中央人民广播电台中国之声、大湾区之声、中国国际广播电台环球资讯广播等频率将进行现场直播；央视新闻、央视频、央视网等中央重点新媒体平台将同步转播。","zh")
    print("tts",len(tts))
    mp3_data = tts
    if mp3_data != None:
        with open("tts.mp3", "wb") as file:
            file.write(mp3_data)
        import os
        os.system("play tts.mp3")
    
