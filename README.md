Standalone-Arduino-AVR-ISP-programmer
=====================================

A standalone programmer for mass-programming AVR chips.

This fork uses a preprocessing step to convert the desired compiled sketch
into a compact C structure thus allowing the programmer to handle much
larger sketches than the original Adafruit version.

The Adafruit version of this project [included as a string](https://github.com/adafruit/Standalone-Arduino-AVR-ISP-programmer/blob/master/images.cpp) the compiled
sketch to be programmed into target devices. Each byte of the sketch was
represented as two characters (hex digits), thus making the string at least
twice as big as the number of bytes in the compiled sketch it represented.
This greatly limited the size of the sketches that could be uploaded by
this programmer.

No big deal if the sketch you want the programmer to load is a bootloader,
since those are small, but the size limitation can be a problem if you wish
to load something other than a bootloader.

Usage
=====

1. In the Arduino IDE, go to Tools->Board and select the type of board that
you will be programming with the standalone programmer. During compilation,
the IDE uses this setting to define various constants for things like
processor type and clock speed. If you compile for a board type different
than the target board, problems can occur at runtime. For example, if you
compile for a board type of "Arduino UNO", but then load the code
onto an Arduino Pro running at 8MHz, print statements will not work properly
because the serial port's settings will be based on the UNO's 16MHz clock.

2. Compile the sketch you want the programmer to load. The Arduino IDE will
compile the sketch in a temporary directory, generating a file with a .hex
extension.

3. Find the hex file. In the Arduino IDE, go to Arduino->Preferences and then
select "Show verbose output during compilation". During compilation, the path
to the hex file (and lots of other stuff) will be displayed.

4. Copy the hex file into another directory for safekeeping. The original is
a temporary file and may be automatically deleted without warning.

5. Edit the Makefile in the hexToBin directory of this project, replacing the
existing hex filename with the name of your hex file.

6. From a terminal window, cd to the hexToBin directory and type "make". This
will generate a hexData.c file in the main project directory. This file
contains a C struct representing your hex image.

7. In the Arduino IDE, go to Tools->Board and select the type of board that
you will be using as the standalone programmer.

8. Compile and upload to the standalone programmer.

9. Your standalone programmer is loaded and ready to go.
