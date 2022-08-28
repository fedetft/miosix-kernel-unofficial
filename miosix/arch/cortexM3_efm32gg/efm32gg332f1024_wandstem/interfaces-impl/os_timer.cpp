/***************************************************************************
 *   Copyright (C) 2016 by Fabiano Riccardi, Sasan                         *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   As a special exception, if other files instantiate templates or use   *
 *   macros or inline functions from this file, or you compile this file   *
 *   and link it with other works to produce a work based on this file,    *
 *   this file does not by itself cause the resulting work to be covered   *
 *   by the GNU General Public License. However the source code for this   *
 *   file must still be made available in accordance with the GNU General  *
 *   Public License. This exception does not invalidate any other reasons  *
 *   why a work based on this file might be covered by the GNU General     *
 *   Public License.                                                       *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, see <http://www.gnu.org/licenses/>   *
 ***************************************************************************/

#include "interfaces/os_timer.h"
#include "time/timeconversion.h"
#include "hsc.h"

#if defined(WITH_DEEP_SLEEP) || defined(WITH_VHT)
#include "rtc.h"
#endif

#ifdef WITH_VIRTUAL_CLOCK
#include "interfaces/virtual_clock.h"
#endif

#ifdef WITH_VHT
#include "interfaces/vht.h"
#endif


using namespace miosix;

namespace miosix {

#if defined(WITH_DEEP_SLEEP) || defined(WITH_VHT)
static Rtc *rtc = nullptr;
#endif

//static TimerProxy<Hsc, VirtualClock> * timerProxy = &TimerProxy<Hsc, VirtualClock>::instance();
using MyTimerProxy = TimerProxy<Hsc>;//, Vht<Hsc, Rtc>>;
static MyTimerProxy * timerProxy = &MyTimerProxy::instance();

long long getTime() noexcept
{
    InterruptDisableLock dLock;
    return IRQgetTime();
}

long long IRQgetTime() noexcept
{
    return timerProxy->IRQgetTimeNs();
}

namespace internal {

void IRQosTimerInit()
{
    // Note: order here is important. VHT expects a working and started RTC (TODO: (s) if not started, call init + start)
    #if defined(WITH_DEEP_SLEEP) || defined(WITH_VHT)
    rtc = &Rtc::instance();
    rtc->IRQinit();
    #endif

    timerProxy->IRQinit(); // inits HSC and correction stack
}

void IRQosTimerSetTime(long long ns) noexcept
{
    timerProxy->IRQsetTimeNs(ns);
}

// TODO: (s) check if in the past?
// FIXME: (s) quando faccio sleep, questa viene chiamata anche se devo andare in deep sleep! why?
void IRQosTimerSetInterrupt(long long ns) noexcept
{
    timerProxy->IRQsetIrqNs(ns);
}

unsigned int osTimerGetFrequency()
{
    InterruptDisableLock dLock;
    return timerProxy->IRQTimerFrequency();
}

} //namespace internal

} //namespace miosix 


/**
 * TIMER1 interrupt routine
 * 
 */
void __attribute__((naked)) TIMER1_IRQHandler()
{
    saveContext();
    asm volatile("bl _Z21TIMER1_IRQHandlerImplv");
    restoreContext();
}

void __attribute__((used)) TIMER1_IRQHandlerImpl()
{
    static miosix::Hsc * hsc = &miosix::Hsc::instance();

    // second part of output compare. If we reached this interrupt, it means
    // we already matched the upper part of the timer and we have now matched the lower part.
    hsc->IRQhandler();

    miosix::ledOff();
}

/**
 * TIMER2 interrupt routine
 * 
 */
void __attribute__((naked)) TIMER2_IRQHandler()
{
    saveContext();
    asm volatile("bl _Z21TIMER2_IRQHandlerImplv");
    restoreContext();
}

void __attribute__((used)) TIMER2_IRQHandlerImpl()
{
    static miosix::Hsc * hsc = &miosix::Hsc::instance();

    // save value of TIMER1 counter right away to check for compare later
    unsigned int lowerTimerCounter = TIMER1->CNT;

    // TIMER2 overflow, pending bit trick.
    if(hsc->IRQgetOverflowFlag()) hsc->IRQhandler();

    // if just overflow, no TIMER2 counter register match, return
    if(!(TIMER2->IF & TIMER_IF_CC0)) return;
    
    // first part of output compare, disable output compare interrupt
    // for TIMER2 and turn of output comapre interrupt of TIMER1
    miosix::ledOn();

    // disable output compare interrupt on channel 0 for most significant timer
    TIMER2->IEN &= ~TIMER_IEN_CC0;
    TIMER2->CC[0].CTRL &= ~TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;
    TIMER2->IFC |= TIMER_IFC_CC0;

    // if by the time we get here, the lower part of the counter has already matched
    // the counter register or got past it, call TIMER1 handler directly
    if(lowerTimerCounter >= TIMER1->CC[0].CCV) TIMER1_IRQHandler();
    else
    {
        // enable output compare interrupt on channel 0 for least significant timer
        TIMER1->IEN |= TIMER_IEN_CC0;
        TIMER1->CC[0].CTRL |= TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;
    }
}

/**
 * TIMER3 interrupt routine
 * 
 */
void __attribute__((naked)) TIMER3_IRQHandler()
{
    saveContext();
    asm volatile("bl _Z21TIMER3_IRQHandlerImplv");
    restoreContext();
}

void __attribute__((used)) TIMER3_IRQHandlerImpl()
{
    static miosix::Hsc * hsc = &miosix::Hsc::instance();

    #ifdef WITH_VHT
    
    static miosix::Vht<miosix::Hsc, miosix::Rtc> * vht = &miosix::Vht<miosix::Hsc, miosix::Rtc>::instance();
    // if-guard
    if(!hsc->IRQgetVhtMatchFlag()) return;

    // PRS captured RTC current value inside the CC_0 of the TIMER3 (lower part of timer)
    hsc->IRQclearVhtMatchFlag();
    
    // replace lower 32-bit part of timer with the VHT registered one
    long long baseActualHsc = hsc->upperTimeTick<<32 | hsc->IRQgetVhtTimerCounter(); //hsc->IRQgetTimeTick() - hsc->IRQgetTimerCounter() + hsc->IRQgetVhtTimerCounter();
    // handle lower part timer overflowed before vht lower part timer
    if(baseActualHsc > hsc->IRQgetTimeTick()) { baseActualHsc -= 1<<16; }

    // update vht correction controller
    if(vht->IRQisCorrectionEnabled())
    {
        vht->IRQupdate(baseActualHsc);
    }
    
    #endif

}

/**
 * RTC interrupt routine
 */
void __attribute__((naked)) RTC_IRQHandler()
{
    saveContext();
    asm volatile("bl _Z14RTChandlerImplv");
    restoreContext();
}

void __attribute__((used)) RTChandlerImpl()
{
    #if defined(WITH_DEEP_SLEEP) || defined(WITH_VHT)

    static miosix::Rtc * rtc = &miosix::Rtc::instance();
    
    // handle RTC overflow
    if(rtc->IRQgetOverflowFlag()) { rtc->IRQoverflowHandler(); }

    // VHT COMP1 register clear (already handled by PRS, just clear)
    #ifdef WITH_VHT
    if(rtc->IRQgetVhtMatchFlag()) { rtc->IRQclearVhtMatchFlag(); }
    #endif

    #endif
}