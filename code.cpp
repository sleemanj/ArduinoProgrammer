#include "optiLoader.h"
#include "hexData.h"
#include "code.h"

/*
 * Bootload images.
 * These are the intel Hex files produced by the optiboot makefile,
 * with a small amount of automatic editing to turn them into C strings,
 * and a header attched to identify them
 */


/*
 * readSignature
 * read the bottom two signature bytes (if possible) and return them
 * Note that the highest signature byte is the same over all AVRs so we skip it
 */

uint16_t readSignature (void)
{
  SPI.setClockDivider(CLOCKSPEED_FUSES); 

  uint16_t target_type = 0;
  Serial.print("\nReading signature:");

  target_type = spi_transaction(0x30, 0x00, 0x01, 0x00);
  target_type <<= 8;
  target_type |= spi_transaction(0x30, 0x00, 0x02, 0x00);

  Serial.println(target_type, HEX);
  if (target_type == 0 || target_type == 0xFFFF) {
    if (target_type == 0) {
      Serial.println("  (no target attached?)");
    }
  }
  return target_type;
}

/*
 * findImage
 *
 * given 'signature' loaded with the relevant part of the device signature,
 * search the hex images that we have programmed in flash, looking for one
 * that matches.
 */
HEX_IMAGE *findImage (uint16_t signature)
{
  HEX_IMAGE *ip;
  Serial.println("Searching for image");

  for (byte i=0; i < NUM_HEX_IMAGES; i++) {
    ip = hexImages[i];

    if (ip && (pgm_read_word(&ip->image_chipsig) == signature)) {
#if VERBOSE
      Serial.print("  Found \"");
      flashprint(&ip->image_name[0]);
      Serial.print("\" for ");
      flashprint(&ip->image_chipname[0]);
      Serial.println();
#endif
      return ip;
    }
  }
  Serial.println(" Not Found");
  return 0;
}

/*
 * programmingFuses
 * Program the fuse/lock bits
 */
boolean programFuses (const byte *fuses)
{
  SPI.setClockDivider(CLOCKSPEED_FUSES); 
    
  byte f;
  Serial.println("\nSetting fuses");

  f = pgm_read_byte(&fuses[FUSE_PROT]);
  if (f) {
    spi_transaction(0xAC, 0xE0, 0x00, f);
    busyWait();
  }
  f = pgm_read_byte(&fuses[FUSE_LOW]);
  if (f) {
    spi_transaction(0xAC, 0xA0, 0x00, f);
    busyWait();
  }
  f = pgm_read_byte(&fuses[FUSE_HIGH]);
  if (f) {
    spi_transaction(0xAC, 0xA8, 0x00, f);
    busyWait();
  }
  f = pgm_read_byte(&fuses[FUSE_EXT]);
  if (f) {
    spi_transaction(0xAC, 0xA4, 0x00, f);
    busyWait();
  }
  return true;
}

/*
 * verifyFuses
 * Verifies a fuse set
 */
boolean verifyFuses (const byte *fuses, const byte *fusemask)
{
  SPI.setClockDivider(CLOCKSPEED_FUSES); 
  byte f;
  Serial.println("Verifying fuses");
  f = pgm_read_byte(&fuses[FUSE_PROT]);
  if (f) {
    uint8_t readfuse = spi_transaction(0x58, 0x00, 0x00, 0x00);  // lock fuse
    readfuse &= pgm_read_byte(&fusemask[FUSE_PROT]);
    if (readfuse != f) 
      return false;
  }
  f = pgm_read_byte(&fuses[FUSE_LOW]);
  if (f) {
    uint8_t readfuse = spi_transaction(0x50, 0x00, 0x00, 0x00);  // low fuse
    readfuse &= pgm_read_byte(&fusemask[FUSE_LOW]);
    if (readfuse != f) 
      return false;
  }
  f = pgm_read_byte(&fuses[FUSE_HIGH]);
  if (f) {
    uint8_t readfuse = spi_transaction(0x58, 0x08, 0x00, 0x00);  // high fuse
    readfuse &= pgm_read_byte(&fusemask[FUSE_HIGH]);
    if (readfuse != f) 
      return false;
  }
  f = pgm_read_byte(&fuses[FUSE_EXT]);
  if (f) {
    uint8_t readfuse = spi_transaction(0x50, 0x08, 0x00, 0x00);  // ext fuse
    readfuse &= pgm_read_byte(&fusemask[FUSE_EXT]);
    if (readfuse != f) 
      return false;
  }
  Serial.println();
  return true;			/* */
}



/*
 * readImagePage
 *
 * Read a page of intel hex image from a string in pgm memory.
*/

// Returns number of bytes decoded
void readImagePage(const HEX_IMAGE *image, uint16_t pageaddr, uint8_t pagesize, byte *page)
{
  uint16_t baseAddr = pgm_read_word(&image->address);
  uint16_t len      = pgm_read_word(&image->length);;
  uint8_t i;

  // 'empty' the page by filling it with 0xFF's
  for (i=0; i < pagesize; i++)
    page[i] = 0xFF;

  if ((pageaddr + pagesize) < baseAddr ||
      (baseAddr + len) < pageaddr)
    return;

  uint16_t dataOffset = pageaddr - baseAddr;
  for (i=0; i < pagesize; i++)
  {
    if ((dataOffset + i) > len) break;
    page[i] = pgm_read_byte(&image->data[dataOffset + i]);
  }
  return;
}


// Send one byte to the page buffer on the chip
void flashWord (uint8_t hilo, uint16_t addr, uint8_t data) {
#if VERBOSE
  Serial.print(data, HEX);  Serial.print(':');
  Serial.print(spi_transaction(0x40+8*hilo,  addr>>8 & 0xFF, addr & 0xFF, data), HEX);
  Serial.print(" ");
#else
  spi_transaction(0x40+8*hilo, addr>>8 & 0xFF, addr & 0xFF, data);
#endif

  busyWait();
}

/*
  From the datasheet...
  The Flash is programmed one page at a time. The memory page is loaded one
  byte at a time by supplying the 6 LSB of the address and data together with
  the Load Program Memory Page instruction. To ensure correct loading of the
  page, the data low byte must be loaded before data high byte is applied for
  a given address. The Program Memory Page is stored by loading the Write
  Program Memory Page instruction with the 7 MSB of the address. If polling
  (RDY/BSY) is not used, the user must wait at least tWD_FLASH before issuing
  the next page (See Table 28-18). Accessing the serial programming interface
  before the Flash write operation completes can result in incorrect
  programming.
*/

// Basically, write the pagebuff (with pagesize bytes in it) into page $pageaddr
boolean flashPage (byte *pagebuff, uint16_t pageaddr, uint8_t pagesize) {  
  SPI.setClockDivider(CLOCKSPEED_FLASH); 


  Serial.print("Flashing page "); Serial.println(pageaddr, HEX);
  for (uint16_t i=0; i < pagesize/2; i++) {

#if VERBOSE
    Serial.print(pagebuff[2*i], HEX); Serial.print(' ');
    Serial.print(pagebuff[2*i+1], HEX); Serial.print(' ');
    if ( i % 16 == 15) Serial.println();
#endif

    flashWord(LOW, i, pagebuff[2*i]);
    flashWord(HIGH, i, pagebuff[2*i+1]);
  }

  // page addr is in bytes, byt we need to convert to words (/2)
  pageaddr = pageaddr / 2;

  uint16_t commitreply = spi_transaction(0x4C, (pageaddr >> 8) & 0xFF, pageaddr & 0xFF, 0);

#if VERBOSE
  Serial.print("  Commit Page: 0x");  Serial.print(pageaddr, HEX);
  Serial.print(" -> 0x"); Serial.println(commitreply, HEX);
#endif

  if (commitreply != pageaddr) 
  {
    error("  Invalid commitreply!");
    return false;
  }

  busyWait();
  return true;
}

// verifyImage does a byte-by-byte verify of the flash hex against the chip
// Thankfully this does not have to be done by pages!
// returns true if the image is the same as the hextext, returns false on any error
boolean verifyImage (const HEX_IMAGE *image)  {
  uint16_t baseAddr = pgm_read_word(&image->address);
  uint16_t len = pgm_read_word(&image->length);

  SPI.setClockDivider(CLOCKSPEED_FLASH); 

  for (uint16_t i=0; i < len; i++)
  {
    uint8_t b = pgm_read_word(&image->data[i]);
    uint16_t addr = baseAddr + i;

    if (b != (spi_transaction(0x20 + 8 * (addr % 2), addr >> 9, addr >> 1, 0) & 0xFF))
    {
#if VERBOSE
        Serial.print("verification error at address 0x"); Serial.print(addr, HEX);
        Serial.print(" Should be 0x"); Serial.print(b, HEX); Serial.print(" not 0x");
        Serial.println((spi_transaction(0x20 + 8 * (addr % 2), addr >> 9, addr >> 1, 0) & 0xFF), HEX);
#endif
       return(false);
    }
  }
  return true;
}


// Send the erase command, then busy wait until the chip is erased

void eraseChip(void) {
  SPI.setClockDivider(CLOCKSPEED_FUSES); 

  spi_transaction(0xAC, 0x80, 0, 0);
  busyWait();
}

// Simply polls the chip until it is not busy any more - for erasing and programming
void busyWait(void)  {
  byte busybit;
  do {
    busybit = spi_transaction(0xF0, 0x0, 0x0, 0x0);
    //Serial.print(busybit, HEX);
  } while (busybit & 0x01);
}


/*
 * Functions specific to ISP programming of an AVR
 */
uint16_t spi_transaction (uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
  uint8_t n, m;
  SPI.transfer(a); 
  n = SPI.transfer(b);
  //if (n != a) error = -1;
  m = SPI.transfer(c);
  return 0xFFFFFF & ((n<<16)+(m<<8) + SPI.transfer(d));
}

