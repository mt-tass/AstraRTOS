#include <stdint.h>

#define RCC_BASE 0x40023800 
#define RCC_CR (*(volatile uint32_t*)(RCC_BASE + 0x00))
#define RCC_PLLCFGR (*(volatile uint32_t*)(RCC_BASE + 0x04))
#define RCC_CFGR (*(volatile uint32_t*)(RCC_BASE + 0x08))
#define FLASH_ACR (*(volatile uint32_t*)(0x40023C00))

void system_init(void){
    RCC_CR = RCC_CR | (1 << 16); // enable HSE clock using HSEON flag 
    while(!(RCC_CR & (1 << 17))); // wait for ready flag to become 1 (HSERDY)
    
    FLASH_ACR = (5 << 0) |    // wait 5 states for latency between flash and CPU (LATENCY[3:0])
                (1 << 8) |    // enable prefetch [PRFTEN]
                (1 << 9) |    // enable cache instructions [ICEN]
                (1 << 10);    // enable cache data read    [DCEN]

    RCC_PLLCFGR = (8 << 0)   |    // divide the HSE by 8 (PLLM)
                  (360 << 6) |    // multiply by 360 (PLLN)
                  (0 << 16)  |    // divide by 2 (PLLP)
                  (1 << 22);      // PLLSRC = HSE

    RCC_CR |= (1 << 24);           // turn on PLL (PLLON)
    while(!(RCC_CR & (1 << 25)));  // wait for ready flag (PLLRDY)

    RCC_CFGR =    (0 << 4)  |    // HPRE, AHB (180MHz)
                  (5 << 10) |    // PPRE1  APB1 / 4 (45 MHz)
                  (4 << 13);     // PPRE2 APB2 / 2 (90 MHz)

    RCC_CFGR |= (2 << 0);   // set PLL as system clock  (SW)
    while((RCC_CFGR & (3 << 2)) != (2 << 2));   // wait till its active 

    /* Value setup for SW: 
       SWS = 0 -> HSI is used
       SWS = 1 -> HSE is used
       SWS = 2 -> PLL is used 
    */ 
}
