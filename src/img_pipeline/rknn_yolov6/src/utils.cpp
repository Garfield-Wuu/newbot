#include "utils.h"

#if(USE_ARM_LIB==1)

int rknn_load(rknn_context &ctx,const char* model_name, unsigned char* model_data, rknn_input_output_num &io_num,
              bool print_perf_detail,bool use_multi_npu_core)
{
    int ret;
    /* Create the neural network */
    printf("Loading mode...\n");
    int            model_data_size = 0;
    model_data      = load_model(model_name, &model_data_size);
    if(print_perf_detail)
    {
        ret = rknn_init(&ctx, model_data, model_data_size, RKNN_FLAG_COLLECT_PERF_MASK, NULL);//测试性能 注意：打开测试会让推理变慢！！！！
        printf("print_perf_detail is true\n");
    }
    else
    {
        ret = rknn_init(&ctx, model_data, model_data_size, 0, NULL);//不测试性能
        printf("print_perf_detail is false\n");
    }

    if (ret < 0) {
      printf("rknn_init error ret=%d\n", ret);
      return -1;
    }


//    RKNN_NPU_CORE_AUTO：表示自动调度模型，自动运行在当前空闲的 NPU 核
//    RKNN_NPU_CORE_0_1_2：表示同时工作在 NPU0、NPU1、NPU2 核上 速度有提升，但是提升不多
    rknn_core_mask core_mask;
    if(use_multi_npu_core)
    {
        core_mask = RKNN_NPU_CORE_0_1_2;
        printf("rknn_set_core_mask: RKNN_NPU_CORE_0_1_2\n");
    }
    else
    {
        core_mask = RKNN_NPU_CORE_AUTO;
        printf("rknn_set_core_mask: RKNN_NPU_CORE_AUTO\n");
    }

    ret = rknn_set_core_mask(ctx, core_mask);
    if (ret < 0) {
      printf("rknn_set_core_mask error ret=%d\n", ret);
      return -1;
    }



    rknn_sdk_version version;
    ret = rknn_query(ctx, RKNN_QUERY_SDK_VERSION, &version, sizeof(rknn_sdk_version));
    if (ret < 0) {
      printf("rknn_init error ret=%d\n", ret);
      return -1;
    }
    printf("sdk version: %s driver version: %s\n", version.api_version, version.drv_version);
    //sdk version: 1.4.0 (a10f100eb@2022-09-09T09:07:14) driver version: 0.8.2 --2023.2.3

    ret = rknn_query(ctx, RKNN_QUERY_IN_OUT_NUM, &io_num, sizeof(io_num));
    if (ret < 0) {
      printf("rknn_init error ret=%d\n", ret);
      return -1;
    }
    printf("model input num: %d, output num: %d\n", io_num.n_input, io_num.n_output);

    return 0;
}

int rknn_config(rknn_context &ctx,rknn_input_output_num &io_num,int &width,int &height,
              rknn_input *inputs,rknn_output *outputs,
              std::vector<float>    &out_scales,std::vector<int32_t>  &out_zps,bool output_want_float)
{
    int ret;
    rknn_tensor_attr input_attrs[io_num.n_input];
    rknn_tensor_attr output_attrs[io_num.n_output];
    memset(input_attrs, 0, sizeof(input_attrs));
    for (int i = 0; i < io_num.n_input; i++) {
      input_attrs[i].index = i;
      ret = rknn_query(ctx, RKNN_QUERY_INPUT_ATTR, &(input_attrs[i]), sizeof(rknn_tensor_attr));
      if (ret < 0) {
        printf("rknn_init error ret=%d\n", ret);
        return -1;
      }
      dump_tensor_attr(&(input_attrs[i]));
    }


    memset(output_attrs, 0, sizeof(output_attrs));
    for (int i = 0; i < io_num.n_output; i++)
    {
      output_attrs[i].index = i;
      ret = rknn_query(ctx, RKNN_QUERY_OUTPUT_ATTR, &(output_attrs[i]), sizeof(rknn_tensor_attr));
      dump_tensor_attr(&(output_attrs[i]));
    }

    int channel = 3;
    if (input_attrs[0].fmt == RKNN_TENSOR_NCHW) {
      printf("model is NCHW input fmt\n");
      channel = input_attrs[0].dims[1];
      height  = input_attrs[0].dims[2];
      width   = input_attrs[0].dims[3];
    } else {
      printf("model is NHWC input fmt\n");
      height  = input_attrs[0].dims[1];
      width   = input_attrs[0].dims[2];
      channel = input_attrs[0].dims[3];
    }

    printf("model input height=%d, width=%d, channel=%d\n", height, width, channel);

    memset(inputs, 0, sizeof(rknn_input)*io_num.n_input);
    for (int i = 0; i < io_num.n_input; i++)
    {
        inputs[i].index        = 0;
        inputs[i].type         = RKNN_TENSOR_UINT8;//RKNN_TENSOR_FLOAT16; //RKNN_TENSOR_UINT8; //注意ufld输入比较特殊，输入像素要做标准化，所以输入要转换成FP16(不量化的情况下)
        inputs[i].size         = width * height * channel *1;//uint8 *1; f16 *2 注意:FP16要乘2(不量化的情况下)
        inputs[i].fmt          = RKNN_TENSOR_NHWC;
        inputs[i].pass_through = 0;
    }

    // You may not need resize when src resulotion equals to dst resulotion

    memset(outputs, 0, sizeof(rknn_output)*io_num.n_output);//数组长度=3或5
    for (int i = 0; i < io_num.n_output; i++)
    {
      if(output_want_float)
         outputs[i].want_float = 1;//注意：ufld模型此处要写1 //want_float=0输出是int8, want_float=1输出是float
      else
         outputs[i].want_float = 0;

      printf("outputs%d want_float = %d\n",i,outputs[i].want_float);
    }

    for (int i = 0; i < io_num.n_output; ++i)
    {
      out_scales.push_back(output_attrs[i].scale);
      out_zps.push_back(output_attrs[i].zp);
    }

    return 0;
}


int rga_resize(cv::Mat &img,cv::Mat &img_resize,cv::Size &size)
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


//    printf("resize with RGA!\n");
//    resize_buf = malloc(height * width * 3);
//    memset(resize_buf, 0x00, height * width * 3);


    src = wrapbuffer_virtualaddr((void*)img.data, img.cols, img.rows, RK_FORMAT_RGB_888);
    dst = wrapbuffer_virtualaddr((void*)img_resize.data, size.width, size.height, RK_FORMAT_RGB_888);


    int ret = imcheck(src, dst, src_rect, dst_rect);
    if (IM_STATUS_NOERROR != ret)
    {
        printf("%d, check error! %s", __LINE__, imStrError((IM_STATUS)ret));
        return -1;
    }
    IM_STATUS STATUS = imresize(src, dst);

    return STATUS;

    // for debug
    //cv::Mat resize_img(cv::Size(width, height), CV_8UC3, resize_buf);
    //cv::imwrite("resize_input.jpg", resize_img);
}





void dump_tensor_attr(rknn_tensor_attr* attr)
{
  printf("  index=%d, name=%s, n_dims=%d, dims=[%d, %d, %d, %d], n_elems=%d, size=%d, fmt=%s, type=%s, qnt_type=%s, "
         "zp=%d, scale=%f\n",
         attr->index, attr->name, attr->n_dims, attr->dims[0], attr->dims[1], attr->dims[2], attr->dims[3],
         attr->n_elems, attr->size, get_format_string(attr->fmt), get_type_string(attr->type),
         get_qnt_type_string(attr->qnt_type), attr->zp, attr->scale);
}

#endif



unsigned char* load_data(FILE* fp, size_t ofst, size_t sz)
{
  unsigned char* data;
  int            ret;

  data = NULL;

  if (NULL == fp) {
    return NULL;
  }

  ret = fseek(fp, ofst, SEEK_SET);
  if (ret != 0) {
    printf("blob seek failure.\n");
    return NULL;
  }

  data = (unsigned char*)malloc(sz);
  if (data == NULL) {
    printf("buffer malloc failure.\n");
    return NULL;
  }
  ret = fread(data, 1, sz, fp);
  return data;
}

unsigned char* load_model(const char* filename, int* model_size)
{
  FILE*          fp;
  unsigned char* data;

  fp = fopen(filename, "rb");
  if (NULL == fp) {
    printf("Open file %s failed.\n", filename);
    return NULL;
  }

  fseek(fp, 0, SEEK_END);
  int size = ftell(fp);

  data = load_data(fp, 0, size);

  fclose(fp);

  *model_size = size;
  return data;
}

int saveFloat(const char* file_name, float* output, int element_size)
{
  FILE* fp;
  fp = fopen(file_name, "w");
  for (int i = 0; i < element_size; i++) {
    fprintf(fp, "%.6f\n", output[i]);
  }
  fclose(fp);
  return 0;
}

void save_ply(const char* filename, const cv::Mat& points, const cv::Mat& color_img)
{
    float LARGE_Z_VAL = 50; //10000;//无效值
    FILE* fp = fopen(filename, "w");

    int point_num=0;
    for(int y = 0; y < points.rows; y++)
    {
        for(int x = 0; x < points.cols; x++)
        {
            cv::Vec3f point = points.at<cv::Vec3f>(y, x);
            if( point[2] >= LARGE_Z_VAL || point[2] == 0)
                continue;
            point_num++;
        }
    }

    printf("顶点数:%d\n",point_num);
    fprintf(fp, "ply\n"
                "format ascii 1.0\n"
                "element vertex %d\n"
                "property float x\n"
                "property float y\n"
                "property float z\n"
                "property uchar red\n"
                "property uchar green\n"
                "property uchar blue\n"
                "end_header\n", point_num);

    for(int y = 0; y < points.rows; y++)
    {
        for(int x = 0; x < points.cols; x++)
        {
            cv::Vec3f point = points.at<cv::Vec3f>(y, x);
            if( point[2] >= LARGE_Z_VAL || point[2] == 0)
                continue;

            cv::Vec3b color = color_img.at<cv::Vec3b>(y, x);

            fprintf(fp, "%f %f %f %d %d %d\n",
                    point[0], point[1], point[2],
                    color[2], color[1], color[0]);
        }
    }
    fclose(fp);
}


string replace_all(string src, string old_value, string new_value)
{
    // 每次重新定位起始位置，防止上轮替换后的字符串形成新的old_value
    for (string::size_type pos(0); pos != string::npos; pos += new_value.length())
    {
        if ((pos = src.find(old_value, pos)) != string::npos)
        {
            src.replace(pos, old_value.length(), new_value);
        }
        else
            break;
    }
    return src;
}

int get_file_id(std::string &path)
{
    //1.获取不带路径的文件名
    string::size_type iPos = path.find_last_of('/')+1;
    string filename = path.substr(iPos, path.length() - iPos);
    //std::cout << filename << std::endl;

    //2.获取ID
    string name = filename.substr(4, filename.length() - 8);
    //std::cout << name << std::endl;

    return atoi(name.c_str());
}

