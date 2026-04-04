#ifndef V4L2_H
#define V4L2_H

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>

#include <ros/ros.h>


#define VIDEO_FORMAT V4L2_PIX_FMT_MJPEG //V4L2_PIX_FMT_YUYV
#define BUFFER_COUNT 4

//使用的摄像头必须要支持MJPEG
//SUPPORT 1.Motion-JPEG
//SUPPORT 2.YUYV 4:2:2

struct FrameBuf
{
  unsigned char *start;
  int length;
};

class V4l2
{

public:
    struct v4l2_buffer buf;//图像数据

    FrameBuf mmap_buffer[BUFFER_COUNT];//图像数据虚拟地址

    int fd;//video设备

    int init_video(const char *dev_name,int width,int height);

    int get_data(FrameBuf *frame_buf);

    void release_video();
};


#endif // V4L2_H
