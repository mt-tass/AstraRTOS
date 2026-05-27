#ifndef EXTI_H
#define EXTI_H

#include <stdint.h>

#define SYSCFG_BASE 0x40013800
#define EXTI_BASE 0x40013C00
#define SYSCFG_EXTICR ((volatile uint32_t*)(SYSCFG_BASE + 0x08)) 
#define EXTI_IMR (*(volatile uint32_t*)(EXTI_BASE + 0x00))
#define EXTI_RTSR (*(volatile uint32_t*)(EXTI_BASE + 0x08))
#define EXTI_FTSR (*(volatile uint32_t*)(EXTI_BASE + 0x0C))
#define EXTI_PR (*(volatile uint32_t*)(EXTI_BASE + 0x14))
#define NVIC_ISER ((volatile uint32_t*)(0xE000E100 + 0x00))
#define EXTI_RISING_EDGE 0
#define EXTI_FALLING_EDGE 1
#define EXTI_BOTH_EDGE 2

void exti_init(uint8_t port , uint8_t pin, uint8_t edge_trigger);

#endif