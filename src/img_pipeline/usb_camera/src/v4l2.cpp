#include "v4l2.h"

#define LOG ROS_INFO


int V4l2::init_video(const char *dev_name,int width,int height)
{
    struct v4l2_fmtdesc fmtdesc;
    int i, ret;

    // Open Device
    fd = open(dev_name, O_RDWR, 0);
    if (fd < 0) {
        LOG("Open %s failed!!!", dev_name);
        return -1;
    }

    // Query Capability
    struct v4l2_capability cap;
    ret = ioctl(fd,VIDIOC_QUERYCAP,&cap);
    if (ret < 0) {
        LOG("VIDIOC_QUERYCAP failed (%d)", ret);
        return ret;
    }

    // Set Stream Format
    fmtdesc.index=0;
    fmtdesc.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
    LOG("Support format:");
    while(ioctl(fd,VIDIOC_ENUM_FMT,&fmtdesc)!=-1)
    {
        LOG("SUPPORT %d.%s",fmtdesc.index+1,fmtdesc.description);
        fmtdesc.index++;
    }

    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = width;
    fmt.fmt.pix.height = height;
    fmt.fmt.pix.pixelformat = VIDEO_FORMAT;
    fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;
    ret = ioctl(fd, VIDIOC_S_FMT, &fmt);
    if (ret < 0)
    {
        LOG("VIDIOC_S_FMT failed (%d)", ret);
        return ret;
    }

    // Get Stream Format
    ret = ioctl(fd, VIDIOC_G_FMT, &fmt);
    if (ret < 0) {
        LOG("VIDIOC_G_FMT failed (%d)", ret);
        return ret;
    }

    struct v4l2_streamparm param;
    memset(&param,0,sizeof(param));
    param.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    param.parm.capture.timeperframe.numerator=1;
    param.parm.capture.timeperframe.denominator=30;
    param.parm.capture.capturemode = 1;
    ret = ioctl(fd, VIDIOC_S_PARM, &param) ;
    if(ret < 0)
    {
        LOG("VIDIOC_S_PARAM failed (%d)", ret);
        return ret;
    }

    ret = ioctl(fd, VIDIOC_G_PARM, &param) ;
    if(ret < 0)  {
        LOG("VIDIOC_G_PARAM failed (%d)", ret);
        return ret;
    }

    // Print Stream Format
    LOG("Stream Format Informations:");
    LOG(" type: %d", fmt.type);
    LOG(" width: %d", fmt.fmt.pix.width);
    LOG(" height: %d", fmt.fmt.pix.height);

    char fmtstr[8];
    memset(fmtstr, 0, 8);
    memcpy(fmtstr, &fmt.fmt.pix.pixelformat, 4);
    LOG(" pixelformat: %s", fmtstr);
    LOG(" field: %d", fmt.fmt.pix.field);
    LOG(" bytesperline: %d", fmt.fmt.pix.bytesperline);
    LOG(" sizeimage: %d", fmt.fmt.pix.sizeimage);
    LOG(" colorspace: %d", fmt.fmt.pix.colorspace);
    //LOG(" priv: %d", fmt.fmt.pix.priv);
    //LOG(" raw_date: %s", fmt.fmt.raw_data);

    // Request buffers
    struct v4l2_requestbuffers reqbuf;
    reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqbuf.memory = V4L2_MEMORY_MMAP;
    reqbuf.count = BUFFER_COUNT;
    ret = ioctl(fd , VIDIOC_REQBUFS, &reqbuf);
    if(ret < 0) {
        LOG("VIDIOC_REQBUFS failed (%d)", ret);
        return ret;
    }


    // Queen buffers
    for(i=0; i<BUFFER_COUNT; i++)
    {
        // Query buffer
        buf.index = i;
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        ret = ioctl(fd , VIDIOC_QUERYBUF, &buf);
        if(ret < 0) {
            LOG("VIDIOC_QUERYBUF (%d) failed (%d)", i, ret);
            return ret;
        }
        // mmap buffer
        mmap_buffer[i].length= buf.length;
        mmap_buffer[i].start = (unsigned char *)mmap(0, buf.length, 
            PROT_READ|PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
        if (mmap_buffer[i].start == MAP_FAILED) {
            LOG("mmap (%d) failed: %s", i, strerror(errno));
            return -1;
        }
        // Queen buffer
        ret = ioctl(fd , VIDIOC_QBUF, &buf);
        if (ret < 0) {
            LOG("VIDIOC_QBUF (%d) failed (%d)", i, ret);
            return -1;
        }
    }

    // Stream On
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ret = ioctl(fd, VIDIOC_STREAMON, &type);
    if (ret < 0)
    {
        LOG("VIDIOC_STREAMON failed (%d)", ret);
        return ret;
    }
    return 0;
}

int V4l2::get_data(FrameBuf *frame_buf)//读取数据到buf
{
    int ret;

    fd_set fds;
    struct timeval tv;
    int r;

    //将fd加入fds集合
    FD_ZERO(&fds);
    FD_SET(fd, &fds);

    /* Timeout. */
    tv.tv_sec = 5;
    tv.tv_usec = 0;

    //监测是否有数据，最多等待5s
    r = select(fd + 1, &fds, NULL, NULL, &tv);
    if(r==-1)
    {
        LOG("select err");
        return -1;
    }
    else if(r==0)
    {
        LOG("select timeout");
        return -1;
    }

    

    // Get frame
    // 捕获数据
    ret = ioctl(fd, VIDIOC_DQBUF, &buf);
    if (ret < 0) {
        LOG("VIDIOC_DQBUF failed (%d)", ret);
        return ret;
    }

    frame_buf->start = mmap_buffer[buf.index].start;
    frame_buf->length = buf.bytesused;

    // Re-queen buffer
    // 将处理完毕的视频帧重新放回驱动程序的队列中，以供下一次捕获
    ret = ioctl(fd, VIDIOC_QBUF, &buf);
    if (ret < 0) {
        LOG("VIDIOC_QBUF failed (%d)", ret);
        return ret;
    }

    return 0;
}

void V4l2::release_video()
{
    // Release the resource
    for (int i=0; i<BUFFER_COUNT; i++) {
        munmap(mmap_buffer[i].start, mmap_buffer[i].length);
    }
    close(fd);
    LOG("Camera release done.");
}

