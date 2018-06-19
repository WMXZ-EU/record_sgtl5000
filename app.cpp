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
#include "core_pins.h"
#include "usb_serial.h"

#define DO_DEBUG 1
#define FSI 5// desired sampling frequency index
uint32_t fsamps[] = {8000, 16000, 32000, 44100, 48000, 96000, 192000, 220500, 240000, 360000};
/*
 * NOTE: changing frequency impacts the macros 
 *      AudioProcessorUsage and AudioProcessorUsageMax
 * defined in stock AudioStream.h
 */

#if defined(__MK20DX256__)
  #define MQUEU 100 // number of buffers in aquisition queue
#elif defined(__MK64FX512__)
  #define MQUEU 550 // number of buffers in aquisition queue
#elif defined(__MK66FX1M0__)
  #define MQUEU 550 // number of buffers in aquisition queue
#else
  #define MQUEU 53 // number of buffers in aquisition queue
#endif
#define SEL_LR 1  // record only a single channel (0 left, 1 right)

  #include "input_i2s.h"
  AudioInputI2S         acq;

  #include "m_queue.h"
  mRecordQueue<int16_t, MQUEU> queue1;

  AudioConnection     patchCord1(acq,SEL_LR, queue1,0);

  #include "control_sgtl5000.h"
  AudioControlSGTL5000 audioShield;

// private 'library' included directly into sketch
const int32_t on = 60;
const int32_t off = 60;
int32_t tstart;
int32_t tstop;

#include "i2s_mods.h"
#include "logger_if.h"
#include "hibernate.h"

//__________________________General Arduino Routines_____________________________________
extern "C" void setup() {
  // put your setup code here, to run once:
  
  uint32_t tt=RTC_TSR;
  uint32_t dt = tt % (on+off);
  if(dt>=on) setWakeupCallandSleep(on+off-dt);      

  #if DO_DEBUG>0
    while(!Serial);
  #endif
  
  AudioMemory (MQUEU+10);
  audioShield.enable();
  audioShield.inputSelect(AUDIO_INPUT_LINEIN);  //AUDIO_INPUT_LINEIN or AUDIO_INPUT_MIC
   //
  I2S_modification(fsamps[FSI],32);

  delay(1);
  SGTL5000_modification(FSI); // must be called after I2S initialization stabilized (0: 8kHz, 1: 16 kHz 2:32 kHz, 3:44.1 kHz, 4:48 kHz, 5:96 kHz, 6:192 kHz)
  
  uSD.init();
  
  #if DO_DEBUG>0
    Serial.println("start");
  #endif
  queue1.begin();
}

void loop() {
  // put your main code here, to run repeatedly:
  static int16_t state=0; // 0: open new file, -1: last file

  if(queue1.available())
  {  // have data on que
    if(state==0)
    { // generate header before file is opened
       uint32_t *header=(uint32_t *) headerUpdate();
       uint32_t *ptr=(uint32_t *) outptr;
       // copy to disk buffer
       for(int ii=0;ii<128;ii++) ptr[ii] = header[ii];
       outptr+=256; //(512 bytes)
       state=1;
    }
    // fetch data from queue
    int32_t * data = (int32_t *)queue1.readBuffer();
    //
    // copy to disk buffer
    uint32_t *ptr=(uint32_t *) outptr;
    for(int ii=0;ii<64;ii++) ptr[ii] = data[ii];
    queue1.freeBuffer(); 
    //
    // adjust buffer pointer
    outptr+=128; // (128 shorts)
    //
    // if necessary reset buffer pointer and write to disk
    // buffersize should be always a multiple of 512 bytes
    if(outptr == (diskBuffer+BUFFERSIZE))
    {
      outptr = diskBuffer;
 
      // write to disk ( this handles also opening of files)
      if(state>=0)
        state=uSD.write(diskBuffer,BUFFERSIZE); // this is blocking

      if(state==0)
      {
        uint32_t tt=RTC_TSR;
        uint32_t dt = tt % (on+off);
        if(dt>=on) 
        { uSD.exit();
          setWakeupCallandSleep(on+off-dt);      
        }
      }
    }
  }
  else
  {  // queue is empty
    // do nothing
  }

   #if DO_DEBUG>0
    // some statistics on progress
    static uint32_t loopCount=0;
    static uint32_t t0=0;
    loopCount++;
    if(millis()>t0+1000)
    {  Serial.printf("loop: %5d %4d; %4d",
             loopCount, uSD.getNbuf(),
             AudioMemoryUsageMax());
       Serial.println();
       AudioMemoryUsageMaxReset();
       t0=millis();
       loopCount=0;
    }
  #endif
  //
  asm("wfi"); // to save some power switch off idle cpu
}

