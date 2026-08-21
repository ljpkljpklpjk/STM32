#ifndef __ENCODER_H
#define __ENCODER_H
#include "sys.h"

/*
 * 旋钮编码器驱动 (两路)
 *
 * 硬件: 5V供电的EC11类编码器, 每路有 S1、S2、KEY 三个信号脚。
 *       S1/S2 为正交输出, KEY 为编码器自带按键(按下接地)。
 *
 * 引脚选择的依据: 编码器按5V供电时, 其上拉输出的高电平是5V,
 *       而 STM32F103 上带 ADC 功能的引脚(PA0~PA7、PB0、PB1)不是5V容忍的,
 *       直接接5V信号会损坏引脚。所以六个编码器信号全部安排在
 *       5V容忍(FT)引脚上: PB5/PB6/PB7 与 PB10/PB11/PA8。
 *       继电器、蜂鸣器这类"输出"不存在这个问题, 已换到非FT引脚上。
 *
 * 解码方式: 编码器1的S1原本可用外部中断, 但两路编码器难免落在同一条
 *       EXTI 线上(如 PA1 与 PB1 同属 EXTI1), 因此统一改为 1ms 定时器
 *       中断内轮询 + 状态机解码, 顺带完成按键消抖。
 *
 * 用法: TIM3 的 1ms 中断里调用 Encoder_Scan(), 主循环用
 *       Encoder_Get_Delta() / Encoder_Get_Key() 取增量和按键事件。
 */

//----------------- 编码器1(温度设定) 端口定义 -----------------
#define ENC1_S1_GPIO        GPIOB
#define ENC1_S1_PIN         GPIO_Pin_5
#define ENC1_S2_GPIO        GPIOB
#define ENC1_S2_PIN         GPIO_Pin_6
#define ENC1_KEY_GPIO       GPIOB
#define ENC1_KEY_PIN        GPIO_Pin_7

//----------------- 编码器2(湿度设定) 端口定义 -----------------
#define ENC2_S1_GPIO        GPIOB
#define ENC2_S1_PIN         GPIO_Pin_10
#define ENC2_S2_GPIO        GPIOB
#define ENC2_S2_PIN         GPIO_Pin_11
#define ENC2_KEY_GPIO       GPIOA
#define ENC2_KEY_PIN        GPIO_Pin_8

#define ENC_NUM             2       //编码器路数
#define ENC_TEMP            0       //编码器1: 调温度
#define ENC_HUMI            1       //编码器2: 调湿度

#define ENC_KEY_DEBOUNCE    20      //按键消抖时间, 单位ms

void Encoder_Init(void);
/* 在1ms定时中断中调用 */
void Encoder_Scan(void);
/* 取出并清零旋转增量, 顺时针为正, 逆时针为负 */
s16  Encoder_Get_Delta(u8 ch);
/* 取出并清除按键按下事件, 有按下返回1, 否则返回0 */
u8   Encoder_Get_Key(u8 ch);

#endif
