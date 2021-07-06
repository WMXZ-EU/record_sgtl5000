/* SGTL5000 Recorder for Teensy 3.X
 * Copyright (c) 2018, Walter Zimmer
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice, development funding notice, and this permission
 * notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
 

#ifndef _LOGGER_IF_H
#define _LOGGER_IF_H

#include "core_pins.h"

#include "config.h"
//==================== local uSD interface ========================================

#ifndef BUFFERSIZE
  #define BUFFERSIZE (8*1024)
#endif
data_t diskBuffer[BUFFERSIZE];
data_t *outptr = diskBuffer;

#include "SD.h"
#include "TimeLib.h"

#ifndef USE_SDIO
  #define USE_SDIO 0
#endif

  #if defined(__MK20DX256__)
    #define SD_CS  10
    #define SD_CONFIG SdSpiConfig(SD_CS, DEDICATED_SPI, SPI_FULL_SPEED)
    #define SD_MOSI  7
    #define SD_MISO 12
    #define SD_SCK  14
    
  #elif defined(__MK64FX512__) || defined(__MK66FX1M0__)
    #if USE_SDIO==1
      // Use FIFO SDIO or DMA_SDIO
      #define SD_CONFIG SdioConfig(FIFO_SDIO)
  //    #define SD_CONFIG SdioConfig(DMA_SDIO)
    #else
      #define SD_CS  10
      #define SD_CONFIG SdSpiConfig(SD_CS, DEDICATED_SPI, SPI_FULL_SPEED)
      #define SD_MOSI  7
      #define SD_MISO 12
      #define SD_SCK  14
    #endif
  #elif defined(__IMXRT1062__)
    #if USE_SDIO==1
      // Use FIFO SDIO or DMA_SDIO
      #define SD_CONFIG SdioConfig(FIFO_SDIO)
  //    #define SD_CONFIG SdioConfig(DMA_SDIO)
    #else
      #define SD_CS  10
      #define SD_CONFIG SdSpiConfig(SD_CS, DEDICATED_SPI, SPI_FULL_SPEED)
      #define SD_MOSI 11
      #define SD_MISO 12
      #define SD_SCK  13
    #endif
  #endif

//------------------------------------------------------------------------------
// Call back for file timestamps.  Only called for file create and sync().
void dateTime(uint16_t* date, uint16_t* time, uint8_t* ms10) 
{ 
  // Return date using FS_DATE macro to format fields.
  *date = FS_DATE(year(), month(), day());

  // Return time using FS_TIME macro to format fields.
  *time = FS_TIME(hour(), minute(), second());
  
  // Return low time bits in units of 10 ms.
  *ms10 = second() & 1 ? 100 : 0;
}

class c_mFS
{
  private:
  SDClass sd;
  File file;
  
  public:
    void init(void)
    { Serial.println("Using SdFat");

      #if USE_SDI0==0
        SPI.setMOSI(SD_MOSI);
        SPI.setMISO(SD_MISO);
        SPI.setSCK(SD_SCK);
      #endif
      if (!sd.sdfs.begin(SD_CONFIG)) sd.sdfs.errorHalt("sd.begin failed");
    
      // Set Time callback
      FsDateTime::callback = dateTime;
    }

    void mkDir(char * dirname)  { if(!sd.exists(dirname)) sd.mkdir(dirname);  }
    void chDir(char * dirname)  { sd.sdfs.chdir(dirname);   }
    
    void exit(void)
    {
      #if defined(__MK20DX256__)
      digitalWriteFast(SD_CS,LOW);
        int ii;
        for(ii=0; SPI.transfer(0xff)!=0xFF && ii<30000; ii++) ;
        Serial.println(ii); Serial.flush();
      digitalWriteFast(SD_CS,HIGH);

      sd.end();
      SPI.end();
      #endif  
    }
    
    void open(char * filename)
    {
      file = sd.open(filename,FILE_WRITE);
      if(!file) sd.sdfs.errorHalt("file.open failed");
    }

    void close(void)
    {
      file.truncate();
      file.close();
    }

    uint32_t write(void *buffer, uint32_t nbuf)
    {
      if (nbuf != file.write(buffer, nbuf)) sd.sdfs.errorHalt("write failed");
      return nbuf;
    }

    uint32_t read(void *buffer, uint32_t nbuf)
    {      
      if ((int)nbuf != file.read(buffer, nbuf)) sd.sdfs.errorHalt("read failed");
      return nbuf;
    }
};

class c_uSD
{
  public:
    c_uSD(void): state(-1) { }
    void init(void);
    void exit(void);
    void close(void);

    void chDir(void);
    int16_t write(void * data, int32_t ndat, int mustClose);

    uint32_t nCount=0;
    int16_t getStatus() {return state;}
    
  private:
    int16_t state; // 0 initialized; 1 file open; 2 data written; 3 to be closed; -1 error

    c_mFS mFS;

};
c_uSD uSD;


/*
 *  Logging interface support / implementation functions 
 */

char * generateDirectory(char *filename);
char * generateFilename(char *filename);

char *makeDirname(void)
{ static char dirname[80];
  return generateDirectory(dirname);
}

char *makeFilename(void)
{ static char filename[80];
  return generateFilename(filename);
}

char * headerUpdate(void);

//____________________________ FS Interface implementation______________________
void c_uSD::init(void)
{
  mFS.init();
  //
  state=0;
}

void c_uSD::exit(void)
{ mFS.exit();
  state=-1;
}

void c_uSD::close(void)
{ mFS.close();
  state=0;
}

void c_uSD::chDir(void)
{ char * dirName=makeDirname();
  mFS.mkDir(dirName);
  mFS.chDir(dirName);
}

int16_t c_uSD::write(void *data, int32_t ndat, int mustClose)
{
  if(state == 0)
  { // open file
    char *filename = makeFilename();
    if(!filename) {state=-1; return state;} // flag to do nothing anymore
    //
    mFS.open(filename);

    state=1; // flag that file is open
  }
  
  if(state == 1 || state == 2)
  {  // write to disk
    state=2;
    mFS.write((unsigned char *) data, 2*ndat);
    nCount++;
    if(mustClose) state=3;
  }
  
  if(state == 3)
  {
    mFS.close();
    state=0;  // flag to open new file
  }
  return state;
}

#endif
