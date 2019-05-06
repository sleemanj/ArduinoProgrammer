// Standalone AVR ISP programmer Library
// October 2014 by James Sleeman <james@gogo.co.nz> 
//
// Heritage;
//   August 2011 by Limor Fried / Ladyada / Adafruit
//   Jan 2011 by Bill Westfield ("WestfW")

#include <Arduino.h>
#include <SPI.h>

#include "ArduinoProgrammer.h"
#include "ChipData.h"


#define ARDP_CLOCK 9     // self-generate 8mhz clock - handy!

byte ArduinoProgrammer::begin(bool clockOutputOn, byte resetPin) 
{
  _resetPin       = resetPin;
  pmode           = 0;
  if(clockOutputOn)
  {
     pinMode(ARDP_CLOCK, OUTPUT);
    // setup high freq PWM on pin 9 (timer 1)
    // 50% duty cycle -> 8 MHz
    OCR1A = 0;
    ICR1 = 1;
    // OC1A output, fast PWM
    TCCR1A = _BV(WGM11) | _BV(COM1A1);
    TCCR1B = _BV(WGM13) | _BV(WGM12) | _BV(CS10); // no clock prescale
  }
  
  
  ARDP_PRINTLN(F("ArduinoProgrammer Library"));
  /*
  ARDP_PRINTLN(F("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"));
  ARDP_PRINTLN(F("James Sleeman <james@gogo.co.nz>;   http://sparks.gogo.co.nz"));  
  ARDP_PRINTLN();
  ARDP_PRINTLN(F("Originally derived from:"));
  ARDP_PRINTLN(F("  FesterBesterTester hextoBin branch;   http://goo.gl/JAXImV"));
  ARDP_PRINTLN(F("  AdaBootLoader Bootstrap programmer;   http://goo.gl/VKMwVk"));
  ARDP_PRINTLN(F("  OptiLoader;           https://github.com/WestfW/OptiLoader"));
  */
  
  return start_pmode();  
}

byte ArduinoProgrammer::end()
{
  pinMode(ARDP_CLOCK, INPUT);
  return end_pmode();
}

/** Erase chip, set fuses, upload flash, and re-lock the chip (where possible)
 *  binData.data must be in PROGMEM (but binData itself not)
 *    //    byte MyBinary[] PROGMEM = { 0x01, 0xA2, 0xFF .... };
 *    //    ArduinoProgrammer::BinData MyBinData = {
 *    //        0x1234,
 *    //        12345,
 *    //        MyBinary
 *    //    }; 
 */

byte ArduinoProgrammer::uploadFromProgmem(const ChipData &chipData, const BinData &binData)
{  
  return uploadFromProgmemVoidStar(chipData, &binData, ARDP_DATATYPE_BINDATA);
  
  byte errnum = 0;
  unsigned int pageaddr;
  byte *pageBuffer;
  

  // Allocate memory for the pageBuffer
  pageBuffer = (byte *) malloc(chipData.pagesize);
  if(!pageBuffer) return error(ARDP_ERR_OUT_OF_MEMORY);
    
  do
  {
    // Before programming the flash
    if(getSignature() != chipData.signature) { errnum = error(ARDP_ERR_SIG_MISMATCH); break; }
    if((errnum = eraseChip(chipData)))         break; // This has the effect of unlocking
    if((errnum = programFuses(chipData)))      break; // This will also do a verify     
    
    // Program the flash
    // if(errnum = start_pmode(chipData))                 break;
    // Start from the base address
    pageaddr = binData.base_address;      
    while (pageaddr < chipData.chipsize) 
    {
      ARDP_DEBUG(F("Flashing Page "));
      ARDP_DEBUG(pageaddr/chipData.pagesize);
      ARDP_DEBUG(F("..."));
      
      if((errnum = readImagePageProgmem(chipData, binData, pageaddr, pageBuffer)))                                     break;

      boolean blankpage = true;
      for (byte i=0; i<chipData.pagesize; i++) 
      {
        if (pageBuffer[i] != 0xFF) blankpage = false;
      }
      
      if (! blankpage) 
      {
        if ((errnum = flashPage(chipData, pageBuffer, pageaddr))) break;
      }
      
      pageaddr += chipData.pagesize;
      
      ARDP_DEBUGLN(F("OK"));            
    }
    //if(errnum = end_pmode(chipData))       break;
    
    if(errnum = verifyImageProgmem(chipData, binData)) break;
    
    // After programming the flash
    if((errnum = lockChip(chipData)))          break;
    
  } while(0);
  
  if(pageBuffer) free(pageBuffer);
  
  return errnum;
}


// DAMMIT, this is identical to the above with the exception of the PagedBinData type
//         even if I used a (ugh) C++ template that only saves copy-paste, not actual
//         flash space where it wouls still be duplicated.
//
//         I hate strongly typed languages.

byte ArduinoProgrammer::uploadFromProgmem(const ChipData &chipData, const PagedBinData &binData)
{  
  return uploadFromProgmemVoidStar(chipData, &binData, ARDP_DATATYPE_PAGEDBINDATA);  
}

byte ArduinoProgrammer::uploadFromProgmem(const ChipData &chipData, const HexData hexData)
{
  return ARDP_ERR_NOT_IMPLEMENTED;
}

ArduinoProgrammer::ChipData ArduinoProgrammer::getStandardChipData(unsigned int signature)
{
  if(!signature)
  {
    signature = getSignature();
  }
     
  for(byte i = 0; i < (sizeof(_knownChips) / sizeof(ChipData)); i++)
  {
    if(_knownChips[i].signature == signature)
    {     
      return _knownChips[i];
    }
  }
    
  ChipData unknown = {
    0x0000,
    "unknown",
    {0xFF,0xFF,0xFF,0xFF},
    {0xFF,0xFF,0xFF,0xFF},
    0,
    0
  };
 
  return unknown;
}

/** Read a chipData.pagesize worth of bytes from the binary binData.data which is located in pagemem
 *  stuff them into pageBuffer (not bounds checked, make sure it's big enough)
 *  pageBuffer will be emptied (0xFF bytes) first
 */

byte ArduinoProgrammer::readImagePageProgmem(const ChipData &chipData, const BinData &binData, const unsigned int pageaddr, byte *pageBuffer)
{    
  byte  i;

  // 'empty' the page by filling it with 0xFF's
  for (i=0; i < chipData.pagesize; i++)
  {
    pageBuffer[i] = 0xFF;
  }
  
  // This seems to be a bad thing, requesting an address
  // which is below our base address
  if ((pageaddr + chipData.pagesize) < binData.base_address)
  {
    return error(ARDP_ERR_ADDRESS_INVALID);
  }
  
  
  if( (binData.base_address + binData.data_length) < pageaddr )
  { // This page is blank (we don't have data for it) so just 
    // use the already emptied page we made above
    return 0;
  }

  unsigned int dataOffset = pageaddr - binData.base_address;
  for (i=0; i < chipData.pagesize; i++)
  {
    if ((dataOffset + i) > binData.data_length) break;
    pageBuffer[i] = pgm_read_byte(&binData.data[dataOffset + i]);
  }
  return 0;
}

byte ArduinoProgrammer::readImagePageProgmem(const ChipData &chipData, const PagedBinData &binData, const unsigned int pageaddr, byte *pageBuffer)
{    
  byte  i;
    
  if(chipData.pagesize != binData.pagesize)
  {
    // For now, return an address invalid
    // @TODO Re page the data into the new page size, if it fits
    return error(ARDP_ERR_ADDRESS_INVALID);
  }
  
  if(chipData.chipsize < binData.pagesize * binData.pagecount)
  {
    // For now, return an error
    // @TODO ignore any trailing blank pages and see if it fits then, because
    //       that should be OK
    return error(ARDP_ERR_ADDRESS_INVALID);
  }
  
  // 'empty' the page by filling it with 0xFF's
  for (i=0; i < chipData.pagesize; i++)
  {
    pageBuffer[i] = 0xFF;
  }
  
  // This seems to be a bad thing, requesting an address
  // which is below our base address
  if ((pageaddr + chipData.pagesize) < binData.base_address)
  {
    return error(ARDP_ERR_ADDRESS_INVALID);
  }
  
  unsigned int pageIndex = pageaddr/binData.pagesize;
  if(!(long) pgm_read_word(&binData.data[pageIndex]))
  {
    // This page is blank (we don't have data for it) so just 
    // use the already emptied page we made above
    return 0;
  }
      
  for (i=0; i < binData.pagesize; i++)
  {
    pageBuffer[i] = pgm_read_byte(pgm_read_word(&binData.data[pageIndex])+i);
  }
  return 0;
}



/** Start Programming Mode

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

byte ArduinoProgrammer::start_pmode() {
  if(pmode) return 0;
    
  

  pinMode(_resetPin, OUTPUT);
  digitalWrite(_resetPin, LOW);  // reset it right away.

  // following delays may not work on all targets...
  pinMode(SCK, INPUT);
  pinMode(SCK, OUTPUT);
  digitalWrite(SCK, LOW);
    
  digitalWrite(_resetPin, HIGH);
  delay(50);
  
  digitalWrite(_resetPin, LOW);
  delay(50);
  //pinMode(MISO, INPUT);
  //pinMode(MOSI, OUTPUT);

  SPI.setClockDivider(SPI_CLOCK_DIV128); 
  SPI.begin();  

  
  // spi_trasnaction sends 4 bytes, and returns 3 bytes of response
  // Out:     [1].[2].[3].[4]
  // Return:      [1].[2].[3]
  // when we send the 3rd byte, we should therefore get 0x53 in the 2nd return position
  byte retries = 255;
  while(retries--)
  {
    byte result = (spi_transaction(0xAC, 0x53, 0x00, 0x00) >> 8);
    if(result == 0x53)
    {
      pmode = 1;
      ARDP_PRINTLN(F("Programming Mode Started"));
      return  0;
    }
    else
    {
      ARDP_DEBUG(F("Not in sync, result: "));
      ARDP_DEBUG(result);
      ARDP_DEBUG(F("; Attempt: "));
      ARDP_DEBUGLN(255-retries);
    }
    
    pinMode(SCK, OUTPUT);
    digitalWrite(SCK, LOW);

    digitalWrite(_resetPin, HIGH);
    delay(5);
    digitalWrite(_resetPin, LOW);
    delay(50);
  }
  
  return error(ARDP_ERR_NOT_IN_SYNC);
}


/** Erase the chip, this will also unlock it
 *  "The Lock bits can only be erased  with the Chip Erase command."
 *  Page 285 ATMega328 Datasheet
 */

byte ArduinoProgrammer::eraseChip(const ChipData &chipData) {
  ARDP_PRINT(F("Erasing chip..."));
  byte errno = 0;
  SPI.setClockDivider(ARDP_CLOCKSPEED_FUSES);   
  spi_transaction(0xAC, 0x80, 0, 0);    
  errno = busyWait(chipData);
  if(!errno) ARDP_PRINTLN(F("OK"));
  return errno;
}

/** Wait until not busy.
 * See page 301 of ATMega328 datasheet.
 */

byte ArduinoProgrammer::busyWait(const ChipData &chipData)  {
  byte busybit;
  do {
    busybit = spi_transaction(0xF0, 0x0, 0x0, 0x0);
  } while (busybit & 0x01);
  return 0;
}

/** End the programming mode. 
 *  Target RESET will be released so it can run.
 */

byte ArduinoProgrammer::end_pmode () {
  if(!pmode) return 0;
     
  SPCR = 0;				/* reset SPI */
  digitalWrite(MISO, 0);		/* Make sure pullups are off too */
  pinMode(MISO, INPUT);
  digitalWrite(MOSI, 0);
  pinMode(MOSI, INPUT);
  digitalWrite(SCK, 0);
  pinMode(SCK, INPUT);
  digitalWrite(_resetPin, 0);
  pinMode(_resetPin, INPUT);
  pmode = 0;
  ARDP_PRINTLN(F("Programming Mode Ended"));
  
  return 0;
}


/** Send 4 bytes of SPI, return last 3 bytes of response. 
 */

unsigned long ArduinoProgrammer::spi_transaction (byte a, byte b, byte c, byte d) {
  unsigned long aa, bb, cc, dd;
  unsigned long result;
   
  aa = (unsigned long) SPI.transfer(a);   
  bb = (unsigned long) SPI.transfer(b);  
  cc = (unsigned long) SPI.transfer(c);
  dd = (unsigned long) SPI.transfer(d);
  result = 0xFF000000 + (aa<<24) + (bb<<16) + (cc<<8) + dd;

  /*
  ARDP_DEBUG(F("* spi_transaction("));
  ARDP_DEBUG(a, HEX);  ARDP_DEBUG(F(", "));
  ARDP_DEBUG(b, HEX);  ARDP_DEBUG(F(", "));
  ARDP_DEBUG(c, HEX);  ARDP_DEBUG(F(", "));
  ARDP_DEBUG(d, HEX);  ARDP_DEBUG(F("); --> "));
  ARDP_DEBUG(aa, HEX); ARDP_DEBUG(F(", "));
  ARDP_DEBUG(bb, HEX); ARDP_DEBUG(F(", "));
  ARDP_DEBUG(cc, HEX); ARDP_DEBUG(F(", "));
  ARDP_DEBUG(dd, HEX); ARDP_DEBUG(F(" --> "));
  ARDP_DEBUGLN(result, HEX);
  */
  
  return result;
  //return aa & ((bb<<16)+(cc<<8) + dd );
}

/** Output error code.
 *  Return the error code given.
 *  Only those actually generating the error should issue error()
 *  those receiving an error should just pass the code on up the line
 *  as appropriate, the message has already been output.
 */

byte ArduinoProgrammer::error(byte errcode) 
{ 
  ARDP_PRINT(F("Error: "));
  ARDP_PRINTLN(errcode, BIN);
  return errcode;
}

/** Output error code and an associated extra message string. 
 *  Return the error code given.
 */

byte ArduinoProgrammer::error(byte errcode, const char *message) 
{ 
  ARDP_PRINT(F("Error: "));
  ARDP_PRINTLN(errcode, BIN);
  ARDP_PRINT(F("     : "));
  ARDP_PRINTLN(message);
  
  return errcode;
}


/**
 * readSignature
 * read the bottom two signature bytes (if possible) and return them
 * Note that the highest signature byte is the same over all AVRs so we skip it
 */

unsigned int ArduinoProgrammer::getSignature ()
{
  //end_pmode();
  //SPI.setClockDivider(ARDP_CLOCKSPEED_FUSES); 
  //start_pmode();
  unsigned int target_type = 0;
    
  target_type = (spi_transaction(0x30, 0x00, 0x01, 0x00) & 0xFFFF);
  target_type <<= 8;
  target_type |= (spi_transaction(0x30, 0x00, 0x02, 0x00) & 0xFFFF);
  
  if (target_type == 0 || target_type == 0xFFFF) {
    error(ARDP_ERR_INVALID_SIG);
  }
  return target_type & 0xFFFF;
}

/**
 * program and verify the L/H/E fuses, NOT the lock fuses
 * to lock, use lockChip() !
 * 
 * @TODO is there useful information from spi_transaction() 
 *   that we should check?
 */

byte ArduinoProgrammer::programFuses(const ChipData &chipData)
{
  byte errno = 0;
  byte   fuse;
  
  ARDP_PRINT(F("Setting and Verifying fuses..."));
  
  SPI.setClockDivider(ARDP_CLOCKSPEED_FUSES); 
      
  // Low ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  spi_transaction(0xAC, 0xA0, 0x00, chipData.fusebits[ARDP_FUSE_LOW]);
  if((errno = busyWait(chipData))) return errno;
      
  fuse = spi_transaction(0x50, 0x00, 0x00, 0x00);  
  if((fuse & chipData.fusemask[ARDP_FUSE_LOW]) != chipData.fusebits[ARDP_FUSE_LOW])
  {
    return error(ARDP_ERR_FUSE_LOW_VFY);
  }
  
  // High ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  spi_transaction(0xAC, 0xA8, 0x00, chipData.fusebits[ARDP_FUSE_HIGH]);
  if((errno = busyWait(chipData))) return errno;
        
  fuse = spi_transaction(0x58, 0x08, 0x00, 0x00);  
  if((fuse & chipData.fusemask[ARDP_FUSE_HIGH]) != chipData.fusebits[ARDP_FUSE_HIGH])
  {
    return error(ARDP_ERR_FUSE_HIGH_VFY);
  }
  
  
  // Ext ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  spi_transaction(0xAC, 0xA4, 0x00, chipData.fusebits[ARDP_FUSE_EXT]);
  if((errno = busyWait(chipData))) return errno;
  
  fuse = spi_transaction(0x50, 0x08, 0x00, 0x00);  
  if((fuse & chipData.fusemask[ARDP_FUSE_EXT]) != chipData.fusebits[ARDP_FUSE_EXT])
  {
    return error(ARDP_ERR_FUSE_EXT_VFY);
  }
  
  if(!errno) ARDP_PRINTLN(F("OK"));
    
  return errno;
}

/**
 * Set and verify the lock fuse.  Note that the chip can only
 * be unlocked by erasing the chip (according to the AVR ISP
 * document).
 * 
 * @TODO is there useful information from spi_transaction() 
 *   that we should check?
 */

byte ArduinoProgrammer::lockChip(const ChipData &chipData)
{
  byte errno = 0;
  byte   fuse;
  
  SPI.setClockDivider(ARDP_CLOCKSPEED_FUSES); 
  ARDP_PRINT(F("Locking Chip..."));
  
  // Lock ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  spi_transaction(0xAC, 0xE0, 0x00, chipData.fusebits[ARDP_FUSE_LOCK]);
  if((errno = busyWait(chipData))) return errno;
      
  fuse = spi_transaction(0x58, 0x00, 0x00, 0x00);  
  if((fuse & chipData.fusemask[ARDP_FUSE_LOCK]) != chipData.fusebits[ARDP_FUSE_LOCK])
  {
    return error(ARDP_ERR_FUSE_LOCK_VFY);
  }  
  
  if(!errno) ARDP_PRINTLN(F("OK"));
  
  return errno;
}

/** Program 1 page of flash.
 *  
 * See page 300 of ATMega Datasheet
 * See "AVR: In-system programming" document
 */

byte ArduinoProgrammer::flashPage (const ChipData &chipData, byte *pagebuff, unsigned int pageaddr) 
{  
  byte errno = 0;
  //ARDP_PRINT(F("Uploading Page..."));
  SPI.setClockDivider(ARDP_CLOCKSPEED_FLASH); 

  for (unsigned int i=0; i < chipData.pagesize/2; i++) 
  {

    //  Each address within the page is 16 bits (in practicality, only 6 bits really)
    //  Each address contains a word, each word is 2 bytes
    //  The low byte is loaded with command 0x40
    //  The high byte is loaded with command 0x48
    //  The address is split into the high 8 bits, then the low 8 bits, 
    //  Then the data byte    
    //  LOW: (0x40, addr_low_8, addr_high_8, data)
    //  HIG: (0x48, addr_low_8, addr_high_8, data)
   
    spi_transaction(0x40, i>>8 & 0xFF, i & 0xFF, pagebuff[2*i]);    
    if((errno = busyWait(chipData))) return errno;
    
    spi_transaction(0x48, i>>8 & 0xFF, i & 0xFF, pagebuff[2*i+1]);  
    if((errno = busyWait(chipData))) return errno;
  }

  // page addr is in bytes, byt we need to convert to words (/2)
  unsigned int wordaddr = pageaddr / 2;
  
  if ((spi_transaction(0x4C, (wordaddr >> 8) & 0xFF, wordaddr & 0xFF, 0) & 0xFFFF) != wordaddr) 
  {
    return error(ARDP_ERR_COMMIT_FAIL);
  }
      
  if((errno = busyWait(chipData))) return errno;
  
  // Verify
  //ARDP_PRINT(F("... Verifying ..."));
  for (unsigned int i=0; i < chipData.pagesize; i++) 
  {
    byte  w    = 0;
    byte  r    = 0;
        
    w = pagebuff[i];   // What we wrote
    r = (spi_transaction(0x20 + 8 * ((pageaddr+i) % 2), (pageaddr+i) >> 9, (pageaddr+i) >> 1, 0) & 0xFF); // What the chip has
    // NOTE: 
    //   the LSB of the address is of course HIGH or LOW, but this is 
    //   not specified because it's the WORD address we want, hence shifting the address right 1 (9)
    
    if (w != r)
    {
      char buf[120];
      snprintf(buf, sizeof(buf), "Address 0x%.4x; Wrote: 0x%.2x; Read: 0x%.2x;", (pageaddr+i), w, r);
      return error(ARDP_ERR_FLASH_VFY, buf);      
    }
  }
  //ARDP_PRINTLN(F("OK"));
  
  return errno;
}

/** Verify that the flash on the chip matches the given image in binData.data which is in progmem
 */

byte ArduinoProgrammer::verifyImageProgmem (const ChipData &chipData, const BinData &binData)  
{
  ARDP_PRINT(F("Verifying Image..."));
  
  SPI.setClockDivider(ARDP_CLOCKSPEED_FLASH); 
  byte  w    = 0;
  byte  r    = 0;
  unsigned int addr = 0;
  
  for (unsigned int i=0; i < binData.data_length; i++)
  {
    addr = binData.base_address + i;       // Address
    w = pgm_read_word(&binData.data[i]);   // What we wrote
    r = (spi_transaction(0x20 + 8 * (addr % 2), addr >> 9, addr >> 1, 0) & 0xFF); // What the chip has
    // NOTE: 
    //   the LSB of the address is of course HIGH or LOW, but this is 
    //   not specified because it's the WORD address we want, hence shifting the address right 1 (9)
    
    if (w != r)
    {
      char buf[120];
      snprintf(buf, sizeof(buf), "Address 0x%.4x; Wrote: 0x%.2x; Read: 0x%.2x;", addr, w, r);
      return error(ARDP_ERR_FLASH_VFY, buf);      
    }
  }
  
  ARDP_PRINTLN(F("OK"));
  
  return 0;
}

byte ArduinoProgrammer::ripFlashToPagedBinData (const ChipData &chipData, const char *imagename)  
{
  
  ARDP_PRINT(F("Ripping chip into PagedBinData format..."));
  
  byte *pageBuffer;
  char *textBuffer;
    
  
  pageBuffer = (byte *)malloc(chipData.pagesize);
  if(!pageBuffer) return error(ARDP_ERR_OUT_OF_MEMORY);
  
  byte bufSize = strlen(imagename)+40;
  textBuffer = (char *)malloc(bufSize);
  if(!textBuffer)
  {
      free(pageBuffer);
      return error(ARDP_ERR_OUT_OF_MEMORY);
  }
    
  Serial.print(F("\n\n\n"));  
  for(unsigned int i = 0; i < (chipData.chipsize / chipData.pagesize); i++)
  { // For each Page
    bool hasData = false;
    byte r       = 0xFF;
    unsigned int j = 0;
    for(j = 0; j < chipData.pagesize; j++)
    { // For each byte
      //                         i = page number, j = byte in page
      unsigned int byteAddress = (i * chipData.pagesize) + j;
      r = (spi_transaction(0x20 + 8 * (byteAddress % 2), byteAddress >> 9, byteAddress >> 1, 0) & 0xFF); 
      if(r != 0xFF) 
      {
        hasData = true;
      }
      pageBuffer[j] = r;
    }
    
    if(hasData)
    {
      snprintf(textBuffer, bufSize, "const byte %sPage%03d[%d] PROGMEM = {\n  ", imagename, i, chipData.pagesize);
      Serial.print(textBuffer);
      
      /*
      Serial.print(F("byte Page"));      
      Serial.print(i);
      Serial.print('['); Serial.print(chipData.pagesize); Serial.print(']');
      Serial.print(F(" PROGMEM = {\n  "));
      */
      
      for(j = 0; j < chipData.pagesize; j++)
      {
        snprintf(textBuffer, bufSize, "0x%.2x", pageBuffer[j]);
        Serial.print(textBuffer);
        // Serial.print("0x"); Serial.print(pageBuffer[j], HEX); 
        if(j < chipData.pagesize-1) Serial.print(", ");
        if((j % 16) == 15) Serial.print("\n  ");        
      }
      Serial.println(F("\n};\n"));
    }
    else
    { // blank page
      snprintf(textBuffer, bufSize, "#define %sPage%03d NULL\n", imagename, i, chipData.pagesize);
      Serial.print(textBuffer);
      /*
      Serial.print(F("byte *Page"));
      Serial.print(i);
      Serial.println(F(" = NULL;"));
      */
    }    
  }
  snprintf(textBuffer, bufSize, "const byte * const %sPages[] PROGMEM = {\n   ", imagename);
  Serial.print(textBuffer);  
  // Serial.print(F("byte *Pages[] PROGMEM = {\n   "));
  for(unsigned int i = 0; i < (chipData.chipsize / chipData.pagesize); i++)
  { 
    snprintf(textBuffer, bufSize, " %sPage%03d", imagename, i, chipData.pagesize);
    Serial.print(textBuffer);
    /*
       Serial.print(F(" Page"));
       Serial.print(i);
    */
    if(i < ((chipData.chipsize / chipData.pagesize)-1)) Serial.print(", ");
    if((i % 4) == 3) Serial.print("\n   ");
  }
  Serial.println("\n};");
  snprintf(textBuffer, bufSize, "\nArduinoProgrammer::PagedBinData %s = {\n", imagename);
  Serial.print(textBuffer);
  // Serial.println(F("\nPagedBinData MyPagedBinData = {"));
  Serial.print(F("  \"ripped\",\n  0x0000,\n  "));
  Serial.print(chipData.pagesize);
  Serial.print(F(",\n  "));
  Serial.print(chipData.chipsize / chipData.pagesize);
  snprintf(textBuffer, bufSize, ",\n %sPages };\n", imagename);
  Serial.print(textBuffer);
  //Serial.print(F(",\n  Pages};\n\n\n"));
  
  free(pageBuffer);
  free(textBuffer);  
  return 0;
}


byte ArduinoProgrammer::uploadFromProgmemVoidStar(const ChipData &chipData, const void *binData, byte voidStarType)
{  
  byte errnum = 0;
  unsigned int pageaddr;
  byte *pageBuffer;
  
  switch(voidStarType)
    {
      case ARDP_DATATYPE_BINDATA:
      case ARDP_DATATYPE_PAGEDBINDATA:        
        break;
        
      default:
          return error(ARDP_ERR_DATATYPE);
    }   

  // Allocate memory for the pageBuffer
  pageBuffer = (byte *) malloc(chipData.pagesize);
  if(!pageBuffer) return error(ARDP_ERR_OUT_OF_MEMORY);
    
  do
  {
    // Before programming the flash
    if(getSignature() != chipData.signature) { errnum = error(ARDP_ERR_SIG_MISMATCH); break; }
    if((errnum = eraseChip(chipData)))         break; // This has the effect of unlocking
    if((errnum = programFuses(chipData)))      break; // This will also do a verify     
    
    // Program the flash
    // if(errnum = start_pmode(chipData))                 break;
    // Start from the base address
    switch(voidStarType)
    {
      case ARDP_DATATYPE_BINDATA:
        ARDP_DEBUG(F("Uploading and verifying "));
        ARDP_DEBUG(((BinData *)binData)->imagename);
        ARDP_DEBUG("...");
        
        pageaddr = ((BinData *)binData)->base_address;
        break;
        
      case ARDP_DATATYPE_PAGEDBINDATA:
        ARDP_DEBUG(F("Uploading and verifying "));
        ARDP_DEBUG(((PagedBinData *)binData)->imagename);
        ARDP_DEBUG("...");
        
        pageaddr = ((PagedBinData *)binData)->base_address;
        break;
    }    
    
    while (pageaddr < chipData.chipsize) 
    {
    //  ARDP_DEBUG(F("Flashing Page "));
    //  ARDP_DEBUG(pageaddr/chipData.pagesize);
    //  ARDP_DEBUG(F("..."));
      
      switch(voidStarType)
      {
        case ARDP_DATATYPE_BINDATA:
          errnum = readImagePageProgmem(chipData, *((BinData *)binData), pageaddr, pageBuffer);
          break;
          
        case ARDP_DATATYPE_PAGEDBINDATA:
          errnum = readImagePageProgmem(chipData, *((PagedBinData *)binData), pageaddr, pageBuffer);
          break;          
      }  
    
      if(errnum)                                     break;

      boolean blankpage = true;
      for (byte i=0; i<chipData.pagesize; i++) 
      {
        if (pageBuffer[i] != 0xFF) blankpage = false;
      }
      
      if (! blankpage) 
      {
        if ((errnum = flashPage(chipData, pageBuffer, pageaddr))) break;
      }
      
      pageaddr += chipData.pagesize;
      
    //  ARDP_DEBUGLN(F("OK"));            
    }
    
    ARDP_DEBUGLN(F("OK"));
    
    //if(errnum = end_pmode(chipData))       break;
    /*
    switch(voidStarType)
    {
      case ARDP_DATATYPE_BINDATA:
        errnum = verifyImageProgmem(chipData, *((BinData *)binData));
        break;
        
      case ARDP_DATATYPE_PAGEDBINDATA:
        errnum = verifyImageProgmem(chipData, *((PagedBinData *)binData));
        break;
    }  
    if(errnum) break;
    */
    
    // After programming the flash
    if((errnum = lockChip(chipData)))          break;
    
  } while(0);
  
  if(pageBuffer) free(pageBuffer);
  
  return errnum;
}