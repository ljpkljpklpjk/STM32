#ifndef __ACTUATOR_H
#define __ACTUATOR_H
#include "sys.h"

/*
 * 执行机构驱动: 风扇、加湿器、加热器、制冷片、蜂鸣器
 *
 * 风扇/加热器/制冷片经继电器模块驱动, 加湿器由单片机高低电平直接控制,
 * 蜂鸣器为有源蜂鸣器, 高电平响。
 *
 * 继电器模块多为低电平触发(常见的光耦隔离模块), 这里用 RELAY_ACTIVE_LOW
 * 统一处理; 若手上的模块是高电平触发, 把它改成 0 即可, 不用动其它代码。
 *
 * 引脚说明: 这几路都是"输出", 不存在被外部5V灌入的问题,
 *          所以特意安排在非5V容忍的 PA1~PA3 上, 把宝贵的5V容忍(FT)引脚
 *          让给需要接收5V电平的旋钮编码器。
 */

#define RELAY_ACTIVE_LOW    0       //1:继电器低电平吸合  0:高电平吸合

//----------------- 端口定义 -----------------
#define FAN_GPIO            GPIOA
#define FAN_PIN             GPIO_Pin_1      //风扇继电器   PA1
#define HEAT_GPIO           GPIOA
#define HEAT_PIN            GPIO_Pin_2      //加热器继电器 PA2
#define COOL_GPIO           GPIOA
#define COOL_PIN            GPIO_Pin_3      //制冷片继电器 PA3

#define HUMI_GPIO           GPIOA
#define HUMI_PIN            GPIO_Pin_12     //加湿器      PA12, 高电平开
#define BEEP_GPIO           GPIOA
#define BEEP_PIN            GPIO_Pin_11     //蜂鸣器      PA11, 高电平响

void Actuator_Init(void);

void Fan_Set(u8 on);            //风扇:   1开 0关
void Heater_Set(u8 on);         //加热器: 1开 0关
void Cooler_Set(u8 on);         //制冷片: 1开 0关
void Humidifier_Set(u8 on);     //加湿器: 1开 0关
void Beep_Set(u8 on);           //蜂鸣器: 1响 0停

/* 全部执行机构断开, 用于初始化和异常保护 */
void Actuator_AllOff(void);

#endif
