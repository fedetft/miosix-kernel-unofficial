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
#include "interfaces/os_timer.h" // TimerProxy
#include "rtc.h"
#include "hsc.h"
#include "time/timeconversion.h"
#include <sys/ioctl.h>
#include "stdio.h"
#include "miosix.h"
#include "bsp_impl.h"
#include "time_types_spec.h"

using namespace miosix;

/// /def DEBUG_DEEP_SLEEP
/// This debug symbol makes the deep sleep routine to make
/// a led blink when the board enter the stop mode
#define DEBUG_DEEP_SLEEP

namespace miosix {

static TimeConversion* RTCtc = nullptr; // cached RTC time conversion

// forward declaration
void IRQdeepSleep_impl(long long);
void IRQrestartHFXO();
void IRQprepareDeepSleep();
void IRQclearDeepSleep();
void IRQexecutePendingirq();
void IRQresyncClocks();

///
// Main interface impl
///
void IRQdeepSleepInit()
{
    RTCtc = new TimeConversion(rtc->IRQTimerFrequency());   
}

bool IRQdeepSleep(long long abstime)
{
    #ifdef DEBUG_DEEP_SLEEP
    miosix::greenLedOn();
    #endif
    
    // disable VHT ISR to avoid pending interrupts
    #ifdef WITH_VHT 
    vht->IRQdisableCorrection(); 
    #endif

    // 1. RTC set ticks from High Speed Clock and starts
    rtc->IRQsetTimeNs(hsc->IRQgetTimeNs());

    // 2. perform deep sleep
    IRQdeepSleep_impl(vc->IRQuncorrectTimeNs(abstime));

    // 3. re-align HSC and Real Time Clock
    IRQresyncClocks(); 
    
    // re-enable VHT correction or stop RTC
    #ifdef WITH_VHT
    vht->IRQenableCorrection();
    #else
    rtc->IRQstopTimer();
    #endif
    
    #ifdef DEBUG_DEEP_SLEEP
    miosix::greenLedOff();
    #endif

    return true;
}

bool IRQdeepSleep()
{
    return IRQdeepSleep(3600000000000);
}

///
// Utility functions
///

inline void IRQdeepSleep_impl(long long abstime)
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

    // the reason above was not implemented since normal conversion seems to be working just fine
    unsigned long long ticks = RTCtc->ns2tick(abstime);
    //unsigned long long ticks2 = abstime / 32768 * 1e9;
    // when=temp*32/46875;
    
    ioctl(STDOUT_FILENO,IOCTL_SYNC,0);
    
    // pre deep sleep
    IRQprepareDeepSleep();

    PauseKernelLock pkLock; //To run unexpected IRQs without context switch

    // RTC deep sleep function that sets RTC compare register
    rtc->IRQsetDeepSleepIrqTick(ticks);

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

        bool lateWakeUp = false;
        
        // digest all pending interrupts before going in deep sleep
        // needed since efm32 lacks of standby flag that tells us if the micro was
        // able to go in deep sleep mode or not.
        bool pendingNVIC = NVIC->ISPR[0U] > 0; 
        while(pendingNVIC)
        {
            // checks if wake up because of RTC overflow
            if(rtc->IRQgetOverflowFlag())
            {
                rtc->IRQoverflowHandler(); // check if interrupt was because of an overflow
            }

            // if by any means we get past wakeup-time while serving interrupts, break
            if(ticks <= rtc->IRQgetTimeTick())
            {
                rtc->IRQclearMatchFlag();
                NVIC_ClearPendingIRQ(RTC_IRQn);
                lateWakeUp = true;
                break;
            }
            
            IRQexecutePendingirq();

            pendingNVIC = NVIC->ISPR[0U] > 0;
        }
        
        if(lateWakeUp) break;
        __ISB(); // avoids anticipating deep sleep before finishing serving interrupts

        // if this condition is true, we're still waiting for TIMER1 to be triggered
        // while about to enter deep sleep.
        // we need to handle TIMER1 interrupt before sleeping because HSC timer will be
        // power-gated during sleep and TIMER1IRQHandler_Impl will never be called if not
        // by next iteration TIMER2 -> TIMER1
        if(TIMER1->CC[0].CTRL & TIMER_CC_CTRL_MODE_OUTPUTCOMPARE)
        {
            while(!hsc->IRQgetMatchFlag())
            {
                // checks if wake up because of RTC overflow
                if(rtc->IRQgetOverflowFlag())
                {
                    rtc->IRQoverflowHandler(); // check if interrupt was because of an overflow
                }

                // if by any means we get past wakeup-time while serving interrupts, break
                if(ticks <= rtc->IRQgetTimeTick())
                {
                    rtc->IRQclearMatchFlag();
                    NVIC_ClearPendingIRQ(RTC_IRQn);
                    lateWakeUp = true;
                    break;
                }
            }
            if(lateWakeUp) break;

            IRQexecutePendingirq();
        }
        __ISB(); // avoids anticipating deep sleep before finishing serving interrupts

        // Goes into low power mode and waits for RTC pending interrupt
        __DSB(); // data synchronization barrier for memory operations
        __WFI(); // blocking statement. NOP if any interrupt is still pending
        __ISB(); // this ensures that the __WFI primitive is executed before the following

        IRQrestartHFXO(); // restarts High frequency oscillator

        // checks if we went out of deep sleep because of the RTC interrupt
        if(NVIC_GetPendingIRQ(RTC_IRQn))
        {
            // checks if wake up because of RTC overflow
            if(rtc->IRQgetOverflowFlag())
            {
                rtc->IRQoverflowHandler(); // check if interrupt was because of an overflow
            }
            
            // wake up time has past already
            if(ticks <= rtc->IRQgetTimeTick())
            {
                rtc->IRQclearMatchFlag();
                NVIC_ClearPendingIRQ(RTC_IRQn);
                break;
            }

            // we could have another pending interrupt with the overflow one
            if(!rtc->IRQgetOverflowFlag()/* && !(ticks <= rtc->IRQgetTimeTick())*/)
            {
                // Other pending interrupts that are not RTC overflow
                IRQexecutePendingirq();
            }

            //Clear the interrupt (both in the RTC peripheral and NVIC),
            //this is important as pending IRQ prevent WFI from working
            NVIC_ClearPendingIRQ(RTC_IRQn);
        }
        else
        {
            // we have an interrupt set before going to deep sleep, we have to serve it 
            // or the micro will never go into sleep mode and __WFI() macro becomes a NOP instruction
            IRQexecutePendingirq();
        }
    }

    // post deep sleep
    IRQclearDeepSleep();

    // manual wait if we woke up in advance

    // LEGACY CODE, not sure if still needed (RTC COMP1 is used, then old value is taken from HRTB. temp var is necessary)
    /*RTC->COMP1=(when-1) & 0xffffff; //EFM32 compare channels trigger 1 tick late (undocumented quirk)
    while(RTC->SYNCBUSY & RTC_SYNCBUSY_COMP1) ;
    RTC->IFC=RTC_IFC_COMP1;
    RTC->IEN |= RTC_IEN_COMP1;
    //I have to use polling because the interrupt are still disabled
    //This "polling" is particular: __WFI make sleeping the core, only if there aren't pending IRQ. 
    //When a generic interrupt triggers, 
    //the core wakes and check the condition. 
    //If true, it goes forward, otherwise the interrupt is caused by another IRQ. 
    //But this IRQ can't be served because the interrupts are disabled, hence the while-cycle turns in a polling-cycle 
    //(bad and not low power, but definitely very rare)
    while(when>rtc.IRQgetTimeTick()) __WFI();
    RTC->IEN &= ~RTC_IEN_COMP1;
    RTC->IFC=RTC_IFC_COMP1;

    //Clean flag to not resync immediately
    TIMER2->IFC=TIMER_IFC_CC2;
    TIMER2->CC[2].CCV;
    
    RTC->COMP1=HRTB::nextSyncPointRtc-1;*/

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
    EMU->CTRL = 0;
}

inline void IRQclearDeepSleep()
{
    // clearing deep sleep mask
    SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk; 
}

inline void IRQresyncClocks()
{
    #ifdef WITH_VHT // Adjust hsc time using RTC and VHT
    vht->IRQresyncClock();
    #endif
    //timerProxy->recompute correction...
    #else // No VHT, just pass time between each other
    hsc->IRQsetTimeNs(rtc->IRQgetTimeNs());
    #endif
    
}

inline void IRQexecutePendingirq()
{
    IRQresyncClocks();
    fastEnableInterrupts();
    __NOP(); // NOP operation to be sure that the interrupt can be executed
    fastDisableInterrupts();
}

} // namespace miosix