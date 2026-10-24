# Makefile para compilar o firmware fora do STM32CubeIDE (linha de comando / CI).
# O projeto do CubeIDE (.project/.cproject) continua sendo o caminho principal;
# este arquivo usa as mesmas fontes, includes e defines.
#
# Uso:
#   make            gera build/datalogger.elf, .hex e .bin
#   make flash      grava com o STM32CubeProgrammer (ST-Link, SWD)
#   make clean      apaga a pasta build

TARGET   = datalogger
BUILD    = build

PREFIX   = arm-none-eabi-
CC       = $(PREFIX)gcc
AS       = $(PREFIX)gcc -x assembler-with-cpp
OBJCOPY  = $(PREFIX)objcopy
SIZE     = $(PREFIX)size

C_SOURCES  = $(wildcard Core/Src/*.c)
C_SOURCES += $(wildcard FATFS/App/*.c)
C_SOURCES += $(wildcard FATFS/Target/*.c)
C_SOURCES += Middlewares/Third_Party/FatFs/src/diskio.c
C_SOURCES += Middlewares/Third_Party/FatFs/src/ff.c
C_SOURCES += Middlewares/Third_Party/FatFs/src/ff_gen_drv.c
C_SOURCES += Middlewares/Third_Party/FatFs/src/option/syscall.c
C_SOURCES += Middlewares/Third_Party/FatFs/src/option/ccsbcs.c
C_SOURCES += $(wildcard Drivers/STM32F4xx_HAL_Driver/Src/*.c)

ASM_SOURCES = Core/Startup/startup_stm32f411ceux.s
LDSCRIPT    = STM32F411CEUX_FLASH.ld

MCU      = -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
DEFS     = -DUSE_HAL_DRIVER -DSTM32F411xE
INCLUDES = -ICore/Inc -IFATFS/Target -IFATFS/App \
           -IDrivers/STM32F4xx_HAL_Driver/Inc -IDrivers/STM32F4xx_HAL_Driver/Inc/Legacy \
           -IMiddlewares/Third_Party/FatFs/src \
           -IDrivers/CMSIS/Device/ST/STM32F4xx/Include -IDrivers/CMSIS/Include

OPT     ?= -Os -g3
CFLAGS   = $(MCU) $(DEFS) $(INCLUDES) $(OPT) -std=gnu11 -Wall \
           -ffunction-sections -fdata-sections -MMD -MP
LDFLAGS  = $(MCU) -T$(LDSCRIPT) --specs=nano.specs --specs=nosys.specs \
           -Wl,--gc-sections -Wl,-Map=$(BUILD)/$(TARGET).map -Wl,--print-memory-usage \
           -Wl,--start-group -lc -lm -Wl,--end-group

OBJECTS  = $(addprefix $(BUILD)/,$(C_SOURCES:.c=.o)) $(addprefix $(BUILD)/,$(ASM_SOURCES:.s=.o))

all: $(BUILD)/$(TARGET).elf $(BUILD)/$(TARGET).hex $(BUILD)/$(TARGET).bin

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD)/%.o: %.s
	@mkdir -p $(dir $@)
	$(AS) -c $(MCU) $< -o $@

$(BUILD)/$(TARGET).elf: $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	$(SIZE) $@

$(BUILD)/%.hex: $(BUILD)/%.elf
	$(OBJCOPY) -O ihex $< $@

$(BUILD)/%.bin: $(BUILD)/%.elf
	$(OBJCOPY) -O binary -S $< $@

flash: $(BUILD)/$(TARGET).elf
	STM32_Programmer_CLI -c port=SWD mode=UR -w $< -v -rst

clean:
	rm -rf $(BUILD)

-include $(OBJECTS:.o=.d)

.PHONY: all flash clean
