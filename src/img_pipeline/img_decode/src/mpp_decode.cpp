#if(USE_ARM_LIB==1)

#include "mpp_decode.h"

MppDecode::MppDecode()
{
}

void MppDecode::init(int width,int height)
{
	int ret = init_mpp();
	if (ret != MPP_OK)
	{
		printf("mpp_decode init erron (%d) \r\n", ret);
		return;
	}

    ret = init_packet_and_frame(width, height);
	if (ret != MPP_OK)
	{
		printf("mpp_decode init_packet_and_frame (%d) \r\n", ret);
		return;
	}
}

MppDecode::~MppDecode()
{
	if (packet) 
	{
        mpp_packet_deinit(&packet);
        packet = NULL;
    }

	if (frame) 
	{
        mpp_frame_deinit(&frame);
        frame = NULL;
    }

	if (ctx) 
	{
        mpp_destroy(ctx);
        ctx = NULL;
    }

	if (pktBuf) 
	{
        mpp_buffer_put(pktBuf);
        pktBuf = NULL;
    }

    if (frmBuf) 
	{
        mpp_buffer_put(frmBuf);
        frmBuf = NULL;
    }

	if (pktGrp) {
        mpp_buffer_group_put(pktGrp);
        pktGrp = NULL;
    }

    if (frmGrp) {
        mpp_buffer_group_put(frmGrp);
        frmGrp = NULL;
    }

}


int MppDecode::init_mpp()
{
	MPP_RET ret = MPP_OK;
	MpiCmd mpi_cmd = MPP_CMD_BASE;
    MppParam param = NULL;
	
	ret = mpp_create(&ctx, &mpi);
    if (ret != MPP_OK) 
	{
		MPP_ERR("mpp_create erron (%d) \n", ret);
        return ret;
    }

	uint32_t need_split = 0;
	mpi_cmd = MPP_DEC_SET_PARSER_SPLIT_MODE;
	param = &need_split;
	ret = mpi->control(ctx, mpi_cmd, param);
	if (ret != MPP_OK)
	{
        MPP_ERR("MPP_DEC_SET_PARSER_SPLIT_MODE set erron (%d) \n", ret);
        return ret;
    }

	ret = mpp_init(ctx, MPP_CTX_DEC, MppCodingType::MPP_VIDEO_CodingMJPEG);
	if (MPP_OK != ret) 
	{
		MPP_ERR("mpp_init erron (%d) \n", ret);
        return ret;
	}

	MppFrameFormat frmType = MPP_FMT_RGB888;
	param = &frmType;
	mpi->control(ctx, MPP_DEC_SET_OUTPUT_FORMAT, param);

	return MPP_OK;
}


int MppDecode::init_packet_and_frame(int width, int height)
{
	RK_U32 hor_stride = MPP_ALIGN(width, 16);
	RK_U32 ver_stride = MPP_ALIGN(height, 16);
	const size_t buf_size = (size_t)hor_stride * (size_t)ver_stride * 4u;

	int ret;

	// ION internal — 与 gitee 参考版本一致；DMA32 flag (ION|DMA32) 在本平台 MPP 中会导致 segfault，不使用
	ret = mpp_buffer_group_get_internal(&frmGrp, MPP_BUFFER_TYPE_ION);
	if (ret)
	{
		MPP_ERR("frmGrp ION failed (%d), try DMA_HEAP\r\n", ret);
		ret = mpp_buffer_group_get_internal(&frmGrp, MPP_BUFFER_TYPE_DMA_HEAP);
		if (ret) {
			MPP_ERR("frmGrp DMA_HEAP also failed (%d)\r\n", ret);
			return -1;
		}
	}

	ret = mpp_buffer_group_get_internal(&pktGrp, MPP_BUFFER_TYPE_ION);
	if (ret)
	{
		MPP_ERR("pktGrp ION failed (%d), try DMA_HEAP\r\n", ret);
		ret = mpp_buffer_group_get_internal(&pktGrp, MPP_BUFFER_TYPE_DMA_HEAP);
		if (ret) {
			MPP_ERR("pktGrp DMA_HEAP also failed (%d)\r\n", ret);
			mpp_buffer_group_put(frmGrp); frmGrp = NULL;
			return -1;
		}
	}

	ret = mpp_frame_init(&frame);
	if (MPP_OK != ret)
	{
		MPP_ERR("mpp_frame_init failed\n");
		mpp_buffer_group_put(pktGrp); pktGrp = NULL;
		mpp_buffer_group_put(frmGrp); frmGrp = NULL;
		return -1;
	}

	ret = mpp_buffer_get(frmGrp, &frmBuf, buf_size);
	if (ret)
	{
		MPP_ERR("frmGrp mpp_buffer_get erron (%d)\n", ret);
		mpp_frame_deinit(&frame); frame = NULL;
		mpp_buffer_group_put(pktGrp); pktGrp = NULL;
		mpp_buffer_group_put(frmGrp); frmGrp = NULL;
		return -1;
	}

	ret = mpp_buffer_get(pktGrp, &pktBuf, buf_size);
	if (ret)
	{
		MPP_ERR("pktGrp mpp_buffer_get erron (%d)\n", ret);
		mpp_buffer_put(frmBuf); frmBuf = NULL;
		mpp_frame_deinit(&frame); frame = NULL;
		mpp_buffer_group_put(pktGrp); pktGrp = NULL;
		mpp_buffer_group_put(frmGrp); frmGrp = NULL;
		return -1;
	}
	packetSize = buf_size;

	mpp_packet_init_with_buffer(&packet, pktBuf);
	dataBuf = (char *)mpp_buffer_get_ptr(pktBuf);
	if (!dataBuf)
	{
		MPP_ERR("mpp_buffer_get_ptr(pktBuf) NULL\n");
		mpp_packet_deinit(&packet); packet = NULL;
		mpp_buffer_put(pktBuf); pktBuf = NULL;
		mpp_buffer_put(frmBuf); frmBuf = NULL;
		mpp_frame_deinit(&frame); frame = NULL;
		mpp_buffer_group_put(pktGrp); pktGrp = NULL;
		mpp_buffer_group_put(frmGrp); frmGrp = NULL;
		return -1;
	}

	mpp_frame_set_buffer(frame, frmBuf);

	// #region agent log INIT: log init success
	{
		FILE *_lf = fopen("/home/orangepi/.cursor/debug-b311e8.log", "a");
		if (_lf) {
			struct timeval _tv; gettimeofday(&_tv, NULL);
			int frm_fd = mpp_buffer_get_fd(frmBuf);
			void *frm_ptr = mpp_buffer_get_ptr(frmBuf);
			fprintf(_lf, "{\"sessionId\":\"b311e8\",\"hypothesisId\":\"INIT\",\"location\":\"mpp_decode.cpp:init\",\"message\":\"init ok stable\",\"data\":{\"buf_size\":%zu,\"frm_fd\":%d,\"frm_va\":%lu,\"pkt_sz\":%zu},\"timestamp\":%lld}\n",
				buf_size, frm_fd, (unsigned long)(uintptr_t)frm_ptr, packetSize,
				(long long)_tv.tv_sec*1000+_tv.tv_usec/1000);
			fclose(_lf);
		}
	}
	// #endregion

	return 0;
}


int MppDecode::decode(unsigned char *srcFrm, size_t srcLen, cv::Mat &image)
{
	if (!dataBuf || !ctx || !mpi || !packet || !frame)
	{
		MPP_ERR("decode: MPP not initialized (dataBuf=%p)\n", (void *)dataBuf);
		return -1;
	}
	if (!srcFrm || srcLen == 0)
		return -1;
	if (packetSize > 0 && srcLen > packetSize)
	{
		MPP_ERR("decode: srcLen(%zu) > packetSize(%zu), skip to prevent overflow\n", srcLen, packetSize);
		return -1;
	}

	last_rga_buffer = NULL;

	MppTask task = NULL;
	int ret;

	memcpy(dataBuf, srcFrm, srcLen);
	mpp_packet_set_pos(packet, dataBuf);
	mpp_packet_set_length(packet, srcLen);

	ret = mpi->poll(ctx, MPP_PORT_INPUT, MPP_POLL_BLOCK);
	if (ret)
	{
		MPP_ERR("mpp input poll failed\n");
		return ret;
	}

	ret = mpi->dequeue(ctx, MPP_PORT_INPUT, &task);
	if (ret)
	{
		MPP_ERR("mpp task input dequeue failed\n");
		return ret;
	}

	mpp_task_meta_set_packet(task, KEY_INPUT_PACKET, packet);
	mpp_task_meta_set_frame(task, KEY_OUTPUT_FRAME, frame);

	ret = mpi->enqueue(ctx, MPP_PORT_INPUT, task);
	if (ret)
	{
		MPP_ERR("mpp task input enqueue failed\n");
		return ret;
	}

	ret = mpi->poll(ctx, MPP_PORT_OUTPUT, MPP_POLL_BLOCK);
	if (ret)
	{
		MPP_ERR("mpp output poll failed\n");
		return ret;
	}

	ret = mpi->dequeue(ctx, MPP_PORT_OUTPUT, &task);
	if (ret)
	{
		MPP_ERR("mpp task output dequeue failed\n");
		return ret;
	}

	int image_res = -1;
	if (task)
	{
		MppFrame frameOut = NULL;
		mpp_task_meta_get_frame(task, KEY_OUTPUT_FRAME, &frameOut);

		// Original code reads from member `frame` (which has frmBuf), not from frameOut
		if (frame)
		{
			image_res = get_image(frame, image);
		}

		ret = mpi->enqueue(ctx, MPP_PORT_OUTPUT, task);
		if (ret)
			MPP_ERR("mpp task output enqueue failed\n");
	}
	else
	{
		MPP_ERR("output task is NULL\n");
	}

	return image_res;
}

void MppDecode::get_last_rga_layout(RK_U32 *width, RK_U32 *height, RK_U32 *wstride_px, RK_U32 *hstride_px)
{
	if (!width || !height || !wstride_px || !hstride_px)
		return;
	if (!last_rga_buffer)
	{
		*width = *height = *wstride_px = *hstride_px = 0;
		return;
	}
	*width = last_w;
	*height = last_h;
	*wstride_px = last_wstride_px;
	*hstride_px = last_hstride_px;
}

int MppDecode::get_dmabuf_fd(int *out_fd)
{
	if (!out_fd || !last_rga_buffer)
		return -1;
	int raw = mpp_buffer_get_fd(last_rga_buffer);
	if (raw <= 0)
		return -1;
	int d = dup(raw);
	if (d < 0)
	{
		MPP_ERR("dup failed\n");
		return -1;
	}
	*out_fd = d;
	return 0;
}

int MppDecode::get_image(MppFrame &frame, cv::Mat &image)
{
    RK_U32 width    = 0;
    RK_U32 height   = 0;
    RK_U32 h_stride = 0;
    RK_U32 v_stride = 0;
    MppFrameFormat fmt;
    MppBuffer buffer    = NULL;
    RK_U8 *base = NULL;

	last_rga_buffer = NULL;

    if (NULL == frame)
	{
		MPP_ERR("!frame\n");
        return -1;
	}

    width    = mpp_frame_get_width(frame);
    height   = mpp_frame_get_height(frame);
    h_stride = mpp_frame_get_hor_stride(frame);
    v_stride = mpp_frame_get_ver_stride(frame);
    fmt      = mpp_frame_get_fmt(frame);
    buffer   = mpp_frame_get_buffer(frame);
    if (NULL == buffer)
	{
		MPP_ERR("!buffer\n");
        return -1;
	}

    base = (RK_U8 *)mpp_buffer_get_ptr(buffer);
    
	if(height<=0 || width<=0 || base==NULL)
	{
		MPP_ERR("height<=0 || width<=0 || base==NULL\n");
		return -1;
	}

	// RGB888 (3 bytes/pixel) — matches MPP_FMT_RGB888 = 0x10006 on this platform
	// h_stride is in bytes; pixel stride = h_stride / 3 for RGB888
	RK_U32 wstride_px = width;
	if (h_stride >= width * 3u && (h_stride % 3u) == 0u)
		wstride_px = h_stride / 3u;
	if (wstride_px < width)
		wstride_px = width;

	RK_U32 hstride_px = (v_stride >= height) ? v_stride : height;

	size_t row_bytes = (size_t)wstride_px * 3u;
	image = cv::Mat((int)height, (int)width, CV_8UC3, base, row_bytes);

	last_rga_buffer = buffer;
	last_w = width;
	last_h = height;
	last_wstride_px = wstride_px;
	last_hstride_px = hstride_px;

	// #region agent log DECODE: pixel check (first 5 frames)
	{
		static int _gc = 0;
		if (_gc++ < 5) {
			int nz = 0;
			size_t total = (size_t)height * row_bytes;
			for (size_t _i = 0; _i < std::min(total, (size_t)1000); _i++)
				if (base[_i] > 5) nz++;
			unsigned char cp0=0,cp1=0,cp2=0;
			size_t cx = (size_t)(height/2) * row_bytes + (size_t)(width/2) * 3u;
			if (cx + 2 < total) { cp0=base[cx]; cp1=base[cx+1]; cp2=base[cx+2]; }
			FILE *_lf = fopen("/home/orangepi/.cursor/debug-b311e8.log", "a");
			if (_lf) {
				struct timeval _tv; gettimeofday(&_tv, NULL);
				fprintf(_lf, "{\"sessionId\":\"b311e8\",\"hypothesisId\":\"PIXEL\",\"location\":\"mpp_decode.cpp:get_image\",\"message\":\"pixel check\",\"data\":{\"fmt\":%d,\"w\":%u,\"h\":%u,\"hstride\":%u,\"vstride\":%u,\"wstride_px\":%u,\"fd\":%d,\"nz_1k\":%d,\"center\":[%d,%d,%d],\"buf_is_frmBuf\":%d},\"timestamp\":%lld}\n",
					(int)fmt, width, height, h_stride, v_stride, wstride_px,
					mpp_buffer_get_fd(buffer), nz, cp0, cp1, cp2,
					(buffer == frmBuf) ? 1 : 0,
					(long long)_tv.tv_sec*1000+_tv.tv_usec/1000);
				fclose(_lf);
			}
		}
	}
	// #endregion

	return 0;
}

#endif
