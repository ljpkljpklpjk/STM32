#include "control.h"
#include "actuator.h"
#include "encoder.h"
#include "timer.h"
#include "rc522.h"
#include "fuzzy_pid.h"
#include <stdio.h>

SysCtrl_t g_sys;

static const FuzzyPID_Config_t s_temp_config =
{
    TEMP_PID_KP, TEMP_PID_KI, TEMP_PID_KD,
    TEMP_PID_KP_ADJUST, TEMP_PID_KI_ADJUST, TEMP_PID_KD_ADJUST,
    TEMP_PID_ERROR_RANGE, TEMP_PID_RATE_RANGE, TEMP_PID_INTEGRAL_ZONE,
    TEMP_PID_DERIVATIVE_TAU
};
static const FuzzyPID_Config_t s_humi_config =
{
    HUMI_PID_KP, HUMI_PID_KI, HUMI_PID_KD,
    HUMI_PID_KP_ADJUST, HUMI_PID_KI_ADJUST, HUMI_PID_KD_ADJUST,
    HUMI_PID_ERROR_RANGE, HUMI_PID_RATE_RANGE, HUMI_PID_INTEGRAL_ZONE,
    HUMI_PID_DERIVATIVE_TAU
};
static FuzzyPID_t s_temp_pid, s_humi_pid;

/* One mutually exclusive pair: +1 heat/humidify, -1 cool/dehumidify. */
typedef struct
{
    s8 active, last_direction, window_direction;
    u32 changed_at, off_at, window_start, on_ms;
    u8 window_valid;
} RelayPair_t;

static RelayPair_t s_temp_pair, s_humi_pair;
static u32 s_last_sample;
static u8 s_control_valid, s_last_temp_set, s_last_humi_set;

static void Pair_Init(RelayPair_t *pair, u32 now)
{
    pair->active = pair->last_direction = pair->window_direction = 0;
    pair->changed_at = pair->off_at = pair->window_start = now;
    pair->on_ms = 0;
    pair->window_valid = 0;
}

static void Pair_Stop(RelayPair_t *pair, u32 now)
{
    if(pair->active)
    {
        pair->last_direction = pair->active;
        pair->active = 0;
        pair->changed_at = pair->off_at = now;
    }
    pair->window_valid = 0;
}

static void Pair_Apply(RelayPair_t *pair, float demand, u32 now)
{
    s8 direction = demand > 0.0f ? 1 : (demand < 0.0f ? -1 : 0);
    s8 wanted;
    float magnitude = demand < 0.0f ? -demand : demand;
    u32 elapsed;

    /* Latch magnitude per window. Direction/zero changes take effect earlier. */
    elapsed = (u32)(now - pair->window_start);
    if(!pair->window_valid || elapsed >= CONTROL_WINDOW_MS ||
       direction != pair->window_direction)
    {
        pair->window_start = now;
        pair->window_direction = direction;
        pair->window_valid = 1;
        pair->on_ms = (u32)(magnitude * (float)CONTROL_WINDOW_MS / 100.0f + 0.5f);
        if(pair->on_ms > CONTROL_WINDOW_MS) pair->on_ms = CONTROL_WINDOW_MS;
        /* Do not stretch tiny demands into a permanent minimum-power output. */
        if(pair->on_ms < CONTROL_MIN_ON_MS) pair->on_ms = 0;
        if(pair->on_ms < CONTROL_WINDOW_MS &&
           CONTROL_WINDOW_MS - pair->on_ms < CONTROL_MIN_OFF_MS)
            pair->on_ms = CONTROL_WINDOW_MS - CONTROL_MIN_OFF_MS;
        elapsed = 0;
    }
    wanted = elapsed < pair->on_ms ? pair->window_direction : 0;
    if(pair->active && pair->active != wanted)
    {
        if((u32)(now - pair->changed_at) < CONTROL_MIN_ON_MS) return;
        Pair_Stop(pair, now);
        /* Keep this window's timing after a normal pulse ends. */
        pair->window_valid = 1;
    }
    if(!pair->active && wanted)
    {
        if((u32)(now - pair->off_at) < CONTROL_MIN_OFF_MS) return;
        if(pair->last_direction && wanted != pair->last_direction &&
           (u32)(now - pair->off_at) < CONTROL_REVERSE_OFF_MS) return;
        pair->active = wanted;
        pair->changed_at = now;
    }
}

static void Control_Stop(u32 now)
{
    Pair_Stop(&s_temp_pair, now);
    Pair_Stop(&s_humi_pair, now);
    FuzzyPID_Reset(&s_temp_pid);
    FuzzyPID_Reset(&s_humi_pid);
    s_control_valid = 0;
    g_sys.temp_output_x10 = g_sys.humi_output_x10 = 0;
    g_sys.heater = g_sys.cooler = g_sys.fan = g_sys.humidifier = 0;
    g_sys.cold_fan = 0;
    g_sys.alarm = 0;
    Actuator_AllOff();
}

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
    g_sys.cold_fan = 0;
    g_sys.heater = 0;
    g_sys.cooler = 0;
    g_sys.humidifier = 0;
    g_sys.alarm = 0;
    g_sys.temp_output_x10 = g_sys.humi_output_x10 = 0;
    FuzzyPID_Init(&s_temp_pid, &s_temp_config);
    FuzzyPID_Init(&s_humi_pid, &s_humi_config);
    Pair_Init(&s_temp_pair, Tick_Get());
    Pair_Init(&s_humi_pair, Tick_Get());
    s_last_sample = Tick_Get();
    s_last_temp_set = g_sys.temp_set;
    s_last_humi_set = g_sys.humi_set;
    s_control_valid = 0;
}

/*
 * 把当前应有的转速真正写到 PWM 上
 *
 * 等待刷卡期间强制停转: 冷风风扇 PWM 的边沿噪声会盖住 RC522 的 13.56MHz
 * 接收前端, 风扇一转起来就刷不上卡。验证结束立即恢复原转速。
 */
static void ColdFan_Apply(void)
{
    ColdFan_SetDuty((g_sys.state == ST_WAIT_CARD) ? 0 : g_sys.cold_fan);
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

/* Called once per fresh sample, including failures. Inputs are in tenths. */
void Control_Update(s16 temp_x10, s16 humi_x10, u8 data_valid)
{
    u32 now = Tick_Get();
    u32 elapsed = (u32)(now - s_last_sample);
    float dt = (float)CONTROL_SAMPLE_MS / 1000.0f;

    if(!data_valid || temp_x10 < 0 || temp_x10 > 500 ||
       humi_x10 < 200 || humi_x10 > 900)
    {
        Control_Stop(now);
        return;
    }
    if(s_control_valid)
    {
        if(elapsed == 0) return; /* Repeated calls must not integrate twice. */
        if(elapsed >= CONTROL_STALE_MS) Control_Stop(now);
        else dt = (float)elapsed / 1000.0f;
    }
    /* Discard old integral and derivative history after setpoint changes. */
    if(g_sys.temp_set != s_last_temp_set) FuzzyPID_Reset(&s_temp_pid);
    if(g_sys.humi_set != s_last_humi_set) FuzzyPID_Reset(&s_humi_pid);
    s_last_temp_set = g_sys.temp_set;
    s_last_humi_set = g_sys.humi_set;
    FuzzyPID_Update(&s_temp_pid, (float)g_sys.temp_set, (float)temp_x10 / 10.0f, dt);
    FuzzyPID_Update(&s_humi_pid, (float)g_sys.humi_set, (float)humi_x10 / 10.0f, dt);
    g_sys.temp_output_x10 = (s16)(s_temp_pid.output * 10.0f);
    g_sys.humi_output_x10 = (s16)(s_humi_pid.output * 10.0f);
    s_last_sample = now;
    s_control_valid = 1;
    Control_ApplyOutputs();
}

/* Runs in the foreground loop; no PID computations in the timer ISR. */
void Control_ApplyOutputs(void)
{
    u32 now = Tick_Get();
    float cooling;

    if(!s_control_valid) return;
    if((u32)(now - s_last_sample) >= CONTROL_STALE_MS)
    {
        Control_Stop(now);
        return;
    }
    Pair_Apply(&s_temp_pair, s_temp_pid.output, now);
    Pair_Apply(&s_humi_pair, s_humi_pid.output, now);
    g_sys.heater = s_temp_pair.active > 0;
    g_sys.cooler = s_temp_pair.active < 0;
    g_sys.humidifier = s_humi_pair.active > 0;
    g_sys.fan = s_humi_pair.active < 0;

    /* Write OFF outputs first so a polarity change never overlaps in GPIO. */
    if(!g_sys.heater) Heater_Set(0);
    if(!g_sys.cooler) Cooler_Set(0);
    if(!g_sys.humidifier) Humidifier_Set(0);
    if(!g_sys.fan) Fan_Set(0);
    if(g_sys.heater) Heater_Set(1);
    if(g_sys.cooler) Cooler_Set(1);
    if(g_sys.humidifier) Humidifier_Set(1);
    if(g_sys.fan) Fan_Set(1);

    g_sys.cold_fan = 0;
    if(g_sys.cooler)
    {
        cooling = -s_temp_pid.output;
        if(cooling < CONTROL_COLDFAN_MIN_DUTY) cooling = CONTROL_COLDFAN_MIN_DUTY;
        if(cooling > 100.0f) cooling = 100.0f;
        g_sys.cold_fan = (u8)(cooling + 0.5f);
    }
    ColdFan_Apply();
}

/*
 * 报警判断
 * 温度或湿度偏离设定值超过报警阈值时蜂鸣器响, 回到阈值内自动停。
 */
void Control_Alarm(s16 temp_x10, s16 humi_x10, u8 data_valid)
{
    s16 dt, dh;

    if(!data_valid)
    {
        g_sys.alarm = 0;
        Beep_Set(0);
        return;
    }

    dt = temp_x10 - (s16)g_sys.temp_set * 10;
    dh = humi_x10 - (s16)g_sys.humi_set * 10;
    if(dt < 0) dt = -dt;
    if(dh < 0) dh = -dh;

    if(dt > TEMP_ALARM_TH * 10 || dh > HUMI_ALARM_TH * 10) g_sys.alarm = 1;
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
    static SysState_t s_state_last = ST_RUN;
    static u32        s_rf_check   = 0;
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
                s_rf_check        = g_sys.wait_start - RC522_CHECK_MS;  //进入时先检查一次
            }
            break;

        case ST_WAIT_CARD:
        {
            u8 id[CARD_ID_LEN];

            /*
             * 风扇停下来需要一点时间, 期间RC522也可能被干扰复位过。
             * 先确认模块还正常(必要时自动重新初始化), 再去寻卡。
             */
            if(Tick_Elapsed(s_rf_check) >= RC522_CHECK_MS)
            {
                s_rf_check = Tick_Get();
                RC522_Check();
            }

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

    //进出刷卡等待状态时立刻生效, 不必等下一次采集周期
    if(g_sys.state != s_state_last)
    {
        s_state_last = g_sys.state;
        ColdFan_Apply();
    }
}
