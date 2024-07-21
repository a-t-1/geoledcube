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

unsigned long next = 0; // millis of next image

// Reduce brightness to 25%. Limits current to just over 1A. Nicely visible in light and dark conditions.
const int defaultBrightness = (4 * 255) / 100; // dim: 25% brightness

const int n = 256;
int column[n] = {0};
int row[n] = {0};
int fall[n] = {1};
int tail[n] = {4};
int r[n] = {0};
int g[n] = {0};
int b[n] = {0};

const int xShift[8] = { 0, 0, 1, -1, 0, 0, 1, -1};
const int yShift[8] = { 0, 2, -1, 1, -1, 1, -2, 0};


// the setup() method runs once, when the sketch starts
void setup() {
  randomSeed(42);

  for (int i = 0; i < n; i++) {
      column[i] = random(127);
      row[i] = 0;
      fall[i] = random(2, 4) * 5;
      tail[i] = random(4, 32);
      r[i] = random(128, 255);
      g[i] = random(128, 255);
      b[i] = random(128, 255);
  }


  Serial.begin(9600);

  // Start matrix
  matrix.addLayer(&backgroundLayer);
  matrix.begin();
  matrix.setBrightness(defaultBrightness);
  next = millis() + 50;

}

// the loop() method runs over and over again,
// as long as the board has power
void loop() {


  for (int i = 0; i < n; i++) {
    int panel = 1 + (column[i] / 32);
    int x = column[i] % 32;

    for (int j = 0; j < tail[i]; j++) {

      int y = (row[i] / 10) - j;
      if ((y >= 0) && (y < 32)) {
        int xBlock = x / 16;
        int yBlock = y / 8;
        int iBlock = xBlock  + yBlock * 2;

        int x0 = x + xShift[iBlock] * 16;
        int y0 = y + yShift[iBlock] * 8;

        uint8_t red = max(0, r[i] - j * r[i]/ tail[i] - (j%2)*32);
        uint8_t green = max(0, g[i] - j * g[i]/ tail[i] - (j%2)*32);
        uint8_t blue = max(0, b[i] - j * b[i]/ tail[i] - (j%2)*32);
        //uint8_t green = max(0, 255 - j * 255 / tail[i]);


        y0 = y0 + panel * 32;
        backgroundLayer.drawPixel(x0, y0, {red, green, blue});
      }
    }

    row[i] = row[i] + fall[i];
    if (row[i] / 10 - tail[i] > 32) {
      column[i] = random(127);
      row[i] = 0;
      fall[i] = random(2, 4) * 4;
      tail[i] = random(4, 32);
      r[i] = random(16, 255);
      g[i] = random(16, 255);
      b[i] = random(16, 255);
    }

  }
  backgroundLayer.swapBuffers();
  backgroundLayer.fillScreen({0, 0, 0});

  delay(100);
} // loop()

