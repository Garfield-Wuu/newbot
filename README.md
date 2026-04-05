# newbot

ROS Noetic workspace for the Newbot robot on Orange Pi: voice (ASR/TTS, iFlytek Spark), USB camera + RKNN, YDLIDAR, navigation, and STM32 base control. Maintained fork with deploy scripts and docs.

| | 链接 |
|---|------|
| **Upstream（fork 来源）** | [gitee.com/cv-robot/newbot](https://gitee.com/cv-robot/newbot) |
| **本仓库（GitHub）** | [github.com/Garfield-Wuu/newbot](https://github.com/Garfield-Wuu/newbot) |
| **官方文档** | [Newbot Readthedocs — 开发 / 一键部署 / 开机脚本](https://newbot.readthedocs.io/zh-cn/latest/3_how_to_develop.html) |

**维护者**：Garfield-Wuu

**中文摘要**：香橙派 + ROS Noetic「小白」机器人 catkin 工作空间，含语音、视觉、雷达、运动等节点。

---

## 克隆后注意

| 项目 | 说明 |
|------|------|
| **ASR 模型** | `src/audio/scripts/model/` 体积大（含 ONNX 等），已写入 `.gitignore`。克隆后请从**原网盘 / NFS / 备份**拷回该目录，否则离线语音识别不可用。 |
| **编译** | `cd ~/newbot_ws`，先关闭正在运行的 ROS（如 `killall rosmaster`），再 `source /opt/ros/noetic/setup.bash && catkin_make`（内存不足时用 `-j1`）。 |
| **雷达类型** | `export LIDAR_TYPE=YDLIDAR` 或 `M1C1_MINI` 等，须与 **`~/.bashrc`** 和 **`src/config/start.sh`** 一致（见官方文档第四节）。 |

---

## 一键安装脚本说明（`scripts/install_orangepi_board.sh`）

### 适用场景与运行位置

- 脚本设计在 **香橙派（机器人板子）** 上执行，依赖 **开发电脑提供 NFS 共享**，把工程压缩包、已解压的配置目录、以及 YDLidar SDK 编译产物等放在 NFS 里，板子挂载后拷贝/安装。
- 若你 **不用 NFS**（例如只从 GitHub `git clone`），则 **不要照搬第 8 步逻辑**；需自行把工程放到 `~/newbot_ws` 并手动拷贝 `rc.local`、dtb 等（见 [官方文档「一键部署」](https://newbot.readthedocs.io/zh-cn/latest/3_how_to_develop.html#id10)）。

### 运行脚本之前必须准备好的事

1. **开发电脑（NFS 服务端）**
   - 已安装并配置 **`nfs-kernel-server`**，`/etc/exports` 中导出目录与脚本里 **`NFS_EXPORT`** 一致（示例见官方文档 [「安装和挂载 NFS」](https://newbot.readthedocs.io/zh-cn/latest/3_how_to_develop.html#nfs)）。
   - 板子与电脑 **同一局域网可达**（或热点/AP 模式可访问该 IP）。

2. **在 NFS 共享根目录下准备好两套路径（与脚本默认变量对应）**

   脚本里（挂载到板子 `~/nfs` 之后）默认约定为：

   | 变量 | 板子上挂载后的路径 | 里面要有什么 |
   |------|-------------------|--------------|
   | **`NEWBOT_RELEASE_DIR`** | `~/nfs/newbot/newbot_ws_v1.1` | 见下表「发布包目录」 |
   | **`YDLIDAR_SDK_BUILD`** | `~/nfs/newbot_ws/src/lidar_sensors/ydlidar/YDLidar-SDK/build` | 已在 **电脑侧 NFS 目录里** 对 YDLidar-SDK **cmake/make 生成好的 `build` 目录**（脚本内执行 `sudo make install`） |

   **发布包目录 `NEWBOT_RELEASE_DIR`（电脑 NFS 盘上实际路径 = `NFS_EXPORT/newbot/newbot_ws_v1.1/`）必须包含：**

   | 文件/目录 | 作用 |
   |-----------|------|
   | **`newbot_ws.zip`** | 整机工作空间压缩包；脚本会 **`cp` 到板子 `~/`**，再 **`unzip` 解压为 `~/newbot_ws`**。 |
   | **`newbot_ws/`**（与 zip 内容一致的**已解压目录**） | 脚本会从中拷贝 **`newbot_ws/src/config/rc.local` → `/etc`**，以及 **`newbot_ws/src/config/*v2*` → `/boot/dtb/rockchip`**。**仅有 zip 没有解压目录会导致第 8 步失败。** |

   也就是说：**`newbot_ws.zip` 不是放在板子 home 里等脚本来「找」，而是先放在电脑 NFS 的 `.../newbot/newbot_ws_v1.1/` 下**；脚本挂载 NFS 后从该目录拷到板子 `~`。

   **YDLidar SDK：** 在电脑 NFS 共享里维护一份 **`newbot_ws/src/lidar_sensors/ydlidar/YDLidar-SDK`**，并在该目录下建好 **`build/` 且已编译**，使板子挂载后路径 **`~/nfs/newbot_ws/src/.../build`** 存在。若你 NFS 目录名不是 `newbot_ws`，请改脚本里的 **`YDLIDAR_SDK_BUILD`**。

   **电脑侧 NFS 导出目录结构示例**（`NFS_EXPORT` 根目录，与脚本默认变量一致时）：

   ```
   <NFS_EXPORT>/
   ├── newbot/
   │   └── newbot_ws_v1.1/          ← NEWBOT_RELEASE_DIR 挂载后的相对路径
   │       ├── newbot_ws.zip        ← 必备
   │       └── newbot_ws/           ← 必备（已解压，供 rc.local / dtb）
   │           └── src/config/...
   └── newbot_ws/                   ← 与 zip 同结构的工程树，用于 YDLidar
       └── src/lidar_sensors/ydlidar/YDLidar-SDK/build/
   ```

3. **编辑脚本顶部「配置区」**

   - **`NFS_HOST` / `NFS_EXPORT`**：改为你的电脑 IP 与 exports 路径。  
   - **`NEWBOT_RELEASE_DIR`**：若发布包不在 `newbot/newbot_ws_v1.1`，改成实际目录（仍须同时含 **`newbot_ws.zip`** 与 **`newbot_ws/`** 解压树）。  
   - **`LIDAR_TYPE`**：`YDLIDAR` / `M1C1_MINI` / `M1C1_MINI_TTYUSB`，且后续务必与 **`~/newbot_ws/src/config/start.sh`** 里一致。

4. **板子侧**

   - 建议 **新镜像 / 尚未部署过 `~/newbot_ws`** 或已自行备份；脚本会 **删除已有 `~/newbot_ws` 再解压**（有 10 秒取消时间）。  
   - 若脚本放在即将被删除的旧 `~/newbot_ws` 里，**不要在 `~/newbot_ws` 内执行**；应先把 `install_orangepi_board.sh` 拷到如 **`/tmp`** 或 **`~/install_orangepi_board.sh`** 再运行（首次部署常见做法：用官方 zip 里带的脚本路径，或从 NFS 只读拷贝脚本到 `/tmp` 执行）。

5. **网络与权限**

   - 板子可 `ping` 通 `NFS_HOST`；`sudo` 可用（安装包、改 `/etc`、`/boot`）。

### 安装逻辑（脚本按顺序做了什么）

| 步骤 | 内容 |
|------|------|
| **1** | 安装 **NFS 客户端**、**avahi-daemon**；把 **`NFS_HOST:NFS_EXPORT`** 挂载到板子 **`~/nfs`**（若已挂载则跳过）。 |
| **2** | 配置 **avahi** 主机名为 `orangepi`（**同一局域网勿多台同名 `orangepi.local`**）。 |
| **3** | 配置 **中科大 ROS 源**，安装 **`ros-noetic-desktop-full`**；向 **`~/.bashrc`** 追加 **`source /opt/ros/noetic/setup.bash`**、**`ROS_IP` / `ROS_MASTER_URI`** 及所选 **`LIDAR_TYPE`**。 |
| **4** | 安装导航/建图等 **常用 ROS 包**；可选安装 **`ros-noetic-rosbridge-suite`**（默认开，可用 `INSTALL_ROSBRIDGE_SUITE=0` 关闭）。 |
| **5** | 进入 NFS 上的 **`YDLidar-SDK/build`**，执行 **`sudo make install`**（目录不存在则仅警告）。 |
| **6** | 安装 **Python/pip** 及 **`requirements` 风格的一串 pip 包**（语音、网络、加解密等）。 |
| **7** | 重装 **PulseAudio** 并设置 **默认扬声器 / USB 麦克风** 与音量、Mic 增益（设备名与实机声卡一致时才有效，否则需按 `pacmd` 输出自行改脚本）。 |
| **8** | 进入 **`NEWBOT_RELEASE_DIR`**：将 **`newbot_ws.zip` → `~/`**；拷贝 **`rc.local`、dtb**；在 **`~` 删除旧 **`newbot_ws`** 后 **`unzip newbot_ws.zip`** 得到新工作空间。 |
| **9** | 向 **`/boot/orangepiEnv.txt`** 追加 **SPI3 / UART2 / UART9** overlay（**UART2 使能后板载串口调试常失效**，请依赖 SSH/桌面）。 |
| **结束** | 打印后续需 **手动** 执行的 **catkin 编译**、**ASR 模型**、**讯飞环境变量**、**与 start.sh 对齐 LIDAR_TYPE** 的提示，然后 **`reboot`**。 |

### 脚本结束后在板子上还要做的事（官方流程一致）

```bash
killall rosmaster 2>/dev/null || true
cd ~/newbot_ws
source /opt/ros/noetic/setup.bash
catkin_make -j2    # 内存不够改用 -j1
```

- 将 **离线 ASR 模型** 拷入 **`~/newbot_ws/src/audio/scripts/model/`**（若 zip 未含或本仓库 clone 被 `.gitignore` 排除）。  
- 在 **`~/.bashrc`** 增加 **`XUNFEI_APPID` / `XUNFEI_APIKEY` / `XUNFEI_APISECRET`**（及按需其它变量）。  
- 打开 **`~/newbot_ws/src/config/start.sh`**，确认 **`LIDAR_TYPE`** 与 **`~/.bashrc`** 一致。  
- 重启后由 **`rc.local` → start.sh** 拉起的 **`roslaunch pkg_launch all.launch`** 才会使用新环境。

### 脚本文件对照

| 文件 | 用途 |
|------|------|
| **`install_orangepi_board.sh`** | **推荐使用**：变量化配置 + 上述 9 步逻辑。 |
| **`install_snapshot_2026-04-04.sh`** | 与当时 **`nfs/install.sh`** 等价的**快照**，便于与整理版对比。 |

**执行示例（请先把脚本放到不会被删除的路径，例如已从 NFS 拷到 `/tmp`）：**

```bash
bash /tmp/install_orangepi_board.sh
# 或已在本仓库内且不会执行第 8 步删目录时：
bash ~/newbot_ws/scripts/install_orangepi_board.sh
```

---

## RealVNC Server（远程桌面，可选）

在香橙派 **Ubuntu 20.04 aarch64 + LightDM/XFCE** 上安装官方 **RealVNC Server**，以 **服务模式** 共享当前图形桌面（物理显示器 `:0`），与 **xrdp（3389）** 可并存；直连 VNC 常见端口为 **5900**，注意防火墙放行。

| 项目 | 说明 |
|------|------|
| **安装脚本** | [`scripts/install_realvnc_server.sh`](scripts/install_realvnc_server.sh)：下载官方 ARM64 deb、卸载与之冲突的 **tightvncserver**、`dpkg` 安装并 `systemctl enable --now vncserver-x11-serviced.service`。 |
| **执行方式** | 在板子本机终端（需输入 `sudo` 密码）：`chmod +x ~/newbot_ws/scripts/install_realvnc_server.sh` 后执行 `sudo bash ~/newbot_ws/scripts/install_realvnc_server.sh`。 |
| **授权与连接** | 安装后在本机运行 **`sudo vnclicensewiz`**（或图形界面中的 VNC Server 向导），登录 RealVNC 账号或按向导配置直连；电脑端使用 [VNC Viewer](https://www.realvnc.com/en/connect/download/viewer/)。 |
| **仅用 IP 连不上** | 默认可能**不监听 5900**（走云中继）。若要用 **`192.168.x.x:5900` 直连**：执行 **`sudo bash ~/newbot_ws/scripts/enable_realvnc_direct_tcp.sh`**（会写 `/etc/vnc/...` 与 **`/root/.vnc/config.d/vncserver-x11.custom`**），再 **`sudo vncpasswd -service`**，**`ss -tlnp`** 应能看到 **5900**。若仍无 5900，多为 **Connect Lite 等订阅不允许 IP 直连**，只能 **Viewer 登录账号**从列表连或升级套餐。**Windows Viewer** 里该连接「属性 → Security」请**取消勾选 SSO、智能卡**，否则即使用密码也常失败。 |
| **排障** | 查看监听：`ss -tlnp`（确认 5900/5901）；服务状态：`systemctl status vncserver-x11-serviced.service`。 |

官方下载与说明：<https://www.realvnc.com/en/connect/download/vnc/linux/>

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

### 聊天模式录音：避免「还没说完就停」

- **现象**：唤醒后进聊天（如 `#chatgpt`），说一句长话或句中停顿，录音提前结束，ASR 只收到半句。
- **原因**：[`src/audio/scripts/record.py`](src/audio/scripts/record.py) 中 `record_audio()` 用能量阈值判静音；**开始说话后**，连续静音超过 **`max_silence_time_sec / 2`** 即停止。旧默认 **`max_silence_time_sec=2`** → 约 **1 秒**停顿就断句。
- **本仓库改动（2026-04）**：默认改为 **`max_silence_time_sec=4`**（说话后约 **2 秒**静音才结束）；[`src/audio/scripts/main.py`](src/audio/scripts/main.py) 聊天分支显式调用 **`record.record_audio(max_silence_time_sec=4)`**，便于按机子环境再调。
- **仍不满意时**：略增 `max_silence_time_sec`；或在噪声允许时略降 **`silence_volume_threshold`**（默认 2500，底噪大勿过小）。

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

## img_decode 性能说明（RGA / MPP 硬解码）

> 详细调试过程见 [`src/img_pipeline/img_decode/RGA_MPP_硬解码链路调试报告.md`](src/img_pipeline/img_decode/RGA_MPP_硬解码链路调试报告.md)

### 当前状态（2026-04-06 存档）

| 指标 | 值 |
|------|----|
| `/image_raw/compressed`（相机输入） | ~30Hz（MJPEG 1280×720） |
| `/camera/image_raw`（硬解 + 缩放输出） | **~25Hz** |
| MPP JPEG 硬解码 | ✅ 正常（像素非零，RGB888） |
| RGA(fd) 加速缩放 | ⚠️ 仍失败（ION 帧缓冲 PA>4GB，RGA2 无 IOMMU） |
| OpenCV 回退路径 | ✅ 先 `copyTo(heap)` 再 resize，不在 uncached 内存上操作 |
| 节点稳定性 | ✅ 无崩溃；RGA 每 30 帧自动重试 |

### 关键参数（`img_decode.launch`）

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `use_rga` | `true` | 是否尝试 RGA 加速；失败自动回退 OpenCV |
| `scale` | `0.5` | 输出分辨率缩放比（1280×720 → 640×360） |
| `fps_div` | `1` | 帧率分频（1=不分频） |

---

## 许可证与致谢

- 工程源自 **cv-robot / newbot** 生态；使用与二次发布请遵循原项目及依赖库的许可。
- 部署流程与设备树、NFS、VNC 等请以 [官方 Readthedocs](https://newbot.readthedocs.io/zh-cn/latest/3_how_to_develop.html) 为准，本 README 仅作补充。
