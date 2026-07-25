PREFIX = arm-none-eabi-
CC = $(PREFIX)gcc
CXX = $(PREFIX)g++
AS = $(PREFIX)gcc -x assembler-with-cpp
LD = $(PREFIX)g++
OBJCOPY = $(PREFIX)objcopy
SIZE = $(PREFIX)size 

TARGET = astra
BUILD = build

C_SOURCES = app/main.c kernel/port/system_init.c drivers/src/rcc.c drivers/src/uart.c drivers/src/gpio.c drivers/src/exti.c drivers/src/dma.c kernel/src/task.c kernel/src/mutex.c kernel/src/sem.c kernel/src/heap.c
CPP_SOURCES = app/tflm/tflm_bridge.cpp app/tflm/model.cpp app/tflm/debug_log.cpp
AS_SOURCES = kernel/port/startup_stm32f429zi.s 

LDSCRIPT = link/stm32f429zi.ld 

MCU = -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=softfp

INCLUDES = -I drivers/include -I kernel/port -I kernel/include \
           -I app/tflm \
           -I third_party/tflite-micro \
           -I third_party/tflite-micro/tensorflow/lite/micro/tools/make/downloads/flatbuffers/include \
           -I third_party/tflite-micro/tensorflow/lite/micro/tools/make/downloads/gemmlowp \
           -I third_party/tflite-micro/tensorflow/lite/micro/tools/make/downloads/ruy \
           -I third_party/tflite-micro/tensorflow/lite/micro/tools/make/downloads/cmsis/CMSIS/Core/Include \
           -I third_party/tflite-micro/tensorflow/lite/micro/tools/make/downloads/cmsis/CMSIS/DSP/Include \
           -I third_party/tflite-micro/tensorflow/lite/micro/tools/make/downloads/cmsis/CMSIS/NN/Include

CFLAGS  = $(MCU) -Wall -Wextra -Og -g -ffreestanding $(INCLUDES)
CXXFLAGS = $(CFLAGS) -std=c++17 -DTF_LITE_STATIC_MEMORY -fno-rtti -fno-exceptions -fno-threadsafe-statics
ASFLAGS = $(MCU) -Wall

TFLM_LIB = third_party/tflite-micro/gen/cortex_m_generic_cortex-m4_default_gcc/lib/libtensorflow-microlite.a

LDFLAGS = $(MCU) -T$(LDSCRIPT) -nostartfiles --specs=nano.specs --specs=nosys.specs -u _printf_float -Wl,--gc-sections -Wl,-Map=$(BUILD)/$(TARGET).map

C_OBJECTS = $(addprefix $(BUILD)/, $(C_SOURCES:.c=.o))      
CPP_OBJECTS = $(addprefix $(BUILD)/, $(CPP_SOURCES:.cpp=.o))
AS_OBJECTS = $(addprefix $(BUILD)/, $(AS_SOURCES:.s=.o))    

OBJECTS = $(C_OBJECTS) $(AS_OBJECTS) $(CPP_OBJECTS)

all: $(BUILD)/$(TARGET).elf $(BUILD)/$(TARGET).bin size 

$(BUILD)/$(TARGET).elf: $(OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $^ $(TFLM_LIB) -lstdc++ -lc -lm -lgcc

$(BUILD)/$(TARGET).bin: $(BUILD)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/%.o: %.s
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) -c $< -o $@

size: $(BUILD)/$(TARGET).elf
	$(SIZE) $<

flash: $(BUILD)/$(TARGET).elf
	openocd -f interface/stlink.cfg -f target/stm32f4x.cfg -c "program $< verify reset exit"

qemu: $(BUILD)/$(TARGET).elf
	qemu-system-arm -M netduinoplus2 -cpu cortex-m4 -display none -serial null -serial mon:stdio -kernel $<

clean:
	rm -rf $(BUILD)

.PHONY: all clean flash size