# geoledcube
a led cube for visualizing Earth and atmospheric processes



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
