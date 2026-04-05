# RGA + MPP 硬解码链路调试报告

| 项目 | 说明 |
|------|------|
| 适用范围 | `img_decode`：JPEG（压缩图）→ MPP 硬解 → RGB → 缩放 → `/camera/image_raw` |
| 工作空间 | `newbot_ws`，包名 **`img_decode`** |
| 文档版本 | 2026-04-06 修订（**第五阶段**：FD-RGA 全链路打通，≈30Hz / 低 CPU；librga handle 一致性 + system-dma32 输出缓冲 + 环境变量） |

---

## 1. 背景与数据流

```
/usb_camera → /image_raw/compressed (sensor_msgs/CompressedImage, MJPEG)
       → img_decode 订阅
       → MPP 硬解码 MJPEG → RGB888（frmGrp：ION 优先，失败 DMA_HEAP；配合环境变量倾向 <4GB 物理页）
       → 缩放（可配置）：
            • FD-RGA（推荐）：mpp_buffer_get_fd(src) → importbuffer_fd → RGA3 improcess
              → 输出写入 /dev/dma_heap/system-dma32 临时缓冲 → mmap 读回 → memcpy 到 msg
            • VA-RGA（实验）：heap Mat + RGA2_CORE0，失败则 OpenCV
            • OpenCV 兜底：copyTo(cached) + cv::resize
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
| **MPP buffer fd** | `mpp_buffer_get_fd` 在部分分配器上可能返回 `0`（无导出 fd）；若把 `0` 当合法 fd 再 `dup(0)` 会误 dup **stdin**（非 dma-buf）。**最终判据**：仅 `raw < 0` 视为错误；生产路径以 **ION + 有效 fd（通常 ≥3）** 为主 |
| **SIGSEGV** | `init_packet_and_frame` 失败路径未完整回滚，`dataBuf=NULL` 仍 `decode()` → `memcpy` 崩溃 |

### 2.3 修复（仍有效）
- `get_dmabuf_fd`：**仅当 `raw < 0` 判无效**（第五阶段修正：避免把 `0` 一律当「无 fd」而误拒；同时避免在「0 实为 stdin」场景下 `dup(0)`——见 5.6）
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

**历史运行时证据（调试阶段曾写入临时 PIXEL 日志）**：`nz_1k≈1000`、`center` 非零、`buf_is_frmBuf:1`、`fd>0` → **硬解 + 缓冲有效，黑屏问题闭环**。当前代码已移除该埋点；日常验证可用 Foxglove / `rostopic echo` 抽样像素或对比帧率。

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
| `src/rga_resize.cpp` | `rga_resize_fd` 改用 **`importbuffer_fd(fd, &im_handle_param_t)` → `wrapbuffer_handle_t` → `releasebuffer_handle`** 代替旧 `wrapbuffer_fd`（第四阶段）；第五阶段再改为 **dst 亦 handle + system-dma32**（见 5.6） | 正确进入内核 handle 路径 |
| `src/img_decode.cpp` | 永久闩锁改为 **冷却计数器**：失败后计 30 帧冷却再重试；第五阶段拆为 **`g_rga_fd_cooldown` / `g_rga_va_cooldown`** | 不永久锁死，FD 与 VA 实验互不干扰 |
| `src/img_decode.cpp` | OpenCV 回退路径：**先 `image.copyTo(cached)` 将 ION uncached Mat 拷到 heap**，再 `cv::resize(cached, resized, ...)` | **帧率从 5Hz 提升至约 25Hz** |
| `src/mpp_decode.cpp` | 记录 `packetSize = buf_size`；`decode()` 入口增加 `srcLen > packetSize` 越界检查 → return -1 | 防止压缩数据溢出 MPP 输入缓冲 |
| `src/mpp_decode.cpp` | 撤销危险的 `MPP_BUFFER_TYPE_ION \| MPP_BUFFER_FLAGS_DMA32`，回退至稳定 ION + DMA_HEAP fallback | 消除 init 阶段 SIGSEGV |

### 5.4 DMA32 / 低地址物理内存路线说明

**已落地（推荐，第五阶段）**：不依赖 `(ION | DMA32)` 这类曾导致 **SIGSEGV** 的 MPP 类型组合，而是通过

- launch 环境变量 **`RK_DMA_HEAP_USE_DMA32=1`**、**`ION_HEAP_MASK=8`**，配合 **ION internal frmGrp**，使解码帧缓冲更易落在 RGA 可接受的物理地址范围；
- RGA 侧输出缓冲显式从 **`/dev/dma_heap/system-dma32`** 分配，保证 **dst 为合法 dma-buf 且与 src 同为 handle 路径**。

**仍属可选实验（未作为默认）**：MPP 头文件对 `MPP_BUFFER_TYPE_DRM` 举例了 flags（`DRM | CONTIG` 等）。若未来要试 **DRM allocator**，需先确认 `/dev/dri/` 与 MPP DRM 路径可用，**禁止**再次将 `MPP_BUFFER_FLAGS_DMA32` 与 **ION** 直接 OR（本机已证实会快速崩溃）。

### 5.5 第四阶段存档状态（OpenCV 兜底为主时）

| 指标 | 值 | 说明 |
|------|----|------|
| `/image_raw/compressed` | ~30Hz | 相机正常 |
| `/camera/image_raw` | **~25Hz** | copyTo + cv::resize（heap）路径 |
| MPP decode 像素 | 非零 | 硬解正常 |
| RGA(fd) | ⚠️ 仍失败（ION >4G PA + 旧 dst 写法） | 不阻塞功能，冷却重试 + OpenCV |
| img_decode 稳定性 | ✅ 无崩溃 | bounds check；禁止 `ION \| DMA32` 直接 OR |

---

### 5.6 第五阶段问题（librga handle 混用 + dst 虚拟地址 >4G 映射）

#### 5.6.1 现象（第四阶段之后仍失败时的日志）

- librga 打印：`librga only supports the use of handles only or no handles, [src,dst] = [10, 0]`
- `rga_dump_channel_info`：`src` 为 `buffer[handle,fd,va,pa] = [10, 0, 0, 0]`（handle 路径），`dst` 为 `va=0x7f…` 且 handle=0（`wrapbuffer_virtualaddr`）
- dmesg 交替出现：`RGA_MMU unsupported memory larger than 4G`、`map virtual address error`、`type = virt_addr(0x1)`（对用户态 heap 的 VA 做 RGA3 映射失败）
- 帧率可维持 ~26Hz，但主线程 CPU 仍高（大量失败重试 + OpenCV）

#### 5.6.2 根因归纳（与「MPP 分配 + RGA 访问方式」对应）

| 组合 | 说明 |
|------|------|
| **ION + VA-RGA** | 解码缓冲常为 uncached ION；直接 VA 走 RGA 易慢或失败；RGA3 不支持纯 VA，需 RGA2（`IM_SCHEDULER_RGA2_CORE0`），且 heap 拷贝后仍可能 map 失败 |
| **ION + FD-RGA（正确做法）** | `get_dmabuf_fd` + `rga_resize_fd`，**必须**满足下列两点才能稳定：① **src/dst 同时为 handle 路径**；② **dst 侧物理地址须在 <4GB**（RGA2/RGA3 IOMMU 与驱动限制） |
| **DMA_HEAP internal + FD** | `mpp_buffer_get_fd` 可能恒为 `0`（未向用户态导出 dma-buf fd），**无法**作为 FD-RGA 的可靠来源；此前若放宽为 `dup(0)` 会得到非 dma-buf，内核报 `Fail to get dma_buf from fd, ret[-22]` |

#### 5.6.3 本阶段修复（已完成，生产可用）

| 项 | 实现要点 |
|----|----------|
| **handle 一致性** | `rga_resize_fd`：src 继续 `importbuffer_fd(src_fd)` + `wrapbuffer_handle_t`；**dst 不再** `wrapbuffer_virtualaddr`，改为 **`ioctl(/dev/dma_heap/system-dma32, DMA_HEAP_IOCTL_ALLOC)`** 分配输出 dma-buf → `importbuffer_fd(dst_fd)` → `wrapbuffer_handle_t` → `improcess` → **`mmap` + `memcpy` 写回 `msg_pub.data`** |
| **MPP 源缓冲** | `frmGrp` 维持 **ION 优先、失败 DMA_HEAP**（与上游一致）；**FD-RGA 依赖 ION 导出合法 fd**（实测 `frm_fd` 多为十几以上） |
| **环境变量** | `img_decode.launch` 内：`RK_DMA_HEAP_USE_DMA32=1`、`ION_HEAP_MASK=8`，促使 MPP/ION 侧更倾向 **<4GB 物理页**，与 RGA import 兼容 |
| **`get_dmabuf_fd`** | 判据改为 **`raw < 0` 才失败**（与「仅拒绝真错误 fd」一致；生产路径下 fd 为正常 dma-buf） |
| **VA-RGA** | `rga_resize`（纯 VA）使用 **`IM_SCHEDULER_RGA2_CORE0`**（RGA3 对 VA 易 `Invalid argument`） |
| **节点生命周期** | `main` 中 `check_thread.detach()`，避免管道/异常退出时 **`std::terminate`（joinable thread 未 join）** |
| **参数拆分** | `use_rga_fd_experimental` / `use_rga_va_fallback`（默认 FD 开、VA 关）；旧 `use_rga` 仅兼容保留 |

#### 5.6.4 第五阶段效果（实测量级）

| 指标 | 第四阶段（OpenCV 兜底为主） | 第五阶段（FD-RGA 打通） |
|------|------------------------------|-------------------------|
| `/camera/image_raw` | ~20～25Hz | **~30Hz**（贴近 `/image_raw/compressed`） |
| img_decode 主线程 CPU（典型） | ~80%～90% | **~7%** 量级 |
| dmesg 新 RGA 错误 | 可有 `>4G` / import 失败 | **运行期无新增**（旧日志时间戳可早于当前 uptime） |
| 功能 | 稳定 | **稳定 + 硬件缩放生效** |

---

## 6. 主要改动文件（汇总）

| 文件 | 内容摘要 |
|------|----------|
| `src/mpp_decode.h` | `is_ready()`；`get_dmabuf_fd`；`get_last_rga_layout`；`packetSize`；句柄默认 NULL |
| `src/mpp_decode.cpp` | 对齐上游：ION internal frmGrp/pktGrp；`get_image(frame)`；RGB888 stride；初始化失败完整回滚；**`packetSize`**；**`srcLen > packetSize` 越界检查**；**`get_dmabuf_fd`：`raw < 0` 判失败** |
| `src/rga_resize.cpp` | `rga_resize`：**`IM_SCHEDULER_RGA2_CORE0`**（VA 路径）；`rga_resize_fd`：**src/dst 双 handle**；dst 来自 **`/dev/dma_heap/system-dma32` + `DMA_HEAP_IOCTL_ALLOC`**；**`improcess` + `opt.core=IM_SCHEDULER_RGA3_CORE0`**；日志节流 |
| `src/img_decode.h` / `img_decode.cpp` | **`use_rga_fd_experimental`**、**`use_rga_va_fallback`**；三路缩放分支；**FD/VA 各自冷却帧**；OpenCV 兜底 **`copyTo(cached)`**；**`check_thread.detach()`** |
| `launch/img_decode.launch` | **`RK_DMA_HEAP_USE_DMA32=1`**、**`ION_HEAP_MASK=8`**；`use_rga_fd_experimental`（生产默认 true）；`scale` / `fps_div` 等 |
| `../img_encode/src/rga_cvtcolor.cpp` | `improcess` + RGA3 core；失败闩锁 |
| `../rknn_yolov6/src/utils.cpp` | `rga_resize`：`improcess` + RGA3 core；失败闩锁 |

---

## 7. 结论与当前状态（截至第五阶段）

| 子问题 | 状态 | 说明 |
|--------|------|------|
| SIGSEGV / 半初始化 decode | ✅ 已修复 | `is_ready()` 门控 + 完整回滚 + `packetSize` 边界检查 |
| MPP 黑屏 / 全零像素 | ✅ 已闭环 | 对齐上游 buffer + `get_image(frame)` + RGB888 stride |
| `mpp_decode not ready` | ✅ 已避免 | 撤销错误的 external-only 初始化路径 |
| img_decode **FD-RGA** | ✅ **已打通** | **双 handle + system-dma32 输出** + 环境变量；**≈30Hz**，主线程 CPU **约个位数 %** |
| img_decode 关闭 FD 时 | ✅ 可回退 | `use_rga_fd_experimental=false` → OpenCV cached，帧率约 20～25Hz 量级（视负载） |
| rknn / encode RGA（VA） | ⚠️ 依平台 | 可能失败；**闩锁**后转 CPU，减少内核与终端压力 |
| 帧率 `/camera/image_raw` | ✅ **约 30Hz**（推荐配置） | 与 MJPEG 输入同量级；第四阶段纯 OpenCV 兜底时约 25Hz |
| `DRM \| DMA32` 仅 MPP frmGrp | 🔲 未采用 | 曾致 SIGSEGV；**未**走该路径；FD-RGA 用 **dma_heap 输出池 + ION 源 fd** 达成目标 |

---

## 8. 建议验证命令

```bash
# 编译 img_decode
cd ~/newbot_ws && source /opt/ros/noetic/setup.bash
source ~/newbot_ws/devel/setup.bash
catkin_make --pkg img_decode

# 启动（launch 内已含 RK_DMA_HEAP_USE_DMA32 / ION_HEAP_MASK）
roslaunch img_decode img_decode.launch

# 话题帧率（第五阶段：二者应接近）
rostopic hz /image_raw/compressed   # 预期 ~30Hz
rostopic hz /camera/image_raw       # 预期 ~30Hz（FD-RGA 开启时）

# 进程 CPU（主线程）
top -H -p "$(pgrep -f '/img_decode ' | head -1)"

# 内核 RGA：新错误应对应当前 uptime 持续增长后出现；若仅见旧时间戳可对照 /proc/uptime
dmesg | grep -i rga | tail -30

# 关闭硬件缩放对比（仅验证回退）
# rosparam set /img_decode/use_rga_fd_experimental false
```

---

## 9. 一句话总结

前四阶段：RGA core、`improcess`、MPP 对齐上游、**`copyTo(heap)` 把回退帧率从 5Hz 拉到约 25Hz**。第五阶段在 **FD-RGA** 上补齐两件关键事：**librga 要求 src/dst 同用 handle 或同不用**（dst 改为 `system-dma32` 分配再 `importbuffer_fd`），以及 **输出缓冲必须在 <4GB 物理空间**（避免 RGA 对 heap VA 映射失败）；配合 **`RK_DMA_HEAP_USE_DMA32` / `ION_HEAP_MASK`**，实现 **≈30Hz + 极低 img_decode CPU**，回退参数仍可关 FD 走 OpenCV。

---

## 10. 修订记录

| 日期 | 修订内容 |
|------|----------|
| 2026-04-05 | 初版：RGA dmesg、SIGSEGV、fd 判据 |
| 2026-04-06 | 第二阶段：黑屏与 MPP 调试（pipeline -1012、TASK-NOFRMBUF 等） |
| 2026-04-06 | 第三阶段：纠正 fmt=65542→RGB888；对齐 Gitee 上游；improcess+RGA3；rknn/encode 闩锁；移除 DMA sync |
| 2026-04-06 | **第四阶段**：importbuffer_fd 替代 wrapbuffer_fd；冷却重试替代永久闩锁；copyTo(heap)+resize 回退路径（5Hz→25Hz）；packetSize 越界保护；DMA32 崩溃根因确认；存档 |
| 2026-04-06 | **第五阶段**：librga src/dst handle 一致；`rga_resize_fd` 输出用 `/dev/dma_heap/system-dma32`；`get_dmabuf_fd` 改为 `raw < 0`；`rga_resize` 用 RGA2_CORE0；`img_decode` 双参数 + FD/VA 分冷却 + `detach`；launch 环境变量；FD-RGA 生产默认，≈30Hz / ~7% CPU |
