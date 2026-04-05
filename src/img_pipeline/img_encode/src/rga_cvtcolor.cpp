#if(USE_ARM_LIB==1)

#include <opencv2/opencv.hpp>

#include "rga/RgaUtils.h"
#include "rga/im2d.h"
#include "rga/rga.h"

int rga_cvtcolor(const cv::Mat &img_rgb, cv::Mat &img_yuv)
{
    static bool rga_failed_latch = false;
    if (rga_failed_latch)
        return -1;

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
        printf("%d, check error! %s\n", __LINE__, imStrError((IM_STATUS)ret));
        rga_failed_latch = true;
        return -1;
    }

    im_opt_t opt;
    memset(&opt, 0, sizeof(opt));
    opt.core = IM_SCHEDULER_RGA3_CORE0;

    rga_buffer_t pat;
    im_rect pat_rect;
    memset(&pat, 0, sizeof(pat));
    memset(&pat_rect, 0, sizeof(pat_rect));

    IM_STATUS STATUS = improcess(src, dst, pat, src_rect, dst_rect, pat_rect,
                                 -1, NULL, &opt, IM_SYNC);

    if (STATUS != IM_STATUS_SUCCESS && STATUS != IM_STATUS_NOERROR)
    {
        printf("img_encode rga_cvtcolor failed (%d), latching off\n", (int)STATUS);
        rga_failed_latch = true;
    }

    return STATUS;
}

#endif
