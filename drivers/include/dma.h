#ifndef DMA_H
#define DMA_H

#include <stdint.h>

#define DMA1_BASE 0x40026000
#define DMA2_BASE 0x40026400
#define DMA_STREAM_OFFSET(stream) (0x10 + (0x18 * (stream)))
#define DMA_CR(base, stream) (*(volatile uint32_t*)((base)+ DMA_STREAM_OFFSET(stream)+ 0x00))
#define DMA_NDTR(base, stream) (*(volatile uint32_t*)((base)+ DMA_STREAM_OFFSET(stream)+ 0x04))
#define DMA_PAR(base, stream) (*(volatile uint32_t*)((base)+ DMA_STREAM_OFFSET(stream)+ 0x08))
#define DMA_M0AR(base, stream) (*(volatile uint32_t*)((base)+ DMA_STREAM_OFFSET(stream)+ 0x0C))
#define DMA_LIFCR(base) (*(volatile uint32_t*)((base) + 0x08))
#define DMA_HIFCR(base) (*(volatile uint32_t*)((base) + 0x0C))
#define NVIC_ISER ((volatile uint32_t*)(0xE000E100 + 0x00))

void dma_init(uint32_t base , uint8_t stream , uint8_t channel , uint32_t src_addr , uint32_t dest_addr , uint16_t size);
#endif