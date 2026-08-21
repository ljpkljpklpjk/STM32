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

void Actuator_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    //PA1 风扇, PA2 加热, PA3 制冷, PA11 蜂鸣器, PA12 加湿器
    GPIO_InitStructure.GPIO_Pin   = FAN_PIN | HEAT_PIN | COOL_PIN
                                  | BEEP_PIN | HUMI_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    Actuator_AllOff();
}

void Fan_Set(u8 on)         { Relay_Write(FAN_GPIO,  FAN_PIN,  on); }
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
    Heater_Set(0);
    Cooler_Set(0);
    Humidifier_Set(0);
    Beep_Set(0);
}
