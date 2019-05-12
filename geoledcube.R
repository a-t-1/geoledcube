# R code to convert NOAA NCEP/NCAR Reanalysis .gbl files to binary input for geoledcube
# *  https://www.esrl.noaa.gov/psd/data/gridded/data.ncep.reanalysis.html
# *  https://www.ready.noaa.gov/gbl_reanalysis.php
# *  https://www.cpc.ncep.noaa.gov/products/wesley/reanalysis.html

# *  Water vapor and precipitation data are extracted from GBL files at the projected LED locations
# *  and converted single-byte values for every pixel for every 6-hour time step  
# *  Data are stored in binary files on SD card. One file for each 365 day year (ignoring leap years for now)
# *  Spatial resolution is about 3 degrees per LED at the equator


path <- "c:\\gbl"
setwd(path)


# Calculate the led positions on the globe
# One side is calculated first, and then rotated 
# Leds are evenly spaced around the equator
# and the center meridians of one side
# 32x32 leds per panel
res <- 16
lonseq <- seq(0.5-res, res-0.5)/res*45
latseq <- -seq(0.5-res, res-0.5)/res*45

# These sequences are projected onto the side of the cube
yseq <- tan(lonseq/180*pi)
zseq <- tan(latseq/180*pi)

# And gridded
xyz <- expand.grid(x=1, y=yseq, z=zseq)

plot(xyz$y, xyz$z, xaxs="i", yaxs="i", xlim=c(-1,1), ylim=c(-1,1), asp=1)
rect(-1,-1,1,1)
# Points are not evenly spaced because the edges are further from the core

# Cartesian coordinates are converted back to latitude/longitude
r <- sqrt(apply(xyz^2, 1, sum))
lon <- atan2(xyz$y/r, xyz$x/r)*180/pi
lat <- asin(xyz$z/r)*180/pi
lon0 <- lon
lat0 <- lat

# The cartesian coordinates will be rotated
xyzm <- as.matrix(xyz)

# Short functions to make the rotations happen
Ax <- function(q) {
  q <- q/180*pi
  return(matrix(c(1, 0, 0, 0, cos(q), -sin(q), 0, sin(q), cos(q)), c(3,3), byrow=T))
}
Ay <- function(q) {
  q <- q/180*pi
  return(matrix(c(cos(q), 0, sin(q), 0, 1, 0, -sin(q), 0, cos(q)), c(3,3), byrow=T))
}
Az <- function(q) {
  q <- q/180*pi
  return(matrix(c(cos(q), -sin(q), 0, sin(q), cos(q), 0, 0, 0, 1), c(3,3), byrow=T))
}


# These are the rotations for the 6 panels
As <- list(Ay(-90), Az(-90), Az(-180), Az(90), Az(0), Az(180)%*%Ay(-90))

# They result in this pattern:
#                   -----
#  90 -             | 6 | 
#       -----------------
#   0 - | 4 | 5 | 2 | 3 |
#       -----------------
# -90 -     | 1 | 
#           -----
#         |   |   |   |   |
#       -90   0  90  180 270 longitude

# This is exactly how the panels are hooked up
# Note that panel 1 is the last panel in the chain, 
# and the SmartLed shield is hooked up to panel 6


plot(0,0, xlim=c(-150,250), ylim=c(-150,150))
dlon <- c(  0,  90,180, -90,   0,180)
dlat <- c(-90,   0,  0,   0,   0, 90)

# Initialize variables to store coordinates
cubeglobe <- matrix(NA, nrow=6*(res*2)^2, ncol=4)
alllat <- numeric(0)
alllon <- numeric(0)
landtiles <- array(NA, c(length(lat),6))


for (i in 1:6) {
  # select rotation matrix for panel i
  A <- As[[i]] 
  # rotate the panel coordinates
  xyzm_tile <- xyzm %*% A
  # store the coordinates
  cubeglobe[(1:1024)+(i-1)*1024, 1:3] <- xyzm_tile
  # calculate the distance from the core
  r <- sqrt(apply(xyzm_tile^2, 1, sum))
  # calculate longitude/latitude based on positions
  lon <- atan2(xyzm_tile[,2]/r, xyzm_tile[,1]/r)*180/pi
  lat <- asin(xyzm_tile[,3]/r)*180/pi
  # store
  alllat <- c(alllat, lat)
  alllon <- c(alllon, lon)
  points(lon0+dlon[i], lat0+dlat[i], pch=19, cex=.5, col=rainbow(6)[i])
  text(dlon[i], dlat[i], i, font=2, adj=c(0.5,0.5), cex=3)
}

# now let's plot everything in rainbow colors
plot(alllat~alllon, col=rainbow(6144))




require(rgdal)
world <- readGDAL("LMTGSH0a.tif")
image(world)
axis(1)
axis(2)

column <- floor((alllon+180)*5)+1
row <- floor((90-alllat)*5)+1
range(column)
range(row)
worldpixel <- row*1800 + column
range(worldpixel)
dim(world)
land <- (world$band1[worldpixel]>3)
range(land)
plot(alllon, alllat, pch=19, col=c("green", "blue")[land+1], cex=.5, asp=1)

# Convert "land" (6144 true/false) into 6144/8=768 bytes
# Rearrange into 768x8 matrix
landbytes <- matrix(land, 6*(res*2)^2/8,8, byrow=T)
r <- col(landbytes)-1
r <- 2^r
# Multiply by 2^column
landbytes <- landbytes*r
# Sum over rows
landbytes <- apply(landbytes, 1, sum)
# Convert to one line per panel and write to txt file
write.table(matrix(landbytes, 6, (res*2)^2/8, byrow=T), "alllandbytes.txt", sep=",", row.names=F, col.names=F)
# Still need to add a comma at the end of each line in the arduino code!






# Make all lat/lon positive
lon <- (alllon+360)%%360
lat <- alllat+90
# counting latitude from the south pole upwards

# The gbl files have a 2.5 degree resolution
# Convert the lat/lon to index in the gbl matrix
lon_i <- round(lon/2.5)
lat_i <- round(lat/2.5)
latlon_i <- lon_i + (lat_i-1)*144

# Plot again and show where pixel 0 is on each panel
m <- (0:5)*1024+1
plot(lat~lon, col=rainbow(6144))
points(lon[m], lat[m], pch=22, bg=rainbow(6))

# Plot the appriximate index lat/lon locations
plot(lat_i~lon_i, col=rainbow(6144))
points(lon_i[m], lat_i[m], pch=22, bg=rainbow(6))

# Store coordinates
geoledcube <- data.frame(i=1:(32*32*6), lon=alllon, lat=alllat, cubeglobe[,1:3], gbli=latlon_i, land=land)
names(geoledcube)[4:6] <- c("x", "y", "z")
head(geoledcube)
tail(geoledcube)

# And write to csv
write.csv(geoledcube, "geoledcube.csv", row.names=F)


lim <- 2

xlim=c(-lim,lim)
ylim=c(-lim,lim)
zlim=c(-lim,lim)

lx <- c(xlim[1],xlim[2],xlim[2],xlim[1])
ly <- c(ylim[1],ylim[2],ylim[2],ylim[1])
lz <- c(zlim[1],zlim[2],zlim[2],zlim[1])

# Visualize the cube in rgl
require(rgl)
rgl.clear("all")
rgl.light()
rgl.spheres(geoledcube$x,geoledcube$z,-geoledcube$y,color=c("green", "blue")[geoledcube$land+1],radius=0.05,texture=system.file("textures/bump_dust.png",package="rgl"),texmipmap=T, texminfilter="linear.mipmap.linear")




# The following code processes the gbl files
# The code is a translation and abbreviation of the Fortran program CHK_DATA 
# Available from:
# ftp://arlftp.arlhq.noaa.gov/pub/archives/utility/chk_data.f

# PROGRAM CHK_DATA
# 
# !-------------------------------------------------------------------------------
# ! Simple program to dump the first few elements of the data array for each
# ! record of an ARL packed meteorological file. Used for diagnostic testing.
# ! Created: 23 Nov 1999 (RRD)
# !          14 Dec 2000 (RRD) - fortran90 upgrade
# !          18 Oct 2001 (RRD) - expanded grid domain
# !          03 Jun 2008 (RRD) - embedded blanks
# !          06 Jul 2012 (RRD) - handle large grids (>1000)
# !-------------------------------------------------------------------------------


# Data in gbl files are stored in a special way and require some funny unpacking
# Data is stored in monthly gbl files (of unequal size) 
# The data is first extracted, scaled and reduced to single-byte integers
# And saved as monthly raw binary files
# In a second loop, monthly raw binary files are stitched together to yearly files 
# (Binary write segments are commented out to avoid accidentally overwriting good files)

year <- 2016
month <- 1

for (year in (2016:2017)) {
  for (month in (1:12)) {
    # Calculate the number of days this month
    days <- as.numeric(strptime(paste(year+floor(month/12),"/", (month%%12) + 1, "/", 1, sep=""), "%Y/%m/%d") - strptime(paste(year,"/", month, "/", 1, sep=""), "%Y/%m/%d"))
    print(year)
    print(month)
    print(days)
    
    # This index maps the led positions onto the global data field, for each time interval (4x daily)
    ii <- rep(latlon_i, days*4) + 144*73*rep(0:(days*4-1), each=length(latlon_i))
    
    # Open the gbl   
    filename <- paste("RP", year, formatC(month, width=2, flag="0"), ".gbl", sep="")
    con <- file(filename, "rb")
    # Read the label and header
    LABEL <- readChar(con, 50, useBytes = FALSE)
    HEADER <- readChar(con, 108, useBytes = FALSE)
    LABEL
    IYR <- as.numeric(substr(LABEL, 1, 2))
    IMO <- as.numeric(substr(LABEL, 3, 4))
    IDA <- as.numeric(substr(LABEL, 5, 6))
    IHR <- as.numeric(substr(LABEL, 7, 8))
    IFC <- as.numeric(substr(LABEL, 9, 10))
    CGRID <- substr(LABEL, 13, 14)
    KVAR <- substr(LABEL, 15, 18)
    IYR
    IMO
    IDA
    IHR
    IFC
    CGRID
    KVAR
    
    MODEL <- substr(HEADER, 1, 4)
    ICX <- as.numeric(substr(HEADER, 5, 7))
    MN <- as.numeric(substr(HEADER, 8, 9))
    POLE_LAT <- as.numeric(substr(HEADER, 10, 16))
    POLE_LON <- as.numeric(substr(HEADER, 17, 23))
    REF_LAT <- as.numeric(substr(HEADER, 24, 30))
    REF_LON <- as.numeric(substr(HEADER, 31, 37))
    SIZE <- as.numeric(substr(HEADER, 38, 44))
    ORIENT <- as.numeric(substr(HEADER, 45, 51))
    TANG_LAT <- as.numeric(substr(HEADER, 52, 58))
    SYNC_XP <- as.numeric(substr(HEADER, 59, 65))
    SYNC_YP <- as.numeric(substr(HEADER, 66, 72))
    SYNC_LAT <- as.numeric(substr(HEADER, 73, 79))
    SYNC_LON <- as.numeric(substr(HEADER, 80, 86))
    DUMMY <- as.numeric(substr(HEADER, 87, 93)) # 7
    
    # Extract the number of cells in x and y directions
    NX <- as.numeric(substr(HEADER, 94, 96)) # 3
    NY <- as.numeric(substr(HEADER, 97, 99)) # 3
    # Number of layers in the atmosphere
    NZ <- as.numeric(substr(HEADER, 100, 102)) # 3
    K_FLAG <- as.numeric(substr(HEADER, 103, 104)) # 2
    LENH <- as.numeric(substr(HEADER, 105, 108)) # 4
    
    # Read the 2nd header
    HEADER2 <- readChar(con, LENH, useBytes = FALSE)
    NPAR <- rep(0,NZ)
    
    # Jump through the file
    offset <- 0
    for (i in 1:NZ) {
      LEVEL <- as.numeric(substr(HEADER2, offset + 1, offset + 6))
      NPAR[i] <- as.numeric(substr(HEADER2, offset + 7, offset + 8))
      offset <- offset + (8 + 8*NPAR[i])
    }
    
    KNX <- as.numeric(substr(CGRID, 1, 1)) # 1
    KNX
    KNY <- as.numeric(substr(CGRID, 2, 2)) # 1
    KNY
    if ((KNX >= 64) | (KNY >= 64)) {
      NX=(KNX-64)*1000+NX
      NY=(KNY-64)*1000+NY
      
    }
    
    close(con)
    
    NXY = NX*NY
    LEN = NXY+50
    
    
    
    #     NX <- 144
    #     NY <- 73
    #     NXY = NX*NY
    #     NZ <- 18
    con <- file(filename, "rb")
    
    # paste("Grid size and lrec: ", NX,NY,NXY,LEN)
    # paste("Header record size: ", LENH)
    
    # Here, we only extract layer heights, temperature and relative humidity
    hgts <- array(NA, c(NX, NY, days*4, 8))
    temp <- array(NA, c(NX, NY, days*4, 8))
    relh <- array(NA, c(NX, NY, days*4, 8))
    # And data in the ground layer (including precipitation)
    l1 <- array(NA, c(NX, NY, days*4, 5))
    
    # These are dummies for other variables
    # prss <- array(NA, c(NX, NY, days*4)) # pressure
    # t02m <- array(NA, c(NX, NY, days*4)) # temperature at 2m
    # u10m <- array(NA, c(NX, NY, days*4)) # wind speed
    # v10m <- array(NA, c(NX, NY, days*4)) # wind speed
    # tpp <- array(NA, c(NX, NY, days*4))  # precipitation
    
    
    for (hour in 1:(days*4)) {
      
      # header CPACK
      LABEL <- readChar(con, 50, useBytes = FALSE)
      CPACK <- readBin(con, integer(), NXY, size=1)
      
      
      # read data CPACK
      for (l in 1:NZ) {
        for (p in 1:NPAR[l]) {
          # for (i in 1:4) { # read (and dump) CPACK for 4 variables
          # .00000 5PRSS 21 T02M 24 U10M213 V10M190 TPP6230 
          
          LABEL <- readChar(con, 50, useBytes = FALSE)
          CPACK <- readBin(con, integer(), NXY, size=1, signed=F)
          if ((l == 1 ) | ((l %in% 2:9) & (p %in% c(1,2,6)))) {
            KVAR <- substr(LABEL, 15, 18)
            #         print(paste(KVAR, hour))
            NEXP <- as.numeric(substr(LABEL, 19, 22))
            PREC <- as.numeric(substr(LABEL, 23, 36))
            VAR1 <- as.numeric(substr(LABEL, 37, 50)) 
            SCALE=2.0^(7-NEXP)
            CPACKarray <- ((array(CPACK,  c(NX, NY))-127.)/SCALE)
            CPACKarray[1,] <- VAR1+cumsum(CPACKarray[1,])
            BUF <- apply(CPACKarray, 2, cumsum)
          }
          # Store all layer 1 data
          if (l==1) {
            l1[,,hour,p] <- BUF
          }
          # For all other layers up to 9, only store 1, 2nd and 6th variable
          if (l %in% 2:9) {
            if (p==1) {
              hgts[,,hour,l-1] <- BUF # layer height
            }
            if (p==2) {
              temp[,,hour,l-1] <- BUF # temperature
            }
            if (p==6) {
              relh[,,hour,l-1] <- BUF # relative humidity
            }
          }
          
        }
      }
    }
    close(con) # close file
    
    # Plot an image of the air pressure of the first layer
    # Air pressure is closely related to altitude
    # This is a good check if the Earth's topography is visible 
    # and isn't flipped horizontally 
    
    x <- seq(0,360, 2.5)
    y <- seq(-90,90,2.5)
    image(x,y,l1[,,1,1])
    points(lon, lat-90, col=rainbow(6144), pch=19)
    
    # And while we're at it, give the Earth nice terrain colors
    # range(l1[,,1,1])
    # diff(range(l1[,,1,1]))
    breaks <- seq(508,1081)
    image(0:144,0:73,l1[,,1,1], col=terrain.colors(573)[573:1], breaks=breaks)
    
    # An plot the locations of the leds on top
    # with the bg color on the same scale to check if the index extracts the right data
    coli <- findInterval(l1[,,1,1][latlon_i], breaks)
    points(lon_i-.5, lat_i-.5, bg=terrain.colors(573)[573:1][coli], pch=21)
    
    
    
    # Layers are defined by equal pressure and vary in thickness
    # Calculate the difference in height of each cell
    dh <- hgts
    for (i in 2:8) {
      dh[,,,i] <- hgts[,,,i] - hgts[,,,i-1]
    }
    dh[dh<0] <- 0
    
    # Coefficients to convert relative humidity and temperature into water vapor pressure
    c1 <- -44.4644786199568
    c2 <- -0.019902564321678
    c3 <- -3596.74181505465
    c4 <- 11.8905109312045
    
    wvp <- exp(c1 + c2*temp + c3/temp + c4*log(temp))*relh/100
    
    # Not quite sure why and how this calculation came about.
    # wvpa will be scaled later, so 
    # Calculate water content in grams/m2 (Actually not entirely sure about the units)
    # / 8.314 is the gas constant
    # / 6.76 ??
    # / 1000
    # * 101325 convert to standard pressure??
    # * 18 grams/mol
    # Looks like n = PV/RT, but why?
    wvpa <- wvp[,,,1]*dh[,,,1]/temp[,,,1]/8.314/6.76/1000*10125*18
    for (i in 2:8) {
      wvpa <- wvpa + wvp[,,,i]*dh[,,,i]/temp[,,,i]/8.314/6.76/1000*10125*18
      
    }
    
    
    # Convert wvpa to single byte integer
    wvpab <- as.integer(wvpa/40)[ii]
    print(range(wvpab))
    mode(wvpab) <- "raw"
    # Write to binary file
#     con <- file(paste("wvpab", year, month, "new.bin", sep="_"), open="wb")
#     writeBin(wvpab, con)
#     close(con)
    
    
    
    # Extract and store 6-hour precipitation totals from first layer
    tpp <- l1[,,,5]
    # range(as.integer(tpp*2048))
    tppb <- as.integer(tpp*2048)[ii]
    print(range(tppb))
    mode(tppb) <- "raw"
#     con <- file(paste("tppb", year, month, ".bin", sep="_"), open="wb")
#     writeBin(tppb, con)
#     close(con)
    
  } # for (month ...)
} # for (year ...)

# Combine monthly files into yearly files
vars <- c("wvpa", "tpp") # only for wvpa and tpp
for (v in vars) {
  for (year in (2016:2017)) {
    month <- 1
    con <- file(paste(v, "b_", year, "_", month, "_.bin", sep=""), open="rb")
    vy <- readBin(con, "raw", n=6*32*32*31*4)
    close(con)
    for (month in (2:12)) {
      days <- as.numeric(strptime(paste(year+floor(month/12),"/", (month%%12) + 1, "/", 1, sep=""), "%Y/%m/%d") - strptime(paste(year,"/", month, "/", 1, sep=""), "%Y/%m/%d"))
      con <- file(paste(v, "b_", year, "_", month, "_.bin", sep=""), open="rb")
      vy <- c(vy, readBin(con, "raw", n=6*32*32*days*4))
      close(con)
    }
#     con <- file(paste(v, "_", year, ".bin", sep=""), open="wb")
#     writeBin(vy, con)
#     close(con)
    
  }
}

# Done!
# Copy the files to the root of the SD card and insert in Teensy
# Upload arduino code to Teensy, hook everything up and enjoy the geoledcube.
