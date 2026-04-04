#include<vector>
#include<string>
#include<map>
#include <ros/ros.h>
#include <std_msgs/Bool.h>

#include <ros/ros.h>
#include <std_srvs/Empty.h>

using namespace std;

typedef struct AsrCmd
{
    string key;//英文函数名
    string cmd;//命令
    string reply;//回复
};

void parse_config_file(string fileName,vector<AsrCmd> &asr_cmds);

int hex2dec(int hex);

string get_asr_reply(vector<AsrCmd> &asr_cmds,int asr_id);

void start_scan(ros::ServiceClient &start_scan_client);

void stop_scan(ros::ServiceClient &stop_scan_client);

void open_lidar(string key,ros::Publisher &enable_lidar_pub, ros::ServiceClient &stop_scan_client, ros::ServiceClient &start_scan_client);