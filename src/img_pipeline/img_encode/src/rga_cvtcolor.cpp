#if(USE_ARM_LIB==1)

#include <opencv2/opencv.hpp>

#include "rga/RgaUtils.h"
#include "rga/im2d.h"
#include "rga/rga.h"

int rga_cvtcolor(const cv::Mat &img_rgb, cv::Mat &img_yuv)
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

    src = wrapbuffer_virtualaddr((void*)img_rgb.data, img_rgb.cols, img_rgb.rows, RK_FORMAT_RGB_888);
    dst = wrapbuffer_virtualaddr((void*)img_yuv.data, img_rgb.cols, img_rgb.rows, RK_FORMAT_YCbCr_420_P);

    int ret = imcheck(src, dst, src_rect, dst_rect);
    if (IM_STATUS_NOERROR != ret)
    {
        printf("%d, check error! %s", __LINE__, imStrError((IM_STATUS)ret));
        return -1;
    }

    IM_STATUS STATUS = imcvtcolor(src, dst, src.format, dst.format);

    return STATUS;
}

#endif
