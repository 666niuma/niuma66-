#include "main.h"
pid_type_def Chassis_PID[4];
Chassis_Motor_t   Motor;


void MotorPID_Init(Chassis_Motor_t *pid_init);
int myabs(int a)
{ 		   
	int temp;
	if(a<0)  
	  temp=-a;  
	else 
	  temp=a;
	return temp;
}



void MotorPID_Init(Chassis_Motor_t *pid_init); //PID初始化函数

//PID参数
pid_type_def Chassis_PID[4];
float Motor_Kp[5] = {2.9,  2.9,  1.5,  2,   1};
float Motor_Ki[5] = {0.15, 0.15, 0.15, 0.1, 0.2};
float Motor_Kd[5] = {0.1,  0.1,  0.1, 0.1, 0.5};

// 速度解算函数

void Chassis_Init(Chassis_Motor_t *init)
{
	init->motor[0].motor_gpio.gpio = Motor1_Port;
    init->motor[1].motor_gpio.gpio = Motor2_Port;
    init->motor[2].motor_gpio.gpio = Motor3_Port;
    init->motor[3].motor_gpio.gpio = Motor4_Port;
    
    init->motor[0].motor_gpio.in1  = Motor1_IN1;
    init->motor[1].motor_gpio.in1  = Motor2_IN1;
    init->motor[2].motor_gpio.in1  = Motor3_IN1;
    init->motor[3].motor_gpio.in1  = Motor4_IN1;
    
    init->motor[0].motor_gpio.in2  = Motor1_IN1;
    init->motor[1].motor_gpio.in2  = Motor2_IN1;
    init->motor[2].motor_gpio.in2  = Motor3_IN1;
    init->motor[3].motor_gpio.in2  = Motor4_IN1;
}

void MotorPID_Init(Chassis_Motor_t *pid_init)
{
    int i;
    for(i = 0; i < 4; i++)
    {
        Chassis_PID[i].Kp = Motor_Kp[i];
        Chassis_PID[i].Kd = Motor_Kd[i];
        Chassis_PID[i].Ki = Motor_Ki[i];

        Chassis_PID[i].max_iout = 250;
        Chassis_PID[i].max_out = 300;

        Chassis_PID[i].mode = pid_init->pidMode;

        pid_init->motor[i].motor_set.speed_set = 0;
    }
}


void Chassis_Speed_Calc(Chassis_Motor_t *Chassis_speed_calc)
{
    //速度解算
    Chassis_speed_calc->motor[0].motor_set.speed_set = ((sqrt(2) / 2) * Chassis_speed_calc->chassisVx - (sqrt(2) / 2) * Chassis_speed_calc->chassisVy ) * K * 60;
    Chassis_speed_calc->motor[1].motor_set.speed_set = (((-1) * sqrt(2) / 2) * Chassis_speed_calc->chassisVx - (sqrt(2) / 2) * Chassis_speed_calc->chassisVy  ) * K * 60;
    Chassis_speed_calc->motor[2].motor_set.speed_set = (-(sqrt(2) / 2) *Chassis_speed_calc->chassisVx + (sqrt(2) / 2) * Chassis_speed_calc->chassisVy ) * K * 60;
    Chassis_speed_calc->motor[3].motor_set.speed_set = ((sqrt(2) / 2) * Chassis_speed_calc->chassisVx + (sqrt(2) / 2) * Chassis_speed_calc->chassisVy ) * K * 60;
    //速度限制
    for(int i = 0; i < 4; i++)
    {
        if(Chassis_speed_calc->motor[i].motor_set.speed_set > Chassis_speed_calc->maxRpm_fwd[i])
        {
            Chassis_speed_calc->motor[i].motor_set.speed_set = Chassis_speed_calc->maxRpm_fwd[i];
        }
        else if(Chassis_speed_calc->motor[i].motor_set.speed_set < -Chassis_speed_calc->maxRpm_bwd[i])
        {
            Chassis_speed_calc->motor[i].motor_set.speed_set = -Chassis_speed_calc->maxRpm_bwd[i];
        }
    }
}

void Chassis_Speed_TO_Duty(Chassis_Motor_t *ChassisMotor_speed_to_duty)
{
    //速度转化为电机转速
    for(int i = 0; i < 4; i++)
    {
        if(ChassisMotor_speed_to_duty->motor[i].motor_set.speed_set > 0)
        {
            ChassisMotor_speed_to_duty->motor[i].motor_set.duty_set = (ChassisMotor_speed_to_duty->motor[i].motor_set.speed_set / ChassisMotor_speed_to_duty->maxRpm_fwd[i]) * 1000;
        }
        else
        {
            ChassisMotor_speed_to_duty->motor[i].motor_set.duty_set = (ChassisMotor_speed_to_duty->motor[i].motor_set.speed_set / ChassisMotor_speed_to_duty->maxRpm_bwd[i]) * 1000;
        }
    }
}

void Motor_fdb(Chassis_Motor_t *Motor_fdb)
{
    //电机反馈
    for(int i = 0; i < 4; i++)
    {
        Motor_fdb->motor[i].motor_fdb.encoder_fdb = Chassis_ReadEncoder(i + 1);
        Motor_fdb->motor[i].motor_fdb.speed_fdb = (Motor_fdb->motor[i].motor_fdb.encoder_fdb * motor_time / 4) * 60 / (NPN * gear_ratio);
    }
    Motor_fdb->motor[0].motor_fdb.speed_fdb = -Motor_fdb->motor[0].motor_fdb.speed_fdb;

    for(int i = 0; i < 4; i++ )//将此刻的速度赋值给上次速度
    {
        Motor_fdb->motor[i].motor_fdb.last_Vx = Motor_fdb->motor[i].motor_fdb.speed_fdb;
    }
}


void Chassis_PIDCalc(Chassis_Motor_t *pid)
 {   
    int i;
    for(i = 0; i<4; i++)
    {
        if(pid->motor[i].motor_set.speed_set != pid->motor[i].motor_fdb.speed_fdb)
        {
            pid->motor[i].motor_set.pid_speed = PID_Calc(&Chassis_PID[i], pid->motor[i].motor_fdb.speed_fdb, pid->motor[i].motor_set.speed_set);
            
            
            if(myabs(pid->motor[i].motor_set.speed_set) < 0.1)
            {
                pid->motor[i].motor_set.pid_speed = 0;
            }
            float temp;
            
            if(pid->motor[i].motor_set.pid_speed > 0)
            {
                temp = pid->motor[i].motor_set.pid_speed / pid->maxRpm_fwd[i];
                pid->motor[i].motor_set.duty_set = (uint16_t)(temp * 1000);
            }
            else if(pid->motor[i].motor_set.pid_speed < 0)
            {
                temp = pid->motor[i].motor_set.pid_speed / pid->maxRpm_bwd[i];
                pid->motor[i].motor_set.duty_set = (uint16_t)(temp * 1000);
            }
            if(pid->motor[i].motor_set.duty_set > 100){pid->motor[i].motor_set.duty_set = 100;}
            
            //Motor Turn
            if(pid->motor[i].motor_set.pid_speed > 0)
            {
                HAL_GPIO_WritePin(pid->motor[i].motor_gpio.gpio, pid->motor[i].motor_gpio.in1,GPIO_PIN_SET);
                HAL_GPIO_WritePin(pid->motor[i].motor_gpio.gpio, pid->motor[i].motor_gpio.in2,GPIO_PIN_RESET);
            }
            else if(pid->motor[i].motor_set.pid_speed < 0)
            {
                HAL_GPIO_WritePin(pid->motor[i].motor_gpio.gpio, pid->motor[i].motor_gpio.in2,GPIO_PIN_SET);
                HAL_GPIO_WritePin(pid->motor[i].motor_gpio.gpio, pid->motor[i].motor_gpio.in1,GPIO_PIN_RESET);
            }
            else if(pid->motor[i].motor_set.pid_speed == 0)
            {
                 HAL_GPIO_WritePin(pid->motor[i].motor_gpio.gpio, pid->motor[i].motor_gpio.in1,GPIO_PIN_SET);
				 HAL_GPIO_WritePin(pid->motor[i].motor_gpio.gpio, pid->motor[i].motor_gpio.in2,GPIO_PIN_SET);
            }

        }
        else if((pid->motor[i].motor_set.pid_speed == 0) && (pid->motor[i].motor_fdb.speed_fdb == 0))
        {
            pid->motor[i].motor_set.pid_speed = 0;
        }
//        Motor_Control(ChassisMotor.motor.motor_set[0].current_set, ChassisMotor.motor.motor_set[1].current_set, 
//                    ChassisMotor.motor.motor_set[2].current_set, ChassisMotor.motor.motor_set[3].current_set);
    }
}

// void determine_direction(Chassis_Motor_t *Motor, int state)
// {
//     //direction of chassis
//     switch (state)//state 0: stop,init; 1: x; 2: y
//     {
//         case 0;
//             Motor->motor[0].motor_set.speed_set = 0;
//             Motor->motor[1].motor_set.speed_set = 0;
//             Motor->motor[2].motor_set.speed_set = 0;
//             Motor->motor[3].motor_set.speed_set = 0;
//             break;
//         case 1:
//             Motor->chassisVx = move.speed.Speed_Vx;
//             Motor->chassisVy = v0;
//             break;
//         case 2:
//             Motor->chassisVx = v0;
//             Motor->chassisVy = move.speed.Speed_Vy;
//             break;

//     }
// }

