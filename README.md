# newbot

香橙派 + ROS Noetic 的「小白」机器人工作空间（catkin），含语音、视觉、雷达、运动等节点。

**官方开发与刷机文档（必读）**：[Newbot — 如何开发 / 一键部署 / 开机脚本等](https://newbot.readthedocs.io/zh-cn/latest/3_how_to_develop.html)

本仓库维护者：**Garfield-Wuu**（GitHub: [Garfield-Wuu/newbot](https://github.com/Garfield-Wuu/newbot)）

---

## 克隆后注意

| 项目 | 说明 |
|------|------|
| **ASR 模型** | `src/audio/scripts/model/` 体积大（含 ONNX 等），已写入 `.gitignore`。克隆后请从**原网盘 / NFS / 备份**拷回该目录，否则离线语音识别不可用。 |
| **编译** | `cd ~/newbot_ws`，先关闭正在运行的 ROS（如 `killall rosmaster`），再 `source /opt/ros/noetic/setup.bash && catkin_make`（内存不足时用 `-j1`）。 |
| **雷达类型** | `export LIDAR_TYPE=YDLIDAR` 或 `M1C1_MINI` 等，须与 **`~/.bashrc`** 和 **`src/config/start.sh`** 一致（见官方文档第四节）。 |

---

## 部署脚本（本仓库 `scripts/`）

| 文件 | 用途 |
|------|------|
| **`install_snapshot_2026-04-04.sh`** | 2026-04-04 时 NFS 侧 `install.sh` 的**原样快照**，便于与整理版 diff。 |
| **`install_orangepi_board.sh`** | **推荐使用**：顶部 **配置区** 集中填写 NFS 地址、`NEWBOT_RELEASE_DIR`、`LIDAR_TYPE` 等后再执行。默认顺带安装 **`ros-noetic-rosbridge-suite`**（可用环境变量 `INSTALL_ROSBRIDGE_SUITE=0` 关闭）。 |

执行示例：

```bash
bash ~/newbot_ws/scripts/install_orangepi_board.sh
```

---

## 近期开发记录（与官方文档互补）

以下内容来自实机联调，若与随镜像发布的旧代码不一致，以本仓库为准。

### 讯飞星火（Spark Lite）大模型

- WebSocket 地址须为 **`wss://spark-api.xf-yun.com/v1.1/chat`**（文档与控制台一致，勿用明文 `ws://`）。
- **`domain` 必须为 `lite`**（Spark Lite）；误用 `general` 易出现 **11200 / AppIdNoAuthError**。
- 在 **`~/.bashrc`** 或 **启动 `roslaunch` 的同一环境** 中配置：  
  `XUNFEI_APPID`、`XUNFEI_APIKEY`、`XUNFEI_APISECRET`。  
- **`audio/scripts/main.py` 不应再硬编码上述变量**（已移除旧逻辑），否则覆盖环境变量会导致对话恒为「这个问题我还不知道呢」。

### 语音指令：`/tts` 与 `/asr_id`

- **`wakeup_process`** 订阅 **`/asr_id`**，解析 `cfg/asr.cfg`，再发布 **`/tts`**（格式多为 `播报文案#内部key`），并执行**关雷达、运动**等 C++ 侧逻辑。
- **`audio`** 只订阅 **`/tts`**。  
  - 模拟对话可：``rostopic pub -1 /tts std_msgs/String "data: '我在#chatgpt'"``（需 `/enable_wakeup` 为 true）。  
  - **关雷达**不能仅靠 ``#off_lidar`` 的 `/tts`：应发 **`/asr_id`**（`asr.cfg` 第 51 行对应本工程映射为 **`data: 81`**），或手动 `rosservice call /stop_scan` + `/enable_lidar` false。详见联调笔记。

### 远程可视化（Foxglove）

- ARM 上 **`ros-noetic-foxglove-bridge`** 可能不可用；可使用 **`ros-noetic-rosbridge-suite`**，Foxglove 选择 **Rosbridge**，连接 **`ws://<板子IP>:9090`**。
- 大图传 `sensor_msgs/Image` 时建议调大 **`max_message_size`**，或优先使用 **CompressedImage** 话题。

### 工具脚本

- **`scripts/newbot_status_tui.py`**：终端里订阅 `/robot_state`、`/battery`、`/scan`、`/odom` 等的状态面板（需已 `source devel/setup.bash`）。
- **`src/audio/scripts/tools/chat_test.py`**：已加入 `sys.path` 处理，可在任意 cwd 运行；大模型/TTS 失败时会退出并提示，避免写坏 `tts.mp3`。

---

## ROS 启动（与官方一致）

```bash
cd ~/newbot_ws
source devel/setup.bash
roslaunch pkg_launch all.launch
```

关闭开机自启的 ROS 后再手动调试：

```bash
rosnode kill -a
# 或
killall rosmaster
```

---

## 许可证与致谢

- 工程源自 **cv-robot / newbot** 生态；使用与二次发布请遵循原项目及依赖库的许可。
- 部署流程与设备树、NFS、VNC 等请以 [官方 Readthedocs](https://newbot.readthedocs.io/zh-cn/latest/3_how_to_develop.html) 为准，本 README 仅作补充。
