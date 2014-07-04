// Standalone AVR ISP programmer
// August 2011 by Limor Fried / Ladyada / Adafruit
// Jan 2011 by Bill Westfield ("WestfW")
//
// this sketch allows an Arduino to program a flash program
// into any AVR if you can fit the HEX file into program memory
// No computer is necessary. Two LEDs for status notification
// Press button to program a new chip. Piezo beeper for error/success 
// This is ideal for very fast mass-programming of chips!
//
// It is based on AVRISP
//
// using the following pins:
// 10: slave reset
// 11: MOSI
// 12: MISO
// 13: SCK
//  9: 8 MHz clock output - connect this to the XTAL1 pin of the AVR
//     if you want to program a chip that requires a crystal without
//     soldering a crystal in
// ----------------------------------------------------------------------


#include "optiLoader.h"
#include "SPI.h"
#include "code.h"

// Global Variables
int pmode=0;
byte pageBuffer[128];		       /* One page of flash */


/*
 * Pins to target
 */
#define SCK 13
#define MISO 12
#define MOSI 11
#define RESET 10
#define CLOCK 9     // self-generate 8mhz clock - handy!

#define BUTTON A1
#define PIEZOPIN A3

void setup () {
  Serial.begin(57600);			/* Initialize serial for status msgs */
  Serial.println("\nAdaBootLoader Bootstrap programmer (originally OptiLoader Bill Westfield (WestfW))");

  pinMode(PIEZOPIN, OUTPUT);

  pinMode(LED_PROGMODE, OUTPUT);
  pulse(LED_PROGMODE,2);
  pinMode(LED_ERR, OUTPUT);
  pulse(LED_ERR, 2);

  pinMode(BUTTON, INPUT);     // button for next programming
  digitalWrite(BUTTON, HIGH); // pullup
  
  pinMode(CLOCK, OUTPUT);
  // setup high freq PWM on pin 9 (timer 1)
  // 50% duty cycle -> 8 MHz
  OCR1A = 0;
  ICR1 = 1;
  // OC1A output, fast PWM
  TCCR1A = _BV(WGM11) | _BV(COM1A1);
  TCCR1B = _BV(WGM13) | _BV(WGM12) | _BV(CS10); // no clock prescale

  char buf[32];
  sprintf(buf, "sizeof(image_328): %d", imageSize());
  Serial.println(buf);
  sprintf(buf, "sizeof(binary): %d", binSize());
  Serial.println(buf);
}

void loop (void) {
  Serial.println("\nType 'G' or hit BUTTON for next chip");
  while (1) {
    if  ((! digitalRead(BUTTON)) || (Serial.read() == 'G'))
      break;  
  }

  target_poweron();

  uint16_t signature;
  HEX_IMAGE *targetimage;

  if (! (signature = readSignature()))  // Figure out what kind of CPU
    error("Signature fail");
  if (! (targetimage = findImage(signature)))
    error("Image fail");

  eraseChip();

  if (! programFuses(targetimage->image_progfuses)) // get fuses ready to program
    error("Programming Fuses fail");

  if (! verifyFuses(targetimage->image_progfuses, targetimage->fusemask) ) {
    error("Failed to verify fuses");
  } 

  end_pmode();
  start_pmode();

  HEX_PTR *hexLine = targetimage->hexLine;  
  uint16_t pageaddr = 0;
  uint8_t pagesize = pgm_read_byte(&targetimage->image_pagesize);
  uint16_t chipsize = pgm_read_word(&targetimage->chipsize);

  //Serial.println(chipsize, DEC);
  while (pageaddr < chipsize) {
     hexLine = readImagePage(hexLine, pageaddr, pagesize, pageBuffer);

     boolean blankpage = true;
     for (uint8_t i=0; i<pagesize; i++) {
       if (pageBuffer[i] != 0xFF) blankpage = false;
     }
     if (! blankpage) {
       if (! flashPage(pageBuffer, pageaddr, pagesize))
         error("Flash programming failed");
     }
     pageaddr += pagesize;
  }

  // Set fuses to 'final' state
  if (! programFuses(targetimage->image_normfuses))
    error("Programming Fuses fail");

  end_pmode();
  start_pmode();

  Serial.println("\nVerifing flash...");
  if (! verifyImage(targetimage->hexLine) ) {
    error("Failed to verify chip");
  } else {
    Serial.println("\tFlash verified correctly!");
  }

  if (! verifyFuses(targetimage->image_normfuses, targetimage->fusemask) ) {
    error("Failed to verify fuses");
  } else {
    Serial.println("Fuses verified correctly!");
  }
  target_poweroff();
  tone(PIEZOPIN, 4000, 200);
}



void error(char *string) { 
  Serial.println(string); 
  digitalWrite(LED_ERR, HIGH);  
  while(1) {
    tone(PIEZOPIN, 4000, 500);
  }
}

/*

From datasheet...

Power-up sequence:

1. Apply power between VCC and GND while RESET and SCK are set to “0”. In
   some systems, the programmer can not guarantee that SCK is held low
   during power-up. In this case, RESET must be given a positive pulse of
   at least two CPU clock cycles duration after SCK has been set to “0”.

2. Wait for at least 20ms and enable serial programming by sending the
   Programming Enable serial instruction to pin MOSI.

3. The serial programming instructions will not work if the communication
   is out of synchronization. When in sync. the second byte (0x53), will
   echo back when issuing the third byte of the Programming Enable
   instruction. Whether the echo is correct or not, all four bytes of the
   instruction must be transmitted. If the 0x53 did not echo back, give RESET
   a positive pulse and issue a new Programming Enable command.
*/

void start_pmode () {
  pinMode(13, INPUT); // restore to default

  SPI.begin();
  SPI.setClockDivider(SPI_CLOCK_DIV128); 
  
  debug("...spi_init done");
  // following delays may not work on all targets...
  pinMode(RESET, OUTPUT);
  pinMode(SCK, OUTPUT);
  digitalWrite(RESET, LOW);
  digitalWrite(SCK, LOW);
  digitalWrite(RESET, HIGH);
  delay(50);
  digitalWrite(RESET, LOW);
  delay(50);
  pinMode(MISO, INPUT);
  pinMode(MOSI, OUTPUT);
  debug("...spi_transaction");
  spi_transaction(0xAC, 0x53, 0x00, 0x00);
  debug("...Done");
  pmode = 1;
}

void end_pmode () {
  SPCR = 0;				/* reset SPI */
  digitalWrite(MISO, 0);		/* Make sure pullups are off too */
  pinMode(MISO, INPUT);
  digitalWrite(MOSI, 0);
  pinMode(MOSI, INPUT);
  digitalWrite(SCK, 0);
  pinMode(SCK, INPUT);
  digitalWrite(RESET, 0);
  pinMode(RESET, INPUT);
  pmode = 0;
}


/*
 * target_poweron
 * begin programming
 */
boolean target_poweron ()
{
  pinMode(LED_PROGMODE, OUTPUT);
  digitalWrite(LED_PROGMODE, HIGH);
  digitalWrite(RESET, LOW);  // reset it right away.
  pinMode(RESET, OUTPUT);
  delay(100);
  Serial.print("Starting Program Mode");
  start_pmode();
  Serial.println(" [OK]");
  return true;
}

boolean target_poweroff ()
{
  end_pmode();
  digitalWrite(LED_PROGMODE, LOW);
  return true;
}


// vi:syntax=cpp

