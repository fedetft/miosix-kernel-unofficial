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
#include "bsp_impl.h"

using namespace miosix;

namespace miosix {

static Rtc *rtc = nullptr;
static Hsc *hsc = nullptr;
static TimeConversion tc;

long long getTime() noexcept
{
    FastInterruptDisableLock dLock;
    return IRQgetTime();
}

long long IRQgetTime() noexcept
{
    return hsc->IRQgetTimeNs();
}

namespace internal {

void IRQosTimerInit()
{
    rtc = &Rtc::instance();
    rtc->IRQinit();

    hsc = &Hsc::instance();
    hsc->IRQinit();
    IRQosTimerSetTime(80 * 1e9); // TEST: (s) close to overflow at 88ns
    
    tc = TimeConversion(osTimerGetFrequency());
}

void IRQosTimerSetTime(long long ns) noexcept
{
    hsc->IRQsetTimeNs(ns);
}

void IRQosTimerSetInterrupt(long long ns) noexcept
{
    hsc->IRQsetIrqNs(ns); 
}

unsigned int osTimerGetFrequency()
{
    FastInterruptDisableLock dLock;
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
    miosix::ledOn();
    hsc->IRQhandler();
    miosix::ledOff();
}

/**
 * RTC interrupt routine (not used, scheduling uses hsc IRQ handler!)
 */
void __attribute__((naked)) RTC_IRQHandler()
{
    saveContext();
    asm volatile("bl _Z14RTChandlerImplv");
    restoreContext();
}

void __attribute__((used)) RTChandlerImpl()
{
    rtc->IRQhandler();
}