#ifndef GPIO_H
#define GPIO_H

#define GPIOA_BASE 0x40020000
#define GPIOA_MODER (*(volatile uint32_t*)(GPIOA_BASE + 0x00))
#define GPIOA_AFRL (*(volatile uint32_t*)(GPIOA_BASE + 0x20))

#endif