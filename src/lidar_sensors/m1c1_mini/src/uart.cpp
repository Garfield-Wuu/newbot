#include <ros/ros.h>

#include "uart.h"

Uart::Uart()
{
}

Uart::~Uart()
{
    if(fd>0)
    {
        //停止程序的时候发送停止命令A5 F5，并且关闭串口
        unsigned char stop_cmd[2] = {0xA5,0xF5};
        send_data(stop_cmd,2);

        close(fd);
    }
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

int Uart::read_data_repeat(char *buf, int need_read_len)
{
	if(fd <= 0)
		return -1;
	
	int read_cnt=0;
	for(int c=0;c<100;c++)//最多尝试100次
	{
		int len = read(fd, buf+read_cnt, need_read_len-read_cnt);
		if(len<=0)
			return -2;

		read_cnt += len;//累计数据长度

		if(read_cnt>=need_read_len)//读取的累计长度满足需要的总长度，则跳出
			return read_cnt;
	}

	return -3;
}

int Uart::read_lidar_data(std::string &recv_str)
{
    if(fd<=0)
    {
        printf("uart fd error!\n");
        sleep(1);
        return -2;
    }

    recv_str.clear();
	unsigned char l_ch=0;

    for(int c=0;c<100;c++)//最多尝试100次寻找AA 55开头
    {
        int recv_cnt = read(fd, buffer, 1);//阻塞
        if(recv_cnt!=1)//如果没有数据1秒会超时退出,此时recv_cnt=0
        {
            //printf("uart read no data! recv_cnt = %d\n",recv_cnt);
            return -1;
        }

		recv_str.append(buffer,1);//逐个字节依次插入recv_str
		unsigned char ch = buffer[0];
		
		if(l_ch==0xAA && ch==0x55)
		{
			break;
		}

        l_ch = ch;
		
		if(recv_str.size() >= 100)//长度过长则返回
		{
			printf("recv_str100: ");
			for(int i=0;i<recv_str.size();i++)
				printf("%02x ",(unsigned char)recv_str.data()[i]);
			printf("\n");
			return -3;
		}
    }

	if(recv_str.size()>2)
		recv_str = recv_str.substr(recv_str.size()-2);//只保留末尾两个字母AA 55作为数据头


	//再读取两个字节 点云类型 和 点云点数(1~25)
	int need_read_len = 2;
	int read_len = read_data_repeat(buffer, need_read_len);
	if(read_len<need_read_len)
		return -1;

	recv_str.append(buffer,read_len);

	if(recv_str.data()[2]!=0 && recv_str.data()[2]!=1)//点云类型只能是0或1
		return -4;

	if(recv_str.data()[3]>25 || recv_str.data()[3]<1)//点云点数只会在1~25之间
	{
		for(int i=0;i<recv_str.size();i++)
			printf("%02x ",(unsigned char)recv_str.data()[i]);
		printf("\n");
		return -5;
	}

	//再读取其他数据头和数据
	need_read_len = 10+recv_str.data()[3]*2 - recv_str.size();
	if(need_read_len>100 || need_read_len<5)//计算出的长度过长或过短，则返回错误
		return -6;

	read_len = read_data_repeat(buffer, need_read_len);
	if(read_len<need_read_len)
	{
		//printf("read_len=%d < need_read_len=%d !\n",read_len,need_read_len);
		return -1;
	}
		
	recv_str.append(buffer,read_len);

	if(recv_str.size()!=10+recv_str.data()[3]*2)//如果长度不正确
	{
		printf("read len %d != points num %d *2+10 error!\n",recv_str.size(),recv_str.data()[3]);
		for(int i=0;i<recv_str.size();i++)
			printf("%02x ",(unsigned char)recv_str.data()[i]);
		printf("\n");
		return -7;
	}

    return 0;
}

