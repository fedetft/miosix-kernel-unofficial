/***************************************************************************
 *   Copyright (C) 2022 by Sorrentino Alessandro                           *
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

#include "interfaces/deep_sleep.h"
#include "interfaces/os_timer.h"
#include "rtc.h"
#include "hsc.h"
#include "time/timeconversion.h"
#include <sys/ioctl.h>
#include "stdio.h"
#include "miosix.h" // DELETEME: (s)
#include "bsp_impl.h"

#ifdef WITH_VHT
#include "interfaces/vht.h"
#endif

namespace miosix {

static Rtc* rtc = nullptr; 
static Hsc* hsc = nullptr;
static TimeConversion* HSCtc = nullptr;
static TimeConversion* RTCtc = nullptr;

// forward declaration
void doIRQdeepSleep(long long);
void IRQrestartHFXO();
void IRQprepareDeepSleep();
void IRQclearDeepSleep();

///
// Main interface impl
///

void IRQdeepSleepInit()
{
    // instances of RTC and HSC
    rtc     = &Rtc::instance();
    hsc     = &Hsc::instance();

    HSCtc   = new TimeConversion(hsc->IRQTimerFrequency());
    RTCtc   = new TimeConversion(rtc->IRQTimerFrequency());
}

bool IRQdeepSleep(long long abstime)
{
    PauseKernelLock pkLock; //To run unexpected IRQs without context switch
    miosix::greenLedOn();

    // 1. RTC set ticks from High Speed Clock and starts
    rtc->IRQsetTimeNs(hsc->IRQgetTimeNs());
    hsc->IRQstopTimer(); // TODO: (s) remove?

    // 2. Perform deep sleep
    doIRQdeepSleep(abstime);
    
    // 3. Set HSC ticks from Real Time Clock
    hsc->IRQsetTimeNs(rtc->IRQgetTimeNs()); // FIXME: (s) stack overflow, i think it has to do with new irq set
    rtc->IRQstopTimer(); // TODO: (s) should do power gating?
    
    miosix::greenLedOff();

    return true;
}

bool IRQdeepSleep()
{
    return IRQdeepSleep(3600000000000);
}

///
// Utility functions
///

void doIRQdeepSleep(long long abstime)
{
    // This conversion is very critical: we can't use the straightforward method
    // due to the approximation make during the conversion (necessary to be efficient).
    // ns-->tick(HF)->\
    //                  represent a different time. 
    //                  This difference grows with the increasing of time value.
    // ns-->tick(LF)->/
    //
    // This brings problem if we need to do operation that involves the use 
    // of both clocks, aka the sleep (or equivalent) and the deep sleep
    // In this way, I do the
    // ns->tick(HR)[approximated]->tick(LF)[equivalent to the approx tick(HR)]
    // This operation is of course slower than usual, but affordable given 
    // the fact that the deep sleep should be called quite with large time span.
    unsigned long long ticks = RTCtc->ns2tick(abstime);
    //ticks = ticks * 32 / 46875;
    
    ioctl(STDOUT_FILENO,IOCTL_SYNC,0);
    
    // RTC deep sleep function that sets RTC compare register
    rtc->IRQsetDeepSleepIrqTick(ticks);
    
    // TODO: (s) bring everything inside os_timer RTCTimerAdapter (modify IRQhandler)
    // pre deep sleep
    IRQprepareDeepSleep();

    // deep sleep loop
    for(;;)
    {
        // if for whatever reason we're past the deep sleep time, wake up
        if(ticks <= rtc->IRQgetTimeTick())
        {
            rtc->IRQclearMatchFlag();
            NVIC_ClearPendingIRQ(RTC_IRQn);
            break;
        }
        
        // Goes into low power mode and waits for RTC pending interrupt
        __DSB(); // data synchronization barrier for memory operations
        __WFI(); // blocking statement. NOP if any interrupt is still pending

        IRQrestartHFXO(); // restarts High frequency oscillator

        // checks if we went out of deep sleep because of the RTC interrupt
        if(NVIC_GetPendingIRQ(RTC_IRQn))
        {
            //Clear the interrupt (both in the RTC peripheral and NVIC),
            //this is important as pending IRQ prevent WFI from working
            NVIC_ClearPendingIRQ(RTC_IRQn);

            // checks if wake up because of RTC overflow
            if(rtc->IRQgetOverflowFlag())
            {
                rtc->IRQoverflowHandler(); // check if interrupt was because of an overflow
            }
            
            // wake up time has past already
            if(ticks <= rtc->IRQgetTimeTick())
            {
                rtc->IRQclearMatchFlag();
                break;
            }

            if(!rtc->IRQgetOverflowFlag() && !(ticks <= rtc->IRQgetTimeTick()))
            {
                // Se ho altri interrupt (> 1) vanno tutti con clock non corretto
                // Reimplement RTC timer adapter
                //FastInterruptEnableLock eLock(dLock);
                // Here interrupts are enabled, so the software part of RTC 
                // can be updated
                // NOP operation to be sure that the interrupt can be executed
                fastEnableInterrupts();
                __NOP();
                fastDisableInterrupts();
            }
        }
        else
        {
            // we have an interrupt set before going to deep sleep, we have to serve it 
            // or the micro will never go into sleep mode and __WFI() macro becomes a NOP instruction
            fastEnableInterrupts();
            __NOP();
            fastDisableInterrupts();
        }
    }

    // post deep sleep
    IRQclearDeepSleep();

    #ifdef VHT_H
    
    // vht code...

    #endif
}

inline void IRQrestartHFXO()
{
    //This function implements an optimization to improve the time to restore
    //the system state when exiting deep sleep: we need to restart the MCU
    //crystal oscillator, and this takes around 100us. Then, we also need to
    //restart the cc2520 power domain and wait for the voltage to be stable,
    //which incidentally requires again 100us. So, we do both things in
    //parallel.
    CMU->OSCENCMD=CMU_OSCENCMD_HFXOEN; 

    //This locks the CPU till clock is stable
    //because the HFXO won't emit any waveform until it is stabilized
    CMU->CMD=CMU_CMD_HFCLKSEL_HFXO;	
					
    CMU->OSCENCMD=CMU_OSCENCMD_HFRCODIS; //turn off RC oscillator
}

inline void IRQprepareDeepSleep()
{
    // Flag to enable the deepsleep when we will call _WFI, 
    // otherwise _WFI is translated as a simple sleep status, this means that the core is not running 
    // but all the peripheral (HF and LF), are still working and they can trigger exception
    SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk; 
    //SCB->SCR |= SCB_SCR_SLEEPONEXIT_Msk; // TODO: (s) doens't enter deep sleep until least interrupt in ISR is served. Ma noi tanto siamo ad interrupt disabilitati!
    EMU->CTRL = 0;
}

inline void IRQclearDeepSleep()
{
    // clearing deep sleep mask
    SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk; 
}

} // namespace miosix