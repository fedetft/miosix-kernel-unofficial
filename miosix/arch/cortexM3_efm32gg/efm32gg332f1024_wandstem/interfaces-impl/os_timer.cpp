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
#include "kernel/timeconversion.h"
#include "rtc.h"
#include "hsc.h"

using namespace miosix;

namespace miosix {

static Rtc *rtc = nullptr;
static Hsc *hsc = nullptr;
static TimeConversion tc;

long long getTime() noexcept
{
    InterruptDisableLock dLock;
    return IRQgetTime();
}

long long IRQgetTime() noexcept
{
    return hsc->IRQgetTimeNs();
}

namespace internal {

void IRQosTimerInit()
{
    hsc = &Hsc::instance();
    hsc->IRQinit();

    rtc = &Rtc::instance();
    rtc->IRQinit();

    tc = TimeConversion(osTimerGetFrequency());
}

void IRQosTimerSetTime(long long ns) noexcept
{
    hsc->IRQsetTimeNs(ns);
}

void IRQosTimerSetInterrupt(long long ns) noexcept
{
    // TODO: (s) check if in the past?
    // FIXME: (s) called twice every pause?? one from SVC_Handler and other form TIMER1_HandlerImpl
    hsc->IRQsetIrqNs(ns); 
}

unsigned int osTimerGetFrequency()
{
    InterruptDisableLock dLock;
    return hsc->IRQTimerFrequency();
}

} //namespace internal

} //namespace miosix 

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
    // TIMER2 overflow, pending bit trick
    if(hsc->IRQgetOverflowFlag())
    {
        hsc->IRQhandler();
    }
    // first part of output compare, disable output compare interrupt
    // for TIMER2 and turn of output comapre interrupt of TIMER1
    else
    {
        miosix::greenLed::high();

        // disable output compare interrupt on channel 0 for most significant timer
        TIMER2->IEN &= ~TIMER_IEN_CC0;
        TIMER2->CC[0].CTRL &= ~TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;
        TIMER2->IFC |= TIMER_IFC_CC0;

        // enable output compare interrupt on channel 0 for least significant timer
        TIMER1->IEN |= TIMER_IEN_CC0;
        TIMER1->CC[0].CTRL |= TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;
    }
}

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
    // second part of output compare. If we reached this interrupt, it means
    // we already matched the upper part of the timer and we have now matched the lower part.
    hsc->IRQhandler();

    miosix::greenLed::low();
}

// TODO: (s) is that really necessary? we use RTC only when going into deep sleep so...
/**
 * RTC interrupt routine (not used, scheduling uses hsc IRQ handler!)
 */
/*void __attribute__((naked)) RTC_IRQHandler()
{
    saveContext();
    asm volatile("bl _Z14RTChandlerImplv");
    restoreContext();
}

void __attribute__((used)) RTChandlerImpl()
{    
    rtc->IRQoverflowHandler();
}*/