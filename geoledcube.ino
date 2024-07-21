/*  *** Arduino script to drive the geoledcube ***
 *      
 *  https://github.com/a-t-1/geoledcube
 *   
 *  GNU General Public License V3
 *  
 *  
 *  6 32x32 RGB LED Panels form a cube, onto which the land/oceans are projected (green/blue)
 *  
 *  This code projects atmospheric water vapor (blue) and precipitation (red) from the NOAA NCEP/NCAR Reanalysis project: 
 *  https://www.esrl.noaa.gov/psd/data/gridded/data.ncep.reanalysis.html
 *  https://www.ready.noaa.gov/gbl_reanalysis.php
 *  https://www.cpc.ncep.noaa.gov/products/wesley/reanalysis.html
 *  
 *  Water vapor and precipitation data have been extracted from GBL files at the projected LED locations (in R)
 *  and converted single-byte values for every pixel for every 6-hour time step  
 *  Data are stored in binary files on SD card. One file for each 365 day year (ignoring leap years for now)
 *  Spatial resolution is about 3 degrees per LED at the equator
 *  
 *  
 * Hardware: 
 * - Teensy 3.5: https://www.pjrc.com/store/teensy35.html
 * - 8 GB mircoSD card: https://www.adafruit.com/product/1294
 * - SmartMatrix SmartLED Shield (V4) for Teensy 3: https://www.adafruit.com/product/1902, http://docs.pixelmatix.com/SmartMatrix/
 * - 6 RGB LED Panels - 32x32 (1:8 scan rate): https://www.sparkfun.com/products/retired/14633
 * (Alternative: https://www.sparkfun.com/products/14646 will require code changes but should make things easier.)
 * 
 * Libraries: 
 * - SmartLEDShieldV4: https://github.com/pixelmatix/SmartMatrix
 * - SdFat: https://github.com/greiman/SdFat
 * 
 */


#include "SdFat.h"

// Use built-in SD for SPI modes on Teensy 3.5/3.6.
// Teensy 4.0 use first SPI port.
// SDCARD_SS_PIN is defined for the built-in SD on some boards.
#ifndef SDCARD_SS_PIN
const uint8_t SD_CS_PIN = SS;
#else   // SDCARD_SS_PIN
// Assume built-in SD is used.
const uint8_t SD_CS_PIN = SDCARD_SS_PIN;
#endif  // SDCARD_SS_PIN

SdFs sdEx;

// Two files, one for water content and one for precipitation
FsFile file;
FsFile file2;

// 6144 B buffer will fit all 32x32x6 leds
const size_t BUF_DIM = 6144;

// 8,970,240 B file
// Each file contains data for one year: 365 days x 4 times/day * 6144 leds
// const uint32_t FILE_SIZE = 365UL * 4UL * BUF_DIM;


// Each has its own buffer
uint8_t buf[BUF_DIM];
uint8_t buf2[BUF_DIM];

size_t nb = 6144; // number of bytes per image

// template filenames for water content (derived from water vapor pressure atmosphere) and precipitation (total period precipitation)
char filename[14] = "wvpa_2016.bin";
char filename2[13] = "tpp_2016.bin";

void errorHalt(const char* msg) {
  sdEx.errorHalt(msg);
}

/*
    SmartMatrix Features Demo - Louis Beaudoin (Pixelmatix)
    This example code is released into the public domain
*/

#include <SmartLEDShieldV4.h>  // comment out this line for if you're not using SmartLED Shield V4 hardware (this line needs to be before #include <SmartMatrix3.h>)
#include <SmartMatrix3.h>

#define COLOR_DEPTH 24                  // known working: 24, 48 - If the sketch uses type `rgb24` directly, COLOR_DEPTH must be 24
const uint8_t kMatrixWidth = 32;        // known working: 32, 64, 96, 128
const uint8_t kMatrixHeight = 192;       // 6 panels x 32
const uint8_t kRefreshDepth = 24;       // known working: 24, 36, 48
const uint8_t kDmaBufferRows = 2;       // known working: 2-4, use 2 to save memory, more to keep from dropping frames and automatically lowering refresh rate
const uint8_t kPanelType = SMARTMATRIX_HUB75_16ROW_MOD8SCAN; // use SMARTMATRIX_HUB75_16ROW_MOD8SCAN for common 16x32 panels, or use SMARTMATRIX_HUB75_64ROW_MOD32SCAN for common 64x64 panels
// We send data to the panel as if it were a 16x32
// The second half of the 32x32 panel below first half (and 2nd - 6th panel below that)
// But we'll shift data around later because the 32x32 panels have a funny plot order
const uint8_t kMatrixOptions = (SMARTMATRIX_OPTIONS_NONE);      // see http://docs.pixelmatix.com/SmartMatrix for options
const uint8_t kBackgroundLayerOptions = (SM_BACKGROUND_OPTIONS_NONE);
const uint8_t kScrollingLayerOptions = (SM_SCROLLING_OPTIONS_NONE);
const uint8_t kIndexedLayerOptions = (SM_INDEXED_OPTIONS_NONE);

SMARTMATRIX_ALLOCATE_BUFFERS(matrix, kMatrixWidth, kMatrixHeight, kRefreshDepth, kDmaBufferRows, kPanelType, kMatrixOptions);
SMARTMATRIX_ALLOCATE_BACKGROUND_LAYER(backgroundLayer, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kBackgroundLayerOptions);

// Store land/ocean (1 bit) data for each of the 6144 leds in 6144/8=768 bytes
const byte worldbytes[768] =
{
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 254, 255, 255, 255, 252, 127, 248, 255, 240, 63, 192, 255, 241, 63, 128, 255, 255, 28, 128, 255, 255, 1, 128, 255, 255, 3, 0, 255, 255, 3, 0, 254, 255, 3, 0, 255, 255, 7, 0, 254, 255, 7, 0, 255, 255, 7, 0, 255, 255, 255, 129, 255, 255, 255, 131, 255, 255, 255, 193, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 231, 247,
  0, 0, 0, 220, 0, 0, 0, 184, 0, 0, 0, 248, 2, 0, 0, 248, 4, 0, 0, 216, 16, 0, 0, 248, 224, 0, 0, 252, 224, 1, 0, 255, 240, 131, 193, 255, 255, 195, 195, 255, 255, 243, 135, 239, 252, 247, 135, 239, 252, 247, 223, 239, 254, 239, 255, 253, 255, 255, 171, 252, 255, 255, 103, 244, 255, 255, 111, 114, 255, 255, 223, 87, 255, 255, 63, 255, 255, 255, 255, 239, 255, 255, 255, 255, 253, 255, 255, 255, 253, 255, 255, 15, 252, 255, 255, 7, 254, 255, 255, 0, 254, 255, 255, 0, 254, 255, 255, 1, 254, 255, 255, 1, 255, 255, 255, 1, 255, 255, 255, 7, 255, 255, 255, 127, 255, 255, 255, 255,
  252, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 248, 255, 255, 255, 113, 255, 255, 255, 233, 255, 255, 255, 255, 255, 255, 255, 251, 255, 255, 255, 243, 255, 255, 255, 240, 251, 255, 255, 224, 255, 255, 255, 192, 255, 255, 255, 192, 255, 255, 255, 128, 255, 255, 255, 192, 255, 255, 255, 192, 191, 255, 255, 224, 191, 255, 255, 251, 239, 255, 255,
  15, 0, 128, 255, 31, 0, 192, 255, 63, 0, 224, 255, 63, 0, 240, 255, 255, 0, 248, 255, 127, 193, 255, 255, 255, 224, 255, 255, 255, 227, 255, 255, 255, 227, 222, 255, 255, 15, 254, 255, 255, 127, 252, 255, 255, 255, 189, 255, 255, 255, 91, 248, 255, 255, 15, 224, 255, 255, 31, 192, 255, 255, 15, 192, 255, 255, 7, 0, 255, 255, 7, 0, 255, 255, 15, 0, 255, 255, 15, 0, 255, 255, 31, 0, 255, 255, 31, 0, 255, 255, 127, 0, 255, 255, 127, 0, 255, 255, 127, 0, 255, 255, 127, 0, 255, 255, 127, 0, 255, 255, 127, 128, 255, 255, 63, 128, 255, 255, 63, 224, 255, 255, 63, 240, 255, 255, 63, 252  ,
  255, 31, 110, 0, 255, 31, 127, 26, 255, 31, 255, 14, 255, 63, 240, 7, 255, 31, 64, 16, 255, 15, 0, 48, 255, 7, 0, 32, 255, 3, 0, 96, 255, 3, 0, 64, 255, 3, 0, 0, 255, 3, 0, 0, 255, 3, 0, 0, 255, 7, 0, 0, 255, 15, 0, 0, 255, 255, 7, 0, 255, 255, 7, 0, 255, 255, 7, 128, 252, 255, 15, 192, 248, 255, 31, 192, 240, 255, 31, 192, 248, 255, 31, 192, 252, 255, 15, 192, 252, 255, 15, 128, 252, 255, 31, 192, 252, 255, 31, 240, 254, 255, 31, 224, 254, 255, 63, 112, 255, 255, 63, 240, 255, 255, 127, 252, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255  ,
  96, 8, 255, 255, 120, 0, 255, 255, 32, 128, 254, 255, 0, 208, 252, 255, 0, 232, 254, 255, 0, 136, 255, 255, 0, 148, 255, 239, 0, 192, 243, 191, 0, 196, 127, 167, 0, 224, 7, 39, 0, 248, 135, 3, 0, 252, 195, 3, 0, 252, 97, 2, 0, 244, 49, 30, 0, 252, 55, 15, 0, 252, 171, 14, 0, 240, 191, 12, 0, 240, 255, 0, 0, 248, 31, 0, 0, 248, 95, 0, 0, 248, 63, 0, 0, 224, 31, 0, 0, 192, 3, 0, 0, 0, 3, 0, 0, 0, 2, 1, 128, 3, 199, 27, 128, 255, 255, 63, 192, 243, 255, 127, 224, 251, 255, 255, 255, 255, 254, 255, 239, 255, 255, 255, 251, 255, 255, 255
};

rgb24 pixelColor = {128, 128, 0};

// Reduce brightness to 25%. Limits current to just over 1A. Nicely visible in light and dark conditions.
const int defaultBrightness = (25 * 255) / 100; // dim: 25% brightness

unsigned long next = 0; // millis of next image

// the setup() method runs once, when the sketch starts
void setup() {
  Serial.begin(9600);
  //Connect to SD
  if (!sdEx.begin(SdioConfig(FIFO_SDIO))) {
    sdEx.initErrorHalt("SdFatSdioEX begin() failed");
  }
  sdEx.chvol();

  // Start matrix
  matrix.addLayer(&backgroundLayer);
  matrix.begin();
  matrix.setBrightness(defaultBrightness);
  next = millis() + 50;
}

// the loop() method runs over and over again,
// as long as the board has power
void loop() {
  // loop over 1994-2018 period
  // = 24*365*4 = 35040 images
  // 100 msec / image = 3504 seconds = 58 minutes
  // construct the filenames (yes, this can be done more efficiently)
  for (int y = 1994; y < 2018; y++) {
    filename[5] = '0' + y / 1000;
    filename[6] = '0' + (y / 100) % 10;
    filename[7] = '0' + (y / 10) % 10;
    filename[8] = '0' + (y % 10);

    filename2[4] = '0' + y / 1000;
    filename2[5] = '0' + (y / 100) % 10;
    filename2[6] = '0' + (y / 10) % 10;
    filename2[7] = '0' + (y % 10);
    // open files
    Serial.println(filename);
    if (!file.open(filename, O_RDWR | O_CREAT)) {
      errorHalt("open failed");
    }
    Serial.println(filename2);
    if (!file2.open(filename2, O_RDWR | O_CREAT)) {
      errorHalt("open failed");
    }
    // loop over all 365x4 images in one year (leap years are ignored)
    for (int h = 0; h < (365 * 4 - 1); h++) {


      // read 1st image
      if ((int)nb != file.read(buf, nb)) {
        errorHalt("read failed");
      }
      // read 2nd image
      //      Serial.println("read buf");
      if ((int)nb != file2.read(buf2, nb)) {
        errorHalt("read failed");
      }
      //      Serial.println("read buf2");

      // images are "manually" overlaid and reorganized to match the led panel plot order
      for (int panel = 0; panel < 6; panel++) { // loop over all panels
        for (int y = 0; y < 32; y++) { // loop over all rows
          for (int x = 0; x < 32; x++) { // loop over all leds in a row
            int pixel = x + y * 32; // pixel within panel
            byte worldbyte = worldbytes[panel * 128 + pixel / 8]; // extract land/ocean from worldbytes
            boolean land = (worldbyte >> (pixel % 8)) & B0000001; // land/ocean are flipped, oops
            uint8_t blue = min((land ? 24 : 0) + buf[panel * 1024 + pixel], 255); // blue is ocean + first variable (atmospheric water content)
            uint8_t green = (land ? 0 : 48) ; // green is land
            uint8_t red = min(buf2[panel * 1024 + pixel] * 3 , 192); // red is precipitation events, scalled and limited to 192/255

            // Next, we need to reorganize the data so it gets plotted in the right way
            // This was only necessary because I bought this (now retired) 1:8 scan rate 32x32 RGB LED Panel for $15 each
            // https://www.sparkfun.com/products/retired/14633
            // See also: https://www.sparkfun.com/sparkx/blog/2650
            /* The order of the data
              0  1 <- xBlock
              ______  yBlock  ____ iBlock
              A  B  | 0       0 1 |
              C  D  | 1       2 3 |
              E  F  | 2       4 5 |
              G  H  | 3       6 7 |

                The order data is plotted
              A' F'
              B' E'
              C' H'
              D' G'

              Data from block B needs to be plotted to block F'
              It needs to shift down two blocks (yShift=+2), but the horizontal position is good (xShift=0)
              Data from block D needs to be plotted to block E'
              It needs to shift down one block (yShift=+1), and left one block (xShift=-1)

            */

            int xShift[8] = { 0, 0, 1, -1, 0, 0, 1, -1};
            int yShift[8] = { 0, 2, -1, 1, -1, 1, -2, 0};

            int xBlock = x / 16;
            int yBlock = y / 8;
            int iBlock = xBlock  + yBlock * 2;

            int x0 = x + xShift[iBlock] * 16;
            int y0 = y + yShift[iBlock] * 8;


            y0 = y0 + panel * 32;

            


            backgroundLayer.drawPixel(x0, y0, {red, green, blue});
          } // for (int x...
        } // for (int y...
      } // for (int panel...

      while (millis() < next) {} // yes, there are more elegant ways
      next = millis() + 100;
      backgroundLayer.swapBuffers();

    } // for (int h...
    file.close();
    file2.close();
  } // for (int year...
} // loop()

