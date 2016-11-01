MCU=atmega328p
MCU_DUDE=atmega328p
OBJCOPY=avr-objcopy
CC=avr-gcc
AVRSIZE=avr-size
CFLAGS=-Wall -Os -mmcu=$(MCU) -DF_CPU=7372800UL
LDFLAGS=
SOURCES=main.c uart.c ir.c
OBJECTS=$(SOURCES:%.c=%.o)
EXECUTABLE=MegaLEDController

all: $(SOURCES) $(EXECUTABLE).hex

$(EXECUTABLE).elf: $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

$(EXECUTABLE).hex: $(EXECUTABLE).elf
	$(OBJCOPY) -O ihex $< $@
	$(AVRSIZE) $<

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

flash: $(EXECUTABLE).hex
	-@echo "25" > /sys/class/gpio/export
	-@sleep 0.2
	-@echo "out" > /sys/class/gpio/gpio25/direction
	-@echo "1" > /sys/class/gpio/gpio25/value
	sudo avrdude -c linuxspi -P /dev/spidev0.0 -p $(MCU_DUDE) -U flash:w:$(EXECUTABLE).hex:i
	@echo "25" > /sys/class/gpio/export
	@sleep 0.2
	@echo "out" > /sys/class/gpio/gpio25/direction
	@echo "1" > /sys/class/gpio/gpio25/value


fuse:
	-@echo "25" > /sys/class/gpio/export
	-@sleep 0.2
	-@echo "out" > /sys/class/gpio/gpio25/direction
	-@echo "1" > /sys/class/gpio/gpio25/value
	sudo avrdude -c linuxspi -P /dev/spidev0.0 -p $(MCU_DUDE) -U lfuse:w:0xA2:m
	@echo "25" > /sys/class/gpio/export
	@sleep 0.2
	@echo "out" > /sys/class/gpio/gpio25/direction
	@echo "1" > /sys/class/gpio/gpio25/value
#0xE4 target

clean:
	rm -f $(OBJECTS)
	rm -f $(EXECUTABLE).elf
	rm -f $(EXECUTABLE).hex
