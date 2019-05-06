Arduino Programmer Library
=====================================

Arduino library to facilitate programming another AVR with arbitrary code.

I offer no support for this code, I use it in various forms in a private project.

HexData upload was never implemented, because it's just too inefficient to store HexData.

The best way is to upload your desired code to a target chip normally using a normal programmer, as you would normally, and then connect said target to your new uploader you are making and use the "ripFlashToPagedBinData" method of the library.

## Wiring

| Programmer | Target |
| ---------- | ------ |
| MOSI       | MOSI   |
| MISO       | MISO   |
| SCK        | SCK   |
| 10         | RESET   |
| VCC        | VCC   |
| GND        | GND   |


## Example Of Ripping

    #include <ArduinoProgrammer.h>
    
    // Ripping example
    //  the connected target already has the code you want uploaded (not locked)
    //  we will rip it and print the code to upload in the Serial console 
    //  you can then copy and pase it into your programming sketch
    
    ArduinoProgrammer MyProgrammer;

    void setup()
    {
        Serial.begin(57600);

        // Start programming mode
        MyProgrammer.begin();

        // Get details of the chip connected to us (the target)
        ArduinoProgrammer::ChipData TargetChip = MyProgrammer.getStandardChipData();
        
        // Rip code
        uint8_t result = MyProgrammer.ripFlashToPagedBinData(TargetChip, "MyImage");

        // End programming
        MyProgrammer.end();

        // The serial terminal will print out the dump of the target
        //  copy and paste that into the upload
    }

    void loop() { }

## Example Of Uploading

    #include <ArduinoProgrammer.h>

    ArduinoProgrammer MyProgrammer;


      // "MyImage" - the data you intend to upload to the chip
      // --------------------------------------------------------------------------
      // The below image is genertaed by "ripFlashToPagedBinData" in the ripping 
      // example, you copy the output from "ripFlashToPagedBinData and paste here.
      // --------------------------------------------------------------------------
      // --------------------------------------------------------------------------
      // --------------------------------------------------------------------------

            const byte MyImagePage000[128] PROGMEM = {
              0x0c, 0x94, 0x87, 0x00, 0x0c, 0x94, 0xe1, 0x08, 0x0c, 0x94, 0xba, 0x08, 0x0c, 0x94, 0xaf, 0x00, 
              // ...
            };

            // ... and a whole crap load of more data not shown in this example....

            ArduinoProgrammer::PagedBinData MyImage = {
              "ripped",
              0x0000,
              128,
              256,
            MyImagePages };

      // --------------------------------------------------------------------------
      // --------------------------------------------------------------------------
      // --------------------------------------------------------------------------

    void setup()
    {
        Serial.begin(57600);

        // Start programming mode
        MyProgrammer.begin();

        // Get details of the chip connected to us (the target)
        ArduinoProgrammer::ChipData TargetChip = MyProgrammer.getStandardChipData();
        
        // Upload code
        uint8_t result = MyProgrammer.uploadFromProgmem(TargetChip, MyImage);

        if(result == 0)
        {
          // It worked
          Serial.println("Upload Finished");
        }  
        else
        {
          Serial.println("Upload Failed");
        }  
    }
          
    void loop() { }
