from aip import AipSpeech
import os

from api import offline_asr

# 读取文件
def get_file_content(filePath):
    with open(filePath, 'rb') as fp:
        return fp.read()

class BaiduASR():
    def __init__(self, **args):
        super(self.__class__, self).__init__()

        self.api_key_ok = True
        
        #如果用kaldi本地识别不用填写百度的语音识别api key
        APP_ID = os.environ.get('BAIDU_APPID')     #填写控制台中获取的 APPID 信息
        API_KEY = os.environ.get('BAIDU_APIKEY')    #填写控制台中获取的 APIKey 信息
        SECRET_KEY = os.environ.get('BADIU_APISECRET')   #填写控制台中获取的 APISecret 信息
        if APP_ID==None or API_KEY==None or SECRET_KEY==None:
            print("Please set environment variables: BAIDU_APPID,BAIDU_APIKEY,BADIU_APISECRET!!!")
            self.api_key_ok = False
            self.client = AipSpeech("", "", "")
        else:
            self.client = AipSpeech(APP_ID, API_KEY, SECRET_KEY)


    def audio_file_to_text(self, audio_file_name):
        if self.api_key_ok == False:
            return "请提醒我将百度语音识别的api key, api secret, app id设置到环境变量"

        wav_data = get_file_content(audio_file_name)

        try:
            res = self.client.asr(wav_data, 'pcm', 16000, {'dev_pid': 1537, }) # 普通话(纯中文识别)
        except: #网络错误会进except
            return ""

        #print("res=",res)
        if 'result' in res:
            res = res['result'][0]
        else:
            res = ""

        return res


class KaldiASR():
    def __init__(self):
        current_dir = os.path.dirname(os.path.realpath(__file__))  # 获取当前文件夹
        model_path_name = os.path.join(current_dir,"..","model","sherpa-onnx-paraformer-zh-small-2024-03-09")
        print("离线语音识别模型开始初始化...")
        # 初始化语音识别
        offline_asr.init(model_path_name)
        print("离线语音识别模型初始化完成")

    def audio_file_to_text(self, wav_name):
        text = offline_asr.asr(wav_name)
        #print("kaldi识别结果:",text)
        return text

def create_asr(type):
    if type == "baidu":
        asr_client = BaiduASR()
    # elif type == "vosk":
    #     asr_client = VoskASR() #vosk效果太差
    elif type == "kaldi":
        asr_client = KaldiASR()
    else:
        print("asr type error!")
        exit(-1)

    return asr_client


if __name__ == '__main__':
    asr_client = create_asr("kaldi")
    text = asr_client.audio_file_to_text("record.wav")
    print(text)

