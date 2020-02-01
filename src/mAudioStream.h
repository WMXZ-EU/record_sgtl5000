/*
 * SGTL5000 Recorder for Teensy 
 * WMXZ 2019
 */
/* Teensyduino Core Library
 * http://www.pjrc.com/teensy/
 * Copyright (c) 2017 PJRC.COM, LLC.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * 1. The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * 2. If the Software is incorporated into a build system that allows
 * selection among a list of target devices, then similar target
 * devices manufactured by PJRC.COM must be included in the list of
 * target devices and selectable in the same manner.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
// WMXZ: modified to have int32_t data types
// also increased block size by number of channels

#ifndef MAUDIOSTREAM_H
#define MAUDIOSTREAM_H

#include <stdio.h>  // for NULL
#include <string.h> // for memcpy

#include "core_pins.h"
#include "usb_serial.h"

// AUDIO_BLOCK_SAMPLES determines how many samples the audio library processes
// per update.  It may be reduced to achieve lower latency response to events,
// at the expense of higher interrupt and DMA setup overhead.
//
// Less than 32 may not work with some input & output objects.  Multiples of 16
// should be used, since some synthesis objects generate 16 samples per loop.
//
// Some parts of the audio library may have hard-coded dependency on 128 samples.
// Please report these on the forum with reproducible test cases.
// WMXZ: note: multiplication bt NCH
// WMXZ: note: different data size possible

#ifndef AUDIO_BLOCK_SAMPLES
  #if defined(__MK20DX128__) || defined(__MK20DX256__) \
      || defined(__MK64FX512__) || defined(__MK66FX1M0__) \
      || defined(__IMXRT1062__) 
    #define AUDIO_BLOCK_SAMPLES  128
  #elif defined(__MKL26Z64__)
    #define AUDIO_BLOCK_SAMPLES  64
  #endif
#endif

#ifndef NCH
  #define NCH 1
#endif

#ifndef NBYTE
  #define NBYTE 2
#endif

#define AUDIO_BLOCK_SAMPLES_NCH (AUDIO_BLOCK_SAMPLES*NCH)

#define mAudioMemory16(num) ({ \
	static audio_block_t audio_data[num]; \
  static int16_t audioBuffer[num*AUDIO_BLOCK_SAMPLES_NCH]; \
	mAudioStream::initialize_memory(audio_data, num, audioBuffer, sizeof(audioBuffer[0])); \
})

#define mAudioMemory32(num) ({ \
	static audio_block_t audio_data[num]; \
  static int32_t audioBuffer[num*AUDIO_BLOCK_SAMPLES_NCH]; \
	mAudioStream::initialize_memory(audio_data, num, audioBuffer, sizeof(audioBuffer[0])); \
})

#define mAudioMemoryUsageMax() (mAudioStream::memory_used_max)
#define mAudioMemoryUsageMaxReset() (mAudioStream::memory_used_max = mAudioStream::memory_used)

class mAudioStream;
class mAudioConnection;

typedef struct audio_block_struct {
  uint8_t  ref_count;
  uint8_t  dataSize;
  uint16_t memory_pool_index;
  void * data;
//  #if NBYTE==2
//    int16_t  data[AUDIO_BLOCK_SAMPLES_NCH];
//  #elif NBYTE==4
//    int32_t  data[AUDIO_BLOCK_SAMPLES_NCH];
//  #else
//    #error "wrong wordsize"
//  #endif
} audio_block_t;


class mAudioConnection
{
public:
  mAudioConnection(mAudioStream &source, mAudioStream &destination) :
    src(source), dst(destination), src_index(0), dest_index(0),
    next_dest(NULL)
    { connect(); }
  mAudioConnection(mAudioStream &source, unsigned char sourceOutput,
    mAudioStream &destination, unsigned char destinationInput) :
    src(source), dst(destination),
    src_index(sourceOutput), dest_index(destinationInput),
    next_dest(NULL)
    { connect(); }
  friend class mAudioStream;
protected:
  void connect(void);
  mAudioStream &src;
  mAudioStream &dst;
  unsigned char src_index;
  unsigned char dest_index;
  mAudioConnection *next_dest;
};

class mAudioStream
{
public:
  mAudioStream(unsigned char ninput, audio_block_t **iqueue) :
    num_inputs(ninput), inputQueue(iqueue) {
      active = false;
      destination_list = NULL;
      for (int i=0; i < num_inputs; i++) {
        inputQueue[i] = NULL;
      }
      // add to a simple list, for update_all
      // TODO: replace with a proper data flow analysis in update_all
      if (first_update == NULL) {
        first_update = this;
      } else {
        mAudioStream *p;
        for (p=first_update; p->next_update; p = p->next_update) ;
        p->next_update = this;
      }
      next_update = NULL;
    }

//  static void initialize_memory(audio_block_t *data, unsigned int num);
  static void initialize_memory(audio_block_t *data, unsigned int num, void *buffer, unsigned int element_size);
  static uint16_t memory_used;
  static uint16_t memory_used_max;
protected:
  bool active;
  unsigned char num_inputs;
  static audio_block_t * allocate(void);
  static void release(audio_block_t * block);
  void transmit(audio_block_t *block, unsigned char index = 0);
  audio_block_t * receiveReadOnly(unsigned int index = 0);
  audio_block_t * receiveWritable(unsigned int index = 0);
  static bool update_setup(int prio=13);
  static void update_stop(void);
  static void update_all(void) { NVIC_SET_PENDING(IRQ_SOFTWARE); }
  friend void msoftware_isr(void);
  friend class mAudioConnection;
private:
  mAudioConnection *destination_list;
  audio_block_t **inputQueue;
  static bool update_scheduled;
  virtual void update(void) = 0;
  static mAudioStream *first_update; // for update_all
  mAudioStream *next_update; // for update_all
  static audio_block_t *memory_pool;
  static uint32_t memory_pool_available_mask[];
  static uint16_t memory_pool_first_mask;

  static uint16_t dataSize;
};

#if NBYTE==2
  #if defined(__MKL26Z64__)
    #define MAX_AUDIO_MEMORY 6144
  #elif defined(__MK20DX128__)
    #define MAX_AUDIO_MEMORY 12288
  #elif defined(__MK20DX256__)
    #define MAX_AUDIO_MEMORY 49152
  #elif defined(__MK64FX512__)
    #define MAX_AUDIO_MEMORY 163840
  #elif defined(__MK66FX1M0__)
    #define MAX_AUDIO_MEMORY (882*260)
  #elif defined(__IMXRT1062__)
      #define MAX_AUDIO_MEMORY (1024*260)
  #endif
#elif NBYTE==4
  #if defined(__MKL26Z64__)
    #define MAX_AUDIO_MEMORY 6144
  #elif defined(__MK20DX128__)
    #define MAX_AUDIO_MEMORY 12288
  #elif defined(__MK20DX256__)
    #define MAX_AUDIO_MEMORY 49152
  #elif defined(__MK64FX512__)
    #define MAX_AUDIO_MEMORY 163840
  #elif defined(__MK66FX1M0__)
    #define MAX_AUDIO_MEMORY (444*516)
  #elif defined(__IMXRT1062__)
    #define MAX_AUDIO_MEMORY (512*516)
  #endif
#endif

#define MAX_BLOCKS (MAX_AUDIO_MEMORY / AUDIO_BLOCK_SAMPLES_NCH / NBYTE)
#define NUM_MASKS  ((MAX_BLOCKS + 31) / 32)

audio_block_t * mAudioStream::memory_pool;
uint32_t mAudioStream::memory_pool_available_mask[NUM_MASKS];
uint16_t mAudioStream::memory_pool_first_mask;

uint16_t mAudioStream::memory_used = 0;
uint16_t mAudioStream::memory_used_max = 0;

uint16_t mAudioStream::dataSize = 2;

// Set up the pool of audio data blocks
// placing them all onto the free list
//void mAudioStream::initialize_memory(audio_block_t *data, unsigned int num)
void mAudioStream::initialize_memory(audio_block_t *data, unsigned int num, void *dataBuffers, unsigned int element_size)
{
  unsigned int i;
  unsigned int maxnum = MAX_BLOCKS;

  dataSize=element_size;

  if (num > maxnum) num = maxnum;
  __disable_irq();
  memory_pool = data;
  memory_pool_first_mask = 0;
  for (i=0; i < NUM_MASKS; i++) {
    memory_pool_available_mask[i] = 0;
  }
  for (i=0; i < num; i++) {
    memory_pool_available_mask[i >> 5] |= (1 << (i & 0x1F));
  }
  for (i=0; i < num; i++) {
    data[i].memory_pool_index = i;
    data[i].dataSize = element_size;
    if (dataBuffers) { 
			data[i].data = (char *)dataBuffers + i*dataSize*AUDIO_BLOCK_SAMPLES; 
		}
  }
  __enable_irq();

}

// Allocate 1 audio data block.  If successful
// the caller is the only owner of this new block
audio_block_t * mAudioStream::allocate(void)
{
  uint32_t n, index, avail;
  uint32_t *p, *end;
  audio_block_t *block;
  uint16_t used;

  p = memory_pool_available_mask;
  end = p + NUM_MASKS;
  __disable_irq();
  index = memory_pool_first_mask;
  p += index;
  while (1) {
    if (p >= end) {
      __enable_irq();
      return NULL;
    }
    avail = *p;
    if (avail) break;
    index++;
    p++;
  }
  n = __builtin_clz(avail);
  avail &= ~(0x80000000 >> n);
  *p = avail;
  if (!avail) index++;
  memory_pool_first_mask = index;
  used = memory_used + 1;
  memory_used = used;
//  __enable_irq();
  index = p - memory_pool_available_mask;
  block = memory_pool + ((index << 5) + (31 - n));
  block->ref_count = 1;
  __enable_irq();

  if (used > memory_used_max) memory_used_max = used;
  return block;
}

// Release ownership of a data block.  If no
// other streams have ownership, the block is
// returned to the free pool
void mAudioStream::release(audio_block_t *block)
{
  //if (block == NULL) return;
  uint32_t mask = (0x80000000 >> (31 - (block->memory_pool_index & 0x1F)));
  uint32_t index = block->memory_pool_index >> 5;

  __disable_irq();
  if (block->ref_count > 1) {
    block->ref_count--;
  } else {
    memory_pool_available_mask[index] |= mask;
    if (index < memory_pool_first_mask) memory_pool_first_mask = index;
    memory_used--;
  }
  __enable_irq();
}

// Transmit an audio data block
// to all streams that connect to an output.  The block
// becomes owned by all the recepients, but also is still
// owned by this object.  Normally, a block must be released
// by the caller after it's transmitted.  This allows the
// caller to transmit to same block to more than 1 output,
// and then release it once after all transmit calls.
void mAudioStream::transmit(audio_block_t *block, unsigned char index)
{
  for (mAudioConnection *c = destination_list; c != NULL; c = c->next_dest) {
    if (c->src_index == index) {
      if (c->dst.inputQueue[c->dest_index] == NULL) {
        c->dst.inputQueue[c->dest_index] = block;
        block->ref_count++;
      }
    }
  }
}


// Receive block from an input.  The block's data
// may be shared with other streams, so it must not be written
audio_block_t * mAudioStream::receiveReadOnly(unsigned int index)
{
  audio_block_t *in;

  if (index >= num_inputs) return NULL;
  in = inputQueue[index];
  inputQueue[index] = NULL;
  return in;
}

// Receive block from an input.  The block will not
// be shared, so its contents may be changed.
audio_block_t * mAudioStream::receiveWritable(unsigned int index)
{
  audio_block_t *in, *p;

  if (index >= num_inputs) return NULL;
  in = inputQueue[index];
  inputQueue[index] = NULL;
  if (in && in->ref_count > 1) {
    p = allocate();
    if (p) memcpy(p->data, in->data, p->dataSize * AUDIO_BLOCK_SAMPLES);
    in->ref_count--;
    in = p;
  }
  return in;
}


void mAudioConnection::connect(void)
{
  mAudioConnection *p;

  if (dest_index > dst.num_inputs) return;
  __disable_irq();
  p = src.destination_list;
  if (p == NULL) {
    src.destination_list = this;
  } else {
    while (p->next_dest) p = p->next_dest;
    p->next_dest = this;
  }
  src.active = true;
  dst.active = true;
  __enable_irq();
}



// When an object has taken responsibility for calling update_all()
// at each block interval (approx 2.9ms), this variable is set to
// true.  Objects that are capable of calling update_all(), typically
// input and output based on interrupts, must check this variable in
// their constructors.
bool mAudioStream::update_scheduled = false;

void msoftware_isr(void);
bool mAudioStream::update_setup(int prio)
{
  if (update_scheduled) return false;
  attachInterruptVector(IRQ_SOFTWARE, msoftware_isr);
  NVIC_SET_PRIORITY(IRQ_SOFTWARE, prio*16); // 255 = lowest priority
  NVIC_ENABLE_IRQ(IRQ_SOFTWARE);
  update_scheduled = true;
  return true;
}

void mAudioStream::update_stop(void)
{
  NVIC_DISABLE_IRQ(IRQ_SOFTWARE);
  update_scheduled = false;
}

mAudioStream * mAudioStream::first_update = NULL;

void msoftware_isr(void) // AudioStream::update_all()
{
  mAudioStream *p;

  for (p = mAudioStream::first_update; p; p = p->next_update) {
    if (p->active) {
      p->update();
      // TODO: traverse inputQueueArray and release
      // any input blocks that weren't consumed?
    }
  }
}
#endif
