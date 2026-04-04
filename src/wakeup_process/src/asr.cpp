#include "asr.h"

#include <iostream>
#include <fstream>
#include <map>
#include <string>

using namespace std;

void parse_config_file(string fileName,vector<AsrCmd> &asr_cmds) 
{
    ifstream fin(fileName);
    if (!fin.is_open()) {
        cerr << "Failed to open file: " << fileName << endl;
        return;
    }
    
    //cfg文件根据命令所在的行数进行和ID匹配，所以命令所在行数要和离线语音芯片发送的串口ID一一匹配
    string line;
    while (getline(fin, line)) 
    {
        AsrCmd asr_cmd;
        asr_cmd.key = line.substr(0, line.find("="));
        asr_cmd.cmd = line.substr(line.find("=") + 1, line.find("@") - line.find("=") - 1);
        asr_cmd.reply = line.substr(line.find("@") + 1);

        asr_cmds.push_back(asr_cmd);
    }

    fin.close();
}

int hex2dec(int hex)
{
    char hex_str[10];
    sprintf(hex_str,"%02x",hex);
    int num1 = hex_str[0] - '0';
    int num2 = hex_str[1] - '0';
    return num1 * 10 + num2;
}

string get_asr_reply(vector<AsrCmd> &asr_cmds,int asr_id)
{
    int id_index = hex2dec(asr_id)-1;//asr_id是从1开始记的

    if(id_index >= asr_cmds.size())
        return "ASR ID "+to_string(asr_id)+" 错误";
    
    return asr_cmds[id_index].reply + "#" + asr_cmds[id_index].key;
}

void start_scan(ros::ServiceClient &start_scan_client)
{
    // 使用 std::getenv 获取环境变量的值  
    char* lidar_type = std::getenv("LIDAR_TYPE");
    if(lidar_type == NULL || std::string(lidar_type)=="YDLIDAR")
    {
        // YDLIDAR开启雷达扫描
        std_srvs::Empty start_request;
        if(!start_scan_client.call(start_request))
            ROS_WARN("YDLIDAR failed to call start_scan service");
    }
}

void stop_scan(ros::ServiceClient &stop_scan_client)
{
    // 使用 std::getenv 获取环境变量的值  
    char* lidar_type = std::getenv("LIDAR_TYPE");
    if(lidar_type == NULL || std::string(lidar_type)=="YDLIDAR")
    {
        // YDLIDAR停止雷达扫描
        std_srvs::Empty stop_request;
        if (!stop_scan_client.call(stop_request))
            ROS_WARN("YDLIDAR failed to call stop_scan service");
    }
}


void open_lidar(string key,ros::Publisher &enable_lidar_pub, ros::ServiceClient &stop_scan_client, ros::ServiceClient &start_scan_client)
{
    stop_scan(stop_scan_client);//YDLIDAR切换雷达速度的时候必须调用stop scan服务，不然雷达的扫描会缺失
    if(key=="high_lidar") //对于M1C1_MINI来说无法调速，所以都一样
        system("gpio mode 20 out && gpio write 20 1");
    else if(key=="medium_lidar")
        system("gpio mode 20 in");
    else
        system("gpio mode 20 out && gpio write 20 0");

    std_msgs::Bool msg;
    msg.data = true;//打开MOS管
    enable_lidar_pub.publish(msg);
    
    start_scan(start_scan_client);
}



