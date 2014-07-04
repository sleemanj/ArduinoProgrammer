#if ARDUINO >= 100
 #include "Arduino.h"
#else
 #include "WProgram.h"
#endif
#include <avr/pgmspace.h>
#include "SPI.h"

#ifndef _OPTILOADER_H
#define _OPTILOADER_H

#define FUSE_PROT 0			/* memory protection */
#define FUSE_LOW 1			/* Low fuse */
#define FUSE_HIGH 2			/* High fuse */
#define FUSE_EXT 3			/* Extended fuse */

// You may want to tweak these based on whether your chip is
// using an internal low-speed crystal
#define CLOCKSPEED_FUSES   SPI_CLOCK_DIV128 
#define CLOCKSPEED_FLASH   SPI_CLOCK_DIV8

#define LED_ERR 8
#define LED_PROGMODE A0

// Useful message printing definitions

#define debug(string) // flashprint(PSTR(string));


void pulse (int pin, int times);
void flashprint (const char p[]);


uint16_t spi_transaction (uint8_t a, uint8_t b, uint8_t c, uint8_t d);
void error(char *string);

#endif
