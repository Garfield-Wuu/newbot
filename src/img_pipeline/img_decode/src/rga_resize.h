#if(USE_ARM_LIB==1)

#include <opencv2/opencv.hpp>

/** 0 成功，-1 失败（含 imcheck / RGA 调用失败） */
int rga_resize(cv::Mat &img_in, unsigned char *img_out_data, int dst_w, int dst_h);

/**
 * 源为 dma-buf fd（如 MPP 解码缓冲）；wstride_px/hstride_px 为 RGA 要求的像素 stride。
 * dst 为普通用户内存。0 成功，-1 失败。
 */
int rga_resize_fd(int src_fd, int src_w, int src_h, int wstride_px, int hstride_px,
                  unsigned char *dst_data, int dst_w, int dst_h);

#endif
