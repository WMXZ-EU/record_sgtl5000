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
 
/*
 * NOTE: changing frequency impacts the macros 
 *      AudioProcessorUsage and AudioProcessorUsageMax
 * defined in stock AudioStream.h
 */

#ifndef _SGTL5000_MODS_H
#define _SGTL5000_MODS_H
 
#include "core_pins.h"
#include "Wire.h"

// ********************************************** following is to change SGTL5000 samling rates ********************
#define SGTL5000_I2C_ADDR  0x0A  // CTRL_ADR0_CS pin low (normal configuration)
#define CHIP_DIG_POWER		0x0002
#define CHIP_CLK_CTRL     0x0004
#define CHIP_I2S_CTRL     0x0006
#define CHIP_ANA_POWER    0x0030 

unsigned int chipRead(unsigned int reg)
{
  unsigned int val;
  Wire.beginTransmission(SGTL5000_I2C_ADDR);
  Wire.write(reg >> 8);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return 0;
  if (Wire.requestFrom(SGTL5000_I2C_ADDR, 2) < 2) return 0;
  val = Wire.read() << 8;
  val |= Wire.read();
  return val;
}

bool chipWrite(unsigned int reg, unsigned int val)
{
  Wire.beginTransmission(SGTL5000_I2C_ADDR);
  Wire.write(reg >> 8);
  Wire.write(reg);
  Wire.write(val >> 8);
  Wire.write(val);
  if (Wire.endTransmission() == 0) return true;
  return false;
}

unsigned int chipModify(unsigned int reg, unsigned int val, unsigned int iMask)
{
  unsigned int val1 = (chipRead(reg)&(~iMask))|val;
  if(!chipWrite(reg,val1)) return 0;
  return val1;
}

void SGTL5000_modification(uint32_t fs_mode)
{ int sgtl_mode=(fs_mode-2); 
  if(sgtl_mode>3) sgtl_mode = 3; 
  if(sgtl_mode<0) sgtl_mode = 0;
  
//  write(CHIP_CLK_CTRL, 0x0004);  // 44.1 kHz, 256*Fs
//	write(CHIP_I2S_CTRL, 0x0130); // SCLK=32*Fs, 16bit, I2S format
  chipWrite(CHIP_CLK_CTRL, (sgtl_mode<<2));  // 256*Fs| sgtl_mode = 0:32 kHz; 1:44.1 kHz; 2:48 kHz; 3:96 kHz
}

void SGTL5000_enable(void)
{
  chipWrite(CHIP_ANA_POWER, 0x40FF); 
  chipWrite(CHIP_DIG_POWER, 0x0073); 
}

void SGTL5000_disable(void)
{
  chipWrite(CHIP_DIG_POWER, 0); 
  chipWrite(CHIP_ANA_POWER, 0); 
}

#endif
