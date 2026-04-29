#ifndef UART_H
#define UART_H

#include <stdint.h>

#define USART2_BASE 0x40004400
#define USART2_SR (*(volatile uint32_t*)(USART2_BASE + 0x00))
#define USART2_DR (*(volatile uint32_t*)(USART2_BASE + 0x04))
#define USART2_BRR (*(volatile uint32_t*)(USART2_BASE + 0x08))
#define USART2_CR1 (*(volatile uint32_t*)(USART2_BASE + 0x0C))
#define USART2_CR2 (*(volatile uint32_t*)(USART2_BASE + 0x10))

#define TIMEOUT 100000

void uart_init(void);
void uart_send_char(char c);
void uart_send_string(char *str);
int uart_receive_char(void);
void uart_receive_string(char *buffer, int max_length);

#endif