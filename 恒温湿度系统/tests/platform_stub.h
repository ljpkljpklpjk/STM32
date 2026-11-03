#ifndef TEST_PLATFORM_STUB_H
#define TEST_PLATFORM_STUB_H
/* Suppress MCU register headers for host tests only; production is unchanged. */
#define __SYS_H
#define __STM32F10x_H
#include <stdint.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;

#define NVIC_PriorityGroup_2 2
void NVIC_PriorityGroupConfig(u32 group);
#endif
