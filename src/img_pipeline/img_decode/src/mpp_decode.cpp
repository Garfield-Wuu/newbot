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
	
	//创建 MPP context 和 MPP api 接口
	ret = mpp_create(&ctx, &mpi);
    if (ret != MPP_OK) 
	{
		MPP_ERR("mpp_create erron (%d) \n", ret);
        return ret;
    }

	uint32_t need_split = 0;
	//MPP_DEC_SET_PARSER_SPLIT_MODE ：  （仅限解码）
	//自动拼包（建议开启），硬编解码器每次解码就是一个Frame，
	//所以如果输入的数据不确定是不是一个Frame
	//（例如可能是一个Slice、一个Nalu或者一个FU-A分包，甚至可能随意读的任意长度数据），
	//那就必须把该模式打开，MPP会自动分包拼包成一个完整Frame送给硬解码器
	mpi_cmd = MPP_DEC_SET_PARSER_SPLIT_MODE;
	param = &need_split;
	ret = mpi->control(ctx, mpi_cmd, param);
	if (ret != MPP_OK)
	{
        MPP_ERR("MPP_DEC_SET_PARSER_SPLIT_MODE set erron (%d) \n", ret);
        return ret;
    }

	//设置MPP为解码模式 
	//MPP_CTX_DEC ： 解码
	//MPP_VIDEO_CodingAVC ： H.264
	//MPP_VIDEO_CodingHEVC :  H.265
	//MPP_VIDEO_CodingMJPEG : MJPEG
	ret = mpp_init(ctx, MPP_CTX_DEC, MppCodingType::MPP_VIDEO_CodingMJPEG);//这里填解码MJPEG
	if (MPP_OK != ret) 
	{
		MPP_ERR("mpp_init erron (%d) \n", ret);
        return ret;
	}

	//实测此处可以是RGB，但不能是BGR，BGR解码输出不对
	MppFrameFormat frmType = MPP_FMT_RGB888; //MPP_FMT_RGB888; //MPP_FMT_YUV420P;
	param = &frmType;
	mpi->control(ctx, MPP_DEC_SET_OUTPUT_FORMAT, param);

	return MPP_OK;
}


int MppDecode::init_packet_and_frame(int width, int height)
{
	RK_U32 hor_stride = MPP_ALIGN(width, 16);
    RK_U32 ver_stride = MPP_ALIGN(height, 16);
    
	int ret;
	ret = mpp_buffer_group_get_internal(&frmGrp, MPP_BUFFER_TYPE_ION);
	if(ret)
	{
		MPP_ERR("frmGrp mpp_buffer_group_get_internal erron (%d)\r\n",ret);
		return -1;
	}
    

	ret = mpp_buffer_group_get_internal(&pktGrp, MPP_BUFFER_TYPE_ION);
	if(ret)
	{
		MPP_ERR("frmGrp mpp_buffer_group_get_internal erron (%d)\r\n",ret);
		return -1;
	}
	ret = mpp_frame_init(&frame); /* output frame */
    if (MPP_OK != ret) 
	{
        MPP_ERR("mpp_frame_init failed\n");
        return -1;
    }
	ret = mpp_buffer_get(frmGrp, &frmBuf, hor_stride * ver_stride * 4);
    if (ret) 
	{
        MPP_ERR("frmGrp mpp_buffer_get erron (%d) \n", ret);
        return -1;
    }
	ret = mpp_buffer_get(pktGrp, &pktBuf, hor_stride * ver_stride * 4); //2);
    if (ret) 
	{
        MPP_ERR("pktGrp mpp_buffer_get erron (%d) \n", ret);
        return -1;
    }
	mpp_packet_init_with_buffer(&packet, pktBuf);
	dataBuf = (char *)mpp_buffer_get_ptr(pktBuf);

	mpp_frame_set_buffer(frame, frmBuf);
    return 0;
}


int MppDecode::decode(unsigned char *srcFrm, size_t srcLen, cv::Mat &image)
{
	MppTask task = NULL;
	int ret;

	//int pktEos = 0;
	memcpy(dataBuf, srcFrm, srcLen);//拷贝到mpp_buffer

	mpp_packet_set_pos(packet, dataBuf);
    mpp_packet_set_length(packet, srcLen);

	// if(pktEos)
	// {
	// 	mpp_packet_set_eos(packet);
	// }

	ret = mpi->poll(ctx, MPP_PORT_INPUT, MPP_POLL_BLOCK);
    if (ret) 
	{
        MPP_ERR("mpp input poll failed\n");
        return ret;
    }

	ret = mpi->dequeue(ctx, MPP_PORT_INPUT, &task);  /* input queue */
    if (ret) 
	{
        MPP_ERR("mpp task input dequeue failed\n");
        return ret;
    }

	mpp_task_meta_set_packet(task, KEY_INPUT_PACKET, packet);
    mpp_task_meta_set_frame (task, KEY_OUTPUT_FRAME,  frame);

	ret = mpi->enqueue(ctx, MPP_PORT_INPUT, task);  /* input queue */
    if (ret) 
	{
        MPP_ERR("mpp task input enqueue failed\n");
        return ret;
    }

	/* poll and wait here */
    ret = mpi->poll(ctx, MPP_PORT_OUTPUT, MPP_POLL_BLOCK);
    if (ret) 
	{
        MPP_ERR("mpp output poll failed\n");
        return ret;
    }

	ret = mpi->dequeue(ctx, MPP_PORT_OUTPUT, &task); /* output queue */
    if (ret) 
	{
        MPP_ERR("mpp task output dequeue failed\n");
        return ret;
    }

	int image_res = 0;

	if (task)
	{
		MppFrame frameOut = NULL;
		mpp_task_meta_get_frame(task, KEY_OUTPUT_FRAME, &frameOut);

		if (frame) 
		{
			image_res = get_image(frame,image);

            // if (mpp_frame_get_eos(frameOut))
            // {
			// 	MPP_DBG("found eos frame\n");
			// }
        }
		else
		{
			image_res = -1;
		}

		ret = mpi->enqueue(ctx, MPP_PORT_OUTPUT, task);
        if (ret)
        {
			MPP_ERR("mpp task output enqueue failed\n");
			return ret;
		}
	}
	else
	{
		MPP_ERR("!tast\n");
		return -1;
	}

	return image_res;//如果获取图像错误，从最后返回错误值，而不提前return
}


int MppDecode::get_image(MppFrame &frame,cv::Mat &image)
{
    RK_U32 width    = 0;
    RK_U32 height   = 0;
    RK_U32 h_stride = 0;
    RK_U32 v_stride = 0;
    MppFrameFormat fmt;
    MppBuffer buffer    = NULL;
    RK_U8 *base = NULL;

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

    base = (RK_U8 *)mpp_buffer_get_ptr(buffer);//这里的base是不带cache的内存，如果用mmcpy到用户内存会很慢，后续必须通过rga拷贝或者缩放
    
	if(height<=0 || width<=0 || base==NULL)
	{
		MPP_ERR("height<=0 || width<=0 || base==NULL\n");
		return -1;
	}

    image = cv::Mat(height, width, CV_8UC3, base);

	//printf("%dx%d %dx%d %d %d\n",width,height,h_stride,v_stride,fmt,base);

	return 0;
}

#endif

