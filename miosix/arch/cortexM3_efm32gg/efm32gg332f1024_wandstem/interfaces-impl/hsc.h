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
        TIMER1->CC[0].CCV = (uint16_t) (v & 0xFFFFUL); // lower part
        TIMER2->CC[0].CCV = (uint16_t) ((v >> 16) & 0xFFFFUL); // upper part
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
        return (TIMER2->IF & TIMER_IF_CC0) & (TIMER1->IF & TIMER_IF_CC0);
    }

    static inline void IRQclearMatchFlag()
    {
        TIMER2->IFS &= ~TIMER_IFS_CC0;
        TIMER1->IFS &= ~TIMER_IFS_CC0;
    }

    static inline void IRQforcePendingIrq()
    {
        NVIC_SetPendingIRQ(TIMER2_IRQn);
    }

    static inline void IRQstopTimer()
    {
        TIMER1->CMD=TIMER_CMD_STOP;
    }

    static inline void IRQstartTimer()
    {
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
        //Power the timers up and PRS system
        CMU->HFPERCLKEN0 |= CMU_HFPERCLKEN0_TIMER1 | CMU_HFPERCLKEN0_TIMER2 | CMU_HFPERCLKEN0_PRS;

        // configure timers (chaning two 16 bit timers to get a 32 one)
        // TIMER1 => lower part
        // TIMER2 => upper part
        // When TIMER1 overflows, TIMER2 counter is updated 
        // (automatic chaninig for neighborhood timers with CTRL_SYNC)
        TIMER1->CTRL = TIMER_CTRL_MODE_UP | TIMER_CTRL_CLKSEL_PRESCHFPERCLK 
                | TIMER_CTRL_PRESC_DIV1 | TIMER_CTRL_SYNC;
        
        TIMER2->CTRL = TIMER_CTRL_MODE_UP | TIMER_CTRL_CLKSEL_TIMEROUF 
                | TIMER_CTRL_SYNC;

        // reset TIMER1, needed if you want run after the flash
        TIMER1->CMD=TIMER_CMD_STOP;
        TIMER1->CTRL=0;
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

        //Enable necessary interrupt lines
        TIMER1->IEN = 0; //TIMER_IEN_CC0;
        TIMER2->IEN = TIMER_IEN_OF; // signaling overflow, needed for pending bit trick

        TIMER1->CC[1].CTRL = TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;
        TIMER2->CC[1].CTRL = TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;

        NVIC_SetPriority(TIMER1_IRQn,3);
        NVIC_SetPriority(TIMER2_IRQn,3);

        NVIC_ClearPendingIRQ(TIMER1_IRQn);
        NVIC_ClearPendingIRQ(TIMER2_IRQn);

        NVIC_EnableIRQ(TIMER1_IRQn);
        NVIC_EnableIRQ(TIMER2_IRQn);
    }

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

#endif /* HIGH_SPEED_CLOCK */

