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
 
#ifndef _HIBERNATE_H
#define _HIBERNATE_H

 // derived from duff's lowpower modes
 // 06-jun-17: changed some compiler directives
 //            added high speed mode of K66
 // using hibernate disconnects from USB so serial monitor will break (pre TD 1.42)
 
#include "core_pins.h"

/******************* Seting Alarm **************************/
#ifdef __MK66FX1M0__

#define RTC_IER_TAIE_MASK       0x4u
#define RTC_SR_TAF_MASK         0x4u

void rtcSetup(void)
{
   SIM_SCGC6 |= SIM_SCGC6_RTC;// enable RTC clock
   RTC_CR |= RTC_CR_OSCE;// enable RTC
}

void rtcSetAlarm(uint32_t nsec)
{ // set alarm nsec seconds in the future
   RTC_TAR = RTC_TSR + nsec;
   RTC_IER |= RTC_IER_TAIE_MASK;
}

/********************LLWU**********************************/
#define LLWU_ME_WUME5_MASK       0x20u
#define LLWU_F3_MWUF5_MASK       0x20u
#define LLWU_MF5_MWUF5_MASK      0x20u

static void llwuISR(void)
{
    //
#if defined(HAS_KINETIS_LLWU_32CH)
    LLWU_MF5 |= LLWU_MF5_MWUF5_MASK; // clear source in LLWU Flag register
#else
    LLWU_F3 |= LLWU_F3_MWUF5_MASK; // clear source in LLWU Flag register
#endif
    //
    RTC_IER = 0;// clear RTC interrupts
}

static void llwuSetup(void)
{
  attachInterruptVector( IRQ_LLWU, llwuISR );
  NVIC_SET_PRIORITY( IRQ_LLWU, 2*16 );
//
  NVIC_CLEAR_PENDING( IRQ_LLWU );
  NVIC_ENABLE_IRQ( IRQ_LLWU );
//
  LLWU_PE1 = 0;
  LLWU_PE2 = 0;
  LLWU_PE3 = 0;
  LLWU_PE4 = 0;
#if defined(HAS_KINETIS_LLWU_32CH)
  LLWU_PE5 = 0;
  LLWU_PE6 = 0;
  LLWU_PE7 = 0;
  LLWU_PE8 = 0;
#endif
  LLWU_ME  = LLWU_ME_WUME5_MASK; //rtc alarm
//   
    SIM_SOPT1CFG |= SIM_SOPT1CFG_USSWE;
    SIM_SOPT1 |= SIM_SOPT1_USBSSTBY;
//
    PORTA_PCR0 = PORT_PCR_MUX(0);
    PORTA_PCR1 = PORT_PCR_MUX(0);
    PORTA_PCR2 = PORT_PCR_MUX(0);
    PORTA_PCR3 = PORT_PCR_MUX(0);

    PORTB_PCR2 = PORT_PCR_MUX(0);
    PORTB_PCR3 = PORT_PCR_MUX(0);
}

/********************* go to deep sleep *********************/
#define SMC_PMPROT_AVLLS_MASK   0x2u
#define SMC_PMCTRL_STOPM_MASK   0x7u
#define SCB_SCR_SLEEPDEEP_MASK  0x4u

// see SMC section (e.g. p 339 of K66) 
#define VLLS3 0x3 // RAM retained I/O states held
#define VLLS2 0x2 // RAM partially retained
#define VLLS1 0x1 // I/O states held
#define VLLS0 0x0 // all stop

#define VLLS_MODE VLLS0
static void gotoSleep(void)
{  
//  /* Make sure clock monitor is off so we don't get spurious reset */
   MCG_C6 &= ~MCG_C6_CME0;
   //

// if K66 is running in highspeed mode (>120 MHz) reduce speed
// is defined in kinetis.h and mk20dx128c
#if defined(HAS_KINETIS_HSRUN) && F_CPU > 120000000
    kinetis_hsrun_disable( );
#endif   
   /* Write to PMPROT to allow all possible power modes */
   SMC_PMPROT = SMC_PMPROT_AVLLS_MASK;
   /* Set the STOPM field to 0b100 for VLLSx mode */
   SMC_PMCTRL &= ~SMC_PMCTRL_STOPM_MASK;
   SMC_PMCTRL |= SMC_PMCTRL_STOPM(0x4); // VLLSx

   SMC_VLLSCTRL =  SMC_VLLSCTRL_VLLSM(VLLS_MODE);
   /*wait for write to complete to SMC before stopping core */
   (void) SMC_PMCTRL;

   SYST_CSR &= ~SYST_CSR_TICKINT;      // disable systick timer interrupt
   SCB_SCR |= SCB_SCR_SLEEPDEEP_MASK;  // Set the SLEEPDEEP bit to enable deep sleep mode (STOP)
       asm volatile( "wfi" );  // WFI instruction will start entry into STOP mode
   // will never return, but wake-up results in call to ResetHandler() in mk20dx128.c
}

void setWakeupCallandSleep(uint32_t nsec)
{  // set alarm to nsec secods in future and go to hibernate
   rtcSetup();
   llwuSetup();
   
   rtcSetAlarm(nsec);
   yield();
   gotoSleep();
}
#endif

#ifdef __IMXRT1062__
/*********************************************************************************/
// see also https://github.com/manitou48/teensy4/blob/master/rtc.ino
#define SNVS_DEFAULT_PGD_VALUE (0x41736166U)
#define SNVS_LPSR_PGD_MASK                       (0x8U)
#define SNVS_LPSRTCMR      (IMXRT_SNVS.offset050)
#define SNVS_LPSRTCLR      (IMXRT_SNVS.offset054)
//----------------------------------------------------
#define SNVS_LPTAR        (IMXRT_SNVS.offset058)

#define SNVS_LPCR_SRTC_ENV_MASK         (0x1U)
#define SNVS_LPCR_SRTC_ENV(x)           (((uint32_t)(((uint32_t)(x)) << 0U)) & SNVS_LPCR_SRTC_ENV_MASK)

#define SNVS_LPCR_LPTA_EN_MASK          (0x2U)
#define SNVS_LPCR_LPTA_EN(x)            (((uint32_t)(((uint32_t)(x)) << 1U)) & SNVS_LPCR_LPTA_EN_MASK)

#define SNVS_LPCR_MC_ENV_MASK           (0x4U)
#define SNVS_LPCR_MC_ENV(x)             (((uint32_t)(((uint32_t)(x)) << 2U)) & SNVS_LPCR_MC_ENV_MASK)

#define SNVS_LPCR_LPWUI_EN_MASK         (0x8U)
#define SNVS_LPCR_LPWUI_EN(x)           (((uint32_t)(((uint32_t)(x)) << 3U)) & SNVS_LPCR_LPWUI_EN_MASK)

#define SNVS_LPCR_SRTC_INV_EN_MASK      (0x10U)
#define SNVS_LPCR_SRTC_INV_EN(x)        (((uint32_t)(((uint32_t)(x)) << 4U)) & SNVS_LPCR_SRTC_INV_EN_MASK)

#define SNVS_LPCR_DP_EN_MASK            (0x20U)
#define SNVS_LPCR_DP_EN(x)              (((uint32_t)(((uint32_t)(x)) << 5U)) & SNVS_LPCR_DP_EN_MASK)

#define SNVS_LPCR_TOP_MASK              (0x40U)
#define SNVS_LPCR_TOP(x)                (((uint32_t)(((uint32_t)(x)) << 6U)) & SNVS_LPCR_TOP_MASK)

#define SNVS_LPCR_PWR_GLITCH_EN_MASK    (0x80U)
#define SNVS_LPCR_PWR_GLITCH_EN(x)      (((uint32_t)(((uint32_t)(x)) << 7U)) & SNVS_LPCR_PWR_GLITCH_EN_MASK)

#define SNVS_LPCR_LPCALB_EN_MASK        (0x100U)
#define SNVS_LPCR_LPCALB_EN(x)          (((uint32_t)(((uint32_t)(x)) << 8U)) & SNVS_LPCR_LPCALB_EN_MASK)

#define SNVS_LPCR_LPCALB_VAL_MASK       (0x7C00U)
#define SNVS_LPCR_LPCALB_VAL(x)         (((uint32_t)(((uint32_t)(x)) << 10U)) & SNVS_LPCR_LPCALB_VAL_MASK)

#define SNVS_LPCR_BTN_PRESS_TIME_MASK   (0x30000U)
#define SNVS_LPCR_BTN_PRESS_TIME(x)     (((uint32_t)(((uint32_t)(x)) << 16U)) & SNVS_LPCR_BTN_PRESS_TIME_MASK)

#define SNVS_LPCR_DEBOUNCE_MASK         (0xC0000U)
#define SNVS_LPCR_DEBOUNCE(x)           (((uint32_t)(((uint32_t)(x)) << 18U)) & SNVS_LPCR_DEBOUNCE_MASK)

#define SNVS_LPCR_ON_TIME_MASK          (0x300000U)
#define SNVS_LPCR_ON_TIME(x)            (((uint32_t)(((uint32_t)(x)) << 20U)) & SNVS_LPCR_ON_TIME_MASK)

#define SNVS_LPCR_PK_EN_MASK            (0x400000U)
#define SNVS_LPCR_PK_EN(x)              (((uint32_t)(((uint32_t)(x)) << 22U)) & SNVS_LPCR_PK_EN_MASK)

#define SNVS_LPCR_PK_OVERRIDE_MASK      (0x800000U)
#define SNVS_LPCR_PK_OVERRIDE(x)        (((uint32_t)(((uint32_t)(x)) << 23U)) & SNVS_LPCR_PK_OVERRIDE_MASK)

#define SNVS_LPCR_GPR_Z_DIS_MASK        (0x1000000U)
#define SNVS_LPCR_GPR_Z_DIS(x)          (((uint32_t)(((uint32_t)(x)) << 24U)) & SNVS_LPCR_GPR_Z_DIS_MASK)

#define SNVS_LPSR_LPTA                  (0x1U)

void rtc_init() 
{ CCM_CCGR2 |= CCM_CCGR2_IOMUXC_SNVS(CCM_CCGR_ON);
  SNVS_LPGPR = SNVS_DEFAULT_PGD_VALUE;
  SNVS_LPSR = SNVS_LPSR_PGD_MASK;
  // ? calibration
  // ? tamper pins
  
  SNVS_LPCR &= ~SNVS_LPCR_LPTA_EN_MASK; // clear alarm
  while (SNVS_LPCR & SNVS_LPCR_LPTA_EN_MASK); 
  SNVS_LPTAR=0;

  SNVS_LPCR |= 1;             // start RTC
  while (!(SNVS_LPCR & 1));
}

extern void *__rtc_localtime; 
void rtc_set_time(uint32_t secs) 
{ //uint32_t secs = 1547051415;
  SNVS_LPCR &= ~1;   // stop RTC
  while (SNVS_LPCR & 1);
  SNVS_LPSRTCMR = (uint32_t)(secs >> 17U);
  SNVS_LPSRTCLR = (uint32_t)(secs << 15U);
  SNVS_LPCR |= 1;             // start RTC
  while (!(SNVS_LPCR & 1));
}

uint32_t rtc_secs() {
  uint32_t seconds = 0;
  uint32_t tmp = 0;

  /* Do consecutive reads until value is correct */
  do
  { seconds = tmp;
    tmp = (SNVS_LPSRTCMR << 17U) | (SNVS_LPSRTCLR >> 15U);
  } while (tmp != seconds);

  return seconds;
}

void rtc_stopAlarm()
{
  SNVS_LPSR |= 1;
  SNVS_LPCR &= ~SNVS_LPCR_LPTA_EN_MASK;
  while (SNVS_LPCR & SNVS_LPCR_LPTA_EN_MASK); 
}
void rtc_isr(void)
{ 
  SNVS_LPSR |= 1;
  Serial.println("Alarm");
  asm("DSB");
}

void rtc_initAlarm(uint32_t prio=13)
{
  SNVS_LPSR |= 1;
  attachInterruptVector(IRQ_SNVS_IRQ, rtc_isr);
  NVIC_SET_PRIORITY(IRQ_SNVS_IRQ, prio*16); // 8 is normal priority
  NVIC_DISABLE_IRQ(IRQ_SNVS_IRQ);
}

void rtc_setAlarm(uint32_t alarmSeconds)
{   uint32_t tmp = SNVS_LPCR; //save control register

    /* disable SRTC alarm interrupt */
    rtc_stopAlarm();

    SNVS_LPTAR=alarmSeconds;
    while(SNVS_LPTAR != alarmSeconds);

    NVIC_ENABLE_IRQ(IRQ_SNVS_IRQ);

    SNVS_LPCR = tmp | SNVS_LPCR_LPTA_EN_MASK; // restore control register and set alarm
    while(!(SNVS_LPCR & SNVS_LPCR_LPTA_EN_MASK)); 
}

uint32_t rtc_getAlarm()
{
  return SNVS_LPTAR;  
}

void doShutdown(void) 
{ 
  SNVS_LPCR |=(1<<6);
}

void setWakeupCallandSleep(uint32_t nsec)
{
  rtc_init();
  rtc_set_time(now());   //LPSRTC will start at 0 otherwise
  rtc_initAlarm(13);
  rtc_setAlarm(nsec);
  doShutdown();
}
#endif
#endif