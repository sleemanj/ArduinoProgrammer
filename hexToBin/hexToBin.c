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
  size_t totalLength;
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

  totalLength = 0;
  for (n=0; n < lineCount; n++)
  {
    totalLength += hexData[n].length;
  }

  printf("\nHEX_IMAGE PROGMEM hexImage =\n{\n");
  printf("  \"%s\",        // image_name\n", argv[1]);
  printf("  \"atmega328p\",           // image_chipname\n");
  printf("  0x950F,                 // image_chipsig for 328P\n");
  
  printf("  //  FUSE NOTE ! \n")
  printf("  //  A given fuse byte is only actually changed if the fuse is NON ZERO \n")
  printf("  //  Notice that the post program fuses (second line) below have 0x00 in the L/H/E\n");
  printf("  //  So that means the L/H/E settings from the pre program settings will be retained.\n")
  printf("  //  The lock fuse however will be changed to 0x0F in post program\n")
  printf("  //  So the actual settings for L/H/E are 0xFF 0xDA 0x05 which in Arduino Land (see boards.txt)\n")
  printf("  //  is 'Arduino Duemilanove w/ ATmega328' (16Mhz Crystal/Oscillator)\n")
  printf("  //  the fuse mask is used in verification to remove unused bits from the comparison\n")
  
  
  printf("  {0x3F, 0xFF, 0xDA, 0x05},  // pre program fuses (prot/lock, low, high, ext)\n");
  printf("  {0x0F, 0x00, 0x00, 0x00},  // post program fuses, 0x00 means 'same as pre program'\n");
  printf("  {0x3F, 0xFF, 0xFF, 0x07},  // fuse mask, this is used in verifying (I think if you didn't use 0x00 it would not be necessary!)\n");
  printf("  32768,                     // size of chip flash in bytes\n");
  printf("  128,                       // size in bytes of flash page\n");
  printf("  0x%04X,                    // image base address\n", hexData[0].address);
  printf("  %ld,                        // image size\n", totalLength);
  printf("  {\n    ");

  for (n=0; n < lineCount; n++)
  {
    for (int i=0; i < hexData[n].length; i++)
    {
      if (i) printf(", "); else if (n) printf(",\n    ");
      printf("0x%02X", hexData[n].data[i]);
    }
  }

  printf("\n");
  printf("  }\n");
  printf("};\n\n");

  printf("HEX_IMAGE *hexImages[] = {\n");
  printf(" &hexImage,\n");
  printf("};\n");

  printf("uint8_t NUM_HEX_IMAGES = sizeof(hexImages)/sizeof(hexImages[0]);\n");
  return 0;
}

