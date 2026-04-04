
import os
import asyncio
import edge_tts

# import pyttsx3
# from aip import AipSpeech
# import requests

from api import database
from api import offline_tts
from api.online_tts import transl_youdao,transl_baidu,transl_sougou

class EdgeTTS():
    def __init__(self, voice="zh-CN-XiaoxiaoNeural"):
        self.voice = voice
        self.db = database.DataBase()

    async def async_get_speech(self, text):
        try:
            tts = edge_tts.Communicate(text=text, voice=self.voice)
            mp3_data = b""
            async for message in tts.stream():
                if message["type"] == "audio":
                    mp3_data += message["data"]
                else:
                    pass  # 单词offset数据不需要，跳过
            return mp3_data
        except Exception as e:
            print("edge tts合成失败:", e)
            return None

    async def run_speech(self,text):
        # Wait for at most 1 second
        try:
            return await asyncio.wait_for(self.async_get_speech(text), timeout=5) #超时5秒
        except asyncio.TimeoutError:
            print('tts timeout! 5s')
            return None

    def text_to_speech(self, text, use_database=True):
        if text=="":
            return None

        if use_database:
            mp3_data = self.db.get_audio(text)
            if mp3_data!=None:
                #print("数据条数:",self.db.get_count(),"命中数据库:",text)
                return mp3_data

        event_loop = asyncio.new_event_loop()
        mp3_data = event_loop.run_until_complete(self.run_speech(text))
        event_loop.close()

        if use_database:
            if mp3_data!=None:#在线生成的数据正常
                self.db.save_audio(text,mp3_data)#如果没有命中，则保存数据库
                #print("数据条数:",self.db.get_count(),"未命中，已在线生成并保存数据库:",text)

        return mp3_data


class YoudaoTTS():
    def __init__(self):
        self.db = database.DataBase()
        self.tts = transl_youdao.YoudaoTranslate()
        pass
    def text_to_speech(self, text, use_database=True):
        if use_database:
            mp3_data = self.db.get_audio("youdao:"+text) #加上前缀和其他TTS的结果区分保存
            if mp3_data != None:
                #print("youdao数据条数:",self.db.get_count(),"命中数据库:",text)
                return mp3_data

        mp3_data = self.tts.get_tts(text,"zh")

        if use_database:
            if mp3_data != None:  # 在线生成的数据正常
                self.db.save_audio("youdao:"+text, mp3_data)  # 如果没有命中，则保存数据库
                #print("youdao数据条数:",self.db.get_count(),"未命中，已在线生成并保存数据库:",text)
        return mp3_data

class BaiduTTS():
    def __init__(self):
        self.db = database.DataBase()
        self.tts = transl_baidu.BaiduTranslate()
        pass
    def text_to_speech(self, text, use_database=True):
        if use_database:
            mp3_data = self.db.get_audio("baidu:" + text)  # 加上前缀和其他TTS的结果区分保存
            if mp3_data != None:
                # print("数据条数:",self.db.get_count(),"命中数据库:",text)
                return mp3_data

        mp3_data = self.tts.get_tts(text, "zh")

        if use_database:
            if mp3_data != None:  # 在线生成的数据正常
                self.db.save_audio("baidu:" + text, mp3_data)  # 如果没有命中，则保存数据库
                # print("数据条数:",self.db.get_count(),"未命中，已在线生成并保存数据库:",text)
        return mp3_data

class SougouTTS():
    def __init__(self):
        self.db = database.DataBase()
        self.tts = transl_sougou.SougouTranslate()
        pass
    def text_to_speech(self, text, use_database=True):
        if use_database:
            mp3_data = self.db.get_audio("sougou:" + text)  # 加上前缀和其他TTS的结果区分保存
            if mp3_data != None:
                # print("数据条数:",self.db.get_count(),"命中数据库:",text)
                return mp3_data

        mp3_data = self.tts.get_tts(text, "zh-CHS")

        if use_database:
            if mp3_data != None:  # 在线生成的数据正常
                self.db.save_audio("sougou:" + text, mp3_data)  # 如果没有命中，则保存数据库
                # print("数据条数:",self.db.get_count(),"未命中，已在线生成并保存数据库:",text)
        return mp3_data

class EspeakTTS():
    def __init__(self):
        pass

    def text_to_speech(self, text, use_database=False):
        if text=="":
            return None
        
        '''
        #pyttsx3这个库有时候会抽风，播放不完整
        engine = pyttsx3.init()
        engine.setProperty('voice', 'zh')  # 开启支持中文
        engine.say(text)
        engine.runAndWait()
        '''

        #直接用命令行播放
        os.system('espeak -v zh "%s"'%(text))
        return None

class KaldiTTS():
    def __init__(self):
        current_dir = os.path.dirname(os.path.realpath(__file__))  # 获取当前文件夹
        model_path_name = os.path.join(current_dir,"..","model","vits-icefall-zh-aishell3")
        self.tts = offline_tts.tts_init(model_path_name)

    def text_to_speech(self, text, use_database=False):
        offline_tts.tts_run(self.tts, text)
        return None

def create_tts(type):
    if type == "edge":
        tts_client = EdgeTTS()
    elif type == "youdao":
        tts_client = YoudaoTTS()
    elif type == "baidu":
        tts_client = BaiduTTS()
    elif type == "sougou":
        tts_client = SougouTTS()
    elif type == "espeak":
        tts_client = EspeakTTS()
    elif type == "kaldi":
        tts_client = KaldiTTS()
    else:
        print("tts type error!")
        exit(-1)

    return tts_client

if __name__ == '__main__':
    for method in ("edge",): #("edge","youdao","baidu","sougou","espeak","kaldi"):
        print("--------------------------------------------------\ntts method=",method)
        tts_client = create_tts(method)
        mp3_data = tts_client.text_to_speech("已打开热点，启动后请说扫一扫开始配网",use_database=False) #主人您好，请说小白你好唤醒我，请说小白小白和我对话
        if mp3_data != None:
            with open("tts.mp3", "wb") as file:
                file.write(mp3_data)
            os.system("play tts.mp3")
            
            
