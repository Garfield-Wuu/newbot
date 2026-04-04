import time
import sherpa_onnx
#import soundfile as sf
import os
import threading
import queue
import numpy as np
import sounddevice as sd

class Constants:
    vits_model="model.onnx"
    vits_lexicon="lexicon.txt"
    vits_tokens="tokens.txt"
    vits_data_dir=""
    vits_dict_dir="dict"
    tts_rule_fsts="number.fst"
    sid=79
    debug=False
    provider="cpu"
    num_threads=1
    speed=1.0
    matcha_acoustic_model="model-steps-3.onnx"
    matcha_vocoder="hifigan_v2.onnx"
    matcha_lexicon="lexicon.txt"
    matcha_tokens="tokens.txt"
    matcha_dict_dir="dict"
     
     

args = Constants()

sample_queue = queue.Queue()
sample_rate = None
exit_event = threading.Event()
generate_finished = False
volume_gain=1

def generated_audio_callback(samples: np.ndarray, progress: float):
    #print("generated audio callback")
    samples*=volume_gain #幅度增大倍数
    #print(len(samples),"samples=",samples)
    sample_queue.put(samples)

    # 1 means to keep generating
    # 0 means to stop generating
    return 1

def play_audio_callback(outdata: np.ndarray, frames: int, time, status: sd.CallbackFlags):
    #print("play_audio_callback")

    if sample_queue.empty() and generate_finished:
        exit_event.set()

    #从队列中取出数据并写入outdata播放出来
    # outdata is of shape (frames, num_channels)
    if sample_queue.empty():
        outdata.fill(0)
        return

    n = 0
    while n < frames and not sample_queue.empty():
        remaining = frames - n
        k = sample_queue.queue[0].shape[0]

        if remaining <= k:
            outdata[n:, 0] = sample_queue.queue[0][:remaining]
            sample_queue.queue[0] = sample_queue.queue[0][remaining:]
            n = frames
            if sample_queue.queue[0].shape[0] == 0:
                sample_queue.get()
            break
        outdata[n : n + k, 0] = sample_queue.get()
        n += k
    if n < frames:
        outdata[n:, 0] = 0


def play_audio_thread_fun():
    #print("play audio thread start")
    with sd.OutputStream(
        channels=1,
        callback=play_audio_callback,
        dtype="float32",
        samplerate=sample_rate,
        blocksize=1024,
    ):
        exit_event.wait() #卡住直到exit_event.set()被调用
    exit_event.clear() #清除事件，以便再次触发
    #print("play audio thread exit")


def tts_init(model_path_name):
    #加上绝对路径
    args.vits_model=os.path.join(model_path_name,args.vits_model)
    args.vits_lexicon=os.path.join(model_path_name,args.vits_lexicon)
    args.vits_tokens=os.path.join(model_path_name,args.vits_tokens)
    args.tts_rule_fsts=os.path.join(model_path_name,args.tts_rule_fsts)
    global  volume_gain
    if "vits-icefall-zh-aishell3" in model_path_name:
        args.vits_dict_dir=""
        volume_gain=15
    else:
        args.vits_dict_dir=os.path.join(model_path_name,args.vits_dict_dir)
        volume_gain=1
     
    if "matcha-icefall-zh-baker" in model_path_name:
        args.matcha_acoustic_model=os.path.join(model_path_name,args.matcha_acoustic_model)
        args.matcha_vocoder=os.path.join(model_path_name,args.matcha_vocoder)
        args.matcha_lexicon=os.path.join(model_path_name,args.matcha_lexicon)
        args.matcha_tokens=os.path.join(model_path_name,args.matcha_tokens)
        args.matcha_dict_dir=os.path.join(model_path_name,args.matcha_dict_dir)
        args.matcha_data_dir=""

    #注意在sherpa_onnx升级到最新版本之后就会报这个错，因为必须要增加matcha这个参数，已修复2025.1.4
    if "matcha-icefall-zh-baker" in model_path_name:
        tts_config = sherpa_onnx.OfflineTtsConfig(
            model=sherpa_onnx.OfflineTtsModelConfig(
                vits=sherpa_onnx.OfflineTtsVitsModelConfig(
                ),
                matcha=sherpa_onnx.OfflineTtsMatchaModelConfig(
                    acoustic_model=args.matcha_acoustic_model,
                    vocoder=args.matcha_vocoder,
                    lexicon=args.matcha_lexicon,
                    tokens=args.matcha_tokens,
                    data_dir=args.matcha_data_dir,
                    dict_dir=args.matcha_dict_dir,
                ),
                provider=args.provider,
                debug=args.debug,
                num_threads=args.num_threads,
            ),
            rule_fsts=args.tts_rule_fsts,
            max_num_sentences=1,
        )
    else:
        tts_config = sherpa_onnx.OfflineTtsConfig(
            model=sherpa_onnx.OfflineTtsModelConfig(
                vits=sherpa_onnx.OfflineTtsVitsModelConfig(
                    model=args.vits_model,
                    lexicon=args.vits_lexicon,
                    data_dir=args.vits_data_dir,
                    dict_dir=args.vits_dict_dir,
                    tokens=args.vits_tokens,
                ),
                matcha=sherpa_onnx.OfflineTtsMatchaModelConfig(
                ),
                provider=args.provider,
                debug=args.debug,
                num_threads=args.num_threads,
            ),
            rule_fsts=args.tts_rule_fsts,
            max_num_sentences=1,
        )

    if not tts_config.validate():
        raise ValueError("Please check your config")

    print("start to load model...")
    start = time.time()
    tts = sherpa_onnx.OfflineTts(tts_config)
    end = time.time()
    print("load model time=%.2fs"%(end-start))

    return tts



def tts_run(tts,text_in):
    global sample_rate
    sample_rate = tts.sample_rate

    play_back_thread = threading.Thread(target=play_audio_thread_fun)#消费者
    play_back_thread.start()

    start = time.time()
    global generate_finished
    generate_finished=False
    audio = tts.generate(
        text_in,
        sid=args.sid,
        speed=args.speed,
        callback=generated_audio_callback, #生产者
    )
    generate_finished=True
    end = time.time()
    print("generated time=%.2fs"%(end-start))

    if len(audio.samples) == 0:
        print("Error in generating audios. Please read previous error messages.")
        return
    '''
    sf.write(
        "tts.wav",
        audio.samples,
        samplerate=audio.sample_rate,
        subtype="PCM_16",
    )
    '''
    play_back_thread.join() #等待线程退出

if __name__ == "__main__":
    #tts = tts_init("model/vits-icefall-zh-aishell3")
    tts = tts_init("model/matcha-icefall-zh-baker")
    tts_run(tts,"请输入你要说的话")
    while True:
        text_in = input("请输入你要说的话:")
        tts_run(tts, text_in)
        

