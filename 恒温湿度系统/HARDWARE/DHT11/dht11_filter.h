#ifndef __DHT11_FILTER_H
#define __DHT11_FILTER_H
#include "sys.h"

/*
 * DHT11 数据滤波
 *
 * DHT11 本身分辨率只有 1℃/1%RH, 且偶尔会出现跳变的坏点,
 * 这里采用两级滤波提高数据的稳定性:
 *   1. 中值滤波: 连续采样 DHT11_MED_N 次取中位数, 去掉突发的跳变点;
 *   2. 一阶滞后(EMA)滤波: 对中值结果做平滑, 抑制小幅抖动。
 *
 * 为了保留一位小数的平滑效果, 内部用放大 10 倍的定点数保存,
 * 输出时提供整数和 0.1 精度两种接口。
 */

#define DHT11_MED_N       5     //中值滤波采样点数(奇数)
#define DHT11_EMA_ALPHA   30    //一阶滞后系数, 取值0~100, 越小越平滑

typedef struct
{
    s16 temp_x10;               //滤波后温度, 放大10倍
    s16 humi_x10;               //滤波后湿度, 放大10倍
    u8  valid;                  //0:尚未取得有效数据 1:数据有效
    u8  err_cnt;                //连续读取失败次数
} DHT11_Filter_t;

void DHT11_Filter_Init(void);
/* 采集并滤波一次, 返回0成功 1失败 */
u8   DHT11_Filter_Update(void);
/* 取滤波后的值(整数) */
u8   DHT11_Get_Temp(void);
u8   DHT11_Get_Humi(void);
/* 取滤波后的值(放大10倍, 便于显示一位小数) */
s16  DHT11_Get_Temp_x10(void);
s16  DHT11_Get_Humi_x10(void);
/* 传感器数据是否有效 */
u8   DHT11_Data_Valid(void);

#endif
