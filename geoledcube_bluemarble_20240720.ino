/*  *** Arduino script to drive the geoledcube ***

    https://github.com/a-t-1/geoledcube

    GNU General Public License V3


    6 32x32 RGB LED Panels form a cube, onto which the land/oceans are projected (green/blue)

    This code projects atmospheric water vapor (blue) and precipitation (red) from the NOAA NCEP/NCAR Reanalysis project:
    https://www.esrl.noaa.gov/psd/data/gridded/data.ncep.reanalysis.html
    https://www.ready.noaa.gov/gbl_reanalysis.php
    https://www.cpc.ncep.noaa.gov/products/wesley/reanalysis.html

    Water vapor and precipitation data have been extracted from GBL files at the projected LED locations (in R)
    and converted single-byte values for every pixel for every 6-hour time step
    Data are stored in binary files on SD card. One file for each 365 day year (ignoring leap years for now)
    Spatial resolution is about 3 degrees per LED at the equator


   Hardware:
   - Teensy 3.5: https://www.pjrc.com/store/teensy35.html
   - 8 GB mircoSD card: https://www.adafruit.com/product/1294
   - SmartMatrix SmartLED Shield (V4) for Teensy 3: https://www.adafruit.com/product/1902, http://docs.pixelmatix.com/SmartMatrix/
   - 6 RGB LED Panels - 32x32 (1:8 scan rate): https://www.sparkfun.com/products/retired/14633
   (Alternative: https://www.sparkfun.com/products/14646 will require code changes but should make things easier.)

   Libraries:
   - SmartLEDShieldV4: https://github.com/pixelmatix/SmartMatrix
   - SdFat: https://github.com/greiman/SdFat

*/


// Reduce brightness to 25%. Limits current to just over 1A. Nicely visible in light and dark conditions.
const int defaultBrightness = (5 * 255) / 100;  // dim: 25% brightness

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

// Files for blue marble, bathymetry, water content and precipitation
FsFile file0;
FsFile file1;
FsFile file2;
FsFile file3;

// 6144 B buffer will fit all 32x32x6 leds
const size_t BUF_DIM = 6144;
const size_t BUF_DIM3 = 18432;

// 8,970,240 B file (too big)
// Each file contains data for one year: 365 days x 4 times/day * 6144 leds
// const uint32_t FILE_SIZE = BUF_DIM;
// const uint32_t FILE_SIZE = 365UL * 4UL * BUF_DIM;


// Blue marble and bathymetry buffers are 6144*3 (rgb)
// The background blue marble image changes monthly
// Two images are kept in memory to gradually transition from one month to the next
uint8_t bm0[BUF_DIM3];
uint8_t bm1[BUF_DIM3];
uint8_t bath[BUF_DIM3];
// Water content and precipitation buffers are 6144
uint8_t wvpa[BUF_DIM];
uint8_t tpp[BUF_DIM];

// To efficiently transition from one month to the next, we keep track if the month number is even or odd
// Even months are stored in bm0, odd months are stored in bm1
// When stepping to the next month, we only read the next month into the buffer we no longer need
boolean even = false;

size_t nb = 6144;    // number of bytes per image
size_t nb3 = 18432;  // number of bytes per image

// template filenames for water content (derived from water vapor pressure atmosphere) and precipitation (total period precipitation)
char filename0[9] = "bm01.bin";        // monthly image of blue marble (single image, 3 bytes (rgb))
char filename1[9] = "bath.bin";        // image of bathymetry
char filename2[14] = "wvpa_2016.bin";  // gridded water vapor pressure integrated over the atmosphere (4x365 images, 1 byte)
char filename3[13] = "tpp_2016.bin";   // gridded total precipitation (4x365 images, 1 byte)

void errorHalt(const char* msg) {
  sdEx.errorHalt(msg);
}

/*
    SmartMatrix Features Demo - Louis Beaudoin (Pixelmatix)
    This example code is released into the public domain
*/

#include <SmartLEDShieldV4.h>  // comment out this line for if you're not using SmartLED Shield V4 hardware (this line needs to be before #include <SmartMatrix3.h>)
#include <SmartMatrix3.h>

#define COLOR_DEPTH 24                                        // known working: 24, 48 - If the sketch uses type `rgb24` directly, COLOR_DEPTH must be 24
const uint8_t kMatrixWidth = 32;                              // known working: 32, 64, 96, 128
const uint8_t kMatrixHeight = 192;                            // 6 panels x 32
const uint8_t kRefreshDepth = 24;                             // known working: 24, 36, 48
const uint8_t kDmaBufferRows = 2;                             // known working: 2-4, use 2 to save memory, more to keep from dropping frames and automatically lowering refresh rate
const uint8_t kPanelType = SMARTMATRIX_HUB75_16ROW_MOD8SCAN;  // use SMARTMATRIX_HUB75_16ROW_MOD8SCAN for common 16x32 panels, or use SMARTMATRIX_HUB75_64ROW_MOD32SCAN for common 64x64 panels
// We send data to the panel as if it were a 16x32
// The second half of the 32x32 panel below first half (and 2nd - 6th panel below that)
// But we'll shift data around later because the 32x32 panels have a funny plot order
const uint8_t kMatrixOptions = (SMARTMATRIX_OPTIONS_NONE);  // see http://docs.pixelmatix.com/SmartMatrix for options
const uint8_t kBackgroundLayerOptions = (SM_BACKGROUND_OPTIONS_NONE);
const uint8_t kScrollingLayerOptions = (SM_SCROLLING_OPTIONS_NONE);
const uint8_t kIndexedLayerOptions = (SM_INDEXED_OPTIONS_NONE);

SMARTMATRIX_ALLOCATE_BUFFERS(matrix, kMatrixWidth, kMatrixHeight, kRefreshDepth, kDmaBufferRows, kPanelType, kMatrixOptions);
SMARTMATRIX_ALLOCATE_BACKGROUND_LAYER(backgroundLayer, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kBackgroundLayerOptions);

unsigned long next = 0;  // millis of next image

int h = 0;
int panel = 0;
int y = 0;
int x = 0;
int pixel = 0;
int p = 0;
int p3 = 0;
int m = 0;
int m_ = 0;
int h_ = 0;

// uint8_t for final rgb values
uint8_t red = 0;
uint8_t green = 0;
uint8_t blue = 0;

// int for mixing colors
int red0 = 0;
int green0 = 0;
int blue0 = 0;

int red1 = 0;
int green1 = 0;
int blue1 = 0;

int red2 = 0;
int green2 = 0;
int blue2 = 0;


int xShift[8] = { 0, 0, 1, -1, 0, 0, 1, -1 };
int yShift[8] = { 0, 2, -1, 1, -1, 1, -2, 0 };
int xBlock = 0;
int yBlock = 0;
int iBlock = 0;

int xp = 0;
int yp = 0;



// the setup() method runs once, when the sketch starts
void setup() {
  Serial.begin(9600);
  //Connect to SD
  if (!sdEx.begin(SdioConfig(FIFO_SDIO))) {
    sdEx.initErrorHalt("sdEx begin() failed");
  }
  sdEx.chvol();

  if (!file0.open(filename0, O_RDWR | O_CREAT)) {
    errorHalt("file0 open failed");
  }
  if (!file1.open(filename1, O_RDWR | O_CREAT)) {
    errorHalt("file1 open failed");
  }
  if ((int)nb3 != file0.read(bm0, nb3)) {
    errorHalt("file 0 read failed");
  }

  if ((int)nb3 != file1.read(bath, nb3)) {
    errorHalt("file 1 read failed");
  }
  file0.close();
  file1.close();

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
  for (int yy = 1994; yy < 2018; yy++) {
    // wvpa file
    filename2[5] = '0' + yy / 1000;
    filename2[6] = '0' + (yy / 100) % 10;
    filename2[7] = '0' + (yy / 10) % 10;
    filename2[8] = '0' + (yy % 10);
    // tpp file
    filename3[4] = '0' + yy / 1000;
    filename3[5] = '0' + (yy / 100) % 10;
    filename3[6] = '0' + (yy / 10) % 10;
    filename3[7] = '0' + (yy % 10);

    // open files
    Serial.println(filename2);
    if (!file2.open(filename2, O_RDWR | O_CREAT)) {
      errorHalt("open failed");
    }
    // Serial.println(filename2);
    if (!file3.open(filename3, O_RDWR | O_CREAT)) {
      errorHalt("open failed");
    }
    // loop over all 365x4 images in one year (leap years are ignored)
    for (int h = 0; h < (365 * 4 - 1); h++) {

      // read images

      // read 1st image: wvpa
      if ((int)nb != file2.read(wvpa, nb)) {
        errorHalt("read failed");
      }
      // read 2nd image: tpp
      if ((int)nb != file3.read(tpp, nb)) {
        errorHalt("read failed");
      }

      // START Blue marble simulations 
      // keep track of months
      Serial.print("\t");
      Serial.print(h);  // count of the 6-hour within the year
      h_ = h % (4 * 31);
      Serial.print("\t");
      Serial.print(h_);  // count of the 6-hour within the month
      m = h / (4 * 31) + 1;
      Serial.print("\t");
      Serial.print(m);  // month (used for sea-ice visualization)
      Serial.println("");
      if (m != m_) {  // if it's a new month
        m_ = m;
        even = m % 2;
        filename0[2] = '0' + (m / 10) % 10;
        filename0[3] = '0' + (m % 10);

        // open files
        //    Serial.println(filename);
        if (!file0.open(filename0, O_RDWR | O_CREAT)) {
          errorHalt("open failed");
        }

        // Even months are stored in bm0, odd months are stored in bm1
        // When stepping to the next month, we only read the next month into the buffer we no longer need
        if (even) {
          if ((int)nb3 != file0.read(bm0, nb3)) {
            errorHalt("read failed");
          }
        } else {
          if ((int)nb3 != file0.read(bm1, nb3)) {
            errorHalt("read failed");
          }
        }

        file0.close();
      }

      // END Blue marble simulations 

      // images are "manually" overlaid and reorganized to match the led panel plot order
      for (panel = 0; panel < 6; panel++) {  // loop over all panels
        for (y = 0; y < 32; y++) {           // loop over all rows
          for (x = 0; x < 32; x++) {         // loop over all leds in a row
            //            int pixel = x + y * 32; // pixel within panel
            p = (x + y * 32 + panel * 1024);  // pixel within panel
            p3 = p * 3;                       // byte within panel
            // rgb0 and rgb1 are for the blue marble colors
            red0 = int(bm0[p3]);
            green0 = int(bm0[p3 + 1]);
            blue0 = int(bm0[p3 + 2]);
            red1 = int(bm1[p3]);
            green1 = int(bm1[p3 + 1]);
            blue1 = int(bm1[p3 + 2]);
            // red2   = int( bath[p3    ]);
            // green2 = int( bath[p3 + 1]);
            // blue2  = int( bath[p3 + 2]);
            
            // rgb2 is for the wvpa (blueish white clouds) 
            red2 = int(wvpa[p])/2;    // blue is ocean + first variable (atmospheric water content)
            green2 = int(wvpa[p])/2;  // green is land
            blue2 = int(wvpa[p]);   // red is precipitation events, scaled and limited to 192/255

            // add the colors
            if (even) {
              red = min(red0 * h_ / 124 + red1 * (124 - h_) / 124 + red2, 255);          // red
              green = min(green0 * h_ / 124 + green1 * (124 - h_) / 124 + green2, 255);  // green
              blue = min(blue0 * h_ / 124 + blue1 * (124 - h_) / 124 + blue2, 255);          // blue
            } else {
              red = min(red1 * h_ / 124 + red0 * (124 - h_) / 124 + red2, 255);          // red
              green = min(green1 * h_ / 124 + green0 * (124 - h_) / 124 + green2, 255);  // green
              blue = min(blue1 * h_ / 124 + blue0 * (124 - h_) / 124 + blue2, 255);          // blue
            }
            /*
              red   = min(red0   + red2  /2, 255);
              green = min(green0 + green2/2, 255);
              blue  = min(blue0  + blue2 /1, 255);
            */





            xBlock = x / 16;
            yBlock = y / 8;
            iBlock = xBlock + yBlock * 2;
            xp = x + xShift[iBlock] * 16;
            yp = y + yShift[iBlock] * 8;
            yp = yp + panel * 32;

            backgroundLayer.drawPixel(xp, yp, { red, green, blue });
          }  // for (int x...
        }    // for (int y...
      }      // for (int panel...

      while (millis() < next) {}  // yes, there are more elegant ways
      next = millis() + 100;
      backgroundLayer.swapBuffers();
    }  // for (int h...
    file2.close();
    file3.close();

  }  // for (int m...
}  // loop()
