#include "exti.h"

void exti_init(uint8_t port , uint8_t pin , uint8_t edge_trigger){
    uint8_t index = pin/4;
    SYSCFG_EXTICR[index] &= ~(0xF << (pin%4)*4);
    SYSCFG_EXTICR[index] |= (port << (pin%4)*4);
    EXTI_IMR |= (1<<pin);
    switch (edge_trigger){
    case EXTI_RISING_EDGE:
        EXTI_RTSR |= (1 << pin);
        break;
    case EXTI_FALLING_EDGE:
        EXTI_FTSR |= (1 << pin);
        break;
    case EXTI_BOTH_EDGE:
        EXTI_RTSR |= (1 << pin);
        EXTI_FTSR |= (1 << pin);
        break;
    default:
        break;
    }
    if(pin == 0){
        NVIC_ISER[0] |= (1 << 6);
    }
}

//interrupt handler include in main and remove from here
void EXTI0_IRQHandler(void){
    EXTI_PR |= (1 << 0);
    //semaphore_give_from_isr
}