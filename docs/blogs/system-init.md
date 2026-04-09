# System Initialization Clock Configuration

The clock is a simple continuous square wave toggling between high and low in a continuous frequency, with each clock ticks (clock edges) the cpu fetches the next instruction, the ALU computes and register launches new values. Nothing really happens between each clock tick. Hence, this frequency defines how fast a code/instructions are being executed on the micro processor.

The internal clock (HSI) runs at around 16MHz which is not really fast enough for anything realtime, say for example you want to run a audio sample at 44 kHz at 16MHz you would have about 363 clock cycles to run between samples which is not enough. Say you have a 180 MHz clock, which would mean 4000 cycles in between samples, a lot better for a RTOS running multiple tasks

However a HSI (which is just a RC oscillator) has not the best accuracy, might be fine for the internal CPU, but not good enough for external peripherals that need precise timings. Hence, we use the HSE (external crystal oscillator) lot more accurate.

## PLL

The PLL (Phase-Locked Loop) is a internal harware circuit that takes in clock frequency as input and multiplies it to a higher frequency. However, it cant multiply by just any random number instead it has strict constraints on input and output ranges. So the PLL is divided into 3 stages:

PLLM divides the HSE down to 1 MHz because. PLLN multiplies that up to about 360, PLLP divides it back down to the final SYSCLK frequency

```
HSE (8MHz) -> divide by PLLM -> multiply by PLLN -> divide by PLLP -> SYSCLK
```

With our values we get a System Clock of 180 MHz: `8MHz / 8 * 360 / 2 = 180 MHz`

## Flash Wait States and Caching

The problem with this is that the flash memory is not fast enough to keep up, flash needs about 30ns per read and at 180 MHz instruction is called every 5.5ns, to solve this we make it so that the CPU pauses 5 cycles on every flash to let the memeory catch up, the code enables instruction cache, data cache and prefetch buffer for optimisation

- **Instruction Cache:** instead of fetching from the flash every single time we keep a cache which remembers recently fetched intructions and runs them instantly when called in loops
- **Data Cache:** similar to the instruction cache but for data instead
- **Prefetch Buffer:** while the CPU is executing instructions this buffer start fetching the next instruction from the flash

After system clock is defined it is distibuted into 3 buses AHB (180MHz), APB1 (45MHz) and APB2 (90MHz), which are each specified for specific peripherals

## Register Sequence

Flow of `system_init()` is as follows:

1. Enable HSE, `RCC_CR` bit 16 and wait bit 17
2. Set flash wait states `FLASH_ACR` (5 cycles latency)
3. Configure the PLL
4. Enable PLL, `RCC_CR` bit 24 and wait bit 25
5. Set bus prescalers
6. Switch SYSCLK to follow PLL
