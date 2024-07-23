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

byte life[6][34][34] = {0};
byte newlife[6][34][34] = {0};
byte age[6][34][34] = {255};
const int n = 6 * 32 * 32 * 3 / 4; // 75%

int panel = 0;
int x = 0;
int y = 0;

byte neighbors = 0;
boolean frozen = false;
int minchanges = 20;

int changehistory[6] = {0};


const int xShift[8] = { 0, 0, 1, -1, 0, 0, 1, -1};
const int yShift[8] = { 0, 2, -1, 1, -1, 1, -2, 0};

int msdelay = 500;

// the setup() method runs once, when the sketch starts
void setup() {
  randomSeed(42);


  Serial.begin(9600);

  // Start matrix
  matrix.addLayer(&backgroundLayer);
  matrix.begin();
  matrix.setBrightness(defaultBrightness);


  seed();
  lifecycle();
  lifecycle();
  lifecycle();
  reset_age();
  appear();
  delay(1000);

}

// the loop() method runs over and over again,
// as long as the board has power
void loop() {

  frozen = lifecycle();

  drawlife(255);
  if (msdelay > 100) {
    msdelay -= 10;
  }
  delay(msdelay);


  if (frozen) {
    fade();
    seed();
    lifecycle();
    lifecycle();
    lifecycle();
    reset_age();
    appear();
    msdelay = 500;
  }




} // loop()

void seed() {
  for (panel = 0; panel < 6; panel++) {
    for (x = 0; x < 32; x++) {
      for (y = 0; y < 32; y++) {
        life[panel][x + 1][y + 1] = 0;
        age[panel][x + 1][y + 1] = 255;
      }
    }
  }
  for (int i = 0; i < n; i++) {
    panel = random(0, 6);
    x = random(0, 32);
    y = random(0, 32);
    /*
      Serial.print("\t");
      Serial.print(panel);
      Serial.print("\t");
      Serial.print(x);
      Serial.print("\t");
      Serial.print(y);
    */
    life[panel][x + 1][y + 1] = 1;
    age[panel][x + 1][y + 1] = 0;
  }

}

void reset_age() {
  for (panel = 0; panel < 6; panel++) {
    for (x = 0; x < 32; x++) {
      for (y = 0; y < 32; y++) {
        age[panel][x + 1][y + 1] = 255 - 255 * life[panel][x + 1][y + 1];
      }
    }
  }

}

void fade() {
  for (uint8_t i = 255; i > 0; i--) {
    drawlife(i);
    delay(1);
  }

}
void appear() {
  for (uint8_t i = 1; i < 255; i++) {
    drawlife(i);
    delay(1);
  }
}
void drawlife(uint8_t i) {
  for (panel = 0; panel < 6; panel++) {
    for (x = 0; x < 32; x++) {
      for (y = 0; y < 32; y++) {
        int xBlock = x / 16;
        int yBlock = y / 8;
        int iBlock = xBlock  + yBlock * 2;

        int x0 = x + xShift[iBlock] * 16;
        int y0 = y + yShift[iBlock] * 8;
        y0 = y0 + panel * 32;
        if (life[panel][x + 1][y + 1] == 1) {
          backgroundLayer.drawPixel(x0, y0, {i, i - min(age[panel][x + 1][y + 1], i), i - min(age[panel][x + 1][y + 1], i)});
        } else {
          backgroundLayer.drawPixel(x0, y0, {0, 0, i - min(age[panel][x + 1][y + 1], i)});
        }
      }
    }
  }
  backgroundLayer.swapBuffers();
  backgroundLayer.fillScreen({0, 0, 0});
}







boolean lifecycle() {
  for (panel = 0; panel < 6; panel++) {

    for (x = 0; x < 32; x++) {
      for (y = 0; y < 32; y++) {

        neighbors = 0;
        //        if (x > 0) {
        //          if (y > 0) {
        neighbors += life[panel][x - 1 + 1][y - 1 + 1];
        //          }
        neighbors += life[panel][x - 1 + 1][y + 1];
        //          if (y < 31) {
        neighbors += life[panel][x - 1 + 1][y + 1 + 1];
        //          }
        //        }
        //        if (y > 0) {
        neighbors += life[panel][x + 1][y - 1 + 1];
        //        }
        // neighbors += life[panel][x+1][y+1];
        //        if (y < 31) {
        neighbors += life[panel][x + 1][y + 1 + 1];
        //        }
        //        if (x < 31) {
        //          if (y > 0) {
        neighbors += life[panel][x + 1 + 1][y - 1 + 1];
        //          }
        neighbors += life[panel][x + 1 + 1][y + 1];
        //          if (y < 31) {
        neighbors += life[panel][x + 1 + 1][y + 1 + 1];
        //          }
        //        }
        if (age[panel][x + 1][y + 1] < 255) {
          age[panel][x + 1][y + 1]++;
        }
        if (life[panel][x + 1][y + 1]) {
          if ((neighbors < 2) | (neighbors > 3)) {
            newlife[panel][x + 1][y + 1] = 0;
            age[panel][x + 1][y + 1] = 0;
          }
        } else {
          if (neighbors == 3) {
            newlife[panel][x + 1][y + 1] = 1;
            age[panel][x + 1][y + 1] = 0;
          }
        }
      }
    }
  }
  boolean frozen = true;
  for (int i = 5; i > 0; i--) {
    changehistory[i] = changehistory[i - 1];
  }
  int changes = 0;
  for (panel = 0; panel < 6; panel++) {
    for (x = 0; x < 32; x++) {
      for (y = 0; y < 32; y++) {
        if (life[panel][x + 1][y + 1] != newlife[panel][x + 1][y + 1]) {
          changes++;
        };
        life[panel][x + 1][y + 1] = newlife[panel][x + 1][y + 1];
      }
    }
  }
  changehistory[0] = changes;
  Serial.println(
    (changehistory[0] == changehistory[3]) &
    (changehistory[1] == changehistory[4]) &
    (changehistory[2] == changehistory[5])
  );

  frozen = (changehistory[0] == changehistory[3]) &
           (changehistory[1] == changehistory[4]) &
           (changehistory[2] == changehistory[5]);

  panel = 0;
  for (y = 0; y < 32; y++) {
    life[panel][0][y + 1] = life[3][y + 1][32];
    life[panel][33][y + 1] = life[1][32 - y][32];
  }
  for (x = 0; x < 32; x++) {
    life[panel][x + 1][0] = life[4][x + 1][32];
    life[panel][x + 1][33] = life[2][32 - x][32];
  }

  panel = 1;
  for (y = 0; y < 32; y++) {
    life[panel][0][y + 1] = life[4][32][y + 1];
    life[panel][33][y + 1] = life[2][1][y + 1];
  }
  for (x = 0; x < 32; x++) {
    life[panel][x + 1][0] = life[5][1][x + 1];
    life[panel][x + 1][33] = life[0][32][x + 1];
  }
  panel = 2;
  for (y = 0; y < 32; y++) {
    life[panel][0][y + 1] = life[1][32][y + 1];
    life[panel][33][y + 1] = life[3][1][y + 1];
  }
  for (x = 0; x < 32; x++) {
    life[panel][x + 1][0] = life[5][x + 1][32];
    life[panel][x + 1][33] = life[0][32 - x][32];
  }
  panel = 3;
  for (y = 0; y < 32; y++) {
    life[panel][0][y + 1] = life[2][32][y + 1];
    life[panel][33][y + 1] = life[4][1][y + 1];
  }
  for (x = 0; x < 32; x++) {
    life[panel][x + 1][0] = life[5][32][32 - x];
    life[panel][x + 1][33] = life[0][1][32 - x];
  }
  panel = 4;
  for (y = 0; y < 32; y++) {
    life[panel][0][y + 1] = life[3][32][y + 1];
    life[panel][33][y + 1] = life[1][1][y + 1];
  }
  for (x = 0; x < 32; x++) {
    life[panel][x + 1][0] = life[5][32 - x][1];
    life[panel][x + 1][33] = life[0][x + 1][1];
  }
  panel = 5;
  for (y = 0; y < 32; y++) {
    life[panel][0][y + 1] = life[1][y + 1][1];
    life[panel][33][y + 1] = life[3][32 - y][1];
  }
  for (x = 0; x < 32; x++) {
    life[panel][x + 1][0] = life[4][32 - x][1];
    life[panel][x + 1][33] = life[2][x + 1][1];
  }

  return (frozen);
}


