#if(USE_ARM_LIB==1)

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>
#include <rockchip/rk_mpi.h>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
}

#include <string>
#include <vector>

#define MPP_ALIGN(x, a)         (((x)+(a)-1)&~((a)-1))


typedef struct {
        // global flow control flag
        RK_U32 frm_eos;
        RK_U32 pkt_eos;
        RK_U32 frame_count;
        RK_U64 stream_size;

        // base flow context
        MppCtx ctx;
        MppApi *mpi;
        MppEncPrepCfg prep_cfg;
        MppEncRcCfg rc_cfg;
        MppEncCodecCfg codec_cfg;

        // input / output
        MppBuffer frm_buf;
        MppEncSeiMode sei_mode;

        // paramter for resource malloc
        RK_U32 width;
        RK_U32 height;
        RK_U32 hor_stride;
        RK_U32 ver_stride;
        MppFrameFormat fmt;
        MppCodingType type;
        RK_U32 num_frames;

        // resources
        size_t frame_size;
        /* NOTE: packet buffer may overflow */
        size_t packet_size;

        // rate control runtime parameter
        RK_S32 gop;
        RK_S32 fps;
        RK_S32 bps;
        //FILE *fp_output;
        //FILE *fp_outputx;
} MppContext;

class MppEncode
{
public:
        MppEncode();
        ~MppEncode();

        void init(int wid,int hei,int jpeg_quality);
        int encode(unsigned char *in_data, int in_size,std::vector<unsigned char> &jpeg_data);

private:
        MppContext mpp_enc_data;
        void *buf_ptr;
        MppFrame frame;
};

#endif
