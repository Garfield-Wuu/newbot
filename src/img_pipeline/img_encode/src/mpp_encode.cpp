#if(USE_ARM_LIB==1)

#include "mpp_encode.h"

MppEncode::MppEncode()
{

}

MppEncode::~MppEncode()
{
    MPP_RET ret = MPP_OK;
    ret = mpp_enc_data.mpi->reset(mpp_enc_data.ctx);
    if (ret)
    {
        printf("mpi->reset failed\n");
    }

    if (mpp_enc_data.ctx)
    {
        mpp_destroy(mpp_enc_data.ctx);
        mpp_enc_data.ctx = NULL;
    }

    if (mpp_enc_data.frm_buf)
    {
        mpp_buffer_put(mpp_enc_data.frm_buf);
        mpp_enc_data.frm_buf = NULL;
    }
}

void MppEncode::init(int wid,int hei,int jpeg_quality)
{
    //清空配置
    memset(&mpp_enc_data, 0, sizeof(MppContext));

    mpp_enc_data.width = wid;
    mpp_enc_data.height = hei;
    int fps = 30;

    //mpp编码图像的行和列都是按16位对齐的，如果输出的行列不是16的整数，则需要在编码时将数据按照16位对齐。
    //此函数就是为了得到行列补齐16整除的数据，比如行是30，通过MPP_ALIGN（30，16）；的输出就是32；
    mpp_enc_data.hor_stride = MPP_ALIGN(mpp_enc_data.width, 16);
    mpp_enc_data.ver_stride = mpp_enc_data.height; //MPP_ALIGN(mpp_enc_data.height, 16);
    //实测高度是360的时候，也可以正常运行，高度不用是16的倍数

    //经测试，只有MPP_FMT_YUV420SP(Y+UV交替)和MPP_FMT_YUV420P(Y+U+V)才可以
    mpp_enc_data.fmt = MPP_FMT_YUV420P;
    mpp_enc_data.type = MPP_VIDEO_CodingMJPEG;//MPP_VIDEO_CodingAVC;
    mpp_enc_data.fps = fps;
    mpp_enc_data.gop = fps*2;
    mpp_enc_data.bps = wid*hei/8*mpp_enc_data.fps;//压缩后每秒视频的bit位大小

    switch (mpp_enc_data.fmt & MPP_FRAME_FMT_MASK)
    {
        case MPP_FMT_YUV420SP:
        case MPP_FMT_YUV420P: {
            mpp_enc_data.frame_size = mpp_enc_data.hor_stride * mpp_enc_data.ver_stride * 3 / 2;
        } break;

        case MPP_FMT_YUV422_YUYV :
        case MPP_FMT_YUV422_YVYU :
        case MPP_FMT_YUV422_UYVY :
        case MPP_FMT_YUV422_VYUY :
        case MPP_FMT_YUV422P :
        case MPP_FMT_YUV422SP : {
            mpp_enc_data.frame_size = mpp_enc_data.hor_stride * mpp_enc_data.ver_stride * 2;
        } break;

        case MPP_FMT_RGB444 :
        case MPP_FMT_BGR444 :
        case MPP_FMT_RGB555 :
        case MPP_FMT_BGR555 :
        case MPP_FMT_RGB565 :
        case MPP_FMT_BGR565 :
        case MPP_FMT_RGB888 :
        case MPP_FMT_BGR888 :
        case MPP_FMT_RGB101010 :
        case MPP_FMT_BGR101010 :
        case MPP_FMT_ARGB8888 :
        case MPP_FMT_ABGR8888 :
        case MPP_FMT_BGRA8888 :
        case MPP_FMT_RGBA8888 : {
            mpp_enc_data.frame_size = mpp_enc_data.hor_stride * mpp_enc_data.ver_stride * 3;
        } break;

        default: {
            mpp_enc_data.frame_size = mpp_enc_data.hor_stride * mpp_enc_data.ver_stride * 4;
        } break;
    }

    MPP_RET ret = MPP_OK;
    //开辟编码时需要的内存
    ret = mpp_buffer_get(NULL, &mpp_enc_data.frm_buf, mpp_enc_data.frame_size);
    if (ret)
    {
        printf("failed to get buffer for input frame ret %d\n", ret);
        goto MPP_INIT_OUT;
    }

    //创建 MPP context 和 MPP api 接口
    ret = mpp_create(&mpp_enc_data.ctx, &mpp_enc_data.mpi);
    if (ret)
    {
        printf("mpp_create failed ret %d\n", ret);
        goto MPP_INIT_OUT;
    }

    /*初始化编码还是解码，以及编解码的格式
    MPP_CTX_DEC ： 解码
    MPP_CTX_ENC ： 编码
    MPP_VIDEO_CodingAVC ： H.264
    MPP_VIDEO_CodingHEVC :  H.265
    MPP_VIDEO_CodingMJPEG : MJPEG*/
    ret = mpp_init(mpp_enc_data.ctx, MPP_CTX_ENC, mpp_enc_data.type);
    if (ret)
    {
        printf("mpp_init failed ret %d\n", ret);
        goto MPP_INIT_OUT;
    }

    /*设置编码码率、质量、定码率变码率*/
    mpp_enc_data.rc_cfg.change  = MPP_ENC_RC_CFG_CHANGE_ALL;
    mpp_enc_data.rc_cfg.rc_mode = MPP_ENC_RC_MODE_VBR;//CBR 为 Constant Bit Rate，固定码率模式。在固定码率模式下，目标码率起决定性作用。
    //VBR 为 Variable Bit Rate，可变码率模式。在可变码率模式下，最大最小码率起决定性作用。

    mpp_enc_data.rc_cfg.quality = MPP_ENC_RC_QUALITY_MEDIUM; //MPP_ENC_RC_QUALITY_MEDIUM;//表示编码图像质量模式，共分为七档，主要在 VBR 模式下有生效，因为 CBR 模式为码率优先，图像质量模式不起作用

    if (mpp_enc_data.rc_cfg.rc_mode == MPP_ENC_RC_MODE_CBR)
    {
        /* constant bitrate has very small bps range of 1/16 bps */
        mpp_enc_data.rc_cfg.bps_target   = mpp_enc_data.bps;
        mpp_enc_data.rc_cfg.bps_max      = mpp_enc_data.bps * 17 / 16;
        mpp_enc_data.rc_cfg.bps_min      = mpp_enc_data.bps * 15 / 16;
    }
    else if (mpp_enc_data.rc_cfg.rc_mode ==  MPP_ENC_RC_MODE_VBR)
    {
        if (mpp_enc_data.rc_cfg.quality == MPP_ENC_RC_QUALITY_CQP)
        {
            /* constant QP does not have bps */
            mpp_enc_data.rc_cfg.bps_target   = -1;
            mpp_enc_data.rc_cfg.bps_max      = -1;
            mpp_enc_data.rc_cfg.bps_min      = -1;
        }
        else
        {
            /* variable bitrate has large bps range */
            mpp_enc_data.rc_cfg.bps_target   = mpp_enc_data.bps;
            mpp_enc_data.rc_cfg.bps_max      = mpp_enc_data.bps * 17 / 16;
            mpp_enc_data.rc_cfg.bps_min      = mpp_enc_data.bps * 1 / 16;
        }
    }

    /* fix input / output frame rate */
    mpp_enc_data.rc_cfg.fps_in_flex      = 0;//为 0 表 示 输 入 帧 率 固 定 ， 帧 率 计 算 方 式 为 fps_in_num/fps_in_denorm，可以表示分数帧率。
    //为 1 表示输入帧率可变，可变帧率的情况下，帧率不固定，对应的码率计算与分配的规则变为按实际时间进行计算。
    mpp_enc_data.rc_cfg.fps_in_num       = mpp_enc_data.fps;//表示输入帧率分数值的分子部分，如为 0 表示默认 30fps
    mpp_enc_data.rc_cfg.fps_in_denorm    = 1;//表示输入帧率分数值的分母部分。如为 0 表示为 1
    mpp_enc_data.rc_cfg.fps_out_flex     = 0;
    mpp_enc_data.rc_cfg.fps_out_num      = mpp_enc_data.fps;
    mpp_enc_data.rc_cfg.fps_out_denorm   = 1;

    mpp_enc_data.rc_cfg.gop              = mpp_enc_data.gop;//表示 Group Of Picture，即两个 I 帧之间的间隔
    mpp_enc_data.rc_cfg.skip_cnt         = 0;


     // drop frame or not when bitrate overflow
    //新加的
    mpp_enc_data.rc_cfg.drop_mode        = MPP_ENC_RC_DROP_FRM_DISABLED;
    mpp_enc_data.rc_cfg.drop_threshold   = 20;
    mpp_enc_data.rc_cfg.drop_gap         = 1;

    //注意：必须要设置qp，否则264的数据包会特别大！！！
    mpp_enc_data.rc_cfg.qp_init = -1;
    mpp_enc_data.rc_cfg.qp_max = 51;
    mpp_enc_data.rc_cfg.qp_min = 10;

    mpp_enc_data.rc_cfg.qp_max_i = 51;
    mpp_enc_data.rc_cfg.qp_min_i = 10;
    mpp_enc_data.rc_cfg.qp_delta_ip = 2;


    //所有RC设置生效
    ret = mpp_enc_data.mpi->control(mpp_enc_data.ctx, MPP_ENC_SET_RC_CFG, &mpp_enc_data.rc_cfg);
    if (ret)
    {
        printf("mpi control enc set rc cfg failed ret %d\n", ret);
        goto MPP_INIT_OUT;
    }


    /*设置编码参数：宽高、对齐后宽高等参数*/
   //mpp_enc_data.bps = mpp_enc_data.width * mpp_enc_data.height / 8 * mpp_enc_data.fps;
   mpp_enc_data.prep_cfg.change        = MPP_ENC_PREP_CFG_CHANGE_INPUT |
           MPP_ENC_PREP_CFG_CHANGE_ROTATION |
           MPP_ENC_PREP_CFG_CHANGE_FORMAT;
   mpp_enc_data.prep_cfg.width         = mpp_enc_data.width;
   mpp_enc_data.prep_cfg.height        = mpp_enc_data.height;
   mpp_enc_data.prep_cfg.hor_stride    = mpp_enc_data.hor_stride;
   mpp_enc_data.prep_cfg.ver_stride    = mpp_enc_data.ver_stride;
   mpp_enc_data.prep_cfg.format        = mpp_enc_data.fmt;
   mpp_enc_data.prep_cfg.rotation      = MPP_ENC_ROT_0;

   //所有PREP设置生效
   ret = mpp_enc_data.mpi->control(mpp_enc_data.ctx, MPP_ENC_SET_PREP_CFG, &mpp_enc_data.prep_cfg);
   if (ret)
   {
       printf("mpi control enc set prep cfg failed ret %d\n", ret);
       goto MPP_INIT_OUT;
   }

    /*设置264相关的其他编码参数*/
    mpp_enc_data.codec_cfg.coding = mpp_enc_data.type;

    switch (mpp_enc_data.codec_cfg.coding)
    {
        case MPP_VIDEO_CodingAVC :
        {
            mpp_enc_data.codec_cfg.h264.change = MPP_ENC_H264_CFG_CHANGE_ALL; //MPP_ENC_H264_CFG_CHANGE_PROFILE |
                    //MPP_ENC_H264_CFG_CHANGE_ENTROPY |
                    //MPP_ENC_H264_CFG_CHANGE_TRANS_8x8;
    //         *
    //         * H.264 profile_idc parameter
    //         * 66  - Baseline profile
    //         * 77  - Main profile
    //         * 100 - High profile
    //         *
            mpp_enc_data.codec_cfg.h264.profile  = 66; //100;//表示 SPS 中的 profile_idc 参数

            //在相同配置情况下，High profile（HP）可以比Main profile（MP）降低10%的码率。
            //Baseline profile多应用于实时通信领域，Main profile多应用于流媒体领域，High profile则多应用于广电和存储领域。

    //        *
    //         * H.264 level_idc parameter
    //         * 10 / 11 / 12 / 13    - qcif@15fps / cif@7.5fps / cif@15fps / cif@30fps
    //         * 20 / 21 / 22         - cif@30fps / half-D1@@25fps / D1@12.5fps
    //         * 30 / 31 / 32         - D1@25fps / 720p@30fps / 720p@60fps
    //         * 40 / 41 / 42         - 1080p@30fps / 1080p@30fps / 1080p@60fps
    //         * 50 / 51 / 52         - 4K@30fps
    //         *
            mpp_enc_data.codec_cfg.h264.level    = 31; //40;//表示 SPS 中的 level_idc 参数，其中 10 表示 level 1.0 一般配置为 level 4.1 即可满足要求
            mpp_enc_data.codec_cfg.h264.entropy_coding_mode  = 1;//熵编码格式 0 – CAVLC，自适应变长编码 1 – CABAC，自适应算术编码
            mpp_enc_data.codec_cfg.h264.cabac_init_idc  = 0;//表示协议语法中的 cabac_init_idc，在 entropy_coding_mode 为 1 时有效，有效值为 0~2
            mpp_enc_data.codec_cfg.h264.transform8x8_mode = 1;//8x8 变换使能标志。0 – 为关闭，在 Baseline/Main profile 时固定关闭。1 – 为开启，在 High profile 时可选可启

            //具体配置的含义看MPP开发参考.pdf
        }
        break;
        case MPP_VIDEO_CodingMJPEG :
        {
            mpp_enc_data.codec_cfg.jpeg.change  = MPP_ENC_JPEG_CFG_CHANGE_QP;
            mpp_enc_data.codec_cfg.jpeg.quant   = jpeg_quality;//JPEG编码质量设置
        }
        break;
        case MPP_VIDEO_CodingVP8 :
        case MPP_VIDEO_CodingHEVC :
        default :
        {
            printf("support encoder coding type %d\n", mpp_enc_data.codec_cfg.coding);
        }
        break;
    }


    //所有CODEC_CFG设置生效
    ret = mpp_enc_data.mpi->control(mpp_enc_data.ctx, MPP_ENC_SET_CODEC_CFG, &mpp_enc_data.codec_cfg);
    if (ret)
    {
        printf("mpi control enc set codec cfg failed ret %d\n", ret);
        goto MPP_INIT_OUT;
    }


    //optional
    mpp_enc_data.sei_mode = MPP_ENC_SEI_MODE_ONE_FRAME;
    ret = mpp_enc_data.mpi->control(mpp_enc_data.ctx, MPP_ENC_SET_SEI_CFG, &mpp_enc_data.sei_mode);
    if (ret)
    {
        printf("mpi control enc set sei cfg failed ret %d\n", ret);
        goto MPP_INIT_OUT;
    }



    //////////////
	buf_ptr = mpp_buffer_get_ptr(mpp_enc_data.frm_buf);

	ret = mpp_frame_init(&frame);
	if (ret)
	{
		printf("mpp_frame_init failed\n");
        return;
	}

	mpp_frame_set_width(frame, mpp_enc_data.width);
	mpp_frame_set_height(frame, mpp_enc_data.height);
	mpp_frame_set_hor_stride(frame, mpp_enc_data.hor_stride);
	mpp_frame_set_ver_stride(frame, mpp_enc_data.ver_stride);
	mpp_frame_set_fmt(frame, mpp_enc_data.fmt);
	mpp_frame_set_buffer(frame, mpp_enc_data.frm_buf);
	mpp_frame_set_eos(frame, mpp_enc_data.frm_eos);


    return;

MPP_INIT_OUT:

    if (mpp_enc_data.ctx)
    {
        mpp_destroy(mpp_enc_data.ctx);
        mpp_enc_data.ctx = NULL;
    }

    if (mpp_enc_data.frm_buf)
    {
        mpp_buffer_put(mpp_enc_data.frm_buf);
        mpp_enc_data.frm_buf = NULL;
    }

    printf("init mpp failed!\n");
}

int MppEncode::encode(unsigned char *in_data, int in_size,std::vector<unsigned char> &jpeg_data)
{
	MPP_RET ret = MPP_OK;
	MppPacket packet = NULL;

    memcpy(buf_ptr, in_data, in_size);

  	ret = mpp_enc_data.mpi->encode_put_frame(mpp_enc_data.ctx, frame);
	if (ret)
	{
		printf("mpp encode put frame failed\n");
        return ret;
	}

	ret = mpp_enc_data.mpi->encode_get_packet(mpp_enc_data.ctx, &packet);
	if (ret)
	{
		printf("mpp encode get packet failed\n");
        return ret;
	}

	if (!packet)
	{
		printf("!packet\n");
        return -1;
    }

    // send packet here
    //ptr是编码后的数据
    uint8_t *ptr  = (uint8_t*)mpp_packet_get_pos(packet);
    size_t   len  = mpp_packet_get_length(packet);
    if(len<=0)
    {
        printf("encode len error!!!\n");
        return -1;
    }
    
    jpeg_data.assign(ptr,ptr+len);

    mpp_packet_deinit(&packet);//会释放packet，所以需要在上面将packet数据拷贝出去

	return 0;
}


#endif

