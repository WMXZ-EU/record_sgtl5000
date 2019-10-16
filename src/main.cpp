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

#include "config.h"


// adapted from audio gui
  #include "input_i2s.h"
  AudioInputI2S         acq;

  #include "m_queue.h"
  mRecordQueue<MQUEU> queue1;

  AudioConnection     patchCord1(acq,SEL_LR, queue1,0);

  #include "control_sgtl5000.h"
  AudioControlSGTL5000 audioShield;

// private 'library' included directly into sketch
#include "i2s_mods.h"
#include "logger_if.h"
#include "hibernate.h"

// utility for logger
char * headerUpdate(void)
{
  static char header[512];
  sprintf(&header[0], "WMXZ");
  
  sprintf(&header[4], "%04d_%02d_%02d_%02d_%02d_%02d", year(), month(), day(), hour(), minute(), second());
  header[23]=0;

  // add more info to header
  //
  uint32_t *ptr = (uint32_t*) &header[24];
  ptr[0]=millis();
  ptr[1]=micros();

  ptr[2] = fsamps[FSI];
  ptr[3] = (uint32_t) a_on;
  ptr[4] = (uint32_t) a_off;
  ptr[5] = (uint32_t) t_on;

  return header;
}

//******************************Auxillary functions **********************
#include "TimeLib.h"
time_t getTime() { return Teensy3Clock.get(); }

uint16_t generateDirectory(char *filename)
{
  sprintf(filename, "%s_%04d%02d%02d_%02d", DirPrefix, 
             year(), month(), day(), hour());
  #if DO_DEBUG>0
    Serial.println(filename);
  #endif
  return 1;
}
uint16_t generateFilename(char *filename)
{
	sprintf(filename, "%s_%02d%02d%02d.bin", FilePrefix,
		      	  hour(), minute(), second());
	return 1;
}

uint16_t newHour(void)
{
  static int lastHour=-1;
  int _hour = hour();
  if(_hour == lastHour) return 0;
  lastHour = _hour;
  return 1;
}

int32_t record_or_sleep(void)
{
	uint32_t tt = (uint32_t) now();
	int32_t ret = 0; // default: keep recording

	// end of file?
	uint32_t tsx = tt % t_on;
	static uint32_t tso=0;
	if(tsx < tso) ret = -1; // close this file
	tso=tsx;

	// end of acquisition?
	if(a_off>0)
	{
		uint32_t dt = tt % (a_on+a_off);
		if(dt>=a_on) ret = (a_on+a_off-dt); // end of on-time reached
	}

	return (ret); 
}


//__________________________General Arduino Routines_____________________________________
extern "C" void setup() {
  // put your setup code here, to run once:

  // set the Time library to use Teensy 3.0's RTC to keep time
  setSyncProvider(getTime);
  delay(100);
  if (timeStatus()!= timeSet) {
    Serial.println("Unable to sync with the RTC");
  } else {
    Serial.println("RTC has set the system time");
  }

  uint32_t nsec = record_or_sleep();
  if(0) if(nsec>0)
  { SGTL5000_disable();
    I2S_stopClock();
    setWakeupCallandSleep(nsec);
  }

  #if DO_DEBUG>0
    while(!Serial ) asm("wfi");
  #endif
  
  AudioMemory (MQUEU+6);
  audioShield.enable();
  audioShield.inputSelect(AUDIO_SELECT);  //AUDIO_INPUT_LINEIN or AUDIO_INPUT_MIC
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
  static uint32_t tMax=0;

  uint32_t t1=micros();
  if(queue1.available())
  { // have data on queue
    //
    if(newHour()) uSD.chDir();
    //
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
    int32_t * data = (int32_t *)queue1.readBuffer(); // cast to int32 to speed-up following copy
    //
    // copy to disk buffer
    uint32_t *ptr=(uint32_t *) outptr;
    for(int ii=0;ii<64;ii++) ptr[ii] = data[ii];
    queue1.freeBuffer(); 
    //
    // advance buffer pointer
    outptr+=128; // (128 shorts)
    //
    // check if we should record, close file or hibernate
    int32_t nsec = record_or_sleep();
    int mustClose = nsec;
    if((nsec<0) ||(outptr == (diskBuffer+BUFFERSIZE)))
    {
      if(state>=0)
        state=uSD.write(diskBuffer,outptr-diskBuffer, mustClose); // this is blocking
      outptr = diskBuffer;
    }

    if(state==0 && (nsec>0))  // if file is closed and acquisition ended
    {  uSD.exit();
        SGTL5000_disable();
        I2S_stopClock();
        setWakeupCallandSleep(nsec);      
    }
  }
  uint32_t t2=micros();
  if(t2-t1>tMax) tMax=(t2-t1);


  #if DO_DEBUG>0
    // some statistics on progress
    static uint32_t loopCount=0;
    static uint32_t t0=0;
    loopCount++;
    if(millis()>t0+1000)
    {  Serial.printf("loop: %5d; %4d %4d %4d %6d %4d",
             loopCount,
             AudioMemoryUsageMax(), uSD.nCount, queue1.dropCount, tMax, rtc_get() % t_on);
       Serial.println();
       AudioMemoryUsageMaxReset();
       //
       uSD.nCount=0;
       loopCount=0;
       queue1.dropCount=0;
       tMax=0;
       //
       t0=millis();
    }
  #endif
  //
  asm("wfi"); // to save some power switch off idle cpu
}

