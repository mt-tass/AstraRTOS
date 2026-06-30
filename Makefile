PREFIX = arm-none-eabi-
CC = $(PREFIX)gcc
AS = $(PREFIX)gcc -x assembler-with-cpp
LD = $(PREFIX)gcc
OBJCOPY = $(PREFIX)objcopy # converts .elf to .bin
SIZE = $(PREFIX)size 	   # prints table with sizes 


TARGET = astra
BUILD = build

C_SOURCES = app/main.c drivers/stm32f429zi/system_init.c drivers/stm32f429zi/rcc.c drivers/stm32f429zi/uart.c drivers/stm32f429zi/gpio.c kernel/src/task.c kernel/src/mutex.c kernel/src/sem.c kernel/src/heap.c kernel/src/stats.c kernel/src/timer.c kernel/src/queue.c
AS_SOURCES = kernel/port/arm/cortex-m4/startup_stm32f429zi.s 

LDSCRIPT = link/stm32f429zi.ld 

MCU = -mcpu=cortex-m4 -mthumb -mfloat-abi=soft

CFLAGS  = $(MCU) -Wall -Wextra -Og -g -ffreestanding -nostdlib
CFLAGS += -I drivers/stm32f429zi -I kernel/port -Ikernel/include
ASFLAGS = $(MCU) -Wall
LDFLAGS = $(MCU) -T$(LDSCRIPT) -nostdlib -nostartfiles -Wl,--gc-sections -Wl,-Map=$(BUILD)/$(TARGET).map

C_OBJECTS = $(addprefix $(BUILD)/, $(C_SOURCES:.c=.o))		# replace main.c with main.o	
AS_OBJECTS = $(addprefix $(BUILD)/, $(AS_SOURCES:.s=.o))	# creates startup_stm32f429zi.o

OBJECTS = $(C_OBJECTS) $(AS_OBJECTS)

all: $(BUILD)/$(TARGET).elf $(BUILD)/$(TARGET).bin size 

# create .elf file
$(BUILD)/$(TARGET).elf: $(OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $^

# create .bin file (strip meta data)
$(BUILD)/$(TARGET).bin: $(BUILD)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@

# compile source code to machine code	
$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.s
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) -c $< -o $@

# utilities
size: $(BUILD)/$(TARGET).elf
	$(SIZE) $<

flash: $(BUILD)/$(TARGET).elf
	openocd -f interface/stlink.cfg -f target/stm32f4x.cfg -c "program $< verify reset exit"

qemu: $(BUILD)/$(TARGET).elf
	qemu-system-arm -M netduinoplus2 -cpu cortex-m4 -display none -serial null -serial mon:stdio -kernel $<

clean:
	rm -rf $(BUILD)

.PHONY: all clean flash size