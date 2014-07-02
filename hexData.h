
typedef struct
{
  uint8_t length;
  uint16_t address;
  uint8_t type;
  uint8_t checksum;
  uint8_t data[255];
} HEX_DATA;

typedef struct
{
  uint8_t length;
  uint16_t address;
  uint8_t type;
  uint8_t checksum;
  uint8_t *data;
} HEX_PTR;


