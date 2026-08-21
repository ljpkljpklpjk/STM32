#ifndef _TIMER_H
#define _TIMER_H
#include "sys.h"

/*
 * TIM3 作为系统 1ms 节拍:
 *   - 维护全局毫秒计数 g_tick, 供各处做超时/周期判断;
 *   - 在中断里扫描两路旋钮编码器(正交解码 + 按键消抖)。
 *
 * 之所以用轮询而不是外部中断: 编码器1的S1在PA1、编码器2的KEY在PB1,
 * 两者同属 EXTI1 中断线, 无法同时挂外部中断。
 */

void TIM3_Int_Init(u16 arr, u16 psc);
/* 取系统运行的毫秒数 */
u32  Tick_Get(void);
/* 计算从 start 到现在经过的毫秒数(可正确处理溢出) */
u32  Tick_Elapsed(u32 start);

#endif
