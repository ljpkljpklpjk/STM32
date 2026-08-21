#include "timer.h"
#include "encoder.h"

static volatile u32 g_tick = 0;     //系统运行毫秒数

//TIM3中断服务程序: 1ms进入一次
void TIM3_IRQHandler(void)
{
    if(TIM_GetITStatus(TIM3, TIM_IT_Update) != RESET)
    {
        TIM_ClearITPendingBit(TIM3, TIM_IT_Update);
        g_tick++;
        Encoder_Scan();             //1ms扫描一次编码器
    }
}

/*
 * TIM3 初始化
 * 溢出时间 Tout = ((arr+1)*(psc+1))/Ft us, Ft为定时器时钟(APB1倍频后=72MHz)
 * 取 arr=999, psc=71 即为 1ms
 */
void TIM3_Int_Init(u16 arr, u16 psc)
{
    NVIC_InitTypeDef NVIC_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

    TIM_TimeBaseStructure.TIM_Period = arr;
    TIM_TimeBaseStructure.TIM_Prescaler = psc;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);

    TIM_ClearITPendingBit(TIM3, TIM_IT_Update);     //先清标志, 避免初始化后立刻进中断
    TIM_ITConfig(TIM3, TIM_IT_Update, ENABLE);

    NVIC_InitStructure.NVIC_IRQChannel = TIM3_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    TIM_Cmd(TIM3, ENABLE);
}

u32 Tick_Get(void)
{
    return g_tick;
}

//u32做减法, 溢出时结果依然正确
u32 Tick_Elapsed(u32 start)
{
    return (u32)(g_tick - start);
}
