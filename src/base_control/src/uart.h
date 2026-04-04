#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>

#define BUF_SIZE 1024

class Uart
{
public:
    Uart();
    ~Uart();

    int init(std::string &dev,int buad);
    int read_data_test();
    int send_data(unsigned char *buf, int len);
    int read_mcu_data(std::string &recv_str);

    int fd;

private:
    int init_fd(int fd,int nSpeed, int nBits, char nEvent, int nStop);

    

    char buffer[BUF_SIZE];
    int recv_cnt;
    char ch=0,l_ch=0,ll_ch=0,lll_ch=0;
};