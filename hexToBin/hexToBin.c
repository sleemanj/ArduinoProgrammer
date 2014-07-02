#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <hexData.h>


#define MAX_LINE 256

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
  printf("#include \"Arduino.h\"\n");
  printf("#else\n");
  printf("#include \"WProgram.h\"\n");
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

  printf("\nHEX_PTR PROGMEM hexData[] =\n{\n");

  for (n=0; n < lineCount; n++)
  {
    if (n) printf(",\n");

    printf("  {0x%02X, 0x%04X, 0x%02X, 0x%02X, __hexLineData_%d__",
           hexData[n].length, hexData[n].address, hexData[n].type, hexData[n].checksum, n);

    printf("}");
  }

  printf("\n};\n\n");
  return 0;
}

