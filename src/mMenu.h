/*
 * compact Volumetric Acoustic Sensor
 * WMXZ 2019
 */

#ifndef MENU_H
#define MENU_H

#include "core_pins.h"
#include "usb_serial.h"

#include "config.h"

/**************** microGAP Menu ***************************************/

// Global parameters
extern uint32_t gain; // defined in acq.h
#ifndef SHIFT
  #define SHIFT 8
#endif

#include "TimeLib.h"

static char * getDate(char *text)
{
    sprintf(text,"%04d/%02d/%02d",year(), month(), day());
    return text;  
}

static char * getTime(char *text)
{
    sprintf(text,"%02d:%02d:%02d",hour(), minute(), second());
    return text;
}

static void setDate(uint16_t year, uint16_t month, uint16_t day)
{
  setTime(hour(),minute(),second(),day, month, year);
}

static void setTime(uint16_t hour, uint16_t minutes, uint16_t seconds)
{
  setTime(hour,minutes,seconds,day(), month(), year());
}

#define MAX_VAL 1<<17 // maximal input value
int boundaryCheck(int val, int minVal, int maxVal)
{
  if(minVal < maxVal) // standard case
  {
    if(val<minVal) val=minVal;
    if(val>maxVal) val=maxVal;
  }
  else // wrap around when checking hours
  {
    if((val>maxVal) && (val<minVal)) val=maxVal;
    if((val>24)) val=24;
  }
  return val; 
}
int boundaryCheck2(int val, int minVal, int maxVal, int modVal)
{
  if(minVal < maxVal) // standard case
  {
    if(val<minVal) val=minVal;
    if(val>maxVal) val=maxVal;
  }
  else // wrap around when checking hours
  {
    if(val<0) val=0;
    if(val>modVal) val=modVal;
    // shift data to next good value
    if((val>maxVal) && (val<minVal))
    { if(val>(minVal+maxVal)/2) val = minVal; else val=maxVal;
    }
  }
  return val; 
}

char text[40]; // neded for text operations
extern uint32_t a_on;
extern uint32_t a_off;
extern uint32_t t_on;

static void printAll(void)
{ Serial.println("\n----------------------------------------------------------------");
  Serial.printf("Version %s\n\r",VERSION);
  Serial.printf("%c %3d gain\n\r",     'g',gain);
  Serial.println();

  Serial.printf("%c %s date\n\r",      'd',getDate(text));
  Serial.printf("%c %s time\n\r",      't',getTime(text));
  Serial.println();

  Serial.printf("%c %d acq_on\n\r",    'o', a_on);
  Serial.printf("%c %d acq_off\n\r",   'f', a_off);
  Serial.printf("%c %d file_on\n\r",   'c', t_on);
  
  Serial.println();
  Serial.println("exter 'a' to print this");
  Serial.println("exter '?c' to read value c=(g,d,t)");
  Serial.println("  e.g.: ?g will print gain");
  Serial.println("exter '!cval' to write value c=(g,d,t) and val is new value");
  Serial.println("  e.g.: !g1 will set gain to 1<<1");
  Serial.println("exter 'xval' to exit menu (x is delay in minutes, -1 means immediate)");
  Serial.println("  e.g.: x10 will exit and hibernate for 10 minutes");
  Serial.println("        x-1 with exit and start immediately");
  Serial.println("exter ':c' to exter system command c=(s,c,q,d,l,r,e)");
  Serial.println("  e.g ':s' to stop acquisition");
  Serial.println("      ':c' to continue acquisition");
  Serial.println("      ':p' restart program");
  Serial.println("      ':l' to list files in current directory");
  Serial.println("      ':dxxx;' change directory (e.g.: :d/; change to root directory");
  Serial.println("      ':rxxx;' to retrieve file xxx");
  Serial.println("      ':exxx;' to erase file xxx");
  Serial.println();
}

static void doMenu1(void)
{ // for enquiries
    while(!Serial.available());
    char c=Serial.read();
    
    if (strchr("gdtofc", c))
    { switch (c)
      {
        case 'g': Serial.printf("%d\r\n",gain); break;
        //
        case 'd': Serial.printf("%s\r\n",getDate(text));break;
        case 't': Serial.printf("%s\r\n",getTime(text));break;
        //
        case 'o': Serial.printf("%d\r\n",a_on); break;
        case 'f': Serial.printf("%d\r\n",a_off); break;
        case 'c': Serial.printf("%d\r\n",t_on); break;
      }
    }
}

static void doMenu2(void)
{ // for settings
    uint16_t year,month,day,hour,minutes,seconds;
    //
    while(!Serial.available());
    char c=Serial.read();
        
    if (strchr("gpdtofc", c))
    { switch (c)
      { 
        case 'g': gain   = boundaryCheck(Serial.parseInt(),SHIFT,24); break;
        //
        case 'd':     
                  year=   boundaryCheck(Serial.parseInt(),2000,3000);
                  month=  boundaryCheck(Serial.parseInt(),1,12);
                  day=    boundaryCheck(Serial.parseInt(),1,31);
                  setDate(year,month,day);
                  break;
        case 't': 
                  hour=     boundaryCheck(Serial.parseInt(),0,23);
                  minutes=  boundaryCheck(Serial.parseInt(),0,59);
                  seconds=  boundaryCheck(Serial.parseInt(),0,59);
                  setTime(hour,minutes,seconds);
                  break;
        //
        case 'o': a_on    = boundaryCheck(Serial.parseInt(),0,3600); break;
        case 'f': a_off   = boundaryCheck(Serial.parseInt(),0,3600); break;
        case 'c': t_on    = boundaryCheck(Serial.parseInt(),0,3600); break;
      }
    }  
}

/*
 * additional menu items
 * s: stop acquisition
 * c: continue acquisition
 * d: change directory
 * l: list files
 * r: read (download) files
 * e: erase files
 */
void serialCheck(void)
{
  Serial.print(';'); Serial.flush(); while(!Serial.available()); Serial.read(); delay(1);
}

extern int16_t do_acq;

static inline void do_get_string(char c, char *str)
{
  do { while(!Serial.available()); *str++ = Serial.read(); } while (*(str-1) != c);  *(str-1)=0;
}

static void doMenu3(void)
{
    while(!Serial.available());
    char c=Serial.read();
    
    if (strchr("sc?", c))
    { switch (c)
      {
        case 's': // stop acquisition
        { do_acq=0;
          Serial.println("Stop");
          break;
        }
        case 'c': // continue acquisition
        { do_acq=1;
          Serial.println("Start");
          break;
        }
        case '?': // continue acquisition
        { // provide statistics
          break;
        }
      }
    }
}



int16_t doMenu(void)
{
  int16_t ret=0;
  do
  {
    while(!Serial.available());
    char c=Serial.read();
    
    if (strchr("?!xa:", c))
    { switch (c)
      {
        case '?': doMenu1(); break;
        case '!': doMenu2(); break;
        case 'x': ret = Serial.parseInt(); break;
        case 'a': printAll(); break;
        case ':': doMenu3(); break;
      }
    }
  } while(ret==0);
  return ret;
} 

int16_t doMenux(void)
{ 
  if(!Serial.available()) return;
  
    char c=Serial.read();
    
    if (strchr("?!xa:", c))
    { switch (c)
      {
        case '?': doMenu1(); break;
        case '!': doMenu2(); break;
        case 'a': printAll(); break;
        case ':': doMenu3(); break;
      }
    }
} 

#endif
