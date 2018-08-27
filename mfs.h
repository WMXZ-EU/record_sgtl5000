/* wmxzAudio Library for Teensy 3.X
 * Copyright (c) 2017, Walter Zimmer
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
#ifndef MFS_H
#define MFS_H

/* MSF API 
 *  init(void);
 *  void exit(void);
 *  void open(char * filename);
 *  void close(void);
 *  uint32_t write(uint8_t *buffer, uint32_t nbuf);
 *  uint32_t read(uint8_t *buffer, uint32_t nbuf);
 */
 #define SDo 1  // Stock FAT
 #define SdFS 2 // Greimans FAT
 #define uSDFS 3 // CHaN's FAT does no work yet
// note SDFS needs CHECK_PROGRAMMING set to 1 in SdSpiCard.cpp

#define USE_FS SdFS

// Preallocate 8MB file.
const uint64_t PRE_ALLOCATE_SIZE = 8ULL << 20;

//--------------------- For File Time settings ------------------
#if USE_FS != uSDFS
#include <time.h>
#define EPOCH_YEAR 1970 //T3 RTC
#define LEAP_YEAR(Y) (((EPOCH_YEAR+Y)>0) && !((EPOCH_YEAR+Y)%4) && ( ((EPOCH_YEAR+Y)%100) || !((EPOCH_YEAR+Y)%400) ) )
static  const uint8_t monthDays[]={31,28,31,30,31,30,31,31,30,31,30,31}; 

/*  int  tm_sec;
  int tm_min;
  int tm_hour;
  int tm_mday;
  int tm_mon;
  int tm_year;
  int tm_wday;
  int tm_yday;
  int tm_isdst;
*/

struct tm seconds2tm(uint32_t tt)
{ struct tm tx;
  tx.tm_sec   = tt % 60;    tt /= 60; // now it is minutes
  tx.tm_min   = tt % 60;    tt /= 60; // now it is hours
  tx.tm_hour  = tt % 24;    tt /= 24; // now it is days
  tx.tm_wday  = ((tt + 4) % 7) + 1;   // Sunday is day 1 (tbv)

  // tt is now days since EPOCH_Year (1970)
  uint32_t year = 0;  
  uint32_t days = 0;
  while((unsigned)(days += (LEAP_YEAR(year) ? 366 : 365)) <= tt) year++;

  tx.tm_year = 1970+year; // year is offset from 1970 

  // correct for last (actual) year
  days -= (LEAP_YEAR(year) ? 366 : 365);
  tt  -= days; // now tt is days in this year, starting at 0
  
  uint32_t month=0;
  uint32_t monthLength=0;
  for (month=0; month<12; month++) 
  { monthLength = monthDays[month];
    if ((month==1) & LEAP_YEAR(year)) monthLength++; 
    if (tt<monthLength) break;
    tt -= monthLength;
  }
  tx.tm_mon = month + 1;   // jan is month 1  
  tx.tm_mday = tt + 1;     // day of month
  return tx;
}

uint32_t tm2seconds (struct tm *tx) 
{
  uint32_t tt;
  tt=tx->tm_sec+tx->tm_min*60+tx->tm_hour*3600;  

  // count days size epoch until previous midnight
  uint32_t days=tx->tm_mday;

  uint32_t mm=0;
  uint32_t monthLength=0;
  for (mm=0; mm<(tx->tm_mon-1); mm++) days+=monthDays[mm]; 
  if(tx->tm_mon>2 && LEAP_YEAR(tx->tm_year-1970)) days++;

  uint32_t years=0;
  while(years++ < (tx->tm_year-1970)) days += (LEAP_YEAR(years) ? 366 : 365);
  //  
  tt+=(days*24*3600);
  return tt;
}
#endif

/************************** File System Interface****************/
#if USE_FS == SdFS

#include "SdFs.h"

#if defined(__MK20DX256__)
  #define SD_CS 10
  #define SD_CONFIG SdSpiConfig(SD_CS, DEDICATED_SPI, SPI_FULL_SPEED)
  
#elif defined(__MK64FX512__) || defined(__MK66FX1M0__)
  // Use FIFO SDIO or DMA_SDIO
  #define SD_CONFIG SdioConfig(FIFO_SDIO)
  //#define SD_CONFIG SdioConfig(DMA_SDIO)
#endif


// Call back for file timestamps.  Only called for file create and sync().
void dateTime(uint16_t* date, uint16_t* time) {

  struct tm tx=seconds2tm(RTC_TSR);
    
  // Return date using FS_DATE macro to format fields.
  *date = FS_DATE(tx.tm_year, tx.tm_mon, tx.tm_mday);

  // Return time using FS_TIME macro to format fields.
  *time = FS_TIME(tx.tm_hour, tx.tm_min, tx.tm_sec);
}

class c_mFS
{
  private:
  SdFs sd;
  FsFile file;
  
  public:
    void init(void)
    { Serial.println("Using SdFS");
      #if defined(__MK20DX256__)
          // Initialize the SD card
        SPI.setMOSI(7);
        SPI.setSCK(14);
      #endif  
      if (!sd.begin(SD_CONFIG)) sd.errorHalt("sd.begin failed");
    
      // Set Time callback
      FsDateTime::callback = dateTime;
    }

    void exit(void)
    {
      digitalWriteFast(SD_CS,LOW);
        int ii;
        for(ii=0; SPI.transfer(0xff)!=0xFF && ii<30000; ii++) ;
        Serial.println(ii); Serial.flush();
      digitalWriteFast(SD_CS,HIGH);

      sd.end();
      SPI.end();
/*      
      delay(100);
      #if defined(__MK20DX256__)
        while(SPI.transfer(0xff)!=0xFF) ;
        Serial.println(digitalReadFast(SD_CS)); Serial.flush();
        digitalWriteFast(SD_CS,HIGH); // deactivate uSD (release CS)
        delay(100);
        SPI.end();
        pinMode(SD_CS,INPUT_DISABLE);
        pinMode(SCK,INPUT_DISABLE);
        pinMode(MISO,INPUT_DISABLE);
        pinMode(MOSI,INPUT_DISABLE);
      #endif
    
      pinMode(23,INPUT_DISABLE);
      pinMode(9,INPUT_DISABLE);
      pinMode(11,INPUT_DISABLE);
      pinMode(13,INPUT_DISABLE);
*/     
    }
    
    void open(char * filename)
    {
      if (!file.open(filename, O_CREAT | O_TRUNC |O_RDWR)) {
        sd.errorHalt("file.open failed");
      }
      if (!file.preAllocate(PRE_ALLOCATE_SIZE)) {
        sd.errorHalt("file.preAllocate failed");    
      }
    }

    void close(void)
    {
      file.truncate();
      file.close();
    }

    uint32_t write(uint8_t *buffer, uint32_t nbuf)
    {
      if (nbuf != file.write(buffer, nbuf)) sd.errorHalt("write failed");
      return nbuf;
    }

    uint32_t read(uint8_t *buffer, uint32_t nbuf)
    {      
      if ((int)nbuf != file.read(buffer, nbuf)) sd.errorHalt("read failed");
      return nbuf;
    }
};

#elif  USE_FS == SDo
#include "SPI.h"
#include "SD.h"

#if defined(__MK20DX256__)
  #define SD_CS 10
  
#elif defined(__MK64FX512__) || defined(__MK66FX1M0__)
  #define SD_CS  BUILTIN_SDCARD

#endif

class c_mFS
{
  private:
  File file;

  void die(char *txt) {Serial.println(txt); while(1) asm("wfi");}
  
  public:
    void init(void)
    { 
      Serial.println("Using SDo");
      #if defined(__MK20DX256__)
          // Initialize the SD card
        SPI.setMOSI(7);
        SPI.setSCK(14);
      #endif  
      if (!(SD.begin(SD_CS))) die("error SD.begin");
    }

    void exit(void)
    {
    }
    
    void open(char * filename)
    {
      file = SD.open(filename, FILE_WRITE);
    }

    void close(void)
    {
      file.close();
    }

    uint32_t write(uint8_t *buffer, uint32_t nbuf)
    {
      file.write(buffer, nbuf);
      return nbuf;
    }

    uint32_t read(uint8_t *buffer, uint32_t nbuf)
    {      
      if ((int)nbuf != file.read(buffer, nbuf)) {Serial.println("error file read"); while(1) asm("wfi");}
      return nbuf;
    }
};

#elif  USE_FS == uSDFS
#include "ff.h"
#include "ff_utils.h"

extern "C" uint32_t usd_getError(void);

class c_mFS
{
  private:
    FRESULT rc;     /* Result code */
    FATFS fatfs;    /* File system object */
    FIL fil;        /* File object */

    UINT wr;
    
    TCHAR wfilename[80];
  
    /* Stop with dying message */
    void die(char *str, FRESULT rc) 
    { Serial.printf("%s: Failed with rc=%u.\n\r", str, rc); while(1) asm("wfi");}
    
  public:
    void init(void)
    {
      Serial.println("Using uSDFS");
      rc = f_mount (&fatfs, (TCHAR *)_T("1:/"), 0);      /* Mount/Unmount a logical drive */
      if (rc) die((char*)"mount", rc);
    }
    
    void exit(void)
    {
      
    }
    
    void open(char * filename)
    {
      char2tchar(filename,80,wfilename);
      //
      // check status of file
      rc =f_stat(wfilename,0);
      Serial.printf("stat %d %x\n",rc,fil.obj.sclust);

      rc = f_open(&fil, wfilename, FA_WRITE | FA_CREATE_ALWAYS);
      Serial.printf(" opened %d %x\n\r",rc,fil.obj.sclust);
      // check if file is Good
      if(rc == FR_INT_ERR)
      { // only option is to close file
        rc = f_close(&fil);
        if(rc == FR_INVALID_OBJECT)
        { Serial.println("unlinking file");
          rc = f_unlink(wfilename);
          if (rc) die((char*)"unlink", rc);
        }
        else
          die((char*)"close", rc);
        
      }
      // retry open file
      rc = f_open(&fil, wfilename, FA_WRITE | FA_CREATE_ALWAYS);
      if(rc) die((char*)"open", rc);
    }
    
    void close(void)
    {
      rc = f_close(&fil);
      if (rc) die((char*)"close", rc);
    }
    
    uint32_t write( uint8_t *buffer, uint32_t nbuf)
    {
      rc = f_write(&fil, buffer, nbuf, &wr);
      if (rc== FR_DISK_ERR) // IO error
      { uint32_t usd_error = usd_getError();
        Serial.printf(" write FR_DISK_ERR : %x\n\r",usd_error);
        // only option is to close file
        // force closing file
        return 0;
      }
      else if(rc) die((char*)"write",rc);
      return nbuf;
    }
    
    uint32_t read(uint8_t *buffer, uint32_t nbuf)
    { return 0;
    }
};
#endif
#endif
