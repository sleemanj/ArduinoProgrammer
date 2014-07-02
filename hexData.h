
#ifndef __HEXDATA_H__
#define __HEXDATA_H__

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct
{
  uint8_t length;
  uint16_t address;
  uint8_t type;
  uint8_t checksum;
  uint8_t *data;
} HEX_PTR;

typedef struct
{
  char image_name[30];        /* Ie "optiboot_diecimila.hex" */
  char image_chipname[12];    /* ie "atmega168" */
  uint16_t image_chipsig;     /* Low two bytes of signature */
  uint8_t image_progfuses[5];    /* fuses to set during programming */
  uint8_t image_normfuses[5];    /* fuses to set after programming */
  uint8_t fusemask[4];
  uint16_t chipsize;
  uint8_t image_pagesize;        /* page size for flash programming */
  uint16_t numLines;             /* number of entries in hexLine array */
  HEX_PTR hexLine[];
} HEX_IMAGE;

int imageSize();
int binSize();

extern HEX_IMAGE *hexImages[];
extern uint8_t NUM_HEX_IMAGES;

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* __HEXDATA_H__ */

