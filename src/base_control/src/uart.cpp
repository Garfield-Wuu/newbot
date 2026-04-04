#include <ros/ros.h>

#include "uart.h"

Uart::Uart()
{
}

Uart::~Uart()
{
    if(fd>0)
        close(fd);
}

int Uart::init(std::string &dev,int buad)
{
    fd = open(dev.c_str(), O_RDWR | O_NOCTTY);

    if(fd<=0)
    {
        ROS_WARN("open %s failed, not exist or need to chmod!",dev.c_str());
        return -1;
    }

    if(init_fd(fd, buad, 8, 'N', 1))
    {
        ROS_ERROR("uart init error!");
        return -1;
    }
}

int Uart::init_fd(int fd,int nSpeed, int nBits, char nEvent, int nStop)
{
    struct termios newtio,oldtio;
    if  ( tcgetattr( fd,&oldtio)  !=  0) {
        perror("SetupSerial 1");
        return -1;
    }
    bzero( &newtio, sizeof( newtio ) );
    newtio.c_cflag  |=  CLOCAL | CREAD;
    newtio.c_cflag &= ~CSIZE;

    switch( nBits )
    {
        case 7:
            newtio.c_cflag |= CS7;
            break;
        case 8:
            newtio.c_cflag |= CS8;
            break;
    }

    switch( nEvent )
    {
        case 'O':
            newtio.c_cflag |= PARENB;
            newtio.c_cflag |= PARODD;
            newtio.c_iflag |= (INPCK | ISTRIP);
            break;
        case 'E':
            newtio.c_iflag |= (INPCK | ISTRIP);
            newtio.c_cflag |= PARENB;
            newtio.c_cflag &= ~PARODD;
            break;
        case 'N':
            newtio.c_cflag &= ~PARENB;
            break;
    }

    switch( nSpeed )
    {
        case 2400:
            cfsetispeed(&newtio, B2400);
            cfsetospeed(&newtio, B2400);
            break;
        case 4800:
            cfsetispeed(&newtio, B4800);
            cfsetospeed(&newtio, B4800);
            break;
        case 9600:
            cfsetispeed(&newtio, B9600);
            cfsetospeed(&newtio, B9600);
            break;
        case 115200:
            cfsetispeed(&newtio, B115200);
            cfsetospeed(&newtio, B115200);
            break;
        case 460800:
            cfsetispeed(&newtio, B460800);
            cfsetospeed(&newtio, B460800);
            break;
        default:
            cfsetispeed(&newtio, B9600);
            cfsetospeed(&newtio, B9600);
            break;
    }

    if( nStop == 1 )
        newtio.c_cflag &=  ~CSTOPB;
    else if ( nStop == 2 )
        newtio.c_cflag |=  CSTOPB;

    
    newtio.c_cc[VTIME]  = 10;//等待时间，0表示永远等待，单位是十分之一秒，10就是1秒
    newtio.c_cc[VMIN] = 0;//最小接收字节个数
    tcflush(fd,TCIFLUSH);
    

    if((tcsetattr(fd,TCSANOW,&newtio))!=0)
    {
        perror("com set error");
        return -1;
    }

    printf("vtime=%d vmin=%d\n",newtio.c_cc[VTIME],newtio.c_cc[VMIN]);

    return 0;
}

int Uart::read_data_test()
{
    if(fd<=0)
    {
        //ROS_WARN("read_mcu_data fd error!");
        sleep(1);
        return -1;
    }

    bool recv_ok_flag = false;
    std::string recv_str;

    while(recv_ok_flag==false)
    {
        recv_cnt = read(fd, buffer, BUF_SIZE);//阻塞
        if(recv_cnt==0)//如果没有数据1秒会超时退出,此时recv_cnt=0
        {
            ROS_WARN("uart read no data!");
            return -2;
        }

        for(int i=0;i<recv_cnt;i++)//遍历收到的字符串
        {
            recv_str.append(&buffer[i],1);//逐个字节依次插入recv_str
            ch = buffer[i];
    

            if(lll_ch=='T' && ll_ch=='A' && l_ch=='\r' && ch=='\n')//必须>=3,否则可能会段错误
            {
                recv_ok_flag = true;
                break;
            }
            
            if(ch=='\n' && recv_str.size() >= 100)//长度过长，并且发现回车则跳出，可能出现MCU报错信息
            {
                recv_ok_flag = true;
                break;
            }
            
            lll_ch = ll_ch;
            ll_ch = l_ch;
            l_ch = ch;
        }
    }
}



int Uart::send_data(unsigned char *buf, int len)
{
    if(fd<=0)
    {
        //ROS_WARN("send_data fd error!");
        return -1;
    }

    int cnt = write(fd, buf, len);

    return cnt;
}

int Uart::read_mcu_data(std::string &recv_str)
{
    if(fd<=0)
    {
        //ROS_WARN("read_mcu_data fd error!");
        sleep(1);
        return -1;
    }

    bool recv_ok_flag = false;
    recv_str.clear();

    while(recv_ok_flag==false)
    {
        recv_cnt = read(fd, buffer, BUF_SIZE);//阻塞
        if(recv_cnt==0)//如果没有数据1秒会超时退出,此时recv_cnt=0
        {
            ROS_WARN("uart read no data!");
            return -2;
        }

        for(int i=0;i<recv_cnt;i++)//遍历收到的字符串
        {
            recv_str.append(&buffer[i],1);//逐个字节依次插入recv_str
            ch = buffer[i];
    

            if(lll_ch=='T' && ll_ch=='A' && l_ch=='\r' && ch=='\n')//必须>=3,否则可能会段错误
            {
                recv_ok_flag = true;
                break;
            }
            
            if(ch=='\n' && recv_str.size() >= 100)//长度过长，并且发现回车则跳出，可能出现MCU报错信息
            {
                recv_ok_flag = true;
                break;
            }
            
            lll_ch = ll_ch;
            ll_ch = l_ch;
            l_ch = ch;
        }
    }

    //校验长度
    if(recv_str.size()<8)//数据太短
    {
        ROS_WARN("recv_str len too short: %d, data: ",recv_str.size(),recv_str.c_str());
        return -3;
    }
    else//数据长度满足
    {
        if(recv_str.data()[0] != 'D' || recv_str.data()[1] != 'A')//和数据头不符
        {
            ROS_WARN("recv data head 0x%x 0x%x error! recv_str: %s",
                        recv_str.data()[0],recv_str.data()[1],recv_str.c_str());
            return -4;
        }


        if(recv_str.data()[2]!=recv_str.size())
        {
            ROS_WARN("struct len %d != read len %d error, recv_str: %s",recv_str.data()[2],recv_str.size(),recv_str.c_str());
            return -5;
        }
    }

    return 0;
}