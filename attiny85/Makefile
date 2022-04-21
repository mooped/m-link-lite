#  Makefile
TARGET = firmware
MCU = attiny85
AVRDUDE_MCU = t85
F_CPU = 8000000

SRCS = $(shell ls *.c)
OBJS = $(SRCS:%.c=$(OBJDIR)/%.o)
LIBS =
CC = avr-gcc
OBJDIR = .build
DEPDIR = .deps
CFLAGS = \
	-O3 -mmcu=$(MCU) -funsigned-char -funsigned-bitfields -ffunction-sections \
	-fpack-struct -fshort-enums -finline-limit=20 -Wall -Wstrict-prototypes \
	-Wundef -std=gnu99 -Wall -D__AVR_ATtiny85__
LDFLAGS = -mmcu=$(MCU) -Wl,--relax -Wl,--gc-sections -lm

all: $(TARGET).hex

$(TARGET).hex: $(OBJS)
	$(CC) $(LDFLAGS) -Wl,-Map=$(OBJDIR)/$(TARGET).map,--cref -o $(OBJDIR)/$(TARGET).elf $(OBJS) $(LIBS)
	avr-objcopy -O ihex -R .eeprom $(OBJDIR)/$(TARGET).elf $(TARGET).hex

$(OBJDIR)/%.o : %.c
	$(CC) -c $(CFLAGS) -MMD -MP -MF $(DEPDIR)/$(@F).d -Wa,-adhlns=$(OBJDIR)/$<.lst $< -o $@ -DF_CPU=$(F_CPU)

clean:
	rm -rf $(OBJDIR) $(TARGET).hex $(DEPDIR)

-include $(shell mkdir -p $(OBJDIR) $(DEPDIR) 2>/dev/null) $(wildcard $(DEPDIR)/*)

flash: $(TARGET).hex
	sudo avrdude -C avrdude.conf -p $(AVRDUDE_MCU) -c atmelice_isp -U flash:w:$(TARGET).hex

fuses:
	# http://eleccelerator.com/fusecalc/fusecalc.php?chip=attiny85&LOW=E2&HIGH=DF&EXTENDED=FF&LOCKBIT=FF
	# LFUSE disable internal clock div/8
	sudo avrdude -C avrdude.conf -p $(AVRDUDE_MCU) -c atmelice_isp -U lfuse:w:0xE2:m -U hfuse:w:0xDF:m -U efuse:w:0xFF:m -U lock:w:0xFF:m

