TARGET = main

PROJECT_ROOT := ../../ARM_M3_design
CMSIS_INC := $(PROJECT_ROOT)/m3designstart/software/cmsis/CMSIS/Include

CC      = arm-none-eabi-gcc
AS      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
OBJDUMP = arm-none-eabi-objdump
SIZE    = arm-none-eabi-size

CPU = cortex-m3

CFLAGS = \
-mcpu=$(CPU) \
-mthumb \
-g3 \
-O0 \
-ffreestanding \
-fdata-sections \
-ffunction-sections \
-Wall

CFLAGS += -I$(CMSIS_INC)

LDFLAGS = \
-T linker.ld \
-nostartfiles \
-Wl,--gc-sections \
-Wl,-Map=$(TARGET).map

SRC = \
main.c \
startup.s

OBJ = $(SRC:.c=.o)
OBJ := $(OBJ:.s=.o)

all: $(TARGET).elf $(TARGET).bin

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.s
	$(AS) $(CFLAGS) -c $< -o $@

$(TARGET).elf: $(OBJ)
	$(CC) $(OBJ) $(LDFLAGS) -o $@
	$(SIZE) $@

$(TARGET).bin:
	$(OBJCOPY) -O binary $(TARGET).elf $(TARGET).bin

dump:
	$(OBJDUMP) -d $(TARGET).elf

clean:
	rm -f *.o *.elf *.bin *.map

run:
	qemu-system-arm \
		-M lm3s6965evb \
		-kernel main.elf \
		-nographic

debug:
	qemu-system-arm \
		-M lm3s6965evb \
		-kernel main.elf \
		-S \
		-gdb tcp::1234 \
		-nographic
gdb:
	gdb-multiarch main.elf -ex "target remote :1234"
