#if(USE_ARM_LIB==1)

#include <opencv2/opencv.hpp>

#include "RgaUtils.h"
#include "im2d.h"
#include "rga.h"

int rga_resize(cv::Mat &img_in,unsigned char *img_out_data,float scale)
{
    // init rga context
    rga_buffer_t src;
    rga_buffer_t dst;
    im_rect      src_rect;
    im_rect      dst_rect;
    memset(&src_rect, 0, sizeof(src_rect));
    memset(&dst_rect, 0, sizeof(dst_rect));
    memset(&src, 0, sizeof(src));
    memset(&dst, 0, sizeof(dst));

    src = wrapbuffer_virtualaddr((void*)img_in.data,  img_in.cols,         img_in.rows, RK_FORMAT_RGB_888);
    dst = wrapbuffer_virtualaddr((void*)img_out_data, img_in.cols * scale, img_in.rows * scale, RK_FORMAT_RGB_888);

    int ret = imcheck(src, dst, src_rect, dst_rect);
    if (IM_STATUS_NOERROR != ret)
    {
        printf("%d, check error! %s", __LINE__, imStrError((IM_STATUS)ret));
        return -1;
    }

    IM_STATUS STATUS;
    if(scale != 1.0)
        STATUS = imresize(src, dst);
    else
        STATUS = imcopy(src, dst);//如果尺寸不变，则拷贝到用户空间，用这个函数可以节约拷贝时间

    return STATUS;
}

#endif
