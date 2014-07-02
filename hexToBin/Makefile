ARDUINO_DIR = /Applications/Arduino.app/Contents/Resources/Java
AVRGCC_DIR = $(ARDUINO_DIR)/hardware/tools/avr/bin
ARDUINO_VERSION := $(shell sed 's/\.//g' $(ARDUINO_DIR)/lib/version.txt)
PROCESSOR = __AVR_ATmega328P__
CFLAGS += -DARDUINO=$(ARDUINO_VERSION) -D$(PROCESSOR)
INCLUDES += -I$(ARDUINO_DIR)/hardware/arduino/cores/arduino -I..

ifeq ($(PROCESSOR), __AVR_ATmega328P__)
  INCLUDES += -I$(ARDUINO_DIR)/hardware/arduino/variants/standard
endif

all:
	gcc $(CFLAGS) $(INCLUDES) hexToBin.c -o hexToBin
	./hexToBin optiboot_atmega328.hex > ../hexData.c
#	$(AVRGCC_DIR)/avr-gcc $(CFLAGS) $(INCLUDES) hexData.c

