import numpy as np
import scipy.io.wavfile as wav

def remove_silence(audio_file, threshold=0.05):
    # 读取音频文件
    sample_rate, data = wav.read(audio_file)

    # 将音频数据转换为单声道
    if len(data.shape) > 1:
        data = data[:, 0]

    # 计算音频数据的能量
    energy = np.abs(data)

    # 设置能量阈值，小于该阈值的部分被视为安静
    energy_threshold = threshold * np.max(energy)

    # 找到第一个非安静样本的索引
    start_index = np.argmax(energy > energy_threshold)

    # 找到最后一个非安静样本的索引
    end_index = len(energy) - np.argmax(np.flip(energy) > energy_threshold)

    # 裁减音频数据
    trimmed_data = data[start_index:end_index]

    # 保存裁剪后的音频文件
    wav.write("cut_audio.wav", sample_rate, trimmed_data)

    print("已成功裁剪音频文件，并保存为: cut_audio.wav")

# 使用示例
remove_silence("sound/deng.wav", threshold=0.03)
