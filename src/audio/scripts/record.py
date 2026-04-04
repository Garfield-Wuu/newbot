import time
import pyaudio
import wave
import time
import os
import struct

chunk = 1024  # 每个缓冲区的帧数
sample_format = pyaudio.paInt16  # 采样位数
channels = 1  # 声道数
rate = 16000  # 采样率（每秒采样点数）
pa = pyaudio.PyAudio()
stream = pa.open(format=sample_format,
                 channels=channels,
                 rate=rate,
                 frames_per_buffer=chunk,
                 input=True)
stream.stop_stream()

def record_audio(max_time_sec = 10, max_silence_time_sec = 2, silence_volume_threshold = 2500): #板子上2500(有风扇和雷达噪音干扰),电脑上1000
    
    frames = []
    silence_time = 0
    speak_time = 0

    print('开始录音...')

    t1 = time.time()

    stream.start_stream()
    for i in range(0, int(rate / chunk * max_time_sec)):  # 最大录制时间为10秒
        data = stream.read(chunk)

        frames.append(data)
        # 判断是否已经停止说话
        # 将bytes类型的音频数据转换为int16格式
        data_list = struct.unpack(f'{chunk*channels}h', data)
        # 计算音频数据的最大值
        volume_value = max(data_list)
        #print("%d:%d "%(i,volume_value),end=" ")
        # 判断是否已经停止说话
        if volume_value < silence_volume_threshold:
            silence_time += 1
        else:
            silence_time = 0
            speak_time += 1
        if silence_time > int(rate / chunk * max_silence_time_sec):  # 起始最大安静持续时间max_silence_time_sec秒
            print("达到最大起始安静时间 silence_time=%d speak_time=%d"%(silence_time,speak_time))
            break

        if i>=int(rate / chunk * max_silence_time_sec) and silence_time > int(rate / chunk * max_silence_time_sec/2): #起始时间结束后，安静时长变成max_silence_time_sec/2秒
            print("达到最大安静时间 silence_time=%d speak_time=%d"%(silence_time,speak_time))
            break

    t2 = time.time()

    # 停止录音
    print('录音完成 %.2f s'%(t2-t1))


    stream.stop_stream()
    # stream.close()
    # pa.terminate()

    if speak_time<=2: #只有两次或者两次以下的时间超过安静阈值，说明没有人说话
        return None #返回None表示录音文件无效
    
    # 保存录音文件
    filename = os.path.join(os.path.expanduser('~'),".ros/record.wav")
    wf = wave.open(filename, 'wb')
    wf.setnchannels(channels)
    wf.setsampwidth(pa.get_sample_size(sample_format))
    wf.setframerate(rate)
    wf.writeframes(b''.join(frames))
    wf.close()
    print('File saved as', filename)

    return filename


def get_mic_value():
    stream.start_stream()

    data = stream.read(chunk)

    # 将bytes类型的音频数据转换为int16格式
    data_list = struct.unpack(f'{chunk*channels}h', data)
    # 计算音频数据的最大值
    max_volume_value = max(data_list)

    stream.stop_stream()

    return max_volume_value


if __name__ == "__main__":
    record_audio()

