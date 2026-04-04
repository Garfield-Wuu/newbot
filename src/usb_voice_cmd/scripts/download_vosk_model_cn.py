#!/usr/bin/env python3
"""下载并解压 Vosk 小型中文模型到 ~/vosk-model-small-cn-0.22（与节点默认路径一致）。"""
import os
import shutil
import sys
import tempfile
import urllib.request
import zipfile

ZIP_URL = "https://alphacephei.com/vosk/models/vosk-model-small-cn-0.22.zip"
ZIP_NAME = "vosk-model-small-cn-0.22.zip"
HOME = os.path.expanduser("~")
DEST = os.path.join(HOME, "vosk-model-small-cn-0.22")


def _has_model(path):
    if not os.path.isdir(path):
        return False
    if os.path.isfile(os.path.join(path, "am", "final.mdl")):
        return True
    if os.path.isfile(os.path.join(path, "final.mdl")):
        return True
    return False


def main():
    if _has_model(DEST):
        print("模型已存在: {}".format(DEST))
        return 0

    print("下载: {}".format(ZIP_URL))
    tmp = tempfile.mkdtemp(prefix="vosk_dl_")
    try:
        zpath = os.path.join(tmp, ZIP_NAME)
        urllib.request.urlretrieve(ZIP_URL, zpath)
        print("解压到 {} ...".format(HOME))
        with zipfile.ZipFile(zpath, "r") as zf:
            zf.extractall(HOME)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    if _has_model(DEST):
        print("完成。默认路径: {}".format(DEST))
        return 0
    inner = os.path.join(DEST, os.path.basename(DEST.rstrip(os.sep)))
    if _has_model(inner):
        print("完成。模型在嵌套目录: {}".format(inner))
        print("usb_voice_cmd 节点会自动使用该路径。")
        return 0
    print("解压完成但未在预期路径检测到模型，请检查 {} 下目录结构。".format(HOME), file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
