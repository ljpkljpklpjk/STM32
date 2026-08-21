#ifndef __DISPLAY_H
#define __DISPLAY_H
#include "sys.h"

/*
 * OLED 显示层 (128x64, SPI)
 *
 * 界面采用 ASCII 字模, 原因: 例程自带的 16x16 汉字库(Hzk1)里只有
 * "中景园电子"等与本项目无关的字, 缺少"温度/湿度/加热"等字模。
 * 用现成的 ASCII 字库既不用手工取模, 也不会因字模索引错位导致乱码。
 * 若后续需要中文界面, 只要用取模软件把汉字追加到 Hzk1 里,
 * 再把本文件中的 OLED_ShowString 换成 OLED_ShowChinese 即可。
 *
 * 布局:
 *   y=0   16号字   T:25.3C  SET:25C      当前温度 / 温度设定值
 *   y=16  16号字   H:60.2%  SET:60%      当前湿度 / 湿度设定值
 *   y=32  12号字   HEAT COOL FAN HUM     各执行机构的通断状态
 *   y=48  12号字   状态提示行
 */

void Display_Init(void);
/* 刷新整屏, temp_x10/humi_x10 为放大10倍的实测值 */
void Display_Update(s16 temp_x10, s16 humi_x10);
/* 开机欢迎界面 */
void Display_Welcome(void);

#endif
