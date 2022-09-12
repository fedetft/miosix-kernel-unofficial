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
#include "interfaces-impl/correction_types.h"

using namespace miosix;

namespace miosix {

static TimerProxySpec * timerProxy = &TimerProxySpec::instance();

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
    Rtc::instance().IRQinit();
    //rtc->IRQinit();
    #endif

    timerProxy->IRQinit(); // inits HSC and correction stack
}

void IRQosTimerSetTime(long long ns) noexcept
{
    timerProxy->IRQsetTimeNs(ns);
    // TODO: (s) also RTC for VHT alignment
}

// time assumed not to be on the past, checked by caller
void IRQosTimerSetInterrupt(long long ns) noexcept
{
    timerProxy->IRQsetIrqNs(ns);
}

// TODO: (s) make it IRQ
// time assumed not to be on the past, checked by caller
void osTimerSetEvent(long long ns) noexcept
{
    timerProxy->waitEvent(ns);
}

unsigned int osTimerGetFrequency()
{
    InterruptDisableLock dLock;
    return timerProxy->IRQTimerFrequency();
}

} //namespace internal

} //namespace miosix 

#define TIMER_INTERRUPT_DEBUG

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
    static miosix::TimerProxySpec * timerProxy = &miosix::TimerProxySpec::instance();
    static miosix::Hsc * hsc = timerProxy->getHscReference();

    // second part of output compare for timer or event match. If we reached this interrupt, it means
    // we already matched the upper part of the timer and we have now matched the lower part.
    
    //hsc->IRQhandler();

    #ifdef TIMER_INTERRUPT_DEBUG
    if(hsc->IRQgetMatchFlag() || hsc->lateIrq) miosix::ledOff();
    #endif

    if(hsc->IRQgetMatchFlag() || hsc->lateIrq || hsc->IRQgetOverflowFlag())
        hsc->IRQhandler();

    if(hsc->IRQgetEventFlag() || hsc->lateEvent)
    {
        Hsc::IRQclearEventFlag();
        long long tick = hsc->IRQgetTimeTick();
        if(tick >= hsc->IRQgetEventTick())
        {
            timerProxy->signalEventTimeout();
            hsc->lateEvent = false;
        }
    }
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

// Each thread is scheduled according to the scheduler every 10ms.
// It can happen that we've recieved a TIMER2 interrupt and we set TIMER1 for lower part of timer match
// and, while still waiting, we get a request to sleep. IRQsetosInterrupt is called that
// re-sets the next interrupt as now + sleepTime and enters in sleepCpu with interrupt disabled
// after clearing the match flag and resetting TIMER1.
// TIMER1 is not called, TIMER2 is then re-called when sleep finishes, as expected.
// TIMER2 sets TIMER1 and waits, as expected.
// This is why i cannot set both timers when setting the next osinterrupt but i have to set them progressively
// everytime i get upper tick match. So the algorithm is now as follows:
// set TIMER2 CC --ISR--> set TIMER1CC
// Only problem is if we have sleep less than then time that intercurs between the two interrups
// but since it's maximum (2^16)-1 we have maximum 1.36ms. Also, if we have lowerTimerCounter >= hsc->IRQgetNextCCticksLower()
// it may cause problems if we're calling TIMER2 twice for the sleep while waiting for TIMER1?
void __attribute__((used)) TIMER2_IRQHandlerImpl()
{
    static miosix::Hsc * hsc = &miosix::Hsc::instance();

    // save value of TIMER1 counter right away to check for compare later
    unsigned int lowerTimerCounter = TIMER1->CNT;

    // TIMER2 overflow, pending bit trick.
    if(hsc->IRQgetOverflowFlag()) hsc->IRQhandler();

    // if just overflow, no TIMER2 counter register match, return
    //if(!(TIMER2->IF & TIMER_IF_CC0)) return;
    
    // first part of output compare for timer match, disable output compare interrupt
    // for TIMER2 and turn of output comapre interrupt of TIMER1
    if(TIMER2->IF & TIMER_IF_CC0)
    {
        #ifdef TIMER_INTERRUPT_DEBUG
        miosix::ledOn();
        #endif

        // disable output compare interrupt on channel 0 for most significant timer
        TIMER2->CC[0].CCV = 0;
        TIMER2->IEN &= ~TIMER_IEN_CC0;
        TIMER2->CC[0].CTRL &= ~TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;
        TIMER2->IFC |= TIMER_IFC_CC0;
        

        // set and enable output compare interrupt on channel 0 for least significant timer
        TIMER1->CC[0].CCV = hsc->IRQgetNextCCticksLower(); // underflow handling
        TIMER1->IEN |= TIMER_IEN_CC0;
        TIMER1->CC[0].CTRL |= TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;

        // if by the time we get here, the lower part of the counter has already matched
        // the counter register or got past it, call TIMER1 handler directly instead of waiting
        // for the other round of compare
        if(lowerTimerCounter >= hsc->IRQgetNextCCticksLower()) 
        {
            TIMER1->IFS |= TIMER_IFS_CC0;
            NVIC_SetPendingIRQ(TIMER1_IRQn); //TIMER1_IRQHandler();
        }
    }

    // first part of output compare for event match, disable output compare interrupt
    // for TIMER2 and turn of output comapre interrupt of TIMER1
    if(TIMER2->IF & TIMER_IF_CC1)
    {
        // disable output compare interrupt on channel 1 for most significant timer
        TIMER2->IEN &= ~TIMER_IEN_CC1;
        TIMER2->CC[1].CTRL &= ~TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;
        TIMER2->IFC |= TIMER_IFC_CC1;

        // set and enable output compare interrupt on channel 1 for least significant timer
        TIMER1->CC[1].CCV = hsc->IRQgetNextCCeventLower(); // underflow handling
        TIMER1->IEN |= TIMER_IEN_CC1;
        TIMER1->CC[1].CTRL |= TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;

        // if by the time we get here, the lower part of the counter has already matched
        // the counter register or got past it, call TIMER1 handler directly instead of waiting
        // for the other round of compare
        if(lowerTimerCounter >= hsc->IRQgetNextCCeventLower()) 
        {
            TIMER1->IFS |= TIMER_IFS_CC1;
            NVIC_SetPendingIRQ(TIMER1_IRQn); //TIMER1_IRQHandler();
        }
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
    long long syncPointActualHsc = hsc->upperTimeTick | hsc->IRQgetVhtTimerCounter();
    // handle lower part timer overflowed before vht lower part timer
    if(syncPointActualHsc > hsc->IRQgetTimeTick()) { syncPointActualHsc -= 1<<16; }

    // update vht correction controller
    if(vht->IRQisCorrectionEnabled())
    {
        vht->IRQupdate(syncPointActualHsc);
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