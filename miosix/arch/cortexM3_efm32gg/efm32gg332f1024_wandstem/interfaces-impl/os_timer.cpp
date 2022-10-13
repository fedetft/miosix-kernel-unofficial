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
#include <atomic>

using namespace miosix;

namespace miosix {


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
    // TODO: (s) remove init + instances! init inside constructor
    // Note: order here is important. VHT expects a working and started RTC (TODO: (s) if not started, call init + start)
    #if defined(WITH_DEEP_SLEEP) || defined(WITH_VHT)
    rtc->IRQinit();
    //rtc->IRQinit();
    #endif

    #if defined(WITH_VHT)
    vht->IRQinit();
    #endif

    vc->IRQinit(); // inits HSC and correction stack
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

#define TIMER_INTERRUPT_DEBUG
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
    // only enabled TIMER1 IRQ, both on same PRS
    if(TIMER1->IF & TIMER_IF_CC2)
    {
        events::IRQsignalEvent();
        TIMER1->IFC |= TIMER_IFC_CC2;
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
    // save value of TIMER3 counter right away to check for compare later
    unsigned int upperTimerCounter = TIMER3->CNT;

    #ifdef TIMER_INTERRUPT_DEBUG
    if(hsc->IRQgetMatchFlag() || hsc->lateIrq) miosix::ledOff();
    #endif

    // timer flag (CC[0])
    if(hsc->IRQgetMatchFlag() || hsc->lateIrq || hsc->IRQgetOverflowFlag())
        hsc->IRQhandler();

    
    // second part of output compare for trigger or timeout (CC[1]). If we reached this interrupt, it means
    // we already matched the upper part of the timer and we have now matched the lower part.
    if(hsc->IRQgetTriggerFlag() || hsc->lateTrigger)
    {
        Hsc::IRQclearTriggerFlag();

        // trigger
        if(events::getEventDirection() == events::EventDirection::OUTPUT)
        {
            // (2) CMOA cleared, we can terminate trigger procedure
            if(TIMER2->CC[1].CTRL & TIMER_CC_CTRL_CMOA_CLEAR)
            {
                // disable output compare interrupt on channel 1 for least significant timer
                TIMER2->IEN &= ~TIMER_IEN_CC1;
                TIMER2->CC[1].CTRL &= ~TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;
                TIMER2->IFC |= TIMER_IFC_CC1;

                // disconnect TIMER2->CC1 from PEN completely
                TIMER2->CC[1].CTRL &= ~TIMER_CC_CTRL_CMOA_CLEAR;
                TIMER2->ROUTE &= ~TIMER_ROUTE_LOCATION_LOC0;
                TIMER2->ROUTE &= ~TIMER_ROUTE_CC1PEN;

                #ifdef TIMER_EVENT_DEBUG
                greenLedOff();
                #endif

                // signal trigger
                events::IRQsignalTrigger();
            }
            else  // trigger
            {
                long long tick = hsc->IRQgetTimeTick();
                if(tick >= hsc->IRQgetTriggerTick())
                {
                    // (1) trigger was fired and we now need to clear STXON using CMOA
                    // disconnect TIMER2->CC1 to pin PA9 (stxon)
                    TIMER2->CC[1].CTRL &= ~TIMER_CC_CTRL_CMOA_SET;
                    TIMER2->CC[1].CTRL |= TIMER_CC_CTRL_CMOA_CLEAR;
                    TIMER2->CC[1].CCV = static_cast<uint32_t>(TIMER2->CNT+10);
                }
                // lower 32-bit timer may match but not he upper 32-bit extended part, make another round
                else
                {
                    // disable output compare interrupt on channel 1 for least significant timer
                    TIMER2->IEN &= ~TIMER_IEN_CC1;
                    TIMER2->CC[1].CTRL &= ~TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;
                    TIMER2->IFC |= TIMER_IFC_CC1;

                    // re-enable CC on most significant timer
                    TIMER3->IEN |= TIMER_IEN_CC1;
                    TIMER3->CC[1].CTRL |= TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;
                }
            }
        }
        // timeout
        else if(events::getEventDirection() == events::EventDirection::INPUT)
            events::IRQsignalEventTimeout();
        else ; // DISABLED

        hsc->lateTrigger = false;
    }

    // first part of output compare for event timeout or trigger (CC[1])
    // if(TIMER1->IF & TIMER_IF_CC1)
    // {   
    //     #ifdef TIMER_EVENT_DEBUG
    //     greenLedOff();
    //     #endif

    //     // disable output compare interrupt on channel 1 for least significant timer
    //     TIMER1->IEN &= ~TIMER_IEN_CC1;
    //     TIMER1->CC[1].CTRL &= ~TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;
    //     TIMER1->IFC |= TIMER_IFC_CC1;

    //     // set and enable output compare interrupt on channel 1 for most significant timer
    //     TIMER2->CC[1].CCV = hsc->IRQgetNextCCeventUpper();
    //     TIMER2->IEN |= TIMER_IEN_CC1;
    //     TIMER2->CC[1].CTRL |= TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;

    //     if(events::getEventDirection() == events::EventDirection::OUTPUT)
    //     {
    //         // (0) TRIGGER: connect TIMER2->CC1 to pin PA9 (stxon) on #0
    //         TIMER2->ROUTE |= TIMER_ROUTE_CC1PEN;
    //         TIMER2->CC[1].CTRL |= TIMER_CC_CTRL_CMOA_SET;
    //         TIMER2->ROUTE |= TIMER_ROUTE_LOCATION_LOC0;
    //     }
        
    //     // force interrupt pending if we're under 1.365312ms (TIMER2->CCV = 0) or we already match the upper part.
    //     // Note, because of quirk, without this we would be expecting 14 (15 - 1) but we're reading 15 causing a deadlock.
    //     if (Hsc::IRQgetNextCCeventUpper() == 0 || Hsc::IRQgetNextCCeventUpper() == TIMER2->CNT-1)
    //     {
    //         TIMER2->IFS |= TIMER_IFS_CC1;
    //         Hsc::IRQforcePendingEvent();  //TIMER2_IRQHandler();
    //     }
        
    //     // if by the time we get here, the upper part of the counter has already matched
    //     // the counter register or got past it, call TIMER2 handler directly instead of waiting
    //     // for the other round of compare
    //     // first check if not enough since we could be still expecting a lower value after overflow
    //     // (e.g. i'm at 65123, i wait for --6513--*OVERFLOW*-->14)
    //     // TODO: (s) i'm pretty sure it's wrong...
    //     /*else if(upperTimerCounter >= hsc->IRQgetNextCCeventUpper() && TIMER2->CNT-1 >= Hsc::IRQgetNextCCeventUpper())
    //     {
    //         TIMER2->IFS |= TIMER_IFS_CC1;
    //         Hsc::IRQforcePendingEvent(); //TIMER2_IRQHandler();
    //     }*/
    // }

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
    // save value of TIMER2 counter right away to check for compare later
    unsigned int lowerTimerCounter = TIMER2->CNT;

    // TIMER3 overflow, pending bit trick.
    if(hsc->IRQgetOverflowFlag()) hsc->IRQhandler();
    
    // first part of output compare for timer match, disable output compare interrupt
    // for TIMER3 and turn of output comapre interrupt of TIMER2
    if(TIMER3->IF & TIMER_IF_CC0)
    {
        #ifdef TIMER_INTERRUPT_DEBUG
        miosix::ledOn();
        #endif

        // disable output compare interrupt on channel 0 for most significant timer
        TIMER3->CC[0].CCV = 0;
        TIMER3->IEN &= ~TIMER_IEN_CC0;
        TIMER3->CC[0].CTRL &= ~TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;
        TIMER3->IFC |= TIMER_IFC_CC0;
        

        // set and enable output compare interrupt on channel 0 for least significant timer
        TIMER2->CC[0].CCV = hsc->IRQgetNextCCticksLower(); // underflow handling
        TIMER2->IEN |= TIMER_IEN_CC0;
        TIMER2->CC[0].CTRL |= TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;

        // if by the time we get here, the lower part of the counter has already matched
        // the counter register or got past it, call TIMER1 handler directly instead of waiting
        // for the other round of compare
        // FIXME: (s) test! see same event case comment
        /*if(lowerTimerCounter >= hsc->IRQgetNextCCticksLower()) 
        {
            TIMER1->IFS |= TIMER_IFS_CC0;
            Hsc::IRQforcePendingIrq(); //TIMER1_IRQHandler();
        }*/
    }

    // first part of output compare for event trigger or timeout (CC[1])
    if(TIMER3->IF & TIMER_IF_CC1)
    {
        #ifdef TIMER_EVENT_DEBUG
        greenLedOn();
        #endif

        // disable output compare interrupt on channel 1 for most significant timer
        TIMER3->IEN &= ~TIMER_IEN_CC1;
        TIMER3->CC[1].CTRL &= ~TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;
        TIMER3->IFC |= TIMER_IFC_CC1;

        // set and enable output compare interrupt on channel 1 for least significant timer
        TIMER2->CC[1].CCV = hsc->IRQgetNextCCtriggerLower();
        TIMER2->IEN |= TIMER_IEN_CC1;
        TIMER2->CC[1].CTRL |= TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;

        // trigger specific case
        if(events::getEventDirection() == events::EventDirection::OUTPUT)
        {
            // connect TIMER2->CC1 to pin PA9 (stxon) on #0
            TIMER2->ROUTE |= TIMER_ROUTE_CC1PEN;
            TIMER2->CC[1].CTRL |= TIMER_CC_CTRL_CMOA_SET;
            TIMER2->ROUTE |= TIMER_ROUTE_LOCATION_LOC0;
        }
    }

    // second part of output compare for ttimeout or trigger (CC[1]). If we reached this interrupt, it means
    // we already matched the lower part of the timer and we have now matched the upper part.
    // if(hsc->IRQgetEventFlag() || hsc->lateEvent)
    // {
    //     Hsc::IRQclearEventFlag();

    //     // (2) TRIGGER: CMOA cleared, we can terminate trigger procedure
    //     if(TIMER2->CC[1].CTRL & TIMER_CC_CTRL_CMOA_CLEAR)
    //     {
    //         TIMER2->CC[1].CTRL &= ~TIMER_CC_CTRL_CMOA_CLEAR;
    //         TIMER2->IEN &= ~TIMER_IEN_CC1;
    //         TIMER2->ROUTE &= ~TIMER_ROUTE_LOCATION_LOC0;
    //         TIMER2->ROUTE &= ~TIMER_ROUTE_CC1PEN;

    //         #ifdef TIMER_EVENT_DEBUG
    //         greenLedOn();
    //         #endif

    //         // signal trigger
    //         events::IRQsignalEventTimeout();
    //     }
    //     else  // trigger or event
    //     {
    //         long long tick = hsc->IRQgetTimeTick();
    //         if(tick >= hsc->IRQgetEventTick())
    //         {
    //             // trigger case
    //             if(events::getEventDirection() == events::EventDirection::OUTPUT) 
    //             {
    //                 // (1) TRIGGER: specific case trigger, trigger was fired and we now need to clear STXON using CMOA
    //                 if(TIMER2->CC[1].CTRL & TIMER_CC_CTRL_CMOA_SET)
    //                 {
    //                     // disconnect TIMER2->CC1 to pin PA9 (stxon)
    //                     TIMER2->CC[1].CTRL &= ~TIMER_CC_CTRL_CMOA_SET;
    //                     TIMER2->CC[1].CTRL |= TIMER_CC_CTRL_CMOA_CLEAR;
    //                     TIMER2->CC[1].CCV = static_cast<uint32_t>(TIMER2->CNT+1); // takes max 2^16 / 48e6 [s] = 1.365312ms
    //                 }
    //             }
    //             // event case
    //             else if (events::getEventDirection() == events::EventDirection::INPUT)
    //             {
    //                 #ifdef TIMER_EVENT_DEBUG
    //                 greenLedOff();
    //                 #endif

    //                 events::IRQsignalEventTimeout(); // signal event timeout
    //             }
    //             else; // DISABLED, shouldn't get here
    //         }
    //         // lower 32-bit timer may match but not he upper 32-bit extended part, make another round
    //         else
    //         {
    //             TIMER1->IEN |= TIMER_IEN_CC1;
    //             TIMER1->CC[1].CTRL |= TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;
    //         }
            
    //         hsc->lateEvent = false;
    //     }
    // }

    // // Timestamping, wake up thread waiting for timestamp
    // if(TIMER2->IF & TIMER_IF_CC2)
    // {
    //     TIMER2->IFC |= TIMER_IFC_CC2;
    //     events::IRQsignalEvent();
    // }
}

/**
 * TIMER3 interrupt routine
 * 
 */
// void __attribute__((naked)) TIMER3_IRQHandler()
// {
//     saveContext();
//     asm volatile("bl _Z21TIMER3_IRQHandlerImplv");
//     restoreContext();
// }

// void __attribute__((used)) TIMER3_IRQHandlerImpl()
// {
//     greenLedOn();
//     #ifdef WITH_VHT
       
//     // if-guard
//     if(!hsc->IRQgetVhtMatchFlag()) return;

//     // PRS captured RTC current value inside the CC_0 of the TIMER3 (lower part of timer)
//     hsc->IRQclearVhtMatchFlag();
    
//     // replace lower 32-bit part of timer with the VHT registered one
//     long long syncPointActualHsc = hsc->upperTimeTick | hsc->IRQgetVhtTimerCounter();
//     // handle lower part timer overflowed before vht lower part timer
//     if(syncPointActualHsc > hsc->IRQgetTimeTick()) { syncPointActualHsc -= 1<<16; }

//     // update vht correction controller
//     if(vht->IRQisCorrectionEnabled())
//         vht->IRQupdate(syncPointActualHsc);

//     #endif
//     greenLedOff();
// }

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

    // VHT COMP1 register clear (already handled by PRS, just clear)
    #ifdef WITH_VHT
    if(rtc->IRQgetVhtMatchFlag()) { rtc->IRQclearVhtMatchFlag(); }
    #endif

    #endif
}