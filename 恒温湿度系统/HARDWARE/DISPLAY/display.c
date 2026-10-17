#include "display.h"
#include "oled.h"
#include "control.h"
#include "dht11_filter.h"
#include "timer.h"
#include <stdio.h>
#include <string.h>

/*
 * 单行最多可显示的字符数。
 * 注意: 例程的 OLED_ShowString 里换行判断是 x > 128-size1, 比实际屏宽保守一格,
 * 且换行时 y 只加了2(并非一个字高)。因此这里把每行字符数取到"不会触发换行"
 * 的上限: 16号字 15个, 12号字 20个, 避免最后一个字符被甩到下一行造成花屏。
 */
#define LINE16_CHARS    15
#define LINE12_CHARS    20

static char s_buf[32];

//把字符串用空格补齐到指定长度再显示, 覆盖掉上一次残留的内容, 省去整屏清屏
static void ShowLine(u8 x, u8 y, char *str, u8 size1, u8 width)
{
    u8 len = (u8)strlen(str);
    if(width > sizeof(s_buf) - 1) width = sizeof(s_buf) - 1;
    while(len < width) { str[len++] = ' '; }
    str[len] = '\0';
    OLED_ShowString(x, y, (u8 *)str, size1);
}

void Display_Init(void)
{
    OLED_Init();
    OLED_Clear();
}

void Display_Welcome(void)
{
    OLED_Clear();
    OLED_ShowString(8,  8,  (u8 *)"Temp&Humi Ctrl", 16);
    OLED_ShowString(20, 32, (u8 *)"System Starting", 12);
    OLED_ShowString(20, 48, (u8 *)"Please wait...", 12);
    OLED_Refresh();
}

void Display_Update(s16 temp_x10, s16 humi_x10)
{
    //---------------- 第1行: 温度 ----------------
    if(DHT11_Data_Valid())
        sprintf(s_buf, "T:%d.%dC S:%dC", temp_x10 / 10, temp_x10 % 10, g_sys.temp_set);
    else
        sprintf(s_buf, "T:--.-C S:%dC", g_sys.temp_set);
    ShowLine(0, 0, s_buf, 16, LINE16_CHARS);

    //---------------- 第2行: 湿度 ----------------
    if(DHT11_Data_Valid())
        sprintf(s_buf, "H:%d.%d%% S:%d%%", humi_x10 / 10, humi_x10 % 10, g_sys.humi_set);
    else
        sprintf(s_buf, "H:--.-%% S:%d%%", g_sys.humi_set);
    ShowLine(0, 16, s_buf, 16, LINE16_CHARS);

    //---------------- 第3行: 执行机构状态 ----------------
    sprintf(s_buf, "HT:%c CL:%c FN:%c HM:%c",
            g_sys.heater     ? '1' : '0',
            g_sys.cooler     ? '1' : '0',
            g_sys.fan        ? '1' : '0',
            g_sys.humidifier ? '1' : '0');
    ShowLine(0, 32, s_buf, 12, LINE12_CHARS);

    //---------------- 第4行: 状态提示(正常运行时顺便显示冷风风扇占空比) ----------------
    switch(g_sys.state)
    {
        case ST_WAIT_CARD:
        {
            //显示刷卡剩余时间, 方便判断还有多久超时
            u32 el = Tick_Elapsed(g_sys.wait_start);
            u32 left = (el >= CARD_WAIT_MS) ? 0 : (CARD_WAIT_MS - el) / 1000;
            sprintf(s_buf, "Swipe card... %us", (unsigned int)left);
            break;
        }
        case ST_ADJ_TEMP:
            sprintf(s_buf, "ADJ TEMP: %d C", g_sys.temp_set);
            break;

        case ST_ADJ_HUMI:
            sprintf(s_buf, "ADJ HUMI: %d %%", g_sys.humi_set);
            break;

        case ST_RUN:
        default:
            if(g_sys.card_denied)       sprintf(s_buf, "Card denied!");
            else if(!DHT11_Data_Valid())sprintf(s_buf, "Sensor error!");
            else if(g_sys.alarm)        sprintf(s_buf, "** ALARM **");
            else                        sprintf(s_buf, "Running CF:%3d%%", g_sys.cold_fan);
            break;
    }
    ShowLine(0, 48, s_buf, 12, LINE12_CHARS);

    OLED_Refresh();     //整屏一次性刷出, 无闪烁
}
