
"""
This file demonstrates how to use sherpa-onnx Python API to transcribe
file(s) with a non-streaming model.
Please refer to
https://k2-fsa.github.io/sherpa/onnx/index.html
to install sherpa-onnx and to download the pre-trained models
used in this file.
"""
import time
import wave
from typing import List, Tuple
import os
import numpy as np
import sherpa_onnx
 

class Constants:
    encoder="" # or 如果用zipformer模型需要修改成zipformer的 encoder-epoch-12-avg-4.int8.onnx
    decoder="" # or 如果用zipformer模型需要修改成zipformer的decoder-epoch-12-avg-4.int8.onnx
    joiner="" # or 如果用zipformer模型需要修改成zipformer的joiner-epoch-12-avg-4.int8.onnx
    tokens="tokens.txt" # 如果用zipformer模型需要修改成zipformer的tokens.txt
    num_threads=1
    sample_rate=16000
    feature_dim=80
    decoding_method="greedy_search" # Or modified_ Beam_ Search, only used when the encoder is not empty
    contexts="" # 关键词微调，只在modified_ Beam_ Search模式下有用
    context_score=1.5
    debug=False
    modeling_unit="char"
    paraformer="model.int8.onnx" # 实际上使用的是该模型

global args,contexts_list,recognizer
args = Constants()
 
def encode_contexts(args, contexts: List[str]) -> List[List[int]]:
    tokens = {}
    with open(args.tokens, "r", encoding="utf-8") as f:
        for line in f:
            toks = line.strip().split()
            tokens[toks[0]] = int(toks[1])
    return sherpa_onnx.encode_contexts(
        modeling_unit=args.modeling_unit, contexts=contexts, sp=None, tokens_table=tokens
    )
 
 
def read_wave(wave_filename: str) -> Tuple[np.ndarray, int]:
    """
    Args:
      wave_filename:
        Path to a wave file. It should be single channel and each sample should
        be 16-bit. Its sample rate does not need to be 16kHz.
    Returns:
      Return a tuple containing:
       - A 1-D array of dtype np.float32 containing the samples, which are
       normalized to the range [-1, 1].
       - sample rate of the wave file
    """
 
    with wave.open(wave_filename) as f:
        assert f.getnchannels() == 1, f.getnchannels()
        assert f.getsampwidth() == 2, f.getsampwidth()  # it is in bytes
        num_samples = f.getnframes()
        samples = f.readframes(num_samples)
        samples_int16 = np.frombuffer(samples, dtype=np.int16)
        samples_float32 = samples_int16.astype(np.float32)
 
        samples_float32 = samples_float32 / 32768
        return samples_float32, f.getframerate()
 
# 初始化（因为用到的是paraformer，所以实际上初始化的是paraformer的识别）
def init(model_path_name):
    global args
    
    #加上绝对路径
    args.tokens = os.path.join(model_path_name,args.tokens)
    args.paraformer = os.path.join(model_path_name,args.paraformer)
    
    global recognizer
    global contexts_list
    contexts_list=[]
    if args.encoder:
        contexts = [x.strip().upper() for x in args.contexts.split("/") if x.strip()]
        if contexts:
            print(f"Contexts list: {contexts}")
        contexts_list = encode_contexts(args, contexts)
 
        recognizer = sherpa_onnx.OfflineRecognizer.from_transducer(
            encoder=args.encoder,
            decoder=args.decoder,
            joiner=args.joiner,
            tokens=args.tokens,
            num_threads=args.num_threads,
            sample_rate=args.sample_rate,
            feature_dim=args.feature_dim,
            decoding_method=args.decoding_method,
            context_score=args.context_score,
            debug=args.debug,
        )
    elif args.paraformer:
        recognizer = sherpa_onnx.OfflineRecognizer.from_paraformer(
            paraformer=args.paraformer,
            tokens=args.tokens,
            num_threads=args.num_threads,
            sample_rate=args.sample_rate,
            feature_dim=args.feature_dim,
            decoding_method=args.decoding_method,
            debug=args.debug,
        )
 
# 语音识别
# *sound_files 要识别的音频路径
# return 识别后的结果
def asr(*sound_files):
    global args
    global recognizer
    global contexts_list
    start_time = time.time()
 
    streams = []
    total_duration = 0
    for wave_filename in sound_files:
        samples, sample_rate = read_wave(wave_filename)
        duration = len(samples) / sample_rate
        total_duration += duration
        if contexts_list:
            s = recognizer.create_stream(contexts_list=contexts_list)
        else:
            s = recognizer.create_stream()
        s.accept_waveform(sample_rate, samples)
 
        streams.append(s)
 
    recognizer.decode_streams(streams)
    results = [s.result.text for s in streams]
    end_time = time.time()
 
    for wave_filename, result in zip(sound_files, results):
        return f"{result}"
