/*
 * 恒温恒湿控制系统
 * 主控: STM32F103C8T6 (最小系统板)
 *
 * 功能:
 *   1. DHT11 采集环境温湿度, 经中值 + 一阶滞后滤波后使用;
 *   2. 带回差的开关控制: 加热器/制冷片调温, 加湿器/除湿风扇调湿;
 *   3. 制冷片旁边的冷风风扇用 PWM 调速: 室温比目标温度高得越多转得越快,
 *      高出10℃及以上满速, 不高于目标温度停转;
 *   4. 温湿度偏离设定值超过阈值时蜂鸣器报警, 回到阈值内自动解除;
 *   5. 0.96寸 SPI OLED 显示实测值、设定值、各执行机构状态与冷风风扇占空比;
 *   6. 两个旋钮编码器分别调整温度、湿度设定值;
 *      按下编码器按键后需刷 RFID 卡验证身份, 通过后才能调整。
 *
 * 详细接线见工程根目录的 接线说明.md
 */

#include "sys.h"
#include "delay.h"
#include "usart.h"

#include "dht11.h"
#include "dht11_filter.h"
#include "oled.h"
#include "display.h"
#include "rc522.h"
#include "encoder.h"
#include "actuator.h"
#include "control.h"
#include "timer.h"

#define SAMPLE_PERIOD_MS    2000    //DHT11 采集周期, 必须大于1s
#define DISPLAY_PERIOD_MS   200     //显示刷新周期
#define DENIED_SHOW_MS      2000    //"非授权卡"提示保持时间

int main(void)
{
    u32 t_sample = 0, t_display = 0, t_denied = 0;

    delay_init();
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    uart_init(9600);                //调试串口, PA9/PA10

    Actuator_Init();                //先把所有执行机构断开, 避免上电误动作
    Control_Init();
    Display_Init();
    Display_Welcome();

    Encoder_Init();
    RC522_Init();
    DHT11_Filter_Init();

    //DHT11 上电后需要约1s越过不稳定态
    delay_ms(1000);
    while(DHT11_Init())             //初始化并检测DHT11是否存在
    {
        OLED_Clear();
        OLED_ShowString(0, 24, (u8 *)"DHT11 NOT FOUND", 12);
        OLED_ShowString(0, 40, (u8 *)"check wiring PA0", 12);
        OLED_Refresh();
        printf("DHT11 init failed, check wiring on PA0\r\n");
        delay_ms(1000);
    }

    TIM3_Int_Init(999, 71);         //1ms 系统节拍, 内部扫描编码器

    //先采一轮, 避免开机瞬间显示无效值
    DHT11_Filter_Update();
    t_sample  = Tick_Get();
    t_display = Tick_Get();

    OLED_Clear();

    while(1)
    {
        //---------------- 按键 / 刷卡 / 旋钮 ----------------
        Control_HandleInput();

        //"非授权卡"提示定时清除
        if(g_sys.card_denied)
        {
            if(t_denied == 0) t_denied = Tick_Get();
            else if(Tick_Elapsed(t_denied) > DENIED_SHOW_MS)
            {
                g_sys.card_denied = 0;
                t_denied = 0;
            }
        }
        else t_denied = 0;

        //---------------- 温湿度采集与控制 ----------------
        if(Tick_Elapsed(t_sample) >= SAMPLE_PERIOD_MS)
        {
            t_sample = Tick_Get();

            DHT11_Filter_Update();

            //调整设定值的过程中依然保持控制和报警, 不中断恒温恒湿
            Control_Update(DHT11_Get_Temp(), DHT11_Get_Humi(), DHT11_Data_Valid());
            Control_Alarm (DHT11_Get_Temp(), DHT11_Get_Humi(), DHT11_Data_Valid());

            printf("T=%d.%dC H=%d.%d%% Tset=%d Hset=%d HT=%d CL=%d FN=%d CF=%d HM=%d AL=%d\r\n",
                   DHT11_Get_Temp_x10() / 10, DHT11_Get_Temp_x10() % 10,
                   DHT11_Get_Humi_x10() / 10, DHT11_Get_Humi_x10() % 10,
                   g_sys.temp_set, g_sys.humi_set,
                   g_sys.heater, g_sys.cooler, g_sys.fan, g_sys.cold_fan,
                   g_sys.humidifier, g_sys.alarm);
        }

        //---------------- 显示刷新 ----------------
        if(Tick_Elapsed(t_display) >= DISPLAY_PERIOD_MS)
        {
            t_display = Tick_Get();
            Display_Update(DHT11_Get_Temp_x10(), DHT11_Get_Humi_x10());
        }
    }
}
