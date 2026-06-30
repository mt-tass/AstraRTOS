#ifndef UART_H
#define UART_H

#include <stdint.h>

#define USART1_BASE 0x40011000
#define USART2_BASE 0x40004400
#define USART3_BASE 0x40004800
#define USART_SR(base) (*((volatile uint32_t *)((base) + 0x00)))
#define USART_DR(base) (*((volatile uint32_t *)((base) + 0x04)))
#define USART_BRR(base) (*((volatile uint32_t *)((base) + 0x08)))
#define USART_CR1(base) (*((volatile uint32_t *)((base) + 0x0C)))
#define USART_CR2(base) (*((volatile uint32_t *)((base) + 0x10)))
#define USART_CR3(base) (*((volatile uint32_t *)((base) + 0x14)))
#define NVIC_ISER1 (*(volatile uint32_t *)(0xE000E104))
#define ICSR (*((volatile uint32_t *)0xE000ED04))

#define TIMEOUT 1000

void uart_init(uint32_t base);
void uart_send_char(uint32_t base, char c);
void uart_send_string(uint32_t base, char *str);
int uart_receive_char_polling(uint32_t base);
void uart_receive_string_polling(uint32_t base, char *buffer, int max_length);
int uart_receive_byte(uint32_t base, char *byte);

#endif