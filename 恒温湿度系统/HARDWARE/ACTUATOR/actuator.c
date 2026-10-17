#include "actuator.h"

//继电器输出: on=1表示要吸合
static void Relay_Write(GPIO_TypeDef *port, u16 pin, u8 on)
{
#if RELAY_ACTIVE_LOW
    if(on) GPIO_ResetBits(port, pin);
    else   GPIO_SetBits(port, pin);
#else
    if(on) GPIO_SetBits(port, pin);
    else   GPIO_ResetBits(port, pin);
#endif
}

/*
 * 冷风风扇 PWM 初始化: PB9 = TIM4_CH4
 * 定时器时钟 72MHz, 预分频 0, 自动重装 2879 -> 载波 25kHz,
 * 既符合PWM风扇推荐的 21~28kHz, 又超出人耳敏感范围, 低速时不会有啸叫声。
 * 占空比初始为 0, 上电风扇不转, 由控制层按温差给出转速。
 */
static void ColdFan_PwmInit(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef TIM_OCInitStructure;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_InitStructure.GPIO_Pin   = COLDFAN_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    /*
     * 这里特意用 2MHz 而不是 50MHz: 边沿越缓, 高频谐波越少。
     * 50MHz 速率下 PWM 边沿的宽带噪声会落进 RC522 的 13.56MHz 接收频段,
     * 风扇一转起来就刷不上卡; 换成 2MHz 后转速控制精度不受任何影响。
     */
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(COLDFAN_GPIO, &GPIO_InitStructure);

    TIM_TimeBaseStructure.TIM_Period        = COLDFAN_PWM_PERIOD;
    TIM_TimeBaseStructure.TIM_Prescaler     = 0;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode   = TIM_CounterMode_Up;
    TIM_TimeBaseInit(COLDFAN_TIM, &TIM_TimeBaseStructure);

    TIM_OCInitStructure.TIM_OCMode      = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse       = 0;
    TIM_OCInitStructure.TIM_OCPolarity  = TIM_OCPolarity_High;
    TIM_OC4Init(COLDFAN_TIM, &TIM_OCInitStructure);

    TIM_OC4PreloadConfig(COLDFAN_TIM, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(COLDFAN_TIM, ENABLE);
    TIM_Cmd(COLDFAN_TIM, ENABLE);
}

void Actuator_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    //PA1 除湿风扇, PA2 加热, PA3 制冷, PA11 蜂鸣器, PA12 加湿器
    GPIO_InitStructure.GPIO_Pin   = FAN_PIN | HEAT_PIN | COOL_PIN
                                  | BEEP_PIN | HUMI_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    ColdFan_PwmInit();              //PB9 冷风风扇 PWM, 初始占空比0

    Actuator_AllOff();
}

void Fan_Set(u8 on)         { Relay_Write(FAN_GPIO,  FAN_PIN,  on); }

/*
 * 设置冷风风扇转速, duty 为占空比百分比(0~100)
 * 0%: 输出恒低, 风扇停转(2线风扇由MOS管切地时同样停转);
 * 100%: 比较值取 ARR+1, 输出恒高, 满速。
 */
void ColdFan_SetDuty(u8 duty)
{
    u16 ccr;

    if(duty > 100) duty = 100;
    ccr = (u16)(((u32)duty * (COLDFAN_PWM_PERIOD + 1)) / 100);
    TIM_SetCompare4(COLDFAN_TIM, ccr);
}

void Heater_Set(u8 on)      { Relay_Write(HEAT_GPIO, HEAT_PIN, on); }
void Cooler_Set(u8 on)      { Relay_Write(COOL_GPIO, COOL_PIN, on); }

void Humidifier_Set(u8 on)
{
    if(on) GPIO_SetBits(HUMI_GPIO, HUMI_PIN);
    else   GPIO_ResetBits(HUMI_GPIO, HUMI_PIN);
}

void Beep_Set(u8 on)
{
    if(on) GPIO_SetBits(BEEP_GPIO, BEEP_PIN);
    else   GPIO_ResetBits(BEEP_GPIO, BEEP_PIN);
}

void Actuator_AllOff(void)
{
    Fan_Set(0);
    ColdFan_SetDuty(0);
    Heater_Set(0);
    Cooler_Set(0);
    Humidifier_Set(0);
    Beep_Set(0);
}
