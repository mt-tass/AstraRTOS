#include "dma.h"

void dma_init(uint32_t base , uint8_t stream , uint8_t channel , uint32_t src_addr , uint32_t dest_addr , uint16_t size){
    DMA_CR(base,stream) &= ~(1 << 0); //disable the stream before starting any dma operation
    while(DMA_CR(base,stream) & (1 << 0)){
        //wait till the stream closes , last bits passby before shutting off
    }
    DMA_CR(base,stream) &= ~(0b111 << 25); //clear the channel slot
    DMA_CR(base,stream) |= (channel << 25);
    DMA_PAR(base,stream) = src_addr;
    DMA_M0AR(base,stream) = dest_addr;
    DMA_NDTR(base,stream) = size;
    DMA_CR(base, stream) |= (1 << 8);
    DMA_CR(base, stream) |= (1 << 4) | (1 << 3); //trigger interrupt when half full and full full
    NVIC_ISER[0] |= (1 << 16); //dma1 stream 5
    DMA_CR(base, stream) |= (1 << 0);
}

//dma stream 5 irq handler , use this inside main
void DMA1_Stream5_IRQHandler(void){
    if (DMA_HIFCR(DMA1_BASE) & (1 << 10)){
        DMA_HIFCR(DMA1_BASE) = (1 << 10);
        //semaphore give first half buffer
    }
    if (DMA_HIFCR(DMA1_BASE) & (1 << 11)){
        DMA_HIFCR(DMA1_BASE) = (1 << 11);
        //semaphore give second half buffer
    }
}