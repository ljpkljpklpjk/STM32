#include "encoder.h"

typedef struct
{
    GPIO_TypeDef *s1_port;
    u16           s1_pin;
    GPIO_TypeDef *s2_port;
    u16           s2_pin;
    GPIO_TypeDef *key_port;
    u16           key_pin;

    u8   last_state;        //上一次的AB相组合状态
    s8   accum;             //一个机械格内累计的1/4步
    s16  delta;             //待读取的旋转增量
    u8   key_last;          //上一次按键电平
    u8   key_timer;         //按键消抖计时
    u8   key_event;         //按下事件标志
} Encoder_t;

static Encoder_t g_enc[ENC_NUM];

/*
 * 正交解码查表法
 * 索引 = (上次状态<<2) | 本次状态, 状态为 (S1<<1)|S2
 * 表值: +1 顺时针, -1 逆时针, 0 无效或未变化
 */
static const s8 ENC_TABLE[16] = {
     0, -1,  1,  0,
     1,  0,  0, -1,
    -1,  0,  0,  1,
     0,  1, -1,  0
};

void Encoder_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    u8 i;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB, ENABLE);

    //编码器1: PB5(S1) PB6(S2) PB7(KEY) + 编码器2: PB10(S1) PB11(S2), 上拉输入
    GPIO_InitStructure.GPIO_Pin  = ENC1_S1_PIN | ENC1_S2_PIN | ENC1_KEY_PIN
                                 | ENC2_S1_PIN | ENC2_S2_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    //编码器2: PA8(KEY), 上拉输入
    GPIO_InitStructure.GPIO_Pin  = ENC2_KEY_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    g_enc[ENC_TEMP].s1_port  = ENC1_S1_GPIO;   g_enc[ENC_TEMP].s1_pin  = ENC1_S1_PIN;
    g_enc[ENC_TEMP].s2_port  = ENC1_S2_GPIO;   g_enc[ENC_TEMP].s2_pin  = ENC1_S2_PIN;
    g_enc[ENC_TEMP].key_port = ENC1_KEY_GPIO;  g_enc[ENC_TEMP].key_pin = ENC1_KEY_PIN;

    g_enc[ENC_HUMI].s1_port  = ENC2_S1_GPIO;   g_enc[ENC_HUMI].s1_pin  = ENC2_S1_PIN;
    g_enc[ENC_HUMI].s2_port  = ENC2_S2_GPIO;   g_enc[ENC_HUMI].s2_pin  = ENC2_S2_PIN;
    g_enc[ENC_HUMI].key_port = ENC2_KEY_GPIO;  g_enc[ENC_HUMI].key_pin = ENC2_KEY_PIN;

    for(i = 0; i < ENC_NUM; i++)
    {
        u8 a = GPIO_ReadInputDataBit(g_enc[i].s1_port, g_enc[i].s1_pin);
        u8 b = GPIO_ReadInputDataBit(g_enc[i].s2_port, g_enc[i].s2_pin);
        g_enc[i].last_state = (u8)((a << 1) | b);
        g_enc[i].accum     = 0;
        g_enc[i].delta     = 0;
        g_enc[i].key_last  = 1;
        g_enc[i].key_timer = 0;
        g_enc[i].key_event = 0;
    }
}

/* 在1ms定时中断中调用 */
void Encoder_Scan(void)
{
    u8 i;

    for(i = 0; i < ENC_NUM; i++)
    {
        u8 a, b, state, idx, key;
        s8 dir;

        //---------- 旋转解码 ----------
        a = GPIO_ReadInputDataBit(g_enc[i].s1_port, g_enc[i].s1_pin);
        b = GPIO_ReadInputDataBit(g_enc[i].s2_port, g_enc[i].s2_pin);
        state = (u8)((a << 1) | b);

        if(state != g_enc[i].last_state)
        {
            idx = (u8)((g_enc[i].last_state << 2) | state);
            dir = ENC_TABLE[idx];
            g_enc[i].last_state = state;

            if(dir != 0)
            {
                g_enc[i].accum += dir;
                //EC11每个机械档位输出4个边沿, 累计满4步算一格
                if(g_enc[i].accum >= 4)
                {
                    g_enc[i].accum -= 4;
                    g_enc[i].delta++;
                }
                else if(g_enc[i].accum <= -4)
                {
                    g_enc[i].accum += 4;
                    g_enc[i].delta--;
                }
            }
        }

        //---------- 按键消抖 ----------
        key = GPIO_ReadInputDataBit(g_enc[i].key_port, g_enc[i].key_pin);
        if(key != g_enc[i].key_last)
        {
            g_enc[i].key_timer++;
            if(g_enc[i].key_timer >= ENC_KEY_DEBOUNCE)
            {
                g_enc[i].key_last  = key;
                g_enc[i].key_timer = 0;
                if(key == 0) g_enc[i].key_event = 1;   //检测下降沿, 即按下
            }
        }
        else
        {
            g_enc[i].key_timer = 0;
        }
    }
}

s16 Encoder_Get_Delta(u8 ch)
{
    s16 d;
    if(ch >= ENC_NUM) return 0;

    __disable_irq();
    d = g_enc[ch].delta;
    g_enc[ch].delta = 0;
    __enable_irq();

    return d;
}

u8 Encoder_Get_Key(u8 ch)
{
    u8 e;
    if(ch >= ENC_NUM) return 0;

    __disable_irq();
    e = g_enc[ch].key_event;
    g_enc[ch].key_event = 0;
    __enable_irq();

    return e;
}
