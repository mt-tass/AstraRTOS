#include "uart.h"
#include "rcc.h"
#include "gpio.h"
#include "system_init.h"

#define RINGBUFFER_SIZE 128

typedef struct {
    char buffer[RINGBUFFER_SIZE];
    volatile uint32_t head;
    volatile uint32_t tail;
} uart_ringbuffer_t;

static uart_ringbuffer_t ring_buffer[3] = {
    {.head = 0, .tail = 0}, {.head = 0, .tail = 0}, {.head = 0, .tail = 0}};

static int get_uart_idx(uint32_t base) {
    if(base == USART1_BASE) {
        return 0;
    }
    else if(base == USART2_BASE) {
        return 1;
    }
    else if(base == USART3_BASE) {
        return 2;
    }
    else {
        return -1;
    }
}

// Starts UART depending on the base provided
void uart_init(uint32_t base) {
    if(base == USART2_BASE) {
        rcc_enable_gpio(GPIOA_EN);
        rcc_enable_uart(USART2_EN);

        GPIO_MODER(GPIOA_BASE) &= ~((3 << (2 * 2)) | (3 << (3 * 2))); // Clear Pins 2 and 3
        GPIO_MODER(GPIOA_BASE) |= ((2 << (2 * 2)) | (2 << (3 * 2)));  // Sets to AF mode

        GPIO_AFRL(GPIOA_BASE) &= ~((0xF << (2 * 4)) | (0xF << (3 * 4))); // Clear
        GPIO_AFRL(GPIOA_BASE) |= ((7 << (2 * 4)) | (7 << (3 * 4)));      // Selecting AF to USART2

        USART_BRR(base) = 0x1250; // 9600 BaudRate (Condition: Peripheral Clock (APB1) = 45MHz)

        NVIC_ISER1 |= (1 << 6);
    }
    else if(base == USART1_BASE) {
        rcc_enable_gpio(GPIOA_EN);
        rcc_enable_uart(USART1_EN);

        GPIO_MODER(GPIOA_BASE) &= ~((3 << (9 * 2)) | (3 << (10 * 2))); // Clear Pins 9 and 10
        GPIO_MODER(GPIOA_BASE) |= ((2 << (9 * 2)) | (2 << (10 * 2)));  // Sets to AF mode

        GPIO_AFRH(GPIOA_BASE) &= ~((0xF << (1 * 4)) | (0xF << (2 * 4))); // Clear
        GPIO_AFRH(GPIOA_BASE) |= ((7 << (1 * 4)) | (7 << (2 * 4)));      // Selecting AF to USART1

        USART_BRR(base) = 0x249F; // 9600 BaudRate (Condition: Peripheral Clock (APB2) = 90MHz)

        NVIC_ISER1 |= (1 << 5);
    }
    else if(base == USART3_BASE) {
        rcc_enable_gpio(GPIOB_EN);
        rcc_enable_uart(USART3_EN);

        GPIO_MODER(GPIOB_BASE) &= ~((3 << (10 * 2)) | (3 << (11 * 2))); // Clear Pins 10 and 11
        GPIO_MODER(GPIOB_BASE) |= ((2 << (10 * 2)) | (2 << (11 * 2)));  // Sets to AF mode

        GPIO_AFRH(GPIOB_BASE) &= ~((0xF << (2 * 4)) | (0xF << (3 * 4))); // Clear
        GPIO_AFRH(GPIOB_BASE) |= ((7 << (2 * 4)) | (7 << (3 * 4)));      // Selecting AF to USART3

        USART_BRR(base) = 0x1250; // 9600 BaudRate (Condition: Peripheral Clock (APB1) = 45MHz)

        NVIC_ISER1 |= (1 << 7);
    }
    // 8N1
    USART_CR1(base) &= ~(1 << 12);
    USART_CR2(base) &= ~(3 << 12);
    USART_CR1(base) &= ~(1 << 10);

    // USART Enable, TX Enable, RX Enable
    USART_CR1(base) |= (1 << 13) | (1 << 3) | (1 << 2);

    // RXNEIE Enalbe
    USART_CR1(base) |= (1 << 5);
}

void uart_send_char(uint32_t base, char c) {
    while(!(USART_SR(base) & (1 << 7)))
        ;                        // Checking bit 7 of SR
    USART_DR(base) = (uint8_t)c; // Write to DR
}

// After writing to DR the TXE flag (bit 7) resets back

void uart_send_string(uint32_t base, char *str) {
    while(*str) {
        uart_send_char(base, *str++);
    }
}

int uart_receive_char_polling(uint32_t base) {
    USART_CR1(base) &= ~(1 << 5);

    uint32_t initial_ticks = system_ticks;

    while(!(USART_SR(base) & (1 << 5))) { // Checking bit 5 of SR
        if(system_ticks - initial_ticks >= TIMEOUT) {
            return -1; // Timeout if nothing is received (after 1 sec)
        }
    }
    int data = (int)(USART_DR(base) & 0xFF);
    USART_CR1(base) |= (1 << 5);

    return data;
}

void uart_receive_string_polling(uint32_t base, char *buffer, int max_length) {
    int i = 0;
    int c;

    while(i < max_length - 1) {              // Iterate until (max - 1)
        c = uart_receive_char_polling(base); // Receive characters

        if(c == -1)
            break; // Break if timeout
        if(c == '\r' || c == '\n')
            break; // Break if carriage return or newline

        buffer[i++] = (char)c; // Put in buffer
    }

    buffer[i] = '\0'; // Null terminator
}

int uart_receive_byte(uint32_t base, char *byte) {
    int idx = get_uart_idx(base);
    if(idx == -1) {
        return 0;
    }
    if(ring_buffer[idx].head == ring_buffer[idx].tail) {
        return 0;
    }
    *byte = ring_buffer[idx].buffer[ring_buffer[idx].tail];
    ring_buffer[idx].tail = (ring_buffer[idx].tail + 1) % RINGBUFFER_SIZE;

    return 1;
}

static void handle_irq(uint32_t base, int idx) {
    if(USART_SR(base) & (1 << 5)) {
        char recd_byte = (char)USART_DR(base);
        uint32_t next_head = (ring_buffer[idx].head + 1) % RINGBUFFER_SIZE;
        if(next_head != ring_buffer[idx].tail) {
            ring_buffer[idx].buffer[ring_buffer[idx].head] = recd_byte;
            ring_buffer[idx].head = next_head;

            ICSR |= (1 << 28);
        }
    }
}

void USART1_IRQHandler(void) {
    handle_irq(USART1_BASE, 0);
}

void USART2_IRQHandler(void) {
    handle_irq(USART2_BASE, 1);
}

void USART3_IRQHandler(void) {
    handle_irq(USART3_BASE, 2);
}