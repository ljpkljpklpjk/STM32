#include "control.h"
#include "actuator.h"
#include "encoder.h"
#include "timer.h"
#include "rc522.h"
#include <stdio.h>

SysCtrl_t g_sys;

/*
 * 授权卡号列表
 * 这里预置了两张卡, 实际使用时先把卡放到读卡区,
 * 串口会打印出卡号(格式: Card ID: xx xx xx xx), 把打印出来的4个字节填到下面即可。
 */
static const u8 AUTH_CARDS[][CARD_ID_LEN] =
{
    {0x41, 0xA4, 0x26, 0x07},
    {0x47, 0xB5, 0x17, 0x07},
};
#define AUTH_CARD_NUM   (sizeof(AUTH_CARDS)/CARD_ID_LEN)

void Control_Init(void)
{
    g_sys.state       = ST_RUN;
    g_sys.temp_set    = TEMP_DEFAULT;
    g_sys.humi_set    = HUMI_DEFAULT;
    g_sys.pend_ch     = 0;
    g_sys.wait_start  = 0;
    g_sys.card_denied = 0;

    g_sys.fan = 0;
    g_sys.heater = 0;
    g_sys.cooler = 0;
    g_sys.humidifier = 0;
    g_sys.alarm = 0;
}

u8 Control_CardAuthorized(u8 *id)
{
    u8 i, j, match;

    for(i = 0; i < AUTH_CARD_NUM; i++)
    {
        match = 1;
        for(j = 0; j < CARD_ID_LEN; j++)
        {
            if(id[j] != AUTH_CARDS[i][j]) { match = 0; break; }
        }
        if(match) return 1;
    }
    return 0;
}

/*
 * 温湿度闭环控制(带回差的开关控制)
 *
 * 以温度为例, 设定值 S, 回差 H:
 *   实测 < S-H  -> 开加热, 关制冷
 *   实测 > S+H  -> 开制冷, 关加热
 *   S-H <= 实测 <= S+H -> 两者都关(落在回差带内, 保持)
 * 这样可以避免在设定值附近继电器来回抖动。
 */
void Control_Update(u8 temp, u8 humi, u8 data_valid)
{
    if(!data_valid)                 //传感器数据无效时全部停机, 防止误动作
    {
        g_sys.heater = 0;
        g_sys.cooler = 0;
        g_sys.fan = 0;
        g_sys.humidifier = 0;
        Heater_Set(0);
        Cooler_Set(0);
        Fan_Set(0);
        Humidifier_Set(0);
        return;
    }

    //---------------- 温度控制 ----------------
    if(temp < g_sys.temp_set - TEMP_HYST)
    {
        g_sys.heater = 1;
        g_sys.cooler = 0;
    }
    else if(temp > g_sys.temp_set + TEMP_HYST)
    {
        g_sys.heater = 0;
        g_sys.cooler = 1;
    }
    else                            //进入回差带, 停止调温
    {
        g_sys.heater = 0;
        g_sys.cooler = 0;
    }

    //---------------- 湿度控制 ----------------
    if(humi < g_sys.humi_set - HUMI_HYST)
    {
        g_sys.humidifier = 1;       //湿度低, 加湿
        g_sys.fan = 0;
    }
    else if(humi > g_sys.humi_set + HUMI_HYST)
    {
        g_sys.humidifier = 0;
        g_sys.fan = 1;              //湿度高, 开风扇除湿
    }
    else
    {
        g_sys.humidifier = 0;
        g_sys.fan = 0;
    }

    //加热和制冷互锁, 任何情况下不允许同时导通
    if(g_sys.heater && g_sys.cooler)
    {
        g_sys.heater = 0;
        g_sys.cooler = 0;
    }

    Heater_Set(g_sys.heater);
    Cooler_Set(g_sys.cooler);
    Fan_Set(g_sys.fan);
    Humidifier_Set(g_sys.humidifier);
}

/*
 * 报警判断
 * 温度或湿度偏离设定值超过报警阈值时蜂鸣器响, 回到阈值内自动停。
 */
void Control_Alarm(u8 temp, u8 humi, u8 data_valid)
{
    s16 dt, dh;

    if(!data_valid)
    {
        g_sys.alarm = 0;
        Beep_Set(0);
        return;
    }

    dt = (s16)temp - (s16)g_sys.temp_set;
    dh = (s16)humi - (s16)g_sys.humi_set;
    if(dt < 0) dt = -dt;
    if(dh < 0) dh = -dh;

    if(dt > TEMP_ALARM_TH || dh > HUMI_ALARM_TH) g_sys.alarm = 1;
    else                                         g_sys.alarm = 0;

    Beep_Set(g_sys.alarm);
}

/*
 * 按键 / 刷卡 / 旋钮 输入处理状态机
 *
 * ST_RUN       : 按下任一编码器按键 -> 记下是哪一路, 转入 ST_WAIT_CARD
 * ST_WAIT_CARD : 刷卡验证。授权卡 -> 进入对应的调整状态;
 *                非授权卡 -> 置提示标志并退回 ST_RUN;
 *                超时或再次按键 -> 退回 ST_RUN
 * ST_ADJ_xxx   : 旋转编码器加减设定值, 再按一次按键保存并退回 ST_RUN
 */
void Control_HandleInput(void)
{
    u8  key_t, key_h;
    s16 d;

    key_t = Encoder_Get_Key(ENC_TEMP);
    key_h = Encoder_Get_Key(ENC_HUMI);

    switch(g_sys.state)
    {
        case ST_RUN:
            //把运行态下的旋转丢弃, 未验证身份不允许直接改设定值
            Encoder_Get_Delta(ENC_TEMP);
            Encoder_Get_Delta(ENC_HUMI);

            if(key_t || key_h)
            {
                g_sys.pend_ch     = key_t ? ENC_TEMP : ENC_HUMI;
                g_sys.state       = ST_WAIT_CARD;
                g_sys.wait_start  = Tick_Get();
                g_sys.card_denied = 0;
            }
            break;

        case ST_WAIT_CARD:
        {
            u8 id[CARD_ID_LEN];

            if(key_t || key_h)              //等待过程中再按一次, 取消
            {
                g_sys.state = ST_RUN;
                break;
            }

            if(Tick_Elapsed(g_sys.wait_start) > CARD_WAIT_MS)   //超时放弃
            {
                g_sys.state = ST_RUN;
                break;
            }

            if(RC522_ReadCardID(id))
            {
                //打印卡号, 便于把新卡录入上方的授权列表
                printf("Card ID: %02X %02X %02X %02X\r\n", id[0], id[1], id[2], id[3]);

                if(Control_CardAuthorized(id))
                {
                    g_sys.state = (g_sys.pend_ch == ENC_TEMP) ? ST_ADJ_TEMP : ST_ADJ_HUMI;
                    //进入调整前清掉残留的旋转增量
                    Encoder_Get_Delta(ENC_TEMP);
                    Encoder_Get_Delta(ENC_HUMI);
                }
                else
                {
                    g_sys.card_denied = 1;  //非授权卡, 由显示层提示
                    g_sys.state = ST_RUN;
                }
            }
            break;
        }

        case ST_ADJ_TEMP:
            d = Encoder_Get_Delta(ENC_TEMP);
            Encoder_Get_Delta(ENC_HUMI);            //调温度时忽略另一路旋转
            if(d != 0)
            {
                s16 v = (s16)g_sys.temp_set + d;
                if(v < TEMP_MIN) v = TEMP_MIN;
                if(v > TEMP_MAX) v = TEMP_MAX;
                g_sys.temp_set = (u8)v;
            }
            if(key_t || key_h) g_sys.state = ST_RUN;    //再按一次退出
            break;

        case ST_ADJ_HUMI:
            d = Encoder_Get_Delta(ENC_HUMI);
            Encoder_Get_Delta(ENC_TEMP);
            if(d != 0)
            {
                s16 v = (s16)g_sys.humi_set + d;
                if(v < HUMI_MIN) v = HUMI_MIN;
                if(v > HUMI_MAX) v = HUMI_MAX;
                g_sys.humi_set = (u8)v;
            }
            if(key_t || key_h) g_sys.state = ST_RUN;
            break;

        default:
            g_sys.state = ST_RUN;
            break;
    }
}
