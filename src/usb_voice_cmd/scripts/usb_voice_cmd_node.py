#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
USB 麦克风 → 短时能量判停录音 → Vosk 中文识别 → 匹配 wakeup_process/cfg/asr.cfg → 发布 /asr_id。

与 audio 节点共用默认麦克风时：订阅 /enable_wakeup，在其为 false 时关闭 PyAudio 流，
以便 chatgpt 路径下 record.record_audio() 独占设备（见 README）。

默认开启 offline_style_gate：按口语「小白你好」进聊天（发 chatgpt 行 asr_id）、
「小白小白」再进入指令窗口（发 wakeup 行 asr_id），避免「你好」等子串误触；与 asr.cfg 字面顺序无关，由 phrase_* 与行 key 映射实现。
"""
from __future__ import print_function

import json
import os
import struct
import threading
import time

import rospkg
import rospy
from std_msgs.msg import Bool, Int32

CHUNK = 1024
RATE = 16000
CHANNELS = 1
SAMPLE_FORMAT_BYTES = 2  # int16


def is_vosk_model_dir(path):
    """判断目录是否为 Vosk 模型根（解压后有时多一层同名子目录）。"""
    if not path or not os.path.isdir(path):
        return False
    if os.path.isfile(os.path.join(path, "am", "final.mdl")):
        return True
    if os.path.isfile(os.path.join(path, "final.mdl")):
        return True
    return False


def resolve_vosk_model_dir(user_path):
    """
    尝试 user_path；若无效再尝试 user_path/basename(user_path)（zip 解压常见套娃）。
    返回有效路径或 None。
    """
    user_path = os.path.expanduser(user_path)
    candidates = [user_path]
    base = os.path.basename(user_path.rstrip(os.sep))
    if base:
        candidates.append(os.path.join(user_path, base))
    for c in candidates:
        if is_vosk_model_dir(c):
            return c
    return None


def hex2dec_uchar(b):
    """与 wakeup_process asr.cpp 中单字节 asr_id 行为一致（两位十六进制字符串再按十进制位权解码）。"""
    s = "{:02x}".format(b & 0xFF)
    return (ord(s[0]) - ord("0")) * 10 + (ord(s[1]) - ord("0"))


def preferred_ros_int_for_line(line_1based):
    """同一逻辑行号可能对应多个字节；优先两位十六进制均为数字的取值（见 doc/ASR_ID_TABLE.md）。"""
    hits = [b for b in range(256) if hex2dec_uchar(b) == line_1based]
    if not hits:
        raise ValueError("no uchar maps to line %s" % line_1based)
    digital = [b for b in hits if all(c in "0123456789" for c in "{:02x}".format(b))]
    pick = min(digital) if digital else min(hits)
    return int(pick)


def parse_asr_cfg(path):
    """
    返回 list of dict: line_1based, key, phrases(list), ros_int
    与 asr.cpp parse_config_file 行序一致。
    """
    entries = []
    with open(path, "r", encoding="utf-8") as f:
        for line_1based, line in enumerate(f, start=1):
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            if "=" not in line or "@" not in line:
                rospy.logwarn("skip malformed asr.cfg line %d: %s", line_1based, line)
                continue
            key = line[: line.index("=")]
            cmd = line[line.index("=") + 1 : line.index("@")]
            phrases = [p.strip() for p in cmd.split("|") if p.strip()]
            ros_int = preferred_ros_int_for_line(line_1based)
            entries.append(
                {
                    "line": line_1based,
                    "key": key,
                    "phrases": phrases,
                    "ros_int": ros_int,
                }
            )
    return entries


def normalize_text(t):
    if not t:
        return ""
    t = t.replace(" ", "").replace("\u3000", "")
    return t.strip().lower()


def best_match_entries(text_norm, entries):
    """最长子串匹配 phrases；返回 (entry, score) 或 (None, 0)。"""
    best = None
    best_score = 0
    for e in entries:
        for p in e["phrases"]:
            pn = normalize_text(p)
            if not pn:
                continue
            if pn in text_norm:
                score = len(pn)
            elif text_norm in pn and len(text_norm) >= 2:
                score = len(text_norm)
            else:
                continue
            if score > best_score:
                best_score = score
                best = e
    return best, best_score


def entry_ros_int_by_key(entries, key):
    for e in entries:
        if e["key"] == key:
            return e["ros_int"]
    return None


class UsbVoiceCmdNode(object):
    def __init__(self):
        rospy.init_node("usb_voice_cmd", anonymous=False)

        self._cfg_path = rospy.get_param("~asr_cfg", "")
        if not self._cfg_path:
            try:
                self._cfg_path = os.path.join(
                    rospkg.RosPack().get_path("wakeup_process"), "cfg", "asr.cfg"
                )
            except rospkg.ResourceNotFound:
                self._cfg_path = ""
        if not self._cfg_path or not os.path.isfile(self._cfg_path):
            rospy.logfatal("asr.cfg 未找到，请设置 ~asr_cfg 或安装 wakeup_process 包: %s", self._cfg_path)
            raise RuntimeError("asr.cfg missing")

        self._entries = parse_asr_cfg(self._cfg_path)
        rospy.loginfo("loaded %d commands from %s", len(self._entries), self._cfg_path)

        # 与 MCU 行号/asr.cfg 一致：chatgpt=第2行(LLM)，wakeup_uni=第1行(短答)。语义上映射为：「小白你好」→聊天，「小白小白」→指令门闩。
        self._ros_int_chatgpt = entry_ros_int_by_key(self._entries, "chatgpt")
        self._ros_int_wakeup_uni = entry_ros_int_by_key(self._entries, "wakeup_uni")
        if self._ros_int_chatgpt is None or self._ros_int_wakeup_uni is None:
            rospy.logfatal("asr.cfg 缺少 chatgpt 或 wakeup_uni 条目")
            raise RuntimeError("asr.cfg missing gate keys")

        self._command_entries = [
            e for e in self._entries if e["key"] not in ("wakeup_uni", "chatgpt")
        ]

        self._offline_style_gate = rospy.get_param("~offline_style_gate", True)
        self._phrase_chat_norm = normalize_text(rospy.get_param("~phrase_chat", "小白你好"))
        self._phrase_cmd_gate_norm = normalize_text(rospy.get_param("~phrase_cmd_gate", "小白小白"))
        self._command_arm_timeout_sec = float(rospy.get_param("~command_arm_timeout_sec", 15.0))

        self._cmd_armed = False
        self._cmd_armed_at = 0.0

        self._model_path_param = rospy.get_param(
            "~vosk_model_path",
            os.path.expanduser("~/vosk-model-small-cn-0.22"),
        )
        try:
            from vosk import Model, KaldiRecognizer
        except ImportError:
            rospy.logfatal("请安装: pip3 install vosk")
            raise

        self._model_path = resolve_vosk_model_dir(self._model_path_param)
        if self._model_path is None:
            rospy.logfatal(
                "未找到 Vosk 模型目录（已尝试: %s 及其同名子目录）。\n"
                "请下载小型中文模型并解压，例如执行:\n"
                "  rosrun usb_voice_cmd download_vosk_model_cn.py\n"
                "或见: https://alphacephei.com/vosk/models 搜索 small-cn",
                self._model_path_param,
            )
            raise RuntimeError("vosk model missing")
        if self._model_path != os.path.normpath(os.path.expanduser(self._model_path_param)):
            rospy.loginfo("使用 Vosk 模型路径: %s", self._model_path)

        self._Model = Model
        self._KaldiRecognizer = KaldiRecognizer
        self._model = self._Model(self._model_path)

        self._input_device = rospy.get_param("~input_device_index", None)
        if self._input_device is not None:
            self._input_device = int(self._input_device)

        self._silence_threshold = int(rospy.get_param("~silence_volume_threshold", 2500))
        self._max_leading_silence_sec = float(rospy.get_param("~max_leading_silence_sec", 2.0))
        self._end_silence_sec = float(rospy.get_param("~end_silence_sec", 1.0))
        self._max_utterance_sec = float(rospy.get_param("~max_utterance_sec", 6.0))
        self._cooldown_sec = float(rospy.get_param("~cooldown_after_publish_sec", 1.5))
        self._min_speech_chunks = int(rospy.get_param("~min_speech_chunks", 4))

        wake = rospy.get_param("~wake_substrings", [])
        self._wake_substrings = [normalize_text(w) for w in wake if w]

        import pyaudio

        self._pyaudio = pyaudio
        self._pa = self._pyaudio.PyAudio()
        self._stream = None

        self._enable_wakeup = True
        self._lock = threading.Lock()
        rospy.Subscriber("/enable_wakeup", Bool, self._on_enable_wakeup, queue_size=1)

        self._pub_asr = rospy.Publisher("/asr_id", Int32, queue_size=5)

        self._last_pub_time = 0.0

        rospy.on_shutdown(self._shutdown_hook)

    def _shutdown_hook(self):
        self._close_stream()
        try:
            self._pa.terminate()
        except Exception:
            pass

    def _on_enable_wakeup(self, msg):
        with self._lock:
            self._enable_wakeup = bool(msg.data)

    def _open_stream(self):
        if self._stream is not None:
            return
        kwargs = dict(
            format=self._pyaudio.paInt16,
            channels=CHANNELS,
            rate=RATE,
            input=True,
            frames_per_buffer=CHUNK,
        )
        if self._input_device is not None:
            kwargs["input_device_index"] = self._input_device
        self._stream = self._pa.open(**kwargs)
        self._stream.stop_stream()
        rospy.loginfo("PyAudio input stream opened (device_index=%s)", self._input_device)

    def _close_stream(self):
        if self._stream is None:
            return
        try:
            self._stream.stop_stream()
            self._stream.close()
        except Exception as ex:
            rospy.logwarn("close stream: %s", ex)
        self._stream = None
        rospy.loginfo("PyAudio input stream closed (release mic for audio node)")

    def _record_utterance_pcm(self):
        """返回 bytes PCM int16 mono 或 None。"""
        chunk_ticks_per_sec = float(RATE) / float(CHUNK)
        max_chunks = int(chunk_ticks_per_sec * self._max_utterance_sec)
        lead_max = int(chunk_ticks_per_sec * self._max_leading_silence_sec)
        end_max = int(chunk_ticks_per_sec * self._end_silence_sec)

        frames = []
        silence_time = 0
        speak_time = 0
        in_speech = False

        self._stream.start_stream()
        try:
            for i in range(max_chunks):
                with self._lock:
                    if not self._enable_wakeup:
                        return None
                data = self._stream.read(CHUNK, exception_on_overflow=False)
                frames.append(data)
                n = len(data) // 2
                if n <= 0:
                    continue
                samples = struct.unpack("<{}h".format(n), data[: n * 2])
                vol = max(abs(x) for x in samples)

                if vol < self._silence_threshold:
                    silence_time += 1
                else:
                    silence_time = 0
                    speak_time += 1
                    in_speech = True

                if not in_speech and silence_time > lead_max:
                    rospy.logdebug("leading silence timeout")
                    return None

                # 勿用 i>=lead_max：lead_max 对应整段「最长前导静音」的 chunk 数（可达数秒），
                # 会强迫短句也要录满那么久才允许用尾静音结束，延迟极大。
                if in_speech and speak_time >= self._min_speech_chunks and silence_time > end_max:
                    break
        finally:
            self._stream.stop_stream()

        if speak_time < self._min_speech_chunks:
            return None
        return b"".join(frames)

    def _vosk_transcribe(self, pcm_bytes):
        rec = self._KaldiRecognizer(self._model, RATE)
        rec.SetWords(False)
        offset = 0
        chunk_b = CHUNK * SAMPLE_FORMAT_BYTES
        while offset < len(pcm_bytes):
            block = pcm_bytes[offset : offset + chunk_b]
            if len(block) < chunk_b:
                block += b"\x00" * (chunk_b - len(block))
            rec.AcceptWaveform(block)
            offset += chunk_b
        try:
            res = json.loads(rec.FinalResult())
        except Exception:
            res = {}
        text = res.get("text", "") or ""
        return text

    def _handle_offline_style(self, tn, raw_text):
        """
        模拟离线芯片门闩：仅完整「phrase_chat」走 LLM（发布 chatgpt 行 ID），
        仅完整「phrase_cmd_gate」发布 wakeup 短答并进入指令窗口；空闲时不转发动作类口令。
        """
        now = time.time()
        if self._cmd_armed and (now - self._cmd_armed_at) > self._command_arm_timeout_sec:
            rospy.loginfo(
                "指令窗口已超时，回到空闲（请先说 %s 再下指令）",
                self._phrase_cmd_gate_norm,
            )
            self._cmd_armed = False

        if not self._cmd_armed:
            if tn == self._phrase_chat_norm:
                m = Int32()
                m.data = self._ros_int_chatgpt
                return (m, "line2/chatgpt", "chatgpt")
            if tn == self._phrase_cmd_gate_norm:
                self._cmd_armed = True
                self._cmd_armed_at = now
                m = Int32()
                m.data = self._ros_int_wakeup_uni
                return (m, "line1/wakeup_uni", "wakeup_uni(cmd_gate)")
            rospy.loginfo(
                "离线门闩[空闲]: 先说「%s」进聊天或「%s」再下指令，当前忽略: %s",
                self._phrase_chat_norm,
                self._phrase_cmd_gate_norm,
                raw_text,
            )
            return None

        if tn == self._phrase_chat_norm:
            self._cmd_armed = False
            m = Int32()
            m.data = self._ros_int_chatgpt
            return (m, "line2/chatgpt", "chatgpt")
        if tn == self._phrase_cmd_gate_norm:
            self._cmd_armed_at = now
            m = Int32()
            m.data = self._ros_int_wakeup_uni
            return (m, "line1/wakeup_uni", "wakeup_uni(re-arm)")

        entry, score = best_match_entries(tn, self._command_entries)
        if entry is None or score < 2:
            rospy.loginfo("离线门闩[指令窗口]: 无匹配 asr.cfg 指令: %s", raw_text)
            return None

        self._cmd_armed = False
        m = Int32()
        m.data = entry["ros_int"]
        return (m, entry["line"], entry["key"])

    def spin(self):
        threading.Thread(target=rospy.spin, daemon=True).start()
        rate = rospy.Rate(50)
        while not rospy.is_shutdown():
            with self._lock:
                en = self._enable_wakeup
            if not en:
                self._close_stream()
                rate.sleep()
                continue

            try:
                self._open_stream()
            except Exception as ex:
                rospy.logerr_throttle(10, "open mic failed: %s", ex)
                rate.sleep()
                continue

            pcm = None
            try:
                pcm = self._record_utterance_pcm()
            except Exception as ex:
                rospy.logwarn("record: %s", ex)
                self._close_stream()
                time.sleep(0.3)
                continue

            if not pcm:
                rate.sleep()
                continue

            now = time.time()
            if now - self._last_pub_time < self._cooldown_sec:
                rate.sleep()
                continue

            try:
                text = self._vosk_transcribe(pcm)
            except Exception as ex:
                rospy.logwarn("vosk: %s", ex)
                rate.sleep()
                continue

            tn = normalize_text(text)
            if not tn:
                rate.sleep()
                continue

            rospy.loginfo("usb_voice_cmd heard: %s", text)

            if self._offline_style_gate:
                pub = self._handle_offline_style(tn, text)
                if pub is None:
                    rate.sleep()
                    continue
                msg, log_line, log_key = pub
                self._pub_asr.publish(msg)
                self._last_pub_time = time.time()
                rospy.loginfo(
                    "publish /asr_id=%d line=%s key=%s",
                    msg.data,
                    log_line,
                    log_key,
                )
                rate.sleep()
                continue

            if self._wake_substrings:
                if not any(w and w in tn for w in self._wake_substrings):
                    rospy.loginfo("wake_substrings not matched, ignore")
                    rate.sleep()
                    continue

            entry, score = best_match_entries(tn, self._entries)
            if entry is None or score < 2:
                rospy.loginfo("no asr.cfg match for: %s", text)
                rate.sleep()
                continue

            msg = Int32()
            msg.data = entry["ros_int"]
            self._pub_asr.publish(msg)
            self._last_pub_time = time.time()
            rospy.loginfo("publish /asr_id=%d line=%d key=%s", msg.data, entry["line"], entry["key"])

            rate.sleep()


def main():
    n = UsbVoiceCmdNode()
    n.spin()


if __name__ == "__main__":
    main()
