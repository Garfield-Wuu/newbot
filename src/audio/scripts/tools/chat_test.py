import os
import sys

# 保证可找到同级的 api、record 等包（勿依赖当前工作目录）
_scripts_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _scripts_root not in sys.path:
    sys.path.insert(0, _scripts_root)

from api import spark_api as spark
from api import llm
from api import tts
from api import asr
import record
#vosk、coqui、 kaldi

if __name__ == '__main__':
    llm_client = llm.create_llm("xunfei")
    tts_client = tts.create_tts("edge")
    asr_client = asr.create_asr("kaldi")
    
    print("0.开始录音")
    audio_file_name = record.record_audio()
    if audio_file_name==None:
        print("没有检测到语音")
        exit(0)
    
    print("1.开始语音识别")
    text_input = asr_client.audio_file_to_text(audio_file_name)
    
    #question = input("请输入问题：")
    #text_input = "你是谁"
    print("input=",text_input)
    print("2.开始大模型对话")
    text_input = "你是我的宠物机器人，你的名字叫小白，你要用一只软萌机器人的语气和我聊天，回答字数必须小于50字，现在我聊天的内容是：" + text_input
    answer = llm_client.chat(text_input)
    print("answer=", answer)

    if not answer or not str(answer).strip():
        print("大模型无有效回复，跳过语音合成与播放（若见 11200/AppIdNoAuthError，请检查讯飞控制台：AppId、APIKey、APISecret 须为同一应用，且已开通星火对话接口）")
        sys.exit(1)

    print("3.开始文本转语音")
    mp3_data = tts_client.text_to_speech(answer, use_database=False)

    if mp3_data is None:
        print("TTS 失败，未生成音频")
        sys.exit(1)

    print("4.开始播放")
    with open("tts.mp3", "wb") as file:
        file.write(mp3_data)
    os.system("play tts.mp3")
    
    
