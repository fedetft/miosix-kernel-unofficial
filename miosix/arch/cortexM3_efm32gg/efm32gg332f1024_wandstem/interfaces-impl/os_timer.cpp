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
#include "interfaces/hw_eventstamping.h"
#include <atomic>

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
void osTimerSetEvent(long long ns) noexcept // DELETEME: (s)
{
    //timerProxy->waitEvent(ns);
}

unsigned int osTimerGetFrequency()
{
    InterruptDisableLock dLock;
    return timerProxy->IRQTimerFrequency();
}

} //namespace internal

} //namespace miosix 

#define TIMER_INTERRUPT_DEBUG

std::atomic<bool> timeToTrigger(false);

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

    // timer flag (CC[0])
    if(hsc->IRQgetMatchFlag() || hsc->lateIrq || hsc->IRQgetOverflowFlag())
        hsc->IRQhandler();

    // event flag timeout or trigger (CC[1]) 
    if(hsc->IRQgetEventFlag() || hsc->lateEvent)
    {
        Hsc::IRQclearEventFlag();
        long long tick = hsc->IRQgetTimeTick();
        if(tick >= hsc->IRQgetEventTick())
        {
            // case timeout
            if(events::getEventDirection() == events::EventDirection::INPUT)
            {
                events::IRQsignalEventTimeout();
                hsc->lateEvent = false;
            }
            // case trigger, wait for "+1" in upper TIMER2 part to trigger STXON with alternate function
            else if(events::getEventDirection() == events::EventDirection::OUTPUT)
            {
                // set and enable output compare interrupt on channel 1 for most significant timer
                if(TIMER2->CC[1].CCV != 0) TIMER2->CC[1].CCV += 1;
                TIMER2->IEN |= TIMER_IEN_CC1; // to wake up waiting thread
                TIMER2->CC[1].CTRL |= TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;

                // connect TIMER2->CC1 to pin PA9 (stxon) on #0
                TIMER2->ROUTE |= TIMER_ROUTE_CC1PEN;
                TIMER2->CC[1].CTRL |= TIMER_CC_CTRL_CMOA_SET;
                TIMER2->ROUTE |= TIMER_ROUTE_LOCATION_LOC0;

                timeToTrigger = true;
            }
            else ; // unkonwn state, shouldn never reach here
        }
        // FIXME: (s) se tick <= devo resettare il TIMER2! 
        // magari ho interrupt y<<16 | z e sono a x<<32 | y<<16 | z ma devo aspettaer (x+1)<<32 | y<<16 | z
    }

    // only enabled TIMER2 IRQ, both on same PRS
    /*if(TIMER1->IF & TIMER_IF_CC2)
    {
        TIMER1->IFC |= TIMER_IFC_CC2;
        events::IRQsignalEvent();
    }*/
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

    // first part of output compare for event match or timeout
    if(TIMER2->IF & TIMER_IF_CC1)
    {   
        // disable output compare interrupt on channel 1 for most significant timer
        TIMER2->IEN &= ~TIMER_IEN_CC1;
        TIMER2->CC[1].CTRL &= ~TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;
        TIMER2->IFC |= TIMER_IFC_CC1;


        // specific case in which our timeout is to trigger wire target GPIO to TIMER1 interrupt to fire 
        // because the alternate function for the GPIO connected to the STXON is
        // connected only to TIMER2 CC1, we have to wait for ticks-1 on the upper part,
        // then wait for the lower part and then wait for the next upper part again 
        //  TIMER2---ISR--->TIMER1---ISR--->TIMER2-->trigger on STXON
        // (upper-1)        (lower)          (+1)
        if(events::getEventDirection() == events::EventDirection::OUTPUT && timeToTrigger)
        {
            events::IRQsignalEventTimeout();
            timeToTrigger = false;

            // disconnect TIMER2->CC1 to pin PA9 (stxon)
            TIMER2->ROUTE &= ~TIMER_ROUTE_CC1PEN;
            TIMER2->CC[1].CTRL &= ~TIMER_CC_CTRL_CMOA_SET;
            TIMER2->ROUTE &= ~TIMER_ROUTE_LOCATION_LOC0;
        }
        // disable output compare interrupt for TIMER2 and turn of output comapre interrupt of TIMER1
        else // event direciton INPUT or just not the right time to trigger
        {
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

    // Timestamping, wake up thread waiting for timestamp
    if(TIMER2->IF & TIMER_IF_CC2)
    {
        TIMER2->IFC |= TIMER_IFC_CC2;
        events::IRQsignalEvent();
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
    if(!hsc->IRQgetVhtMatchFlag()) return; // FIXME: (s) check, shouldn't it be flag from TIMER3?

    // PRS captured RTC current value inside the CC_0 of the TIMER3 (lower part of timer)
    hsc->IRQclearVhtMatchFlag();
    
    // replace lower 32-bit part of timer with the VHT registered one
    long long syncPointActualHsc = hsc->upperTimeTick | hsc->IRQgetVhtTimerCounter();
    // handle lower part timer overflowed before vht lower part timer
    if(syncPointActualHsc > hsc->IRQgetTimeTick()) { syncPointActualHsc -= 1<<16; }

    // update vht correction controller
    if(vht->IRQisCorrectionEnabled())
        vht->IRQupdate(syncPointActualHsc);

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