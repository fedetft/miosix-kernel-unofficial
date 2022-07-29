#include "interfaces/deep_sleep.h"
#include "interfaces/os_timer.h"
#include "rtc.h"
#include "hsc.h"
#include "time/timeconversion.h"
#include <sys/ioctl.h>
#include "stdio.h"
#include "miosix.h" // DELETEME: (s)

namespace miosix {

static Rtc* rtc = nullptr; 
static Hsc* hsc = nullptr;
static TimeConversion* HSCtc = nullptr;
static TimeConversion* RTCtc = nullptr;

// forward declaration
void doIRQdeepSleep(long long);
void IRQrestartHFXO();

///
// Main interface impl
///

void IRQdeepSleepInit()
{
    // instances of RTC and HSC
    rtc     = &Rtc::instance();
    hsc     = &Hsc::instance();

    FastInterruptDisableLock l;
    HSCtc      = new TimeConversion(hsc->IRQTimerFrequency());
    RTCtc      = new TimeConversion(rtc->IRQTimerFrequency());
}

bool IRQdeepSleep(long long abstime)
{
    miosix::ledOn();
    // 1. RTC set ticks from High Speed Clock
    rtc->IRQsetTimeNs(hsc->IRQgetTimeNs());
    iprintf("%lld\n", rtc->IRQgetTimeNs() - hsc->IRQgetTimeNs());

    // 2. Perform deep sleep
    //doIRQdeepSleep(abstime);

    // 3. Set HSC ticks from Real Time Clock
    //hsc->IRQsetTimeNs(rtc->IRQgetTimeNs());

    miosix::ledOff();
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
    //abstime = ticks / 46875 * 32; ?????

    ioctl(STDOUT_FILENO,IOCTL_SYNC,0);
    
    PauseKernelLock pkLock; //To run unexpected IRQs without context switch

    // RTC deep sleep function that sets RTC compare register
    rtc->IRQsetDeepSleepIrqTick(ticks);

    // TODO: (s) bring everything inside os_timer RTCTimerAdapter (modify IRQhandler)
    // Flag to enable the deepsleep when we will call _WFI, 
    // otherwise _WFI is translated as a simple sleep status, this means that the core is not running 
    // but all the peripheral (HF and LF), are still working and they can trigger exception
    SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk; 
    EMU->CTRL=0;

    miosix::greenLed::high();

    // deep sleep loop
    for(;;)
    {
        // if for whatever reason we're past the deep sleep time, wake up
        if(ticks <= rtc->IRQgetTimeTick())
        {
            break;
        }
        
        // Goes into low power mode and waits for RTC pending interrupt
        /*__WFI(); // blocking statement.  

        IRQrestartHFXO(); // restarts High frequency oscillator

        rtc->IRQoverflowHandler(); // check if interrupt was because of an overflow

        // checks if we went out of deep sleep because of the RTC interrupt
        if(NVIC_GetPendingIRQ(RTC_IRQn))
        {
            //Clear the interrupt (both in the RTC peripheral and NVIC),
            //this is important as pending IRQ prevent WFI from working
            NVIC_ClearPendingIRQ(RTC_IRQn);

            // wake up time has past already
            if(ticks <= rtc->IRQgetTimeTick())
            {
                miosix::greenLed::low();
                RTC->IFC |= RTC_IFC_COMP0;
                break;
            }
            else
            {
                // TODO: (s)
                // FIXME: (s) necessario IRQresyncClock()
                // Se ho altri interrupt (> 1) vanno tutti con clock non corretto
                // Reimplement RTC timer adapter
                //FastInterruptEnableLock eLock(dLock);
                // Here interrupts are enabled, so the software part of RTC 
                // can be updated
                // NOP operation to be sure that the interrupt can be executed
                __NOP();
            }
        }
        else // something else generated an interrupt
        {
            // TODO: (s) ...
            // we have an interrupt set before going to deep sleep, we have to serve it 
            // or the micro will never go into sleep mode and __WFI() macro becomes a NOP instruction
            //IRQresyncClock();
            //{
            //    FastInterruptEnableLock eLock(dLock);
            //    //Here interrupts are enabled, so the interrupt gets served
            //    __NOP();
            //}
        }*/
    }

    // post deep sleep
    SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk; 
    //We need this interrupt to continue the sync of HRT with RTC
    //RTC->IEN &= ~RTC_IEN_COMP1; 
}

void IRQrestartHFXO()
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

} // namespace miosix