#ifndef __ACTUATOR_H
#define __ACTUATOR_H
#include "sys.h"

/*
 * 执行机构驱动: 除湿风扇、冷风风扇、加湿器、加热器、制冷片、蜂鸣器
 *
 * 除湿风扇/加热器/制冷片经继电器模块驱动, 加湿器由单片机高低电平直接控制,
 * 蜂鸣器为有源蜂鸣器, 高电平响。
 *
 * 冷风风扇(装在制冷片旁边, 把冷气吹向四周的那个)用 PWM 调速: 只输出一路
 * PWM 信号, 风扇自己独立接电源, 不经过继电器, 也不从单片机取电流。
 * 这一路选在 PB9(TIM4_CH4): 既是硬件定时器通道(波形由硬件产生, 不受中断
 * 影响), 又是5V容忍引脚, 4线PWM风扇的调速线内部上拉到5V也不会损坏引脚。
 *
 * 继电器模块有高电平吸合和低电平吸合两种, 用 RELAY_ACTIVE_LOW 统一处理:
 * 模块是高电平吸合就把宏置 0, 低电平吸合就置 1, 不用动其它代码。
 *
 * 引脚说明: 这几路都是"输出", 不存在被外部5V灌入的问题,
 *          所以特意安排在非5V容忍的 PA1~PA3 上, 把宝贵的5V容忍(FT)引脚
 *          让给需要接收5V电平的旋钮编码器。
 */

#define RELAY_ACTIVE_LOW    0       //1:继电器低电平吸合  0:高电平吸合

//----------------- 端口定义 -----------------
#define FAN_GPIO            GPIOA
#define FAN_PIN             GPIO_Pin_1      //除湿风扇继电器 PA1
#define HEAT_GPIO           GPIOA
#define HEAT_PIN            GPIO_Pin_2      //加热器继电器 PA2
#define COOL_GPIO           GPIOA
#define COOL_PIN            GPIO_Pin_3      //制冷片继电器 PA3

#define HUMI_GPIO           GPIOA
#define HUMI_PIN            GPIO_Pin_12     //加湿器      PA12, 高电平开
#define BEEP_GPIO           GPIOA
#define BEEP_PIN            GPIO_Pin_11     //蜂鸣器      PA11, 高电平响

//----------------- 冷风风扇 PWM 端口定义 -----------------
#define COLDFAN_GPIO        GPIOB
#define COLDFAN_PIN         GPIO_Pin_9      //冷风风扇PWM PB9
#define COLDFAN_TIM         TIM4
#define COLDFAN_PWM_PERIOD  2879            //自动重装值: 72MHz/(2879+1)=25kHz

void Actuator_Init(void);

void Fan_Set(u8 on);            //除湿风扇: 1开 0关(继电器, 只有满速)
void ColdFan_SetDuty(u8 duty);  //冷风风扇: 0~100, 单位%的PWM占空比, 0停转
void Heater_Set(u8 on);         //加热器: 1开 0关
void Cooler_Set(u8 on);         //制冷片: 1开 0关
void Humidifier_Set(u8 on);     //加湿器: 1开 0关
void Beep_Set(u8 on);           //蜂鸣器: 1响 0停

/* 全部执行机构断开, 用于初始化和异常保护 */
void Actuator_AllOff(void);

#endif
