#ifndef ChipData_h
#define ChipData_h

/* Notes
 *
 * The fusemask can be worked out from the datasheets, it represents the fuse bits which
 * are used for the particular chip, for example Page 287 Table 28-6 shows the low 3 bits
 * are used for the extended fuse on the 328/328p, which is of course 0x07
 * 
 * Fuse bits can be calculated here;
 * http://eleccelerator.com/fusecalc/fusecalc.php?chip=atmega328p
 *  
 */

ArduinoProgrammer::ChipData ArduinoProgrammer::_knownChips[]  = 
{
#ifdef ARDUINOPROGRAMMER_M328
  {
    0x950F,                    // Signature
    "m328p",                   // Name
    {0xFF, 0xFF, 0x07, 0x3F},  // Fusemask      { Low, High, Ext, Lock }        
    {0xFF, 0xDA, 0x05, 0x0F},  // Typical fuses { Low, High, Ext, Lock }    
    32768,                     // Total Flash Size
    128                        // Flash Page Size
  },
  
  {
    0x9514,                    // Signature
    "m328",                    // Name
    {0xFF, 0xFF, 0x07, 0x3F},  // Fusemask { Low, High, Ext, Lock }    
    {0xFF, 0xDA, 0x05, 0x0F},  // Typical fuses { Low, High, Ext, Lock }    
    32768,                     // Total Flash Size
    128                        // Flash Page Size
  },
#endif
  
#ifdef ARDUINOPROGRAMMER_M168
  {
    0x940b,                    // Signature
    "m168pa",                  // Name
    {0xFF, 0xFF, 0x07, 0x3F},  // Fusemask { Low, High, Ext, Lock }    
    {0xFF, 0xDD, 0x00, 0x0F},  // Typical fuses { Low, High, Ext, Lock }    
    16384,                     // Total Flash Size
    128                        // Flash Page Size
  },
  
  {
    0x9406,                    // Signature
    "m168",                    // Name
    {0xFF, 0xFF, 0x07, 0x3F},  // Fusemask { Low, High, Ext, Lock }    
    {0xFF, 0xDD, 0x00, 0x0F},  // Typical fuses { Low, High, Ext, Lock }    
    16384,                     // Total Flash Size
    128                        // Flash Page Size
  },
#endif
  
#ifdef ARDUINOPROGRAMMER_M88  
  {
    0x930f,                    // Signature
    "m88pa",                   // Name
    {0xFF, 0xFF, 0x07, 0x3F},  // Fusemask { Low, High, Ext, Lock }    
    {0xFF, 0xDD, 0x00, 0x0F},  // Typical fuses { Low, High, Ext, Lock }    
    8192,                      // Total Flash Size
    64                         // Flash Page Size
  },
  
  {
    0x930a,                    // Signature
    "m88a",                    // Name
    {0xFF, 0xFF, 0x07, 0x3F},  // Fusemask { Low, High, Ext, Lock }    
    {0xFF, 0xDD, 0x00, 0x0F},  // Typical fuses { Low, High, Ext, Lock }    
    8192,                      // Total Flash Size
    64                         // Flash Page Size
  },
#endif
  
#ifdef xARDUINOPROGRAMMER_M48
  {
    0x920a,                    // Signature
    "m48pa",                   // Name
    {0xFF, 0xFF, 0x01, 0x03},  // Fusemask { Low, High, Ext, Lock }    
    {0xFF, 0xDD, 0x00, 0xFF},  // Typical fuses { Low, High, Ext, Lock }    ; NB 48 can only lock completly or not lock at all, so we don't lock    
    4096,                      // Total Flash Size
    64                         // Flash Page Size
  },
  
  {
    0x9405,                    // Signature
    "m48",                     // Name
    {0xFF, 0xFF, 0x01, 0x03},  // Fusemask { Low, High, Ext, Lock }    
    {0xFF, 0xDD, 0x00, 0xFF},  // Typical fuses { Low, High, Ext, Lock }    ; NB 48 can only lock completly or not lock at all, so we don't lock
    4096,                      // Total Flash Size
    64                         // Flash Page Size
  }, 
#endif
};

#endif