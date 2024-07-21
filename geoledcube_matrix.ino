/*  *** Arduino script to drive the geoledcube ***

    https://github.com/a-t-1/geoledcube

    GNU General Public License V3


    6 32x32 RGB LED Panels form a cube

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

rgb24 pixelColor = {128, 128, 0};

// Reduce brightness to 25%. Limits current to just over 1A. Nicely visible in light and dark conditions.
const int defaultBrightness = (100 * 255) / 100; // dim: 25% brightness

int green_array[128][33] = {};
long randNumber[16] = {};
int sloom = 200;
int seeds = 200;






// the setup() method runs once, when the sketch starts
void setup() {
  randomSeed(42);
  Serial.begin(9600);

  // Start matrix
  matrix.addLayer(&backgroundLayer);
  matrix.begin();
  matrix.setBrightness(defaultBrightness);

}

// the loop() method runs over and over again,
// as long as the board has power
void loop() {

  for (int d = 0; d < 32; d++) {
    randNumber[d] = random(seeds);
  }

  for (int a = 0; a < 128; a++) {
    green_array[a][0] = green_array[a][0] - random(4, 10);
    if (green_array[a][0] <= 0) {
      green_array[a][0] = 0;
    }
    if (randNumber[a] <= 10) {
      green_array[a][0] = 34;
    }

  }

    for (uint8_t x = 0; x < 32; x++) {
      for (uint8_t y = 0; y < 32; y++) {
        backgroundLayer.drawPixel(x, y, {10, 0, 10});
      }
    }

  for (int panel = 1; panel <= 4; panel++) {
    for (uint8_t x = 0; x < 32; x++) {
      for (uint8_t y = 0; y < 32; y++) {
        int xShift[8] = { 0, 0, 1, -1, 0, 0, 1, -1};
        int yShift[8] = { 0, 2, -1, 1, -1, 1, -2, 0};

        int xBlock = x / 16;
        int yBlock = y / 8;
        int iBlock = xBlock  + yBlock * 2;

        int x0 = x + xShift[iBlock] * 16;
        int y0 = y + yShift[iBlock] * 8;


        y0 = y0 + panel * 32;




        backgroundLayer.drawPixel(x0, y0, {10, green_array[y + 32 * (panel - 1)][x], 10});

      }
    }
  }
    for (uint8_t x = 0; x < 32; x++) {
      for (uint8_t y = 0; y < 32; y++) {
        backgroundLayer.drawPixel(x, y + 5*32, {10, 0, 10});
      }
    }
  backgroundLayer.swapBuffers();

  for (int c = 0; c < 128; c++) {
    for (int z = 32; z > 0; z--) {
      green_array[c][z] = green_array[c][z - 1];
    }
  }

  delay(sloom);
} // loop()

