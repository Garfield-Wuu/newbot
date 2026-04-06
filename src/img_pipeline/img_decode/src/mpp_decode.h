#ifndef MPP_DECODE_H
#define MPP_DECODE_H

#if(USE_ARM_LIB==1)

//C 标准函数库
#include <stdio.h>
#include <stdint.h>
#include <string.h>

//Linux 函数库
#include <unistd.h>
#include <sys/time.h>
#include <pthread.h>

//C++ 标准函数库
#include <iostream>
#include <vector>


//MPP函数库
#include <rockchip/vpu.h>
#include <rockchip/rk_mpi.h>
#include <rockchip/rk_type.h>
#include <rockchip/vpu_api.h>
#include <rockchip/mpp_err.h>
#include <rockchip/mpp_task.h>
#include <rockchip/mpp_meta.h>
#include <rockchip/mpp_frame.h>
#include <rockchip/mpp_buffer.h>
#include <rockchip/mpp_packet.h>
#include <rockchip/rk_mpi_cmd.h>

#include <opencv2/opencv.hpp>

using namespace cv;
using namespace std;

//宏定义
#define MPP_ALIGN(x, a)   (((x)+(a)-1)&~((a)-1))

#define ESC_START     "\033["
#define ESC_END       "\033[0m"
#define COLOR_GREEN   "32;40;1m"
#define COLOR_RED     "31;40;1m"
#define MPP_DBG(format, args...) (printf( ESC_START COLOR_GREEN "[MPP DBG]-[%s]-[%05d]:" format ESC_END, __FUNCTION__, (int)__LINE__, ##args))
#define MPP_ERR(format, args...) (printf( ESC_START COLOR_RED   "[MPP ERR]-[%s]-[%05d]:" format ESC_END, __FUNCTION__, (int)__LINE__, ##args))

class MppDecode
{
public:
	MppDecode();
	~MppDecode();
	void init(int width,int height);
	int decode(unsigned char *srcFrm, size_t srcLen, cv::Mat &image);
	/** init_packet_and_frame 完全成功且可安全 decode */
	bool is_ready() const { return dataBuf != NULL && ctx != NULL && mpi != NULL; }

	/** dup(2) 后的 dma-buf fd，供 RGA import；调用者必须在用完后 close。仅在最近一次 decode+get_image 成功后有效。 */
	int get_dmabuf_fd(int *out_fd);

	/** 返回 frmDmaFd 原始值（无 dup），调用者禁止 close，生命周期与 MppDecode 对象绑定。
	 *  仅当 frmDmaFd >= 0（system-dma32 路径成功）且 last_rga_buffer 有效时返回 >=0，否则返回 -1。 */
	int get_raw_dmabuf_fd() const { return (frmDmaFd >= 0 && last_rga_buffer) ? frmDmaFd : -1; }
	void get_last_rga_layout(RK_U32 *width, RK_U32 *height, RK_U32 *wstride_px, RK_U32 *hstride_px);

private:
	MppBufferGroup frmGrp   = NULL;
	MppBufferGroup pktGrp   = NULL;
	MppPacket      packet   = NULL;
	MppFrame       frame    = NULL;
	size_t         packetSize;

	MppBuffer      frmBuf   = NULL;
	MppBuffer      pktBuf   = NULL;

	char *dataBuf = NULL;

	MppCtx  ctx   = NULL;
	MppApi *mpi   = NULL;

	/** 上一帧成功解码后，供 RGA 使用的 buffer 与布局（像素 stride 与 im2d wrapbuffer_fd 一致） */
	MppBuffer last_rga_buffer = NULL;
	RK_U32 last_w = 0;
	RK_U32 last_h = 0;
	RK_U32 last_wstride_px = 0;
	RK_U32 last_hstride_px = 0;

	/** 从 /dev/dma_heap/system-dma32 直接分配的 frmBuf（保证 <4GB PA，供 RGA FD 路径使用） */
	int    frmDmaFd   = -1;
	size_t frmDmaSize = 0;
	void  *frmDmaMmap = NULL;

	/** pipeline 模式：持有上一帧 frame，直到下一次 decode() 调用时释放（buffer 生命周期管理） */

	int init_mpp();
	int init_packet_and_frame(int width, int height);
	int get_image(MppFrame &frame, cv::Mat &image);
};



#endif

#endif
