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


#ifndef HSC_H
#define HSC_H

#include "interfaces/os_timer.h"
#include "interfaces-impl/bsp_impl.h"

namespace miosix {
/**
 * Manages the high frequency hardware timer
 * This class is not safe to be accessed by multiple threads simultaneously.
 * 
 * Implements TimerAdapter required functions
 *     static inline unsigned int IRQgetTimerCounter() {}
 *     static inline void IRQsetTimerCounter(unsigned int v) {}
 * 
 *     static inline unsigned int IRQgetTimerMatchReg() {}
 *     static inline void IRQsetTimerMatchReg(unsigned int v) {}
 * 
 *     static inline bool IRQgetOverflowFlag() {}
 *     static inline void IRQclearOverflowFlag() {}
 *     
 *     static inline bool IRQgetMatchFlag() {}
 *     static inline void IRQclearMatchFlag() {}
 *     
 *     static inline void IRQforcePendingIrq() {}
 * 
 *     static inline void IRQstopTimer() {}
 *     static inline void IRQstartTimer() {}
 * 
 *     static unsigned int IRQTimerFrequency() {}
 * 
 *     static void IRQinit() {}
 */

class Hsc : public TimerAdapter<Hsc, 32>
{
public:
    static Hsc& instance()
    {
        static Hsc hsc;
        return hsc;
    }

    ///
    // TimerAdapter extension
    ///
    static inline unsigned int IRQgetTimerCounter()
    {
        return IRQread32Timer();
    }

    static inline void IRQsetTimerCounter(unsigned int v)
    {
        TIMER1->CNT = static_cast<uint16_t> (v & 0xFFFFUL); // lower part
        TIMER2->CNT = static_cast<uint16_t> ((v >> 16) & 0xFFFFUL); // upper part
    }

    static inline unsigned int IRQgetTimerMatchReg()
    {
        return (static_cast<unsigned int>(TIMER2->CC[0].CCV)<<16 ) | TIMER1->CC[0].CCV;
    }

    static inline void IRQsetTimerMatchReg(unsigned int v)
    {
        IRQclearMatchFlag();

        // set output compare timer register
        // because of an undocumented quirk, the compare register is checked 
        // 1 tick late. By subtracting one tick, we have to make sure we do not underflow!
        uint16_t lower_ticks = static_cast<uint16_t> (v & 0xFFFFUL); // lower part
        uint16_t upper_ticks = static_cast<uint16_t> (v >> 16) & 0xFFFFUL; // upper part
        TIMER1->CC[0].CCV = lower_ticks == 0 ? 0 : lower_ticks-1; // underflow handling
        TIMER2->CC[0].CCV = upper_ticks == 0 ? 0 : upper_ticks-1; // underflow handling

        // enable output compare interrupt on channel 0 for most significant timer
        TIMER2->IEN |= TIMER_IEN_CC0;
        TIMER2->CC[0].CTRL |= TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;

        // disable output compare interrupt on channel 0 for least significant timer
        TIMER1->IEN &= ~TIMER_IEN_CC0;
        TIMER1->CC[0].CTRL &= ~TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;
        
    }

    static inline bool IRQgetOverflowFlag()
    {
        return TIMER2->IF & TIMER_IF_OF;
    }

    static inline void IRQclearOverflowFlag()
    {
        TIMER2->IFC |= TIMER_IFC_OF;
    }

    static inline bool IRQgetMatchFlag()
    {
        return TIMER1->IF & TIMER_IF_CC0;
    }

    static inline void IRQclearMatchFlag()
    {
        // disable output compare interrupt
        TIMER1->IEN &= ~TIMER_IEN_CC0; // signal capture and compare register for OS interrupts
        TIMER1->CC[0].CTRL &= ~TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;
        TIMER2->IEN &= ~TIMER_IEN_CC0; // signal capture and compare register for OS interrupts
        TIMER2->CC[0].CTRL &= ~TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;

        // clear output compare flag
        TIMER1->IFC |= TIMER_IFC_CC0;
        TIMER2->IFC |= TIMER_IFC_CC0;

        // clear pending interrupt
        NVIC_ClearPendingIRQ(TIMER1_IRQn);
        NVIC_ClearPendingIRQ(TIMER2_IRQn);
    }

    static inline void IRQforcePendingIrq()
    {
        NVIC_SetPendingIRQ(TIMER1_IRQn);
    }

    static inline void IRQstopTimer()
    {
        TIMER1->CMD=TIMER_CMD_STOP;
    }

    static inline void IRQstartTimer()
    {
        TIMER1->CTRL |= TIMER_CTRL_SYNC;
        TIMER2->CTRL |= TIMER_CTRL_SYNC;

        TIMER1->CMD = TIMER_CMD_START;
        
        //Synchronization is required only when timers are to start.
        //If the sync is not disabled after start, start/stop on another timer
        //(e.g. TIMER0) will affect the behavior of context switch timer!
        TIMER1->CTRL &= ~TIMER_CTRL_SYNC;
        TIMER2->CTRL &= ~TIMER_CTRL_SYNC;
    }

    static unsigned int IRQTimerFrequency()
    {
        return frequency;
    }

    static void IRQinitTimer()
    {
        // clock timers configuration (chaning two 16 bit timers to get a 32 one)
        // TIMER1 => lower part
        // TIMER2 => upper part
        // When TIMER1 overflows, TIMER2 counter is updated 
        // (automatic chaninig for neighborhood timers with CTRL_SYNC)

        // power the timers
        CMU->HFPERCLKEN0 |= CMU_HFPERCLKEN0_TIMER1 | CMU_HFPERCLKEN0_TIMER2 | CMU_HFPERCLKEN0_PRS;

        // timers control config
        TIMER1->CTRL = TIMER_CTRL_MODE_UP | TIMER_CTRL_CLKSEL_PRESCHFPERCLK 
                | TIMER_CTRL_PRESC_DIV1;
        
        TIMER2->CTRL = TIMER_CTRL_MODE_UP | TIMER_CTRL_CLKSEL_TIMEROUF;

        // reset TIMER1, needed if you want run after the flash
        TIMER1->CMD=TIMER_CMD_STOP;
        TIMER1->ROUTE=0;
        TIMER1->IEN=0;
        TIMER1->IFC=~0;
        TIMER1->TOP=0xFFFF;
        TIMER1->CNT=0;
        TIMER1->CC[0].CTRL=0;
        TIMER1->CC[0].CCV=0;
        TIMER1->CC[1].CTRL=0;
        TIMER1->CC[1].CCV=0;
        TIMER1->CC[2].CTRL=0;
        TIMER1->CC[2].CCV=0;

        // reset TIMER2
        TIMER2->CMD=TIMER_CMD_STOP;
        TIMER2->IEN=0;
        TIMER2->IFC=~0;
        TIMER2->TOP=0xFFFF;
        TIMER2->CNT=0;
        TIMER2->CC[0].CTRL=0;
        TIMER2->CC[0].CCV=0;

        //Enable necessary interrupt lines
        TIMER1->IEN = 0; // |= TIMER_IEN_CC0;
        TIMER2->IEN |= TIMER_IEN_OF; // signaling overflow, needed for pending bit trick

        NVIC_SetPriority(TIMER1_IRQn, 3);
        NVIC_SetPriority(TIMER2_IRQn, 3);

        NVIC_ClearPendingIRQ(TIMER1_IRQn);
        NVIC_ClearPendingIRQ(TIMER2_IRQn);

        NVIC_EnableIRQ(TIMER1_IRQn);
        NVIC_EnableIRQ(TIMER2_IRQn);   
    }

    ///
    // VHT extension
    ///
    #ifdef WITH_VHT

    static void IRQinitVhtTimer()
    {
        // enabling TIMER3
        CMU->HFPERCLKEN0 |= CMU_HFPERCLKEN0_TIMER3;

        // TODO: (s) not needed that TIMER2 starts actually, just left here for future use like packet retransmission
        TIMER3->CTRL = TIMER_CTRL_MODE_UP | TIMER_CTRL_CLKSEL_PRESCHFPERCLK 
                        | TIMER_CTRL_PRESC_DIV1; 

        NVIC_SetPriority(TIMER3_IRQn, 8);
        NVIC_ClearPendingIRQ(TIMER3_IRQn);
        NVIC_EnableIRQ(TIMER3_IRQn);
    }

    static void IRQstartVhtTimer()
    {
        __NOP(); // TODO: (s) start TIMER3
    }

    static inline unsigned int IRQgetTimerCounter()
    {
        return 0; // read TIMER3 (lower, RTC COMP1 propagated thought PRS) + TIMER2<<16 (upper)
    }

    static inline bool IRQgetVhtMatchFlag()
    {
        __NOP();
        return 0;
    }

    static inline void IRQclearVhtMatchFlag()
    {
         __NOP();
    }

    static inline void IRQsetVhtMatchReg(unsigned int v)
    {
        IRQclearVhtMatchFlag();

        // undocumented quirk, CC 1 tick after
        unsigned int v_quirk = v == 0 ? 0 : v-1; // handling underflow
        // TODO: (s) finish
    }

    #endif // #ifdef WITH_VHT

private:
    /**
     * Constructor
     */
    Hsc() {};
    Hsc(const Hsc&)=delete;
    Hsc& operator=(const Hsc&)=delete;
    
    static inline unsigned int IRQread32Timer(){
        unsigned int high = TIMER2->CNT;
        unsigned int low = TIMER1->CNT;
        unsigned int high2 = TIMER2->CNT;
        
        if(high == high2) // No lower part overflow
        {
            return (high<<16) | low; // constructing 32 bit number
        }

        return high2<<16 | TIMER1->CNT;
    }

    static const unsigned int frequency = EFM32_HFXO_FREQ; //48000000 Hz if NOT prescaled!
};

}//end miosix namespace

void __attribute__((naked)) TIMER1_IRQHandler(); // forward declaration

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
    static Hsc * hsc = &Hsc::instance();

    // save value of TIMER1 counter right away to check for compare later
    unsigned int lowerTimerCounter = TIMER1->CNT;

    // TIMER2 overflow, pending bit trick.
    if(hsc->IRQgetOverflowFlag()) hsc->IRQhandler();

    // if just overflow, no TIMER2 counter register match, return
    if(!(TIMER2->IF & TIMER_IF_CC0)) return;
    
    // first part of output compare, disable output compare interrupt
    // for TIMER2 and turn of output comapre interrupt of TIMER1
    miosix::ledOn();

    // disable output compare interrupt on channel 0 for most significant timer
    TIMER2->IEN &= ~TIMER_IEN_CC0;
    TIMER2->CC[0].CTRL &= ~TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;
    TIMER2->IFC |= TIMER_IFC_CC0;

    // if by the time we get here, the lower part of the counter has already matched
    // the counter register or got past it, call TIMER1 handler directly
    if(lowerTimerCounter >= TIMER1->CC[0].CCV) TIMER1_IRQHandler();
    else
    {
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
    static Hsc * hsc = &Hsc::instance();

    // second part of output compare. If we reached this interrupt, it means
    // we already matched the upper part of the timer and we have now matched the lower part.
    hsc->IRQhandler();

    miosix::ledOff();
}

#endif /* HIGH_SPEED_CLOCK */

