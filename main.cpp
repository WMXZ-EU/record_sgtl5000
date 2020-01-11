/* SGTL5000 Recorder for Teensy 
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

#if AUDIO_MODE==PJRC
  // adapted from audio gui
    #include "input_i2s.h"
    AudioInputI2S         acq;

    #include "m_queue.h"
    mRecordQueue<MQUEU> queue[NCH];

  #if NCH == 1
    AudioConnection     patchCord1(acq,SEL_LR, queue[0],0);
  #elif NCH == 2
    AudioConnection     patchCord1(acq,0, queue[0],0);
    AudioConnection     patchCord2(acq,1, queue[1],0);
  #endif
#elif AUDIO_MODE==WMXZ
  #include "i2s_mods.h"
  #include "I2S_32.h"
  I2S_32 acq;
    
  #include "m_queue.h"
  mRecordQueue<MQUEU> queue[NCH];

  #if NCH == 1
    mAudioConnection     patchCord1(acq,SEL_LR, queue[0],0);
  #elif NCH == 2
    mAudioConnection     patchCord1(acq,0, queue[0],0);
    mAudioConnection     patchCord2(acq,1, queue[1],0);
  #endif
#endif

#include "control_sgtl5000.h"
AudioControlSGTL5000 audioShield;

// private 'library' included directly into sketch
#include "logger_if.h"
#include "hibernate.h"

// ************************* utility for logger ***************************************
char * headerUpdate(void)
{
  static char header[512];
  sprintf(&header[0], "WMXZ"); // MAGIC word for header
  
  sprintf(&header[4], "%04d_%02d_%02d_%02d_%02d_%02d", year(), month(), day(), hour(), minute(), second());
  header[23]=0;

  // add more info to header
  //
  uint32_t *ptr = (uint32_t*) &header[24];
  ptr[0]=millis();
  ptr[1]=micros();
  //
  ptr[2] = fsamps[FSI];
  ptr[3] = (uint32_t) a_on;
  ptr[4] = (uint32_t) a_off;
  ptr[5] = (uint32_t) t_on;
  //
  uint16_t *sptr= (uint16_t *) &ptr[6];
  sptr[0] = r_h1s;
  sptr[1] = r_h1e;
  sptr[2] = r_h2s;
  sptr[3] = r_h2e;
  sptr[4] = NCH;
  sptr[5] = NBYTE;
  sptr[6] = SEL_LR;
  sptr[7] = AUDIO_SELECT; 
  sptr[8] = MicGain; 
  sptr[9] = AUDIO_MODE;
  //
  ptr = (uint32_t*) &sptr[10]; // for future values
  //
  return header;
}

//******************************Auxillary functions **********************
#include "TimeLib.h"
time_t getTime() { return Teensy3Clock.get(); }

char * generateDirectory(char *filename)
{
  sprintf(filename, "/%s_%04d%02d%02d/%02d", DirPrefix, year(), month(), day(), hour());
  #if DO_DEBUG>0
    Serial.println(filename);
  #endif
  return filename;
}

char * generateFilename(char *filename)
{
	sprintf(filename, "%s_%02d%02d%02d.bin", FilePrefix, hour(), minute(), second());
  #if DO_DEBUG>0
    Serial.println(filename);
  #endif
  return filename;
}

uint16_t newHour(void)
{
  static int lastHour=-1;
  int _hour = hour();
  if(_hour == lastHour) return 0;
  lastHour = _hour;
  return 1;
}

/*
 * timed recording examples
 * 7,7: 7==7                     : 24
 * 7,8: 8 > 7      : t<7         : 7-t
 *                   t>=8        : 7+24-t
 * 8,7: 7 < 8      : t<8 & t>=7  : 8-t 
 * 
 * 7,8 9,10: 10 > 7: t<7        : 7-t
 *                   t>=10      : 7+24-t
 *                   t>=8 & t<9 : 9-t
 * 7,8,9,6:   6 < 7: t>=6 & t<7 : 7-t
 *                   t>=8 & t<9 : 9-t   
 */

int32_t record_or_sleep(void)
{
	uint32_t tt = (uint32_t) now();
	int32_t ret = 0; // default: keep recording

  uint32_t ttd = tt % (24*3600);

  uint32_t r_t1s = r_h1s*3600;
  uint32_t r_t1e = r_h1e*3600;
  uint32_t r_t2s = r_h2s*3600;
  uint32_t r_t2e = r_h2e*3600;
  if(r_t2s<0)
  { // only one period per day
    if(r_t1e == r_t1s) ret=24*3600; //should not happen
    //
    if(r_t1e > r_t1s) // sleep over midnight
    { if(ttd <  r_t1s) ret = r_t1s - ttd;
      if(ttd >= r_t1e) ret = (r_t1s +24*3600) - ttd;
    }
    else  if((ttd >= r_t1e) && (ttd < r_t1s)) ret = (r_t1s - ttd);
  }
  else
  { // two periods per day

    if(r_t2e > r_t1s) // sleep over midnight
    {
      if(ttd <  r_t1s) ret = (r_t1s -ttd);
      if(ttd >= r_t2e) ret = (r_t1s +24*3600)-ttd; 
    }
    else if((ttd >= r_t2e) && (ttd < r_t1s)) ret = (r_t1s - ttd);

    if((r_t2s>r_t1e) && (ttd >= r_t1e) && (ttd < r_t2s)) ret = (r_t2s - ttd);
  }

  static uint32_t tso=0;
  uint32_t tsx = tt % t_on;           // time into file
  uint32_t tsy = tt % (a_on + a_off); // time into aquisition

  if(!ret)
  {
    if((tsy >= a_on) && (a_off>0))   // check end of aquisition
      ret = (a_on + a_off - tsy);
    else if (tsx < tso)              // check end of file
      ret = -1;
  }
   #if DO_DEBUG>0
    if(ret) 
    {  Serial.print("nsec = ");
      Serial.println(ret);     
      Serial.flush();
    }
  #endif
  tso=tsx;
  //
	return (ret); 
}

#if defined(__IMXRT1062__)
  extern "C" uint32_t set_arm_clock(uint32_t frequency);
#endif

void printDate(void)
{ 
   Serial.printf("%04d-%02d-%02d %02d:%02d:%02d\r\n",year(),month(),day(), hour(),minute(),second()); 
}

void stopAcq(int nsec)
{
  #if DO_DEBUG >0
    printDate();
  #endif
  SGTL5000_disable();
  I2S_stopClock();
  setWakeupCallandSleep(nsec);
}


extern "C" void setup() {
  // put your setup code here, to run once:

  #if DO_DEBUG>0
    while(!Serial && (millis()<3000));// asm("wfi");
    Serial.println("\nVersion: " __DATE__ " " __TIME__);
  #endif

  #if defined(__IMXRT1062__)
    set_arm_clock(24000000);
    #if DO_DEBUG>1
        Serial.print("F_CPU_ACTUAL=");
        Serial.println(F_CPU_ACTUAL);
    #endif
  #endif

  // set the Time library to use Teensy 3.0's RTC to keep time
  setSyncProvider(getTime);
  delay(100);
  #if DO_DEBUG >1
    if (timeStatus()!= timeSet) {
      Serial.println("Unable to sync with the RTC");
    } else {
      Serial.println("RTC has set the system time");
    }
  #endif
  #if DO_DEBUG >0
    printDate();
  #endif

  mAudioMemory(MQUEU+6);
  
  audioShield.enable();
  audioShield.inputSelect(AUDIO_SELECT);  //AUDIO_INPUT_LINEIN or AUDIO_INPUT_MIC
   //
  I2S_modification(fsamps[FSI],32);
  delay(10);
  SGTL5000_modification(FSI); // must be called after I2S initialization stabilized 
  //(0: 8kHz, 1: 16 kHz 2:32 kHz, 3:44.1 kHz, 4:48 kHz, 5:96 kHz, 6:192 kHz, 7:384kHz)
  
  if(AUDIO_SELECT == AUDIO_INPUT_MIC)
  {
    audioShield.micGain(MicGain);
  }

  uint32_t nsec = record_or_sleep();
  if(nsec>0)
    stopAcq(nsec);

  uSD.init();
  
  #if DO_DEBUG>0
    Serial.print("Fsamp "); Serial.println(fsamps[FSI]);
    Serial.println("start");
  #endif
  for(int ii=0; ii<NCH; ii++) queue[ii].begin();
}

int16_t tmpStore[NCH*128]; // temporary buffer

void loop() {
  // put your main code here, to run repeatedly:
  static int16_t state=0; // 0: open new file, -1: last file
  static uint32_t tMax=0;

  if(state<0) return;

  int16_t *data;

  static uint32_t t3=millis();
  uint32_t t1=millis();

  // check if we should continue to record, close file or hibernate
  int32_t nsec=0;
  int mustClose;

  nsec=0;
  mustClose=0;
  if(state>0) // we are logging
  {
    nsec = record_or_sleep();
  }
  mustClose = !(nsec==0);

  if(queue[NCH-1].available())
  { // have data on queue
    t3=t1;
    //
    if(newHour()) uSD.chDir();
    //
    if(state==0) //file needs to be opened
    { // generate header before file is opened
       uint32_t *header=(uint32_t *) headerUpdate();
       uint32_t *ptr=(uint32_t *) outptr;
       
       // copy to disk buffer
       for(int ii=0;ii<128;ii++) ptr[ii] = header[ii];
       outptr+=256; //(512 bytes)
       state=1; // flag data ready for filing
    }

    // fetch data from queue and multiplex channels
    for(int ii=0; ii<NCH; ii++)
    {
      data = (int16_t *)queue[ii].readBuffer(); 
      //
      // copy to temporary buffer
      int16_t *ptr= &tmpStore[ii];
      for(int jj=0; jj<128; jj+=NCH) ptr[jj] = data[jj];
      //
      queue[ii].freeBuffer();
    }

    //
    int32_t ndat = 128;
    if(outptr+NCH*128 > diskBuffer+BUFFERSIZE) ndat = (diskBuffer+BUFFERSIZE-outptr)/NCH;
//    ndat=128;
 
    //copy to disk buffer
    int16_t *ptr1=(int16_t *) outptr;
    int16_t *ptr2=(int16_t *) tmpStore;
    for(int jj=0; jj<NCH*ndat; jj++) ptr1[jj] = ptr2[jj];
    //
    // advance buffer pointer
    outptr += (NCH*ndat); // (NCH*ndat shorts)
    //
    // 
    if(mustClose || (outptr == (diskBuffer+BUFFERSIZE)))
    { 
      #if DO_DEBUG>1
        if(mustClose) 
        {
          Serial.println("Closing A");
          Serial.println(state);
          Serial.println((uint32_t)(outptr-diskBuffer));
        }
      #endif
      if(outptr>diskBuffer)
        state=uSD.write(diskBuffer,outptr-diskBuffer, mustClose); // this is blocking
      outptr = diskBuffer;
      if(mustClose) 
      { Serial.print("stateA = "); Serial.println(state);
      }
    }
    //
    if(ndat<128)
    { // copy rest to disk buffer
      int16_t *ptr1=(int16_t *) outptr;
      int16_t *ptr2=(int16_t *) &tmpStore[NCH*ndat];
      for(int jj=0; jj<NCH*(128-ndat); jj++) ptr1[jj] = ptr2[jj];
      //
      outptr += (NCH*(128-ndat));
    }

    if((nsec>0) && (state==0) && (mustClose))  // if file is closed and acquisition ended
    { 
       #if DO_DEBUG>1
         Serial.print("mustClose "); Serial.println(state); 
       #endif
      uSD.exit();
      state=-1;
      mustClose=0;
      stopAcq(nsec);
    }
  }
  else
  { // no audio block usb_serial_available
    // should we close?
    if(state>0 && mustClose)
    {
      #if DO_DEBUG>1
       Serial.println("Closing B");
      #endif
      //but first write remaining data to disk
      state=uSD.write(diskBuffer,outptr-diskBuffer, mustClose); // this is blocking
      outptr = diskBuffer;
      #if DO_DEBUG>1
        if(mustClose) { Serial.print("stateB = "); Serial.println(state);}
      #endif
      mustClose=0;
      if(nsec>0)
      { stopAcq(nsec);
      }

    }
    // bail out if there are no new data within 10 second
    if (millis() > t3 + 10000)
    {
      #if DO_DEBUG >0
        Serial.println("Lacking Audio buffers");
        Serial.println("I2S crashed ?");
        printDate();
      #endif
      uSD.close();
//      state=-1;
//      mustClose=0;
//      setWakeupCallandSleep(10);
      stopAcq(10);
      return;
    }
  }
  uint32_t t2=millis();
  if(t2-t1 > tMax) tMax=(t2-t1);


   #if DO_DEBUG>0
    // some statistics on progress
    static uint32_t loopCount=0;
    static uint32_t t0=0;
    loopCount++;
    if(millis()>t0+1000)
    {  Serial.printf("loop: %5d; %4d %4d %4d %6d %4d",
             loopCount,
             mAudioMemoryUsageMax(), uSD.nCount, queue[0].dropCount, tMax, rtc_get() % t_on);
       Serial.println();
       //
       mAudioMemoryUsageMaxReset();
       //
       uSD.nCount=0;
       loopCount=0;
       queue[0].dropCount=0;
       tMax=0;
       //
       t0=millis();
    }
  #endif

  //
//  asm("wfi"); // to save some power switch off idle cpu
}
