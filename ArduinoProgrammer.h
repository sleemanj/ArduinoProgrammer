#ifndef ArduinoProgrammer_h
#include <Arduino.h>

#define ArduinoProgrammer_h

#define ARDUINOPROGRAMMER_M328
#define ARDUINOPROGRAMMER_M168
#define ARDUINOPROGRAMMER_M88
#define ARDUINOPROGRAMMER_M48

// Indexes into ChipData.fusebits, ChipData.fusemask
#define ARDP_FUSE_LOW  0
#define ARDP_FUSE_HIGH 1
#define ARDP_FUSE_EXT  2
#define ARDP_FUSE_LOCK 3

// You may want to tweak these based on whether your chip is
// using an internal low-speed crystal
#define ARDP_CLOCKSPEED_FUSES   SPI_CLOCK_DIV128 
#define ARDP_CLOCKSPEED_FLASH   SPI_CLOCK_DIV8

#define ARDP_PRINT(...)    Serial.print(__VA_ARGS__);
#define ARDP_PRINTLN(...)  Serial.println(__VA_ARGS__);
//#define ARDP_DEBUG(...)    Serial.print(__VA_ARGS__);
//#define ARDP_DEBUGLN(...)  Serial.println(__VA_ARGS__);
#define ARDP_DEBUG(...)
#define ARDP_DEBUGLN(...)

#define ARDP_STEP(...)     Serial.println(__VA_ARGS__);     while(!Serial.available()) { delay(500); } while(Serial.available()) Serial.read();
// Error codes
// General Errors ~~~~~~~~~~~~~~~~~~~~~~~~~
#define ARDP_ERR                 0b10000000
#define ARDP_ERR_NOT_IN_SYNC     0b10000001
#define ARDP_ERR_INVALID_SIG     0b10000010
#define ARDP_ERR_SIG_MISMATCH    0b10000100
#define ARDP_ERR_OUT_OF_MEMORY   0b10001000
#define ARDP_ERR_NOT_IMPLEMENTED 0b10010000

// Fuse Related Errors ~~~~~~~~~~~~~~~~~~~~
#define ARDP_ERR_FUSE            0b01000000
#define ARDP_ERR_FUSE_LOW_VFY    0b01000001
#define ARDP_ERR_FUSE_HIGH_VFY   0b01000010
#define ARDP_ERR_FUSE_EXT_VFY    0b01000100
#define ARDP_ERR_FUSE_LOCK_VFY   0b01001000

// Programming Errors ~~~~~~~~~~~~~~~~~~~~~
#define ARDP_ERR_FLASH           0b00100000
#define ARDP_ERR_ADDRESS_INVALID 0b00100001
#define ARDP_ERR_COMMIT_FAIL     0b00100010
#define ARDP_ERR_FLASH_VFY       0b00100100
#define ARDP_ERR_EEPROM_FAIL     0b00101000
#define ARDP_ERR_DATATYPE        0b00110000

#define ARDP_DATATYPE_BINDATA        0b00000001
#define ARDP_DATATYPE_PAGEDBINDATA   0b00000010

class ArduinoProgrammer 
{
  public:
      // ChipData stores standard information about each chip we 
      // can program
      
      struct ChipData
      {
        unsigned int signature;        // Low two bytes of signature for a chip
        char  identifier[10];   // ie "m328p", for your use, doesn't matter what
        byte  fusemask[4];      // { Low, High, Ext, Lock }, see chipdata.h for more information on this        
        byte  fusebits[4];      // { Low, High, Ext, Lock}
        unsigned int chipsize;         // Bytes of flash
        byte         pagesize;         // Bytes per page
      };
      
      
      // BinData is produced by hexToBin from a .hex file
      // it represents the linear contents of the flash, byte by byte
      // trailing blanks (represented as 0xFF byte) can (should) be omitted.
      //
      //    byte MyBinary[] PROGMEM = { 0x01, 0xA2 ... 0xFF };
      //    ArduinoProgrammer::BinData MyBinData = {
      //        "myfancyprog.hex",
      //        0x1234,
      //        12345,
      //        MyBinary
      //    };
      //
      // A note about base_address. data_length
      //
      // These two values are in BYTES the avr fuse calcs etc usually
      // refer to WORD addresses (2 bytes per word).
      //
      // The bootloader section of the flash (Boot Flash) is at the 
      // END of the flash, the "normal" program starts at 0x0000
      //
      // The base address here is the address of the first BYTE
      // of where the data will go.
      //
      // The fuse calculators show the address in WORD addressing
      //
      // Eg, The standard arduino boot loader has base_address 0x7800
      //     in BYTES, divide by two to get WORD and it's 0x3C00
      //     (2 bytes per word on AVR8), and you can see that in the
      //     fuse calculators this corresponds to 
      //      "Boot Flash section size=1024 words Boot start address=$3C00"
      
      struct BinData
      {
        char  *imagename;
        unsigned int base_address;
        unsigned int  data_length;
        byte  *data;
      };    
      
      /*
       * 
       * byte  Page1[128] PROGMEM = {0x00 .... };
       * byte  Page2[128] PROGMEM = {0x01 .....};
       * byte  Page3[0] = { }; // Blank Page
       * (...)
       * byte  Pages[][] = ;
       * 
       * PagedBinData MyPagedBinData = { 
       *  'foo.hex',
       *  '0x0000', 
       *  '128',
       *  '256',
       *  { Page1, Page2, Page3... }
       * }
       */
      
      struct PagedBinData
      {
        char          *imagename;                
        unsigned int  base_address;
        byte          pagesize;
        unsigned int  pagecount;        
        byte          **data;
      };
            
      // Alternatively you can use standard .hex file contents as a string INCLUDING NEWLINES      
      //
      // ArduinoProgrammer::HexData MyHexDataString PROGMEM = 
      //    ":107E0000112484B714BE81FFF0D085E080938100F7\n"
      //    ":107E100082E08093C00088E18093C10086E0809377\n"
      //    ...
      //    ":00000001FF\n";  
      
      typedef char* HexData;
      
      // begin() starts the programming mode
      //  clockOutputOn : Turn on an 8MHz clock output on pin 9 which you can feed to XTAL1 of the 
      //                  target if you need to program a chip which is looking for a crystal or clock
      //                  and you don't have one on board, default off
      //  resetPin      : The pin which will be connected to RESET of the target, default 10
      //  dueSlaveSelect: On the Arduino Due, the slave select pin to use, optional
      
      byte begin(bool clockOutputOn = 0, byte resetPin = 10);
      
      // end() ends the programming mode
      byte end();
      
      // Get the standard chipData structure for the given (or detected) signature
      //  signature: if 0 then getSignature() is used to find the current target's signature
      //  returns a ChipData, if no appropriate chip data is known, the returned data
      //  will have a 0x0000 signature, and identifier as "unknown"
      // Note that the ChipData returned has the "typical" fuse values that you would
      // want to use, NOT the current fuse values of the target.
      ChipData   getStandardChipData(unsigned int signature = 0);
      
      // Return the low 16 bytes of the target signature (the high bytes are always the same)
      unsigned int   getSignature();      
      
      // Upload the given binData which has the .data stored in PROGMEM to the target
      // which has the given chipData.
      //
      // The chip will be erased, the low/high/ext fuses programmed, the flash programmed
      // and verified, and lock fuses set.
      // 
      // returns an errcode, or 0 if all OK
      byte    uploadFromProgmem(const ChipData &chipData, const BinData &binData);
      byte    uploadFromProgmem(const ChipData &chipData, const PagedBinData &binData);
      
      // Upload the given HexData which has been stored in  PROGMEM to the target
      // which has the given chipData.
      //
      // The chip will be erased, the low/high/ext fuses programmed, the flash programmed
      // and verified, and lock fuses set.
      //
      // returns an errcode, or 0 if all OK
      byte    uploadFromProgmem(const ChipData &chipData, const HexData hexData);
      
      byte    ripFlashToPagedBinData (const ChipData &chipData, const char *imagename);
      
  protected:
            
      byte _resetPin;       
      byte pmode;
      
      // This array of ChipData is filled in by 
      // chipdata.h
      static ChipData _knownChips[];  
      
      // Start programming mode, note this is done from begin()
      byte   start_pmode();
      
      // NB: Erasing the chip (according to "AVR: In-System Programming")
      //     is the only means to "unlock" the lockbits
      //     after a successful erase, the lock bits will be cleared
      byte   eraseChip(const ChipData &chipData);    
      
      // Progam the fuses set in chipData.fusebits into the target
      // and verify them.
      // Return error code or 0 if all OK
      byte   programFuses(const ChipData &chipData);
      
      // with respect to the specs of chipData (pagesize etc), read the page starting at address pageaddr from the 
      // binData, return it in pageBuffer (which must be of sufficient size, unchecked)
      // binData.data must be in PROGMEM
      byte readImagePageProgmem(const ChipData &chipData, const BinData &binData, const unsigned int pageaddr, byte *pageBuffer);
      byte readImagePageProgmem(const ChipData &chipData, const PagedBinData &binData, const unsigned int pageaddr, byte *pageBuffer);
      
      // with respect to the specs of chipData, write the given buffer of data
      // to the page starting at address pageaddr
      byte flashPage (const ChipData &chipData, byte *pagebuff, unsigned int pageaddr);

      // with respect to the specs of chipData, verify that the chip's flash
      // matches the binData.data which is in PROGMEM
      byte verifyImageProgmem(const ChipData &chipData, const BinData &binData);
      
      // Lock the chip with the lock fuse in chipData.fusebits[ARDP_FUSE_LOCK]
      // a chip can only be unlocked by erasing it
      byte   lockChip(const ChipData &chipData);
      
      // Wait until the target is no busy
      byte   busyWait(const ChipData &chipData);
                        
      // End progrmming mode, note this is done from end()
      byte   end_pmode();
      
      // Send 4 bytes of SPI data, return the last 3 bytes of response 
      unsigned long spi_transaction(byte a, byte b, byte c, byte d);
      
      // report and return the given error code
      byte     error(byte errcode);  
      
      // report and return the given error code and additional message
      byte     error(byte errcode, const char *message);
        
      // Upload data to the flash from a progmem
      // stored data structure, either
      //  BinData with .data in PROGMEM
      //  PagedBinData with .data and .data[0]...[xx]both in PROGMEM
      //  voidStarType is either ARDP_DATATYPE_BINDATA or ARDP_DATATYPE_PAGEDBINDATA
      
      byte    uploadFromProgmemVoidStar(const ChipData &chipData, const void *binData, byte voidStarType);
};

#endif
