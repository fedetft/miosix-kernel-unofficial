/***************************************************************************
 *   Copyright (C) 2022 by Fabiano Riccardi, Sasan, Alessandro Sorrentino  *
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
#include "interfaces-impl/time_types_spec.h"
#include "interfaces/hw_eventstamping.h"
#include "hw_eventstamping_impl.h"
#include <atomic>
#include "e20/e20.h" // DELETEME: (s)
#include "thread" // DELETEME: (s)

using namespace miosix;

namespace miosix {

/*static FixedEventQueue<50,24> queue; // DELETEME: (s)

// DELETEME: (s)
void startDBGthread()
{
    std::thread t([]() { queue.run(); });
    t.detach();
}*/

long long getTime() noexcept
{
    InterruptDisableLock dLock;
    return IRQgetTime();
}

long long IRQgetTime() noexcept
{
    return vc->IRQgetTimeNs();
}

namespace internal {

void IRQosTimerInit()
{
    vc->IRQinit(); // inits HSC and correction stack

    // TODO: (s) remove init + instances! init inside constructor
    // Note: order here is important. VHT expects a working and started RTC (TODO: (s) if not started, call init + start)
    #if defined(WITH_DEEP_SLEEP) || defined(WITH_VHT)
    rtc->IRQinit();
    #endif

    #if defined(WITH_VHT)
    vht->IRQinit();
    #endif
}

void IRQosTimerSetTime(long long ns) noexcept
{
    vc->IRQsetTimeNs(ns);
    // TODO: (s) also RTC for VHT alignment
}

// time assumed not to be on the past, checked by caller
void IRQosTimerSetInterrupt(long long ns) noexcept
{
    vc->IRQsetIrqNs(ns);
}

unsigned int osTimerGetFrequency()
{
    InterruptDisableLock dLock;
    return vc->IRQTimerFrequency();
}

} //namespace internal

} //namespace miosix 

//#define TIMER_INTERRUPT_DEBUG
//#define TIMER_EVENT_DEBUG

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
    // VHT
    if(TIMER1->IF & TIMER_IF_CC1)
    {        
        greenLedOn();
        #ifdef WITH_VHT
        // PRS captured RTC current value inside the CC_1 of the TIMER1 (lower part of timer)
        hsc->IRQclearVhtMatchFlag();
        
        // replace lower 32-bit part of timer with the VHT registered one
        long long syncPointActualHsc = hsc->upperTimeTick | hsc->IRQgetVhtTimerCounter();

        // handle lower part timer overflowed before vht lower part timer
        if(syncPointActualHsc > hsc->IRQgetTimeTick()) { syncPointActualHsc -= 1<<16; }

        // update vht correction controller
        if(vht->IRQisCorrectionEnabled())
            vht->IRQupdate(syncPointActualHsc);
        #endif
        greenLedOff();
    }      

    // TIMESTAMP_IN/OUT
    if(TIMER1->IF & TIMER_IF_CC2)
    {
        TIMER1->IFC = TIMER_IFC_CC2;
        
        std::pair<events::EventDirection, events::EventDirection> eventsDirection = events::getEventsDirection();
        
        // TIMESTAMP_IN
        if(eventsDirection.second == events::EventDirection::INPUT)
        {
            // only enabled TIMER1 IRQ, both on same PRS
            //events::IRQsignalEventTIMESTAMP_IN();
        }
        // TIMESTAMP_OUT
        else if(eventsDirection.second == events::EventDirection::OUTPUT)
        {
            unsigned int matchValue = Hsc::IRQgetTriggerMatchReg(1);
            unsigned short nextCCtriggerUpper = matchValue >> 16;
            // save value of TIMER3 counter right away to check for compare later
            unsigned int upperTimerCounter = TIMER3->CNT;

            // (0) trigger at next cycle 
            if(upperTimerCounter == (nextCCtriggerUpper-1))
            {
                #ifdef TIMER_EVENT_DEBUG
                greenLedOn();
                #endif

                // connect TIMER1->CC2 to pin PE12 (TIMESTAMP_OUT) on #1
                TIMER1->ROUTE |= TIMER_ROUTE_CC2PEN;
                TIMER1->CC[2].CTRL |= TIMER_CC_CTRL_CMOA_SET;
                TIMER1->ROUTE |= TIMER_ROUTE_LOCATION_LOC1;
            }
            // (1) trigger was fired and we now need to clear TIMESTAMP_OUT using CMOA
            // give 1 tick of interval just in case of lower part overflow in 
            else if(upperTimerCounter == nextCCtriggerUpper)
            {
                // disconnect TIMER2->CC1 to pin PE12 (TIMESTAMP_OUT)
                TIMER1->CC[2].CTRL &= ~TIMER_CC_CTRL_CMOA_SET;
                TIMER1->CC[2].CTRL |= TIMER_CC_CTRL_CMOA_CLEAR;

                // disable TIMER1 CC2 interrupt
                //TIMER1->IEN &= ~TIMER_IEN_CC2;

                // register next CC in order to clear CMOA (drives TIMESTAMP_OUT pin low)
                // 10 tick are enough not to have any undefined behavior interval since
                // just to enter and exit an interrupt it takes more than 10 ticks.
                TIMER1->CC[2].CCV = TIMER1->CNT+10;

                // signal trigger
                events::IRQsignalEventTIMESTAMP_OUT();
            }
            // (2) clear of CMOA inside hw eventstamping after wakeup
            else ; // still counting, upperTimerCounter < Hsc::IRQgetNextCCtriggerUpper()
        }
        else ; // DISABLED
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

void __attribute__((used)) TIMER2_IRQHandlerImpl()
{
    #ifdef TIMER_INTERRUPT_DEBUG
    if(Hsc::IRQgetMatchFlag() || hsc->lateIrq) miosix::ledOff();
    #endif

    if(Hsc::IRQgetMatchFlag() || hsc->lateIrq)
    {
        //Optional -- begin
//         if(hsc->upperTimeTick>=hsc->upperIrqTick)
//         {
//             TIMER3->IEN &= ~TIMER_IEN_CC0;
//             TIMER3->CC[0].CTRL &= ~TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;
//             TIMER3->IFC = TIMER_IFC_CC0;
//         }
        //Optional -- end

        TIMER2->IEN &= ~TIMER_IEN_CC0;
        TIMER2->CC[0].CTRL &= ~TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;

        hsc->IRQhandler();
    }
        

    // SFD_STXON
    if(TIMER2->IF & TIMER_IF_CC1)
    {
        TIMER2->IFC = TIMER_IFC_CC1;
        // TODO: (s) irqgetevent...
        std::pair<events::EventDirection, events::EventDirection> eventsDirection = events::getEventsDirection();
        // SFD
        if(eventsDirection.first == events::EventDirection::INPUT)
            events::IRQsignalEventSFD();
        // STXON
        else if(eventsDirection.first == events::EventDirection::OUTPUT)
        {
            unsigned int matchValue = Hsc::IRQgetTriggerMatchReg(0);
            unsigned short nextCCtriggerUpper = matchValue >> 16;
            // save value of TIMER3 counter right away to check for compare later
            unsigned int upperTimerCounter = TIMER3->CNT;

            // (0) trigger at next cycle 
            if(upperTimerCounter == (nextCCtriggerUpper-1))
            {
                // connect TIMER2->CC1 to pin PA9 (stxon) on #0
                TIMER2->ROUTE |= TIMER_ROUTE_CC1PEN;
                TIMER2->CC[1].CTRL |= TIMER_CC_CTRL_CMOA_SET;
                TIMER2->ROUTE |= TIMER_ROUTE_LOCATION_LOC0;
            }
            // (1) trigger was fired and we now need to clear STXON using CMOA
            else if(upperTimerCounter == nextCCtriggerUpper)
            {   
                // disconnect TIMER2->CC1 to pin PA9 (stxon)
                TIMER2->CC[1].CTRL &= ~TIMER_CC_CTRL_CMOA_SET;
                TIMER2->CC[1].CTRL |= TIMER_CC_CTRL_CMOA_CLEAR;

                // register next CC in order to clear CMOA (drives STX_ON pin low)
                // 10 tick are enough not to have any undefined behavior interval since
                // just to enter and exit an interrupt it takes more than 10 ticks.
                unsigned short wakeup = TIMER2->CNT+10;
                TIMER2->CC[1].CCV = wakeup;
                while((static_cast<unsigned short>(TIMER2->CNT) - wakeup) > 0x8000) ;

                // disable TIMER1 CC2 interrupt
                TIMER2->IEN &= ~TIMER_IEN_CC1;
                TIMER2->IFC = TIMER_IFC_CC1;

                events::IRQsignalEventSTXON();
            }
            else ; // still counting, upperTimerCounter < Hsc::IRQgetNextCCtriggerUpper()
        }
        else ; // DISABLED
        
    }

    // Timeout
    if(TIMER2->IF & TIMER_IF_CC2)
    {
        TIMER2->IFC = TIMER_IFC_CC2;

        // save value of TIMER3 counter right away to check for compare later
        unsigned int upperTimerCounter = TIMER3->CNT;
        unsigned int matchValue = Hsc::IRQgetTimeoutMatchReg();
        unsigned short upperTimeout = matchValue >> 16;
        
        // if upper part matches, it's time to trigger
        if((hsc->upperTimeTick | upperTimerCounter) >= (hsc->upperTimeoutTick | upperTimeout))
        {
            TIMER2->IEN &= ~TIMER_IEN_CC2;
            TIMER2->CC[2].CTRL &= ~TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;
            TIMER2->IFC = TIMER_IFC_CC2;

            events::IRQsignalEventTimeout();
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

// Each thread is scheduled according to the scheduler every 10ms.
// It can happen that we've recieved a TIMER3 interrupt and we set TIMER2 for lower part of timer match
// and, while still waiting, we get a request to sleep. IRQsetosInterrupt is called that
// re-sets the next interrupt as now + sleepTime and enters in sleepCpu with interrupt disabled
// after clearing the match flag and resetting TIMER2.
// TIMER2 is not called, TIMER3 is then re-called when sleep finishes, as expected.
// TIMER3 sets TIMER2 and waits, as expected.
// This is why i cannot set both timers when setting the next osinterrupt but i have to set them progressively
// everytime i get upper tick match. So the algorithm is now as follows:
// set TIMER3 CC --ISR--> set TIMER2CC
// Only problem is if we have sleep less than then time that intercurs between the two interrups
// but since it's maximum (2^16)-1 we have maximum 1.36ms. Also, if we have lowerTimerCounter >= hsc->IRQgetNextCCticksLower()
// it may cause problems if we're calling TIMER3 twice for the sleep while waiting for TIMER2?
void __attribute__((used)) TIMER3_IRQHandlerImpl()
{
    // TIMER3 overflow, pending bit trick.
    if(hsc->IRQgetOverflowFlag()) hsc->IRQhandler();

    if(TIMER3->IF & TIMER_IF_CC0)
    {
        TIMER3->IFC = TIMER_IFC_CC0;

        unsigned int matchValue = Hsc::IRQgetTimerMatchReg();
        unsigned short lower16 = matchValue & 0xffff;
        TIMER2->CC[0].CCV = lower16 - 1;
        TIMER2->IEN |= TIMER_IEN_CC0;
        TIMER2->CC[0].CTRL |= TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;
        
        // This is a lateIRQ case. Normally we could check if the 32-bit part of the timer is
        // greated than the match value. This however doesn't take into account the case i which
        // a rollover occurs and therefore this condition is not valid anymore. To do this, we check
        // if the current time belongs or not at the first-half of the cyclic timeline for the timer
        // before rollovering. matchValue is converted to COMP2 before calculation and 0x80000000
        // is exactly half of 0x0xFFFFFFFF
        if(Hsc::IRQgetTimerCounter()-matchValue<0x80000000)
        {
            //TODO: instead of force-pending TIMER2 CC0 we could call IRQHandler from here
            TIMER2->IFS = TIMER_IFS_CC0;
            Hsc::IRQforcePendingIrq();
        }
    }

    // TIMESTAMP_IN input capture
    if(TIMER3->IF & TIMER_IF_CC2)
    {
        TIMER3->IFC = TIMER_IFC_CC2;
        events::IRQsignalEventTIMESTAMP_IN();
    }
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
    // handle RTC overflow
    if(rtc->IRQgetOverflowFlag()) { rtc->IRQoverflowHandler(); }
    #endif
    
    // VHT COMP1 register clear (already handled by PRS, just clear)
    #ifdef WITH_VHT
    if(rtc->IRQgetVhtMatchFlag()) { rtc->IRQclearVhtMatchFlag(); }
    #endif

}
