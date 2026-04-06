#include "img_decode.h"

#include <cerrno>
#include <cmath>
#include <thread>
#if(USE_ARM_LIB==1)
#include <atomic>
#include <unistd.h>

namespace {
// 0 = RGA 可用；>0 = 失败冷却帧计数（每帧递减，归零后自动重试）
std::atomic<int> g_rga_fd_cooldown{0};   // FD-RGA 路径冷却计数
std::atomic<int> g_rga_va_cooldown{0};   // VA-RGA 路径冷却计数
static constexpr int RGA_RETRY_FRAMES = 30;
}
#endif

//如果没有破浪线nh.param后面的参数要加上节点名字，否则获取不到launch中的参数，所以最好加上波浪线
ImgDecode::ImgDecode() : nh("~") 
{
    string pub_raw_image_topic,camera_info_topic;
    string camera_name,camera_info_url;
    int width,height;

    nh.param<string>("sub_jpeg_image_topic", sub_jpeg_image_topic, "/image_raw/compressed");
    nh.param<string>("pub_raw_image_topic", pub_raw_image_topic, "/camera/image_raw");
    nh.param<int>("fps_div", fps_div, 2);
    nh.param<double>("scale", scale, 1.0);

    nh.param<int>("width", width, 1280);
    nh.param<int>("height", height, 720);
    nh.param<bool>("lazy_compressed_subscribe", lazy_compressed_subscribe, false);
    /* 旧参数保留，默认 false；实际由下面两个精细参数控制 */
    nh.param<bool>("use_rga", use_rga, false);
    /* 阶段2：VA-RGA（heap 拷贝后虚拟地址 RGA），默认关 */
    nh.param<bool>("use_rga_va_fallback", use_rga_va_fallback, false);
    /* 阶段3：FD-RGA（importbuffer_fd 路径），默认关；高风险实验 */
    nh.param<bool>("use_rga_fd_experimental", use_rga_fd_experimental, false);

    raw_image_pub = nh.advertise<sensor_msgs::Image>(pub_raw_image_topic, 10);

    if (!lazy_compressed_subscribe)
    {
        compressed_image_sub = nh.subscribe(sub_jpeg_image_topic, 2, &ImgDecode::compressed_image_callback, this);
        ROS_INFO("img_decode: always-on subscribe %s (queue=2)", sub_jpeg_image_topic.c_str());
    }

#if(USE_ARM_LIB==1)
    mpp_decode.init(width, height);
    if (!mpp_decode.is_ready())
        ROS_ERROR("mpp_decode failed to init (buffer group / frame). No JPEG decode until fixed — see terminal for [MPP ERR].");
#endif

}

void ImgDecode::compressed_image_callback(const sensor_msgs::CompressedImageConstPtr& msg)
{

    frame_cnt++;

    if(frame_cnt%fps_div!=0)//分频 减少后续处理负担
    {
        return;
    }


//auto t1 = std::chrono::system_clock::now();

    
#if(USE_ARM_LIB==1)
    if(msg->data.size() <= 4096) //一般情况下，JPEG图像不能小于4KB
    {
        ROS_WARN("jpeg data size error! size = %zu", msg->data.size());
        return;
    }

    if (!mpp_decode.is_ready())
    {
        ROS_WARN_THROTTLE(5.0, "mpp_decode not ready, skip");
        return;
    }

    //硬解码JPEG->RGB
    int ret = mpp_decode.decode((unsigned char*)msg->data.data(), msg->data.size(), image);//msg-->image
    if(ret < 0)
    {
        ROS_WARN("jpeg decode error! size = %zu", msg->data.size());
        return;
    }
#else
    //软解码JPEG->BGR->RGB
    image = cv::imdecode(cv::Mat(msg->data), cv::IMREAD_COLOR);
    cv::cvtColor(image, image, cv::COLOR_BGR2RGB);
    if (image.empty())
    {
        ROS_WARN("Failed to decode compressed image");
        return;
    }
#endif

//auto t2 = std::chrono::system_clock::now();
    

    msg_pub.header = msg->header;//使用原有时间戳
    const int dst_w = std::max(1, (int)std::lround(image.cols * scale));
    const int dst_h = std::max(1, (int)std::lround(image.rows * scale));
    msg_pub.height = dst_h;
    msg_pub.width = dst_w;
    msg_pub.encoding = "rgb8";
    msg_pub.step = dst_w * 3;
    msg_pub.data.resize(static_cast<size_t>(dst_h) * msg_pub.step);

    //避免图像多次拷贝，直接将缩放后的数据写到msg_pub中

#if(USE_ARM_LIB==1)
    {
        bool ok = false;

        // ── 路径A：FD-RGA 实验（use_rga_fd_experimental=true 时启用）────────────
        if (use_rga_fd_experimental)
        {
            int cooldown = g_rga_fd_cooldown.load(std::memory_order_relaxed);
            if (cooldown > 0)
            {
                g_rga_fd_cooldown.store(cooldown - 1, std::memory_order_relaxed);
            }
            else
            {
                RK_U32 rw, rh, rws, rhs;
                mpp_decode.get_last_rga_layout(&rw, &rh, &rws, &rhs);
                // 使用 raw fd（不 dup，不需要 close），缓存 importbuffer_fd 命中同一 fd
                int dmabuf_fd = mpp_decode.get_raw_dmabuf_fd();
                if (dmabuf_fd >= 0)
                {
                    const int rga_fd_ret = rga_resize_fd(dmabuf_fd, (int)rw, (int)rh, (int)rws, (int)rhs,
                                                         msg_pub.data.data(), dst_w, dst_h);
                    if (rga_fd_ret == 0)
                    {
                        ok = true;
                    }
                    else
                    {
                        g_rga_fd_cooldown.store(RGA_RETRY_FRAMES, std::memory_order_relaxed);
                        ROS_WARN_THROTTLE(5.0, "img_decode: FD-RGA failed, cooling %d frames. dmesg|grep rga",
                                         RGA_RETRY_FRAMES);
                    }
                }
                else
                {
                    ROS_WARN_THROTTLE(10.0, "img_decode: get_raw_dmabuf_fd failed (frmDmaFd not ready)");
                }
            }
        }

        if (!ok)
        {
            // ION/uncached Mat 直接 resize 极慢；先 copyTo 到 heap，再操作
            cv::Mat cached;
            image.copyTo(cached);

            // ── 路径B：VA-RGA 实验（use_rga_va_fallback=true 时启用）────────────
            if (use_rga_va_fallback)
            {
                int va_cd = g_rga_va_cooldown.load(std::memory_order_relaxed);
                if (va_cd > 0)
                {
                    g_rga_va_cooldown.store(va_cd - 1, std::memory_order_relaxed);
                }
                else
                {
                    ok = (rga_resize(cached, msg_pub.data.data(), dst_w, dst_h) == 0);
                    if (!ok)
                    {
                        g_rga_va_cooldown.store(RGA_RETRY_FRAMES, std::memory_order_relaxed);
                        ROS_WARN_THROTTLE(10.0, "img_decode: VA-RGA failed, cooling %d frames", RGA_RETRY_FRAMES);
                    }
                }
            }

            // ── 路径C：OpenCV 兜底（始终可用）──────────────────────────────────
            if (!ok)
            {
                cv::Mat resized;
                cv::resize(cached, resized, cv::Size(dst_w, dst_h), 0, 0, cv::INTER_LINEAR);
                memcpy(msg_pub.data.data(), resized.data, msg_pub.data.size());
            }
        }
    }
#else
    //软缩放RGB->小RGB
    cv::Mat resized;
    cv::resize(image, resized, cv::Size(dst_w, dst_h), 0, 0, cv::INTER_LINEAR);
    memcpy(msg_pub.data.data(), resized.data, msg_pub.data.size());
#endif
    
//auto t3 = std::chrono::system_clock::now();

    raw_image_pub.publish(msg_pub);//发布图像

// auto t4 = std::chrono::system_clock::now();

// ROS_WARN_THROTTLE(1,"decode=%d resize=%d pub=%d ms",
// std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count(),
// std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2).count(),
// std::chrono::duration_cast<std::chrono::milliseconds>(t4 - t3).count());
        
}

void ImgDecode::run_check_thread()
{
    if (!lazy_compressed_subscribe)
        return;

    int last_subscribers = 0;
    while(ros::ok())
    {
        int subscribers = raw_image_pub.getNumSubscribers();
        if(subscribers==0 && last_subscribers>0)
        {
            ROS_INFO("decode image subscribers = 0, src sub shutdown");
            compressed_image_sub.shutdown();
        }
        else if(subscribers>0 && last_subscribers==0)
        {
            ROS_INFO("decode image subscribers > 0, src sub start");
            compressed_image_sub = nh.subscribe(sub_jpeg_image_topic, 2, &ImgDecode::compressed_image_callback,this);
        }

        last_subscribers = subscribers;

        usleep(100*1000);//100ms
    }
}



int main(int argc, char** argv)
{
    ros::init(argc, argv, "img_decode");

    ImgDecode img_decode;

    std::thread check_thread(&ImgDecode::run_check_thread, &img_decode);
    check_thread.detach();

    ros::spin();

    return 0;
}




