#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <hexData.h>


#define MAX_LINE 256

typedef struct
{
  uint8_t length;
  uint16_t address;
  uint8_t type;
  uint8_t checksum;
  uint8_t data[255];
} HEX_DATA;


long fileSize(FILE *f)
{
  long startingPos = ftell(f);
  long size;

  fseek(f, 0, SEEK_END);
  size = ftell(f);
  fseek(f, startingPos, SEEK_SET);
  return(size);
}

int numLines(const uint8_t *data)
{
  char line[MAX_LINE];
  int offset = 0;
  int lines = 0;

  while (EOF != sscanf((char*)&data[offset], "%s\r\n", line))
  {
    lines++;
    offset += strnlen(line, MAX_LINE);
    while ('\r' == data[offset] || '\n' == data[offset])
    {
      offset++;
    }
  }
  return(lines);
}

int main(int argc, char *argv[])
{
  uint8_t *fileData;
  char line[MAX_LINE];
  char digit[5];
  HEX_DATA *hexData;
  size_t lineLength;
  FILE *f;
  unsigned long number;
  int i, j, n = 0, fileOffset = 0, lineCount = 0;

  if (argc != 2)
  {
    printf("\nUSAGE: %s <file>\n", argv[0]);
    return(0);
  }

  f = fopen(argv[1], "r");
  if (!f)
  {
    printf("ERROR opening %s\n", argv[1]);
    return(-1);
  }

  fileData = malloc(fileSize(f));
  if (!fileData)
  {
    printf("ERROR failed to allocate %ld bytes\n", fileSize(f));
    return(-1);
  }

  if (fileSize(f) != fread(fileData, sizeof(fileData[0]), fileSize(f), f))
  {
    printf("ERROR reading file\n");
    return(-1);
  }
  fclose(f);

  lineCount = numLines(fileData);

  hexData = (HEX_DATA*) malloc(sizeof(HEX_DATA) * lineCount);
  if (!hexData)
  {
    printf("ERROR failed to allocate %ld bytes\n", sizeof(HEX_DATA) * lineCount);
    return(-1);
  }

  for (n=0; n < lineCount; n++)
  {
    if (EOF == sscanf((char*)&fileData[fileOffset], "%s\r\n", line))
    {
      printf("Unexpected EOF\n");
      return(-1);
    } 

    fileOffset += strnlen(line, MAX_LINE);
    while ('\r' == fileData[fileOffset] || '\n' == fileData[fileOffset])
    {
      fileOffset++;
    }

    lineLength = strnlen(line, MAX_LINE);
    
    if (lineLength < 11)
    {
      printf("ERROR: line too short to be valid.\n");
      return(-1);
    }
    if (':' != line[0])
    {
      printf("ERROR: line must begin with ':'\n");
      return(-1);
    }

    digit[0] = line[1];
    digit[1] = line[2];
    digit[2] = '\0';
    hexData[n].length = (uint8_t) strtol(digit, NULL, 16);

    digit[0] = line[3];
    digit[1] = line[4];
    digit[2] = line[5];
    digit[3] = line[6];
    digit[4] = '\0';
    hexData[n].address = (uint16_t) strtol(digit, NULL, 16);

    digit[0] = line[7];
    digit[1] = line[8];
    digit[2] = '\0';
    hexData[n].type = (uint8_t) strtol(digit, NULL, 16);

    if (lineLength < ((hexData[n].length * 2) + 9))
    {
      printf("ERROR: line shorter than specified by length field\n");
      return(-1);
    }

    for (i=9; i < hexData[n].length * 2 + 9; i++)
    {
      digit[0] = line[i++];
      digit[1] = line[i];
      digit[2] = '\0';
      hexData[n].data[(i-9) / 2] = (uint8_t) strtol(digit, NULL, 16);
    }

    digit[0] = line[i++];
    digit[1] = line[i];
    digit[2] = '\0';
    hexData[n].checksum = (uint8_t) strtol(digit, NULL, 16);
  }

  printf("#if ARDUINO >= 100\n");
  printf(" #include \"Arduino.h\"\n");
  printf("#else\n");
  printf(" #include \"WProgram.h\"\n");
  printf("#endif\n");
  printf("#include <avr/pgmspace.h>\n");
  printf("#include <stdint.h>\n");
  printf("#include \"hexData.h\"\n\n");

  for (n=0; n < lineCount; n++)
  {
    printf("uint8_t PROGMEM __hexLineData_%d__[] = {", n);
    for (j=0; j < hexData[n].length; j++)
    {
      if (j) printf(", ");
      printf("0x%02X", hexData[n].data[j]);
    }
    printf("};\n");
  }

  printf("\nHEX_IMAGE PROGMEM hexImage =\n{\n");
  printf("  \"%s\",        // image_name\n", argv[1]);
  printf("  \"atmega328p\",           // image_chipname\n");
  printf("  0x950F,                 // image_chipsig for 328P\n");
  printf("  {0x3F, 0xFF, 0xDA, 0x05},            // pre program fuses (prot/lock, low, high, ext)\n");
  printf("  {0x0F, 0x00, 0x00, 0x00},            // post program fuses. These are the\n");
  printf("                                       // default settings from the adaFruit\n");
  printf("                                       // project. With these settings, the\n");
  printf("                                       // target processor may be programmed\n");
  printf("                                       // through the arduino IDE as a\n");
  printf("                                       // \"Arduino Pro or Pro Mini (3.3V,\n");
  printf("                                       // 8MHz) w/ ATmega328\".\n");
  printf("  {0x3F, 0xFF, 0xFF, 0x07},  // fuse mask\n");
  printf("  32768,                     // size of chip flash in bytes\n");
  printf("  128,                       // size in bytes of flash page\n");
  printf("  %d,                        // number of entries in the hexLine array\n", lineCount);
  printf("  {\n");

  for (n=0; n < lineCount; n++)
  {
    if (n) printf(",\n");

    printf("    {0x%02X, 0x%04X, 0x%02X, 0x%02X, __hexLineData_%d__}",
           hexData[n].length, hexData[n].address, hexData[n].type, hexData[n].checksum, n);
  }

  printf("\n");
  printf("  }\n");
  printf("};\n\n");

  printf("HEX_IMAGE *hexImages[] = {\n");
  printf(" &hexImage,\n");
  printf("};\n");

  printf("uint8_t NUM_HEX_IMAGES = sizeof(hexImages)/sizeof(hexImages[0]);\n");
  printf("int imageSize()\n");
  printf("{\n");
  printf("  return(0);\n");
//  printf("  return(sizeof(image_328));\n");
  printf("}\n\n");
  printf("int binSize()\n");
  printf("{\n");
  printf("  int sz = sizeof(hexImage);\n");
  for (n=0; n < lineCount; n++)
  {
    printf("  sz += sizeof(__hexLineData_%d__);\n", n);
  }
  printf("  return(sz);\n");
  printf("}\n");
  return 0;
}

