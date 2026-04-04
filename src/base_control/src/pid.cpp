#include "pid.h"

void init_pid(PidController* pid,double Kp,double Ki,double Kd)
{
	pid->Kp = Kp; // 比例系数
    pid->Ki = Ki; // 积分系数
    pid->Kd = Kd; // 微分系数
    pid->integral = 0; // 误差累计
    pid->last_error = 0; // 上一次误差
    pid->output = 0; //输出值
}

//位置式PID控制电机调节步骤：https://zhuanlan.zhihu.com/p/373402745


// 计算PID控制器输出
double calculate_pid_output(PidController* pid,int target,int encoder,int max_pwm) 
{
    // 计算误差
    double error,error_rate;
    error = target - encoder;

    // 计算误差变化率
    error_rate = error - pid->last_error;

    // 计算误差累计
    pid->integral += error;
    if(pid->integral>1000)//积分限幅
        pid->integral = 1000;
    else if(pid->integral<-1000)
        pid->integral = -1000;

    
    // 计算PID输出 （位置式直接赋值）
    pid->output = pid->Kp * error + pid->Ki * pid->integral + pid->Kd * error_rate;

    // 保存上一次误差
    pid->last_error = error;

    //PWM限幅
    if(pid->output>max_pwm)
        pid->output=max_pwm;
    else if(pid->output<-max_pwm)
        pid->output=-max_pwm;

    return pid->output;
}