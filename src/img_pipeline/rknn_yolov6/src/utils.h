#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/time.h>

#if(USE_ARM_LIB==1)
    #include "RgaUtils.h"
    #include "im2d.h"
    #include "rga.h"
    #include "rknn_api.h"
#endif


#include <opencv2/opencv.hpp>

using namespace std;

#if(USE_ARM_LIB==1)
int rga_resize(cv::Mat &img,cv::Mat &img_resize,cv::Size &size);

int rknn_load(rknn_context &ctx,const char* model_name,
              unsigned char* model_data, rknn_input_output_num &io_num,bool print_perf_detail,bool use_multi_npu_core);

int rknn_config(rknn_context &ctx,rknn_input_output_num &io_num,int &width,int &height,
              rknn_input *inputs,rknn_output *outputs,
              std::vector<float>    &out_scales,std::vector<int32_t>  &out_zps,bool output_want_float);

void dump_tensor_attr(rknn_tensor_attr* attr);
#endif

double __get_us(struct timeval t);

unsigned char* load_data(FILE* fp, size_t ofst, size_t sz);

unsigned char* load_model(const char* filename, int* model_size);

int saveFloat(const char* file_name, float* output, int element_size);


void save_ply(const char* filename, const cv::Mat& points, const cv::Mat& color_img);

string replace_all(string src, string old_value, string new_value);

int get_file_id(std::string &path);
