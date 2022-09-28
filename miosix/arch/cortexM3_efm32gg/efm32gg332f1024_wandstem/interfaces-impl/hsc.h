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

#if defined(WITH_DEEP_SLEEP) || defined(WITH_VHT)
#include "interfaces-impl/rtc.h"
#endif

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
 * 
 * Implements VHT required functions
 *     static void IRQinitVhtTimer() {}
 *     static void IRQstartVhtTimer() {}
 *
 *     static inline unsigned int IRQgetVhtTimerCounter() {}
 *     static inline void IRQsetVhtTimerCounter(unsigned int v) {}
 *
 *     static inline bool IRQgetVhtMatchFlag() {}
 *     static inline void IRQclearVhtMatchFlag() {}
 *
 *     static inline void IRQsetVhtMatchReg(unsigned int v) {}
 *
 * Implements Event timeout functions
 *
 */

/**
 * Timers configuration: 
 * 
 * TIMER0->CC[0] free
 * TIMER0->CC[1] free
 * TIMER0->CC[2] free
 * 
 * TIMER1->CC[0] OUTPUT_COMPARE, os IRQ (lower 32-bit)
 * TIMER2->CC[0] OUTPUT_COMPARE, os IRQ (upper 32-bit)
 * 
 * TIMER1->CC[1] OUTPUT_COMPARE, event timeout or trigger (lower 32-bit)
 * TIMER2->CC[1] OUTPUT_COMPARE, event timeout or trigger (upper 32-bit)
 * 
 * TIMER1->CC[2] INPUT_CAPTURE, timestamping (lower 32-bit)
 * TIMER2->CC[2] INPUT_CAPTURE, timestamping (upper 32-bit)
 * 
 * TIMER3->CC[0] INPUT_CAPTURE, VHT
 * TIMER3->CC[1] free
 * TIMER3->CC[2] free
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
        return (TIMER2->CC[0].CCV<<16) | TIMER1->CC[0].CCV;
    }

    static inline void IRQsetTimerMatchReg(unsigned int v)
    {
        // clear previous TIMER2 setting
        TIMER2->IFC |= TIMER_IFC_CC0;
        //NVIC_ClearPendingIRQ(TIMER2_IRQn);

        // extracting lower and upper 16-bit parts from match value
        uint16_t lower_ticks = static_cast<uint16_t> (v & 0xFFFFUL); // lower part
        uint16_t upper_ticks = static_cast<uint16_t> (v >> 16) & 0xFFFFUL; // upper part

        // set output compare timer register
        // because of an undocumented quirk, the compare register is checked 1 tick late. 
        // By subtracting one tick, we have to make sure we do not underflow!
        Hsc::nextCCticksLower = lower_ticks == 0 ? 0 : lower_ticks-1; // underflow handling
        Hsc::nextCCticksUpper = upper_ticks == 0 ? 0 : upper_ticks-1; // underflow handling
        
        // set and enable output compare interrupt on channel 0 for most significant timer
        TIMER2->CC[0].CCV = Hsc::nextCCticksUpper;
        TIMER2->IEN |= TIMER_IEN_CC0;
        TIMER2->CC[0].CTRL |= TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;
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
        TIMER1->CC[0].CCV = 0;

        // clear output compare flag
        TIMER1->IFC |= TIMER_IFC_CC0;

        // clear pending interrupt
        //NVIC_ClearPendingIRQ(TIMER1_IRQn);
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

        TIMER3->CTRL = TIMER_CTRL_MODE_UP | TIMER_CTRL_CLKSEL_PRESCHFPERCLK 
                        | TIMER_CTRL_PRESC_DIV1; 
        TIMER3->IEN |= TIMER_IEN_CC0; // enable interrupt on CC0

        NVIC_SetPriority(TIMER3_IRQn, 8);
        NVIC_ClearPendingIRQ(TIMER3_IRQn);
        NVIC_EnableIRQ(TIMER3_IRQn);

        // setting PRS on channel 4 to capture RTC timer at COMP1 in CC[0]
        TIMER3->CC[0].CTRL=TIMER_CC_CTRL_ICEDGE_RISING
            | TIMER_CC_CTRL_FILT_DISABLE
            | TIMER_CC_CTRL_INSEL_PRS
            | TIMER_CC_CTRL_PRSSEL_PRSCH4
            | TIMER_CC_CTRL_MODE_INPUTCAPTURE;
        PRS->CH[4].CTRL = PRS_CH_CTRL_SOURCESEL_RTC | PRS_CH_CTRL_SIGSEL_RTCCOMP1;
    }

    static void IRQstartVhtTimer()
    {
        // start timer
        TIMER3->CMD = TIMER_CMD_START;
    }

    // TODO: (s) document that read CCV overwrites reg with CCVB
    static inline unsigned int IRQgetVhtTimerCounter()
    {
        return (TIMER2->CNT<<16) | TIMER3->CC[0].CCV;
    }

    static inline void IRQsetVhtTimerCounter(unsigned int v)
    {
        TIMER3->CNT = static_cast<uint16_t>(v & 0xFFFFUL);
    }

    static inline bool IRQgetVhtMatchFlag()
    {
        return TIMER3->IF & TIMER_IF_CC0;
    }

    static inline void IRQclearVhtMatchFlag()
    {
        TIMER3->IFC |= TIMER_IFC_CC0;
    }

    static inline void IRQsetVhtMatchReg(unsigned int v)
    {
        IRQclearVhtMatchFlag();

        // undocumented quirk, CC 1 tick after
        //unsigned int v_quirk = v == 0 ? 0 : v-1; // handling underflow
        // TODO: (s) finish
    }

    #endif // #ifdef WITH_VHT

    ///
    // Event extension
    ///
    static inline unsigned int IRQgetEventMatchReg()
    {
        return (TIMER2->CC[1].CCV<<16) | TIMER1->CC[1].CCV;
    }

    static inline void IRQsetEventMatchReg(unsigned int v)
    {  
        // clear previous TIMER1 setting
        TIMER1->IFC |= TIMER_IFC_CC1;
        //NVIC_ClearPendingIRQ(TIMER1_IRQn);

        // extracting lower and upper 16-bit parts from match value
        uint16_t lower_ticks = static_cast<uint16_t> (v & 0xFFFFUL); // lower part
        uint16_t upper_ticks = static_cast<uint16_t> (v >> 16) & 0xFFFFUL; // upper part

        // set output compare timer register
        // because of an undocumented quirk, the compare register is checked 1 tick late. 
        // By subtracting one tick, we have to make sure we do not underflow!
        Hsc::nextCCeventLower = lower_ticks == 0 ? 0 : lower_ticks-1; // underflow handling
        Hsc::nextCCeventUpper = upper_ticks == 0 ? 0 : upper_ticks-1; // underflow handling
        
        // set and enable output compare interrupt on channel 0 for most significant timer
        TIMER1->CC[1].CCV = Hsc::nextCCeventLower;
        TIMER1->IEN |= TIMER_IEN_CC1;
        TIMER1->CC[1].CTRL |= TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;
    }

    static inline bool IRQgetEventFlag()
    {
        //return TIMER1->IF & TIMER_IF_CC1;
        return TIMER2->IF & TIMER_IF_CC1;
    }

    static inline void IRQclearEventFlag()
    {
        // disable output compare interrupt
        TIMER1->IEN &= ~TIMER_IEN_CC1; // signal capture and compare register for OS interrupts
        //TIMER1->CC[1].CTRL &= ~TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;

        TIMER2->IEN &= ~TIMER_IEN_CC1;
        //TIMER2->CC[1].CTRL &= ~TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;

        // clear output compare flag
        TIMER1->IFC |= TIMER_IFC_CC1;
        TIMER2->IFC |= TIMER_IFC_CC1;

        // clear pending interrupt
        //NVIC_ClearPendingIRQ(TIMER1_IRQn);
    }

    static inline void IRQforcePendingEvent()
    {
        NVIC_SetPendingIRQ(TIMER2_IRQn);
    }

    ///
    // static variables getters
    ///
    static unsigned int IRQgetNextCCticksUpper() { return Hsc::nextCCticksUpper; }
    static unsigned int IRQgetNextCCticksLower() { return Hsc::nextCCticksLower; }

    static unsigned int IRQgetNextCCeventUpper() { return Hsc::nextCCeventUpper; }
    static unsigned int IRQgetNextCCeventLower() { return Hsc::nextCCeventLower; }

    // TODO: (s) set timer3 (input capture) input on CC1, read timestamp from TIMER3 in input capute etc...

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

    inline static unsigned int nextCCticksUpper;
    inline static unsigned int nextCCticksLower;
    inline static unsigned int nextCCeventUpper;
    inline static unsigned int nextCCeventLower;

    static const unsigned int frequency = EFM32_HFXO_FREQ; //48000000 Hz if NOT prescaled!
};

}//end miosix namespace


#endif /* HIGH_SPEED_CLOCK */

