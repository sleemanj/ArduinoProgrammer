#include "hexData.h"

HEX_IMAGE *findImage (uint16_t signature);
uint16_t readSignature (void);
boolean programFuses (const byte *fuses);
void eraseChip(void);
boolean verifyImage(const HEX_IMAGE *image);
void busyWait(void);
boolean flashPage (byte *pagebuff, uint16_t pageaddr, uint8_t pagesize);
byte hexton (byte h);
void readImagePage(const HEX_IMAGE *image, uint16_t pageaddr, uint8_t pagesize, byte *page);
boolean verifyFuses (const byte *fuses, const byte *fusemask);

