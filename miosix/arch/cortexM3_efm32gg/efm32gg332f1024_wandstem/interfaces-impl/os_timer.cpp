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

#ifdef WITH_DEEP_SLEEP
#include "rtc.h"
#endif

#ifdef WITH_VIRTUAL_CLOCK
#include "interfaces/virtual_clock.h"
#endif

#include "e20/e20.h" // DELETEME: (s)
#include "thread" // DELETEME: (s)

using namespace miosix;

static FixedEventQueue<100,12> queue; // DELETEME: (s)

// DELETEME: (s)
void startThread()
{
	std::thread t([]() { queue.run(); });
	t.detach();
}

namespace miosix {

static Hsc *hsc = nullptr;

#ifdef WITH_DEEP_SLEEP
static Rtc *rtc = nullptr;
#endif

#ifdef WITH_VIRTUAL_CLOCK
static VirtualClock *vc = nullptr;
#endif

long long getTime() noexcept
{
    InterruptDisableLock dLock;
    return IRQgetTime();
}

long long IRQgetTime() noexcept
{
    #ifdef WITH_VIRTUAL_CLOCK
    return vc->getVirtualTimeNs(hsc->IRQgetTimeNs());
    #else
    return hsc->IRQgetTimeNs();
    #endif
}

namespace internal {

void IRQosTimerInit()
{
    //startThread(); // DELETEME: (s)

    hsc = &Hsc::instance();
    hsc->IRQinit();

    #ifdef WITH_DEEP_SLEEP
    rtc = &Rtc::instance();
    rtc->IRQinit();
    #endif

    #ifdef WITH_VIRTUAL_CLOCK
    vc = &VirtualClock::instance();
    #endif
}

void IRQosTimerSetTime(long long ns) noexcept
{
    hsc->IRQsetTimeNs(ns);
}

// TODO: (s) check if in the past?
// FIXME: (s) called twice every pause?? one from SVC_Handler and other form TIMER1_HandlerImpl
void IRQosTimerSetInterrupt(long long ns) noexcept
{
    //queue.IRQpost([=]() { iprintf("Next int: %lld (uncorr ns) vs %lld (ns)\n", vc->IRQgetUncorrectedTimeNs(ns), ns); }); // DELETEME: (s)

    // TODO: (s) makes sense to have sleep time in the sleep queue corrected and then de-correct them here?
    #ifdef WITH_VIRTUAL_CLOCK
    hsc->IRQsetIrqNs(vc->IRQgetUncorrectedTimeNs(ns));
    #else
    hsc->IRQsetIrqNs(ns); 
    #endif
    
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