#include "base_control.h"


//开启节点会自动调用一次读取动态参数
void BaseControl::dynamic_reconfigure_callback(base_control::BaseConfig &config, uint32_t level)
{
    setting_data.wheel_circumference_mm=config.wheel_circumference_mm;
    setting_data.pulses_per_wheel_turn_around=config.pulses_per_wheel_turn_around;
    setting_data.pulses_per_robot_turn_around=config.pulses_per_robot_turn_around;

    setting_data.max_pwm=config.max_pwm;

    setting_data.pid_p=config.pid_p;
    setting_data.pid_i=config.pid_i;
    setting_data.pid_d=config.pid_d;

    ROS_INFO("dynamic_reconfigure_callback:\n"

             "wheel_circumference_mm= %d\n"
             "pulses_per_wheel_turn_around= %d\n"
             "pulses_per_robot_turn_around= %d\n"

             "max_pwm= %d\n"

             "pid_p= %d\n"
             "pid_i= %d\n"
             "pid_d= %d",
             setting_data.wheel_circumference_mm, // //轮子的周长
             setting_data.pulses_per_wheel_turn_around, // //轮子转一圈的脉冲数 个
             setting_data.pulses_per_robot_turn_around,  //小车转一圈的脉冲数 个
             setting_data.max_pwm, //pwm最大值,用于PID限幅
             setting_data.pid_p,//P参数 
             setting_data.pid_i,//I参数 
             setting_data.pid_d//D参数
             );


    init_pid(&left_pid, setting_data.pid_p,setting_data.pid_i,setting_data.pid_d);
    init_pid(&right_pid, setting_data.pid_p,setting_data.pid_i,setting_data.pid_d);
    pluses_m = setting_data.pulses_per_wheel_turn_around / ((double)setting_data.wheel_circumference_mm/1000.0) ; // 轮子转一圈的脉冲数 / 轮子周长m  = x pluses/m
    wheel_distance_m =  setting_data.pulses_per_robot_turn_around / pluses_m  / M_PI;//转向360度的脉冲数 / pluses/m  / PI = 转向360度的圆圈周长m / PI = 轮子距离m

    ROS_INFO("so pluses_m = %.2f pluses/m wheel_distance_m = %.2f m\n",pluses_m,wheel_distance_m);

}


//速度指令订阅回调函数
void BaseControl::cmd_vel_callback(const geometry_msgs::Twist::ConstPtr& msg)
{
    target_m_s = msg->linear.x;//目标线速度m/s
    target_rad_s = msg->angular.z;//目标角速度rad/s
}

void BaseControl::enable_lidar_callback(const std_msgs::Bool::ConstPtr& msg)
{
  if (msg->data) 
  {
    enable_lidar = 1;
    ROS_INFO("enable_lidar true");
  }
  else
  {
    enable_lidar = 0;
    ROS_INFO("enable_lidar false");
  }
}

void BaseControl::reset_callback(const std_msgs::Bool::ConstPtr& msg)
{
  if (msg->data) //收到true则将轮速计定位清0
  {
    pose_x=0;
    pose_y=0;
    pose_yaw=0;
    all_encoder1=0;
    all_encoder2=0;
    ROS_INFO("pos reset true");
  }
  else
  {
    ROS_INFO("pos reset false");
  }
}



BaseControl::~BaseControl()
{

}

BaseControl::BaseControl() : nh("~")
{
    string sub_cmd_vel_topic,pub_odom_topic;
    string dev;
    int buad;


    nh.param<string>("sub_cmd_vel_topic", sub_cmd_vel_topic, "/cmd_vel");
    nh.param<string>("pub_odom_topic", pub_odom_topic, "/odom");
    
    nh.param<string>("odom_frame_id", odom_frame_id, "odom");
    nh.param<string>("base_frame_id", base_frame_id, "base_footprint");

    nh.param<string>("dev", dev, "/dev/ttyS3");
    nh.param<int>("buad", buad, 115200);
    nh.param<bool>("ignore_serial_asr_id", ignore_serial_asr_id_, false);
    if (ignore_serial_asr_id_)
        ROS_WARN("ignore_serial_asr_id=true: 串口 MCU 上报的 asr_id 将不会发布到 /asr_id（供 USB 语音模拟离线模块时使用）");


    //订阅主题command
    command_sub = nh.subscribe(sub_cmd_vel_topic, 10, &BaseControl::cmd_vel_callback, this);//速度指令订阅
    enable_lidar_sub = nh.subscribe("/enable_lidar", 10, &BaseControl::enable_lidar_callback, this);//打开或关闭雷达订阅
    reset_sub = nh.subscribe("/reset", 10, &BaseControl::reset_callback, this);

    odom_pub= nh.advertise<nav_msgs::Odometry>(pub_odom_topic, 10); //odom发布
    battery_pub= nh.advertise<sensor_msgs::BatteryState>("/battery", 10);//电池信息发布

    joint_pub = nh.advertise<sensor_msgs::JointState>("/joint_states", 1);//关节状态发布,发布轮子转角

    //imu_pub = nh.advertise<sensor_msgs::Imu>("/imu", 10); //IMU发布
    //mag_pub = nh.advertise<sensor_msgs::MagneticField>("/mag", 10);//磁力计发布


    plot_pub = nh.advertise<std_msgs::Float32MultiArray>("/plot", 10);//目标和实际速度散点图发布，用于PID调试
    tts_pub = nh.advertise<std_msgs::String>("/tts",10);//语音命令字符串发布

    asr_id_pub = nh.advertise<std_msgs::Int32>("/asr_id", 10);
    robot_state_pub = nh.advertise<std_msgs::Int16MultiArray>("/robot_state", 10);

    dynamic_config = boost::bind(&BaseControl::dynamic_reconfigure_callback,this, _1, _2);//动态参数回调
    server.setCallback(dynamic_config);

    //串口初始化
    int ret = uart.init(dev,buad);
    if(ret<0)
        return;


    uart.read_data_test();//把之前缓存的数据读取出来
    uart.read_data_test();//把之前缓存的数据读取出来

    ROS_INFO("device: %s buad: %d open success",dev.c_str(),buad);

}

//发布电池信息
void BaseControl::pub_battery_msg()
{
    battery_msg.header = odom_msg.header;
    battery_msg.voltage = mcu_data.vbat_mv/1000.0;          // Voltage in Volts (Mandatory)
    battery_msg.current=1;          // Negative when discharging (A)  (If unmeasured NaN)
    battery_msg.charge=2;           // Current charge in Ah  (If unmeasured NaN)
    battery_msg.capacity=4;         // Capacity in Ah (last full capacity)  (If unmeasured NaN)
    battery_msg.design_capacity=4;  // Capacity in Ah (design capacity)  (If unmeasured NaN)
    battery_msg.percentage=round((battery_msg.voltage-3)/(4.2-3)*5)/5.0;       // Charge percentage on 0 to 1 range  (If unmeasured NaN)
    if(battery_msg.percentage>1)
        battery_msg.percentage=1;
    else if(battery_msg.percentage<0)
        battery_msg.percentage=0;

    if(mcu_data.charger_connected==1 && mcu_data.fully_charged==0)
        battery_msg.power_supply_status=POWER_SUPPLY_STATUS_CHARGING;
    else if(mcu_data.charger_connected==1 && mcu_data.fully_charged==1)
        battery_msg.power_supply_status=POWER_SUPPLY_STATUS_FULL;
    else
        battery_msg.power_supply_status=POWER_SUPPLY_STATUS_DISCHARGING;     // The charging status as reported. Values defined above

    battery_msg.power_supply_health=POWER_SUPPLY_HEALTH_GOOD;     // The battery health metric. Values defined above
    battery_msg.power_supply_technology=POWER_SUPPLY_TECHNOLOGY_LIPO; // The battery chemistry. Values defined above
    battery_msg.present=true;          // True if the battery is present

    //发布电池信息
    battery_pub.publish(battery_msg);



    int charge_state = 0;
    if(mcu_data.charger_connected==1 && mcu_data.fully_charged==0)
        charge_state = 1;
    else if(mcu_data.charger_connected==1 && mcu_data.fully_charged==1)
        charge_state = 2;
    else
        charge_state = 0;

    if(charge_state!=last_charge_state && charge_state==0)
    {
        std_msgs::String msg;
        msg.data = string("充电器已断开");
        tts_pub.publish(msg);
    }
    else if(charge_state!=last_charge_state && charge_state==1)
    {
        std_msgs::String msg;
        msg.data = string("充电器已连接");
        tts_pub.publish(msg);
    }
    else if(charge_state!=last_charge_state && charge_state==2)
    {
        std_msgs::String msg;
        msg.data = string("电池已充满");
        tts_pub.publish(msg);
    }
    last_charge_state = charge_state;

    if(battery_msg.voltage < 3.5 && battery_msg.voltage > 0.0 && charge_state==0)
    {
        low_power_cnt++;
        if(low_power_cnt>=180)//180s报告一次低电量
        {
            std_msgs::String msg;
            char tts_str[64];
            sprintf(tts_str,"电池电压已降低到%.1f伏，请及时充电！",battery_msg.voltage);
            msg.data = string(tts_str);
            tts_pub.publish(msg);
            
            low_power_cnt = 0;
        }
    }
}

//发布关节信息
void BaseControl::pub_joint_msg(double l_angle,double r_angle)
{
    // 创建并填充消息
    sensor_msgs::JointState joint_state;
    joint_state.header = odom_msg.header;
    joint_state.name.resize(2);
    joint_state.position.resize(2);

    joint_state.name[0] ="l_wheel_joint";
    joint_state.position[0] = l_angle;

    joint_state.name[1] ="r_wheel_joint";
    joint_state.position[1] = r_angle;

    // 发布消息
    joint_pub.publish(joint_state);
}

void BaseControl::control_robot(int target1,int target2)
{
    //调用PID控制器，由编码器target得到pwm
    if(target1==0)//输出pwm=0可以制动
        cmd_data.pwm1 = 0;
    else if(last_target1<=0 && target1>0)//刚前进
        init_pid(&left_pid, setting_data.pid_p,setting_data.pid_i,setting_data.pid_d);
    else if(last_target1>=0 && target1<0)//刚后退
        init_pid(&left_pid, setting_data.pid_p,setting_data.pid_i,setting_data.pid_d);
    else//正常情况
        cmd_data.pwm1 = calculate_pid_output(&left_pid,target1,mcu_data.encoder1,setting_data.max_pwm);
    last_target1 = target1;
    
    if(target2==0)//输出pwm=0可以制动
        cmd_data.pwm2 = 0;
    else if(last_target2<=0 && target2>0)//刚前进
        init_pid(&right_pid, setting_data.pid_p,setting_data.pid_i,setting_data.pid_d);
    else if(last_target2>=0 && target2<0)//刚后退
        init_pid(&right_pid, setting_data.pid_p,setting_data.pid_i,setting_data.pid_d);
    else//正常情况
        cmd_data.pwm2 = calculate_pid_output(&right_pid,target2,mcu_data.encoder2,setting_data.max_pwm);
    last_target2 = target2;

    //发送串口数据
    cmd_data.enable_power = enable_lidar;
    uart.send_data((unsigned char *)&cmd_data,sizeof(CmdData));//串口发送
}

void BaseControl::pub_tf_and_odom(ros::Time ros_time_now,double delta_m_s,double delta_rad_s)
{
    //里程计的偏航角需要转换成四元数才能发布
    odom_quat = tf::createQuaternionMsgFromYaw(pose_yaw);

    //TF时间戳
    odom_trans.header.stamp = ros_time_now;
    //发布坐标变换的父子坐标系
    odom_trans.header.frame_id = odom_frame_id;
    odom_trans.child_frame_id = base_frame_id;
    //tf位置数据：x,y,z,方向
    odom_trans.transform.translation.x = pose_x;
    odom_trans.transform.translation.y = pose_y;
    odom_trans.transform.translation.z = 0.0;
    odom_trans.transform.rotation = odom_quat;


    //里程计时间戳
    odom_msg.header.stamp = ros_time_now;
    //里程计的父子坐标系
    odom_msg.header.frame_id = odom_frame_id;
    odom_msg.child_frame_id = base_frame_id;
    //里程计位置数据：x,y,z,方向
    odom_msg.pose.pose.position.x = pose_x;
    odom_msg.pose.pose.position.y = pose_y;
    odom_msg.pose.pose.position.z = 0.0;
    odom_msg.pose.pose.orientation = odom_quat;
    //线速度和角速度
    odom_msg.twist.twist.linear.x = delta_m_s;
    odom_msg.twist.twist.linear.y = 0.0;
    odom_msg.twist.twist.angular.z = delta_rad_s;


    //发布tf坐标变换
    odom_broadcaster.sendTransform(odom_trans);
        
    //发布里程计
    odom_pub.publish(odom_msg);
}

void BaseControl::pub_plot(vector<float> array)
{
    //发布可视化数据
    std_msgs::Float32MultiArray array_msg;//注意:rqt_plot里要输入/plot/data[0]而不是/plot，否则无法可视化!!!
    array_msg.data = array;
    plot_pub.publish(array_msg);
}



void BaseControl::run()
{
    int ret;

    while(ros::ok())
    {
        ros::spinOnce();//处理回调函数，如果没有这个，按下ctrl c不会立即停止

        if(uart.fd<=0)//x86系统没有这个串口设备
        {
            usleep(20*1000);//20ms
            memset(&mcu_data,0,sizeof(McuData));
            recv_str.assign((char *)&mcu_data,sizeof(McuData));//x86平台模拟假的传感器数据
        }
        else
        {
            ret = uart.read_mcu_data(recv_str);
            if(ret < 0)//报错，则重新读取
            {
                continue;
            }
        }

        //校验数据长度
        if(recv_str.size()!=sizeof(McuData))
        {
            ROS_WARN("recv_str len= %d, != %d !!! recv_str: %s",recv_str.size(),sizeof(McuData),recv_str.c_str());
            continue;
        }

        memcpy(&mcu_data,recv_str.c_str(),sizeof(McuData));

        //获取系统当前时间
        ros::Time ros_time_now = ros::Time::now();
        
        //计算周期dt
        current_time = ros_time_now.toSec();//返回小数的秒
        if(previous_time==0)//第一次无法计算dt,无法计算速度
        {
            previous_time = current_time;
            continue;
        }
        dt = current_time - previous_time;
        previous_time = current_time;
        
        if(pluses_m==0 || wheel_distance_m==0)//这两个参数作为分母不能为0
        {
            ROS_WARN("pluses_m or wheel_distance_m value error!");
            continue;
        }

        //v = (vr+vl)*0.5
        //w = (vr-vl)/ l(轮距)
        //编码器值转换为当前周期的距离和角度，除以时间就可以得到当前周期的速度
        double delta_m   =  (double)(mcu_data.encoder2 + mcu_data.encoder1) * 0.5             / pluses_m;// 当前周期的脉冲数均值 / pluses/m = 距离m
        double delta_rad =  (double)(mcu_data.encoder2 - mcu_data.encoder1)/wheel_distance_m  / pluses_m;// 当前周期的脉冲数差值 / pluses/m / 轮距 = 角度rad

        //vl = v - w*l(轮距)*0.5
        //vr = v + w*l(轮距)*0.5
        //速度转换为编码器目标值
        int target1 = (target_m_s - target_rad_s * wheel_distance_m *0.5) * dt * pluses_m;//左轮速度m/s * dt * pluses/m = 左轮距离m * pluses/m = 左轮脉冲数
        int target2 = (target_m_s + target_rad_s * wheel_distance_m *0.5) * dt * pluses_m;//右轮速度m/s * dt * pluses/m = 右轮距离m * pluses/m = 右轮脉冲数

        //发串口命令控制机器人运动
        control_robot(target1,target2);

        //角度累计
        pose_yaw +=  delta_rad;
        //保持角度范围在0~2*PI
        if(pose_yaw>2*M_PI)
            pose_yaw -= 2*M_PI;
        else if(pose_yaw<0)
            pose_yaw += 2*M_PI;

        //位置累计
        pose_x = pose_x  +  delta_m * cos(pose_yaw);
        pose_y = pose_y  +  delta_m * sin(pose_yaw);

        //发布TF变换和里程计信息(位置和速度信息)
        pub_tf_and_odom(ros_time_now, delta_m/dt, delta_rad/dt);

        //发布轮子角度用于可视化
        //l_angle += 2.0 * M_PI * mcu_data.encoder1 / setting_data.pulses_per_wheel_turn_around;
        //r_angle += 2.0 * M_PI * mcu_data.encoder2 / setting_data.pulses_per_wheel_turn_around;
        //pub_joint_msg(l_angle, r_angle);

        //发布散点图可视化
        // vector<float> array={target1,target2,mcu_data.encoder1,mcu_data.encoder2};
        // pub_plot(array);

        all_encoder1 += mcu_data.encoder1;
        all_encoder2 += mcu_data.encoder2;

        //处理语音命令和发布TTS，0表示没有命令（可由 ignore_serial_asr_id 关闭，避免与 usb_voice_cmd 双源）
        if(!ignore_serial_asr_id_ && mcu_data.asr_id != 0)//有asr命令
        {
            ROS_INFO(">>>>>> asr_id: %d",mcu_data.asr_id);
            std_msgs::Int32 msg;
            msg.data = mcu_data.asr_id;
            asr_id_pub.publish(msg);
        }
        
        if(cnt%50==0)//每50个循环发布一次，因为要求的频率不高 20ms*50=1000ms=1s
        {
            //发布电池信息
            pub_battery_msg();

            //发布屏幕显示的状态信息
            std_msgs::Int16MultiArray array_msg;
            vector<short> state_array={
                mcu_data.encoder1,
                mcu_data.encoder2,
                mcu_data.vbat_mv,
                mcu_data.charger_connected,
                mcu_data.fully_charged,

                cmd_data.pwm1,
                cmd_data.pwm2,
                cmd_data.enable_power
            };
            array_msg.data = state_array;
            robot_state_pub.publish(array_msg);
        }

        cnt++;

        ROS_INFO_THROTTLE(3,//3s间隔打印
            "en_cur=%d %d "
            "en_tar=%d %d "
            "pwm=%d %d "
            "bat=%.1fV ch=%d %d "
            "| pose(%.2f,%.2f) yaw=%.1f dt=%.2fms all_en=%d %d",

            mcu_data.encoder1,
            mcu_data.encoder2,
            target1,
            target2,
            cmd_data.pwm1,
            cmd_data.pwm2,

            mcu_data.vbat_mv/1000.0,
            mcu_data.charger_connected,
            mcu_data.fully_charged,

            pose_x,pose_y,
            pose_yaw*180/M_PI,
            dt*1000,
            
            all_encoder1,
            all_encoder2
            );
    }
}

int main(int argc,char** argv)
{
    setlocale(LC_CTYPE, "zh_CN.utf8");//防止ROS_INFO中文乱码

    ros::init(argc, argv, "base_control");

    BaseControl base_control;
    base_control.run();

    ROS_INFO("base_control exit");
    ros::shutdown();

    return 0;
}
