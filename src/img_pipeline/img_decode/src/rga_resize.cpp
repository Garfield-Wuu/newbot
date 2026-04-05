#if(USE_ARM_LIB==1)

#include <opencv2/opencv.hpp>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

// dma-heap ioctl（内核 5.10+ 通用）
#include <stdint.h>
#ifndef DMA_HEAP_IOCTL_ALLOC
struct dma_heap_allocation_data {
	uint64_t len;
	uint32_t fd;
	uint32_t fd_flags;
	uint64_t heap_flags;
};
#define DMA_HEAP_IOCTL_ALLOC _IOWR('H', 0x0, struct dma_heap_allocation_data)
#endif

#include "RgaUtils.h"
#include "im2d.h"
#include "im2d_buffer.h"
#include "rga.h"

static bool rga_status_ok(IM_STATUS s)
{
	return (s == IM_STATUS_SUCCESS || s == IM_STATUS_NOERROR);
}

/** librga 失败时自身也会刷屏；此处仅节流我们的 printf，减轻终端 I/O 拖慢节点 */
static bool rga_allow_printf(void)
{
	static struct timespec last = {0, 0};
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	if (last.tv_sec == 0 || now.tv_sec - last.tv_sec >= 3 ||
	    (now.tv_sec == last.tv_sec && now.tv_nsec - last.tv_nsec > 500000000L))
	{
		last = now;
		return true;
	}
	return false;
}

static void fill_full_rect(im_rect *r, int w, int h)
{
	if (!r || w < 1 || h < 1)
		return;
	r->x = 0;
	r->y = 0;
	r->width = w;
	r->height = h;
}

int rga_resize(cv::Mat &img_in, unsigned char *img_out_data, int dst_w, int dst_h)
{
	if (!img_out_data || dst_w < 1 || dst_h < 1 || img_in.empty())
		return -1;

	int wstride_px = img_in.cols;
	if (img_in.step[0] != (size_t)img_in.cols * 3u)
		wstride_px = (int)(img_in.step[0] / 3u);
	int hstride_px = img_in.rows;

	rga_buffer_t src, dst, pat;
	im_rect      src_rect, dst_rect, pat_rect;
	memset(&src, 0, sizeof(src));
	memset(&dst, 0, sizeof(dst));
	memset(&pat, 0, sizeof(pat));
	memset(&pat_rect, 0, sizeof(pat_rect));
	fill_full_rect(&src_rect, img_in.cols, img_in.rows);
	fill_full_rect(&dst_rect, dst_w, dst_h);

	src = wrapbuffer_virtualaddr((void *)img_in.data, img_in.cols, img_in.rows, RK_FORMAT_RGB_888,
	                             wstride_px, hstride_px);
	dst = wrapbuffer_virtualaddr((void *)img_out_data, dst_w, dst_h, RK_FORMAT_RGB_888);

	int ret = imcheck(src, dst, src_rect, dst_rect);
	if (ret != IM_STATUS_NOERROR && ret != IM_STATUS_SUCCESS)
	{
		if (rga_allow_printf())
			printf("%d, check error! %s\n", __LINE__, imStrError((IM_STATUS)ret));
		return -1;
	}

	im_opt_t opt;
	memset(&opt, 0, sizeof(opt));
	// VA 路径必须用 RGA2（有 MMU 支持虚拟地址）；RGA3 不支持 wrapbuffer_virtualaddr
	opt.core = IM_SCHEDULER_RGA2_CORE0;

	IM_STATUS st = improcess(src, dst, pat, src_rect, dst_rect, pat_rect,
	                         -1, NULL, &opt, IM_SYNC);

	if (!rga_status_ok(st))
	{
		if (rga_allow_printf())
			printf("rga_resize: improcess fail %dx%d stride %dx%d -> %dx%d : %s (%d)\n",
			       img_in.cols, img_in.rows, wstride_px, hstride_px, dst_w, dst_h, imStrError(st), (int)st);
		return -1;
	}
	return 0;
}

int rga_resize_fd(int src_fd, int src_w, int src_h, int wstride_px, int hstride_px,
                  unsigned char *dst_data, int dst_w, int dst_h)
{
	if (src_fd < 0 || !dst_data || src_w < 1 || src_h < 1 || dst_w < 1 || dst_h < 1)
		return -1;
	if (wstride_px < src_w || hstride_px < src_h)
	{
		if (rga_allow_printf())
			printf("rga_resize_fd: invalid stride src %dx%d stride_px %dx%d\n",
			       src_w, src_h, wstride_px, hstride_px);
		return -1;
	}

	// 使用 importbuffer_fd handle 路径：让 RGA 驱动正确识别 dma-buf，opt.core 才能生效
	im_handle_param_t param;
	memset(&param, 0, sizeof(param));
	param.width  = (uint32_t)wstride_px;
	param.height = (uint32_t)hstride_px;
	param.format = RK_FORMAT_RGB_888;

	rga_buffer_handle_t src_handle = importbuffer_fd(src_fd, &param);
	if (!src_handle)
	{
		if (rga_allow_printf())
			printf("rga_resize_fd: importbuffer_fd failed fd=%d\n", src_fd);
		return -1;
	}

	// dst 也需要 <4GB dma-buf：从 system-dma32 分配输出缓冲
	const size_t dst_size = (size_t)dst_w * dst_h * 3;
	static int heap_fd = -1;
	if (heap_fd < 0)
	{
		heap_fd = open("/dev/dma_heap/system-dma32", O_RDWR | O_CLOEXEC);
		if (heap_fd < 0)
		{
			if (rga_allow_printf())
				printf("rga_resize_fd: open system-dma32 failed\n");
			releasebuffer_handle(src_handle);
			return -1;
		}
	}

	struct dma_heap_allocation_data alloc = {};
	alloc.len = dst_size;
	alloc.fd_flags = O_RDWR | O_CLOEXEC;
	if (ioctl(heap_fd, DMA_HEAP_IOCTL_ALLOC, &alloc) < 0)
	{
		if (rga_allow_printf())
			printf("rga_resize_fd: dma_heap alloc %zu failed\n", dst_size);
		releasebuffer_handle(src_handle);
		return -1;
	}
	int dst_dma_fd = (int)alloc.fd;

	im_handle_param_t dst_param;
	memset(&dst_param, 0, sizeof(dst_param));
	dst_param.width  = (uint32_t)dst_w;
	dst_param.height = (uint32_t)dst_h;
	dst_param.format = RK_FORMAT_RGB_888;

	rga_buffer_handle_t dst_handle = importbuffer_fd(dst_dma_fd, &dst_param);
	if (!dst_handle)
	{
		if (rga_allow_printf())
			printf("rga_resize_fd: importbuffer_fd(dst) failed fd=%d\n", dst_dma_fd);
		close(dst_dma_fd);
		releasebuffer_handle(src_handle);
		return -1;
	}

	rga_buffer_t src, dst, pat;
	im_rect      src_rect, dst_rect, pat_rect;
	memset(&src, 0, sizeof(src));
	memset(&dst, 0, sizeof(dst));
	memset(&pat, 0, sizeof(pat));
	memset(&pat_rect, 0, sizeof(pat_rect));
	fill_full_rect(&src_rect, src_w, src_h);
	fill_full_rect(&dst_rect, dst_w, dst_h);

	src = wrapbuffer_handle_t(src_handle, src_w, src_h, wstride_px, hstride_px, RK_FORMAT_RGB_888);
	dst = wrapbuffer_handle_t(dst_handle, dst_w, dst_h, dst_w, dst_h, RK_FORMAT_RGB_888);

	int ret = imcheck(src, dst, src_rect, dst_rect);
	if (ret != IM_STATUS_NOERROR && ret != IM_STATUS_SUCCESS)
	{
		if (rga_allow_printf())
			printf("%d, rga_resize_fd imcheck error! %s\n", __LINE__, imStrError((IM_STATUS)ret));
		releasebuffer_handle(dst_handle);
		close(dst_dma_fd);
		releasebuffer_handle(src_handle);
		return -1;
	}

	im_opt_t opt;
	memset(&opt, 0, sizeof(opt));
	opt.core = IM_SCHEDULER_RGA3_CORE0;

	IM_STATUS st = improcess(src, dst, pat, src_rect, dst_rect, pat_rect,
	                         -1, NULL, &opt, IM_SYNC);

	releasebuffer_handle(dst_handle);
	releasebuffer_handle(src_handle);

	if (rga_status_ok(st))
	{
		// RGA 写入 dma-buf 成功，mmap 读出到用户 buffer
		void *mapped = mmap(NULL, dst_size, PROT_READ, MAP_SHARED, dst_dma_fd, 0);
		if (mapped != MAP_FAILED)
		{
			memcpy(dst_data, mapped, dst_size);
			munmap(mapped, dst_size);
		}
		else
		{
			if (rga_allow_printf())
				printf("rga_resize_fd: mmap dst_dma_fd failed\n");
			close(dst_dma_fd);
			return -1;
		}
	}

	close(dst_dma_fd);

	if (!rga_status_ok(st))
	{
		if (rga_allow_printf())
			printf("rga_resize_fd: fail fd=%d src %dx%d stride %dx%d -> dst %dx%d : %s (%d)\n",
			       src_fd, src_w, src_h, wstride_px, hstride_px, dst_w, dst_h, imStrError(st), (int)st);
		return -1;
	}
	return 0;
}

#endif
