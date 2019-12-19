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

//==================== local uSD interface ========================================
// this implementation used SdFat-beta from Bill Greiman
// which needs to be installed as local library 

#ifndef BUFFERSIZE
  #define BUFFERSIZE (8*1024)
#endif
int16_t diskBuffer[BUFFERSIZE];
int16_t *outptr = diskBuffer;

#include "mfs.h"

class c_uSD
{
  public:
    c_uSD(void): state(-1) { }
    void init(void);
    void exit(void);
    void close(void);

    void chDir(void);
    int16_t write(int16_t * data, int32_t ndat, int mustClose);

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

int16_t c_uSD::write(int16_t *data, int32_t ndat, int mustClose)
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
