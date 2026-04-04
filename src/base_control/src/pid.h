
typedef struct
{
	double Kp; // 比例系数
    double Ki; // 积分系数
    double Kd; // 微分系数
    double integral; // 误差累计
    double last_error; // 上一次误差
	double output; //输出值
}PidController;

void init_pid(PidController* pid,double Kp,double Ki,double Kd);
double calculate_pid_output(PidController* pid,int target,int encoder,int max_pwm);