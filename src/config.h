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

uint32_t fsamps[] = {8000, 16000, 32000, 44100, 48000, 96000, 192000, 220500, 240000, 360000};
/*
 * NOTE: changing frequency impacts the macros 
 *      AudioProcessorUsage and AudioProcessorUsageMax
 * defined in stock AudioStream.h
 */

#define DO_DEBUG 1
#define FSI 5// desired sampling frequency index
#define NCH 1
#define SEL_LR 1  // record only a single channel (0 left, 1 right)

#define AUDIO_SELECT AUDIO_INPUT_LINEIN 
//#define AUDIO_SELECT AUDIO_INPUT_MIC

#if defined(__MK20DX256__)
  #define MQUEU (100/NCH) // number of buffers in aquisition queue
#elif defined(__MK64FX512__)
  #define MQUEU (200/NCH) // number of buffers in aquisition queue
#elif defined(__MK66FX1M0__)
  #define MQUEU (600/NCH) // number of buffers in aquisition queue
#else
  #define MQUEU 53 // number of buffers in aquisition queue
#endif
  

// definitions for logging
#define BUFFERSIZE (8*1024)

// times for acquisition and filing
uint32_t a_on = 60; // acquisition on time
uint32_t a_off = 0; // acquisition off time
uint32_t t_on = 20; // file on time

