# RGA + MPP 硬解码链路调试报告

| 项目 | 说明 |
|------|------|
| 适用范围 | `img_decode`：JPEG（压缩图）→ MPP 硬解 → RGB → 缩放 → `/camera/image_raw` |
| 工作空间 | `newbot_ws`，包名 **`img_decode`** |
| 文档版本 | 2026-04-06 修订（合并第四阶段：性能提升至 25Hz、RGA 失败自动重试、copyTo 缓存路径、DMA32 崩溃根因） |

---

## 1. 背景与数据流

```
/usb_camera → /image_raw/compressed (sensor_msgs/CompressedImage, MJPEG)
       → img_decode 订阅
       → MPP 硬解码 MJPEG → RGB888（ION/DMA_HEAP 缓冲）
       → 缩放：RGA（dma-buf fd → RGA3）优先，失败则 OpenCV（含闩锁）
       → /camera/image_raw (sensor_msgs/Image, rgb8)
```

**帧率上限说明（硬件事实）**：`v4l2-ctl --list-formats-ext` 确认该摄像头 **MJPEG 1280×720 = 30fps**（`Interval: 0.033s`）；10fps 限制属于 **YUYV 格式**，与 img_decode 使用的 MJPEG 无关。因此若 `/image_raw/compressed` 为 25～30Hz 但 `/camera/image_raw` 仅 5Hz，**瓶颈在 img_decode 的解码 / 缩放环节**，不是相机上限。

---

## 2. 第一阶段问题（RGA 加速失败 + 节点崩溃）

### 2.1 现象
- `RgaBlit fail: Invalid argument`
- dmesg：`rga_dma_buf map dma buffer error, ret[-22]`、`unsupported memory larger than 4G`
- `[img_decode-5] process has died ... exit code -11`（SIGSEGV）

### 2.2 根因
| 根因 | 说明 |
|------|------|
| **RGA core 选择** | 默认易落到 RGA2（无 IOMMU），dma-buf / 高物理地址页映射失败 |
| **MPP buffer fd** | `mpp_buffer_get_fd` 可能返回 `0`；`if (raw < 0)` 会漏判，`dup(0)` 误用 stdin |
| **SIGSEGV** | `init_packet_and_frame` 失败路径未完整回滚，`dataBuf=NULL` 仍 `decode()` → `memcpy` 崩溃 |

### 2.3 修复（仍有效）
- `get_dmabuf_fd`：`if (raw <= 0)` 判无效
- `init_packet_and_frame`：失败路径完整释放；`is_ready()` 门控
- **librga 1.9.x**：`imconfig(IM_CONFIG_SCHEDULER_CORE, …)` 在部分调用路径下 **dump 仍为 `set_core[0x0]`**，不可靠；改为 **`improcess(..., im_opt_t *opt, IM_SYNC)` 且 `opt.core = IM_SCHEDULER_RGA3_CORE0`**（见第三阶段）

---

## 3. 第二阶段问题（话题有数据但画面全黑 / 全零像素）

### 3.1 现象
- `rostopic hz /camera/image_raw` 可能有稳定频率，但 Foxglove **黑屏**或像素采样全 0
- 或 `mpp_decode not ready, skip`（初始化失败）

### 3.2 已用日志排除的结论
- JPEG 输入有效（长度、SOI `0xFFD8`）
- 黑屏阶段：MPP 输出缓冲读回全零，或 `frameOut` 无 buffer（错误 API 用法）

### 3.3 曾被误判、需更正的点
| 误判 | 更正（以 `rockchip/mpp_frame.h` 为准） |
|------|----------------------------------------|
| `fmt:65542` = ARGB8888 | **`65542 = 0x10006 = MPP_FMT_RGB888`**（`MPP_FRAME_FMT_RGB + 6`）。ARGB8888 为 `+10`（0x1000A） |
| 必须用 `mpp_buffer_group_get_external` | **错误**：external group 需 `mpp_buffer_commit` 等流程；直接 `mpp_buffer_get(external)` 会导致 **初始化失败**（`is_ready()==false`） |
| internal group 解码器不写像素 | **错误**：与上游可工作实现不符；问题在 **与上游不一致的改法**（见 3.4） |

### 3.4 第三阶段最终修复（对齐 Gitee 上游 `mpp_decode.cpp`）
参考：`https://gitee.com/cv-robot/newbot/.../img_decode/src/mpp_decode.cpp`

| 要点 | 说明 |
|------|------|
| **buffer group** | `frmGrp` / `pktGrp` 均使用 **`mpp_buffer_group_get_internal`**；**优先 ION**，失败再 **DMA_HEAP** |
| **decode 读哪一帧** | Task 模式 dequeue 后应 **`get_image(frame, image)`**（成员 `frame`，已 `mpp_frame_set_buffer(frame, frmBuf)`），**不要**误用 `mpp_task_meta_get_frame` 的 `frameOut` 当主输出（易导致空 buffer / 生命周期混乱） |
| **get_image** | RGB888：`h_stride` 常为 **字节 stride（宽×3）**，用 `wstride_px = h_stride/3` 构造 `cv::Mat(..., step=row_bytes)` |
| **DMA_BUF_SYNC** | ION 侧多为 **uncached** 映射；上游未做 sync。试验性加入的 `DMA_BUF_IOCTL_SYNC` **非必须**，已移除以降低每帧开销 |

**运行时证据（PIXEL 日志）**：`nz_1k≈1000`、`center` 非零、`buf_is_frmBuf:1`、`fd>0` → **硬解 + 缓冲有效，黑屏问题闭环**。

---

## 4. 第三阶段问题（RGA 仍报错、CPU 高、帧率约 5Hz）

### 4.1 现象
- 终端大量 `RgaBlit fail: Invalid argument`、`rga_dump_opt: set_core[0x0]`（旧版）或 `set_core[0x1]`（已指定 RGA3 仍失败）
- `rostopic hz /camera/image_raw` 约 **5～6Hz**；htop **整机 CPU 很高**

### 4.2 根因归纳
| 现象 | 根因 |
|------|------|
| `set_core[0x0]` | **`imconfig` 未生效到实际 submit**（librga 1.9.3） |
| `set_core[0x1]` 仍 `Invalid argument` | 多为 **其它节点** 的 **VA 路径** RGA（如 `rknn_yolov6` 640×360→640×352），与 `img_decode` 的 **fd 路径** 不是同一调用栈 |
| `img_decode` 闩锁 OpenCV | 首帧 `rga_resize_fd` 失败后会 **闩锁**，之后全程 **OpenCV 在 uncached 的 `cv::Mat` 视图上 resize**，极慢 → 帧率下降 |
| 整机 CPU 高 | **需区分**：htop 前列常为 **`cursor-server`（远程 IDE）**；不等价于 `img_decode` 占满 CPU |

### 4.3 工程措施（已完成）
| 文件 | 措施 |
|------|------|
| `img_decode/src/rga_resize.cpp` | `rga_resize` / `rga_resize_fd` 使用 **`improcess` + `im_opt_t.core = IM_SCHEDULER_RGA3_CORE0`** |
| `img_encode/src/rga_cvtcolor.cpp` | 同上；**首次失败后静态闩锁**，避免每帧 RGA + 刷屏 |
| `rknn_yolov6/src/utils.cpp` | `rga_resize` 同上；**失败闩锁**，回退调用方 OpenCV 路径 |

---

## 5. 第四阶段问题（帧率卡在 ~5Hz / importbuffer_fd 失败 / DMA32 崩溃）

### 5.1 现象
- `/image_raw/compressed` ≈ 30Hz（相机正常），但 `/camera/image_raw` ≈ 5Hz
- 日志 `img_decode: RGA(fd) failed -> latched OpenCV-only for this process`，之后永久停留低性能路径
- dmesg：`RGA_MMU unsupported memory larger than 4G`、`scheduler core[4]`（RGA2）反复出现
- `importbuffer_fd` **100% 失败**（日志 `H1: import_failed`）

### 5.2 根因归纳

| 根因 | 说明 |
|------|------|
| **ION buffer 物理地址 >4GB** | MPP `frmGrp` 使用普通 ION 分配，OrangePi 3B 物理内存布局中 ION 帧缓冲可能分配在 >4GB PA 区域；`importbuffer_fd` 时内核拒绝：`RGA_MMU unsupported memory larger than 4G`（core[4]=RGA2 无 IOMMU） |
| **`wrapbuffer_fd` 旧 API 无效** | 旧路径 `wrapbuffer_fd()` 不走 handle 系统，`opt.core` 指定 RGA3 不生效，仍调度到 RGA2 |
| **永久闩锁** | 首帧失败后全程 OpenCV，**没有自动重试机制** |
| **OpenCV 回退太慢** | 直接在 ION uncached `cv::Mat` 视图上做 `cv::resize`，每帧大量不可缓存内存读，速度约为 heap 上的 1/5，帧率仅 5Hz |
| **ION \| DMA32 flag 崩溃** | 尝试 `(MppBufferType)(MPP_BUFFER_TYPE_ION \| MPP_BUFFER_FLAGS_DMA32)` 传入 MPP 库，4 秒内 SIGSEGV；头文件注释 DMA32 flag 仅举例 `MPP_BUFFER_TYPE_DRM`，ION 不支持此组合 |

### 5.3 本阶段修复（已完成）

| 文件 | 改动 | 效果 |
|------|------|------|
| `src/rga_resize.cpp` | `rga_resize_fd` 改用 **`importbuffer_fd(fd, &im_handle_param_t)` → `wrapbuffer_handle_t` → `releasebuffer_handle`** 代替旧 `wrapbuffer_fd`；保留调试日志 | 正确进入内核 handle 路径 |
| `src/img_decode.cpp` | 永久闩锁改为 **冷却计数器**（`g_rga_cooldown`）：失败后计 30 帧冷却，冷却后自动重试；失败/成功均有 throttled 日志 | 不永久锁死，周期性自愈 |
| `src/img_decode.cpp` | OpenCV 回退路径：**先 `image.copyTo(cached)` 将 ION uncached Mat 拷到 heap**，再 `cv::resize(cached, resized, ...)` | **帧率从 5Hz 提升至约 25Hz** |
| `src/mpp_decode.cpp` | 记录 `packetSize = buf_size`；`decode()` 入口增加 `srcLen > packetSize` 越界检查 → return -1 | 防止压缩数据溢出 MPP 输入缓冲 |
| `src/mpp_decode.cpp` | 撤销危险的 `MPP_BUFFER_TYPE_ION \| MPP_BUFFER_FLAGS_DMA32`，回退至稳定 ION + DMA_HEAP fallback | 消除 init 阶段 SIGSEGV |

### 5.4 DMA32 路线说明（未完成，待后续）

MPP 头文件注释仅对 `MPP_BUFFER_TYPE_DRM` 举例了 flags 组合（`DRM | CONTIG`、`DRM | SECURE`）。ION 配合 DMA32 flag 在本平台 MPP 库中会触发 segfault（运行时证据：进程在 4 秒内崩溃，debug log 甚至未能写入）。

安全路线（**尚未验证，下一阶段再做**）：
```c
// 仅 DRM 明确支持 flags，可能使缓冲落在 DMA32 zone：
mpp_buffer_group_get_internal(&frmGrp,
    (MppBufferType)(MPP_BUFFER_TYPE_DRM | MPP_BUFFER_FLAGS_DMA32));
```
需先确认 `/dev/dri/` 存在且 MPP DRM allocator 可用，**不冒险**在当前稳定版中使用。

### 5.5 当前状态（本版本存档时）

| 指标 | 值 | 说明 |
|------|----|------|
| `/image_raw/compressed` | ~30Hz | 相机正常 |
| `/camera/image_raw` | **~25Hz** | copyTo + cv::resize（heap）路径 |
| MPP decode 像素 | 非零（`nz_1k≈1000`） | 硬解正常 |
| RGA(fd) | ⚠️ 仍失败（`importbuffer_fd` ION >4G PA） | **不阻塞功能**，回退 OpenCV |
| img_decode 稳定性 | ✅ 无崩溃 | bounds check + DMA32 回退 |
| RGA 重试 | 每 30 帧自动重试 | 不永久锁死 |

---

## 6. 主要改动文件（汇总）

| 文件 | 内容摘要 |
|------|----------|
| `src/mpp_decode.h` | `is_ready()`；`get_dmabuf_fd`；`get_last_rga_layout`；`packetSize` 成员；句柄默认 NULL |
| `src/mpp_decode.cpp` | 对齐上游：ION internal frmGrp/pktGrp；`get_image(frame)`；RGB888 stride；初始化失败完整回滚；**`packetSize` 记录**；**`srcLen > packetSize` 越界检查**；调试日志 PIXEL/INIT |
| `src/rga_resize.cpp` | `rga_resize_fd`：**`importbuffer_fd` + `wrapbuffer_handle_t` + `releasebuffer_handle`**（替代旧 `wrapbuffer_fd`）；**`improcess` + `opt.core=RGA3_CORE0`**；日志节流 |
| `src/img_decode.cpp` | `use_rga`；**冷却计数器重试**（每 30 帧，替代永久闩锁）；**OpenCV 回退先 `copyTo(cached)` 再 resize**；`lazy_compressed_subscribe` |
| `launch/img_decode.launch` | `use_rga`、`scale`、`fps_div` 等 |
| `../img_encode/src/rga_cvtcolor.cpp` | `improcess` + RGA3 core；失败闩锁 |
| `../rknn_yolov6/src/utils.cpp` | `rga_resize`：`improcess` + RGA3 core；失败闩锁 |

---

## 7. 结论与当前状态（存档时）

| 子问题 | 状态 | 说明 |
|--------|------|------|
| SIGSEGV / 半初始化 decode | ✅ 已修复 | `is_ready()` 门控 + 完整回滚 + `packetSize` 边界检查 |
| MPP 黑屏 / 全零像素 | ✅ 已闭环 | 对齐上游 buffer + `get_image(frame)` + RGB888 stride |
| `mpp_decode not ready` | ✅ 已避免 | 撤销错误的 external-only 初始化路径 |
| img_decode RGA（fd） | ⚠️ 仍失败 | ION 缓冲 PA >4GB，`importbuffer_fd` 被内核拒绝；**不阻塞功能**，30 帧冷却后自动重试 |
| rknn / encode RGA（VA） | ⚠️ 依平台 | 可能失败；**闩锁**后转 CPU，减少内核与终端压力 |
| 帧率 `/camera/image_raw` | ✅ **约 25Hz** | copyTo(heap) + cv::resize；相机 30Hz 输入，img_decode 约 25Hz 输出 |
| 整机 CPU | ℹ️ 需分项看 | 远程开发时 Cursor 进程常占大头 |
| DMA32 加速（未完成） | 🔲 下一阶段 | `DRM \| DMA32` 路线待验证；当前稳定性优先 |

---

## 8. 建议验证命令

```bash
# 编译 img_decode
cd ~/newbot_ws && source /opt/ros/noetic/setup.bash
catkin_make --pkg img_decode

# 话题帧率
rostopic hz /image_raw/compressed   # 预期 ~30Hz
rostopic hz /camera/image_raw       # 预期 ~25Hz

# 内核 RGA 状态
dmesg | grep -i rga | tail -30

# 调试日志（需节点运行时写入）
cat ~/.cursor/debug-b311e8.log | head -5
```

---

## 9. 一句话总结

四个阶段的核心链路：① RGA core 选择与 fd 合法性；② MPP 初始化对齐上游解决黑屏；③ `improcess + im_opt_t.core` 替代不可靠的 `imconfig`；④ **`copyTo(heap)` 回退路径将帧率从 5Hz 提升至 25Hz**，RGA(fd) 虽仍因 ION 高地址失败，但已具备冷却自动重试能力，整体稳定。

---

## 10. 修订记录

| 日期 | 修订内容 |
|------|----------|
| 2026-04-05 | 初版：RGA dmesg、SIGSEGV、fd 判据 |
| 2026-04-06 | 第二阶段：黑屏与 MPP 调试（pipeline -1012、TASK-NOFRMBUF 等） |
| 2026-04-06 | 第三阶段：纠正 fmt=65542→RGB888；对齐 Gitee 上游；improcess+RGA3；rknn/encode 闩锁；移除 DMA sync |
| 2026-04-06 | **第四阶段**：importbuffer_fd 替代 wrapbuffer_fd；冷却重试替代永久闩锁；copyTo(heap)+resize 回退路径（5Hz→25Hz）；packetSize 越界保护；DMA32 崩溃根因确认；存档 |
