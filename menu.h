/* SGTL5000 Recorder for Teensy 
 * Copyright (c) 2020, Walter Zimmer
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

#ifndef _MENU_H_
#define _MENU_H

#include "core_pins.h"
#include "usb_serial.h"

#include "config.h"
int do_acq = 0;

static void printAll(void)
{
  Serial.println();
  Serial.println("exter 'a' to print this");
  Serial.println();
  Serial.println("exter '?c' to read value c=(g,d,t)");
  Serial.println("  e.g.: ?g will print gain");
  Serial.println();
  Serial.println("exter '!cv' to write value c=(g,d,t) and v is new value");
  Serial.println("  e.g.: !g1 will set gain to 1<<1");
  Serial.println();
  Serial.println("exter 'xv' to exit menu (v is delay in seconds, -1 means immediate)");
  Serial.println("  e.g.: x10 will exit and hibernate for 10 seconds");
  Serial.println("        x-1 with exit and start immediately");
  Serial.println();
  Serial.println("exter ':c' to exter system command c=(s,c)");
  Serial.println("  e.g ':s' to stop acquisition");
  Serial.println("      ':c' to continue acquisition");
  Serial.println();

}
static void doMenu1(void) // ?
{
    while(!Serial.available());
    char c=Serial.read();

}

static void doMenu2(void) // !
{   while(!Serial.available());
    char c=Serial.read();
}

static void doMenu3(void) // :
{
    while(!Serial.available());
    char c=Serial.read();
    
    if (strchr("sc", c))
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
      }
    }
}


int doMenu(void)
{  
    if(!Serial.available()) return 0;
    int ret=0;
    while(!ret)
    {
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
    }

    return ret;
}
#endif