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


#pragma once

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

//> NEW CONFIGURATION <//
/**
 * Timers configuration: 
 * TIMER0 [Unclocked][free]
 * TIMER1 [Clocked]
 * TIMER2 [Clocked]
 * TIMER3 [Unclocked][OFU]
 * 
 * TIMER0->CC[0] free
 * TIMER0->CC[1] free
 * TIMER0->CC[2] free
 * 
 * TIMER1->CC[0] free
 * TIMER1->CC[1] INPUT_CAPTURE, VHT <- RTC OUTPUT_COMPARE (PRS ch4)
 * TIMER1->CC[2] OUTPUT_COMPARE TIMESTAMP_OUT -> (only lower 32-bit) / INPUT_CAPTURE (lower 32-bit) <- TIMESTAMP_IN (PRS ch1)
 * 
 * TIMER2->CC[0] OUTPUT_COMPARE, os IRQ (lower 32-bit)
 * TIMER2->CC[1] OUTPUT_COMPARE trigger STXON -> (only lower 32-bit) / INPUT_CAPTURE (lower 32-bit) <- SFD (PRS ch0)
 * TIMER2->CC[2] OUTOUT_COMPARE, timeout (only lower 32-bit)
 * 
 * TIMER3->CC[0] OUTPUT_COMPARE, os IRQ (upper 32-bit)
 * TIMER3->CC[1] INPUT_CAPTURE, (upper 32-bit) <- SFD (PRS ch0)
 * TIMER3->CC[2] INPUT_CAPTURE, timestamping (upper 32-bit) TIMESTAMP_IN (PRS ch1)
 * 
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
        TIMER2->CNT = static_cast<uint16_t> (v & 0xFFFFUL); // lower part
        TIMER3->CNT = static_cast<uint16_t> ((v >> 16) & 0xFFFFUL); // upper part
    }

    static inline unsigned int IRQgetTimerMatchReg()
    {
        return (TIMER3->CC[0].CCV<<16) | TIMER2->CC[0].CCV;
    }

    static inline void IRQsetTimerMatchReg(unsigned int v)
    {
        // clear previous TIMER2 setting
        TIMER3->IFC = TIMER_IFC_CC0;
        //NVIC_ClearPendingIRQ(TIMER2_IRQn);

        // extracting lower and upper 16-bit parts from match value
        uint16_t lower_ticks = static_cast<uint16_t> (v & 0xFFFFUL); // lower part
        uint16_t upper_ticks = static_cast<uint16_t> (v >> 16) & 0xFFFFUL; // upper part

        // set output compare timer register
        // because of an undocumented quirk, the compare register is checked 1 tick late. 
        Hsc::nextCCticksLower = lower_ticks-1; // underflow handling
        Hsc::nextCCticksUpper = upper_ticks-1; // underflow handling

        // set and enable output compare interrupt on channel 0 for most significant timer
        TIMER3->CC[0].CCV = Hsc::nextCCticksUpper;
        TIMER3->IEN |= TIMER_IEN_CC0;
        TIMER3->CC[0].CTRL |= TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;
    
        // if upper part matches already, activate lower part interrupt directly
        if(TIMER3->CNT == upper_ticks)
        {
            ledOn();
            // static_cast<TIMER_TypeDef *>(0x40010C00UL)->CNT // DELETEME: (s)
            // disable output compare interrupt on channel 0 for most significant timer
            TIMER3->IEN &= ~TIMER_IEN_CC0;
            TIMER3->CC[0].CTRL &= ~TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;
            TIMER3->IFC = TIMER_IFC_CC0;
            
            // set and enable output compare interrupt on channel 0 for least significant timer
            TIMER2->CC[0].CCV = Hsc::nextCCticksLower; // underflow handling
            TIMER2->IEN |= TIMER_IEN_CC0;
            TIMER2->CC[0].CTRL |= TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;

            // not only we're already matching 32-bit extension and 16-bit upper part 
            // but also we got past the lower 16-bit
            // NOTE: may not occur if lower_ticks is close to 0xffff
            if(TIMER2->CNT >= lower_ticks) 
            {
                TIMER2->IFS = TIMER_IFS_CC0;
                Hsc::IRQforcePendingIrq();
            }
        }
    }

    static inline bool IRQgetOverflowFlag()
    {
        return TIMER3->IF & TIMER_IF_OF;
    }

    static inline void IRQclearOverflowFlag()
    {
        TIMER3->IFC = TIMER_IFC_OF;
    }

    static inline bool IRQgetMatchFlag()
    {
        return TIMER2->IF & TIMER_IF_CC0;
    }

    static inline void IRQclearMatchFlag()
    {
        // clear output compare flag
        TIMER2->IFC = TIMER_IFC_CC0;

        // clear pending interrupt
        //NVIC_ClearPendingIRQ(TIMER1_IRQn);
    }

    static inline void IRQforcePendingIrq()
    {
        NVIC_SetPendingIRQ(TIMER2_IRQn);
    }

    static inline void IRQstopTimer()
    {
        TIMER1->CMD=TIMER_CMD_STOP;
        TIMER2->CMD=TIMER_CMD_STOP;
    }

    static inline void IRQstartTimer()
    {
        TIMER1->CTRL |= TIMER_CTRL_SYNC;
        TIMER2->CTRL |= TIMER_CTRL_SYNC;
        TIMER3->CTRL |= TIMER_CTRL_SYNC;

        TIMER2->CMD = TIMER_CMD_START;
        
        //Synchronization is required only when timers are to start.
        //If the sync is not disabled after start, start/stop on another timer
        //(e.g. TIMER0) will affect the behavior of context switch timer!
        TIMER1->CTRL &= ~TIMER_CTRL_SYNC;
        TIMER2->CTRL &= ~TIMER_CTRL_SYNC;
        TIMER3->CTRL &= ~TIMER_CTRL_SYNC;
    }

    static unsigned int IRQTimerFrequency()
    {
        return frequency;
    }

    static void IRQinitTimer()
    {
        // clock timers configuration (chaning two 16 bit timers to get a 32 one)
        // TIMER2 => lower part
        // TIMER3 => upper part
        // When TIMER2 overflows, TIMER3 counter is updated 
        // (automatic chaninig for neighborhood timers with CTRL_SYNC)
        // TIMER1 is used just for timestamp_out, check top of file

        // power the timers
        CMU->HFPERCLKEN0 |= CMU_HFPERCLKEN0_TIMER1 | CMU_HFPERCLKEN0_TIMER2 | CMU_HFPERCLKEN0_TIMER3 | CMU_HFPERCLKEN0_PRS;

        // timers control config
        TIMER1->CTRL = TIMER_CTRL_MODE_UP | TIMER_CTRL_CLKSEL_PRESCHFPERCLK 
                | TIMER_CTRL_PRESC_DIV1;
        TIMER2->CTRL = TIMER_CTRL_MODE_UP | TIMER_CTRL_CLKSEL_PRESCHFPERCLK 
                | TIMER_CTRL_PRESC_DIV1;
        TIMER3->CTRL = TIMER_CTRL_MODE_UP | TIMER_CTRL_CLKSEL_TIMEROUF;

        // reset timers after flashing

        // TIMER1
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
        
        // TIMER2
        TIMER2->CMD=TIMER_CMD_STOP;
        TIMER2->ROUTE=0;
        TIMER2->IEN=0;
        TIMER2->IFC=~0;
        TIMER2->TOP=0xFFFF;
        TIMER2->CNT=0;
        TIMER2->CC[0].CTRL=0;
        TIMER2->CC[0].CCV=0;
        TIMER2->CC[1].CTRL=0;
        TIMER2->CC[1].CCV=0;
        TIMER2->CC[2].CTRL=0;
        TIMER2->CC[2].CCV=0;

        // TIMER3
        TIMER3->CMD=TIMER_CMD_STOP;
        TIMER3->ROUTE=0;
        TIMER3->IEN=0;
        TIMER3->IFC=~0;
        TIMER3->TOP=0xFFFF;
        TIMER3->CNT=0;
        TIMER3->CC[0].CTRL=0;
        TIMER3->CC[0].CCV=0;
        TIMER3->CC[1].CTRL=0;
        TIMER3->CC[1].CCV=0;
        TIMER3->CC[2].CTRL=0;
        TIMER3->CC[2].CCV=0;

        // signaling overflow, needed for pending bit trick
        TIMER3->IEN |= TIMER_IEN_OF;
        
        NVIC_SetPriority(TIMER1_IRQn, 4);
        NVIC_SetPriority(TIMER2_IRQn, 3);
        NVIC_SetPriority(TIMER3_IRQn, 3);

        NVIC_ClearPendingIRQ(TIMER1_IRQn);
        NVIC_ClearPendingIRQ(TIMER2_IRQn);
        NVIC_ClearPendingIRQ(TIMER3_IRQn);

        NVIC_EnableIRQ(TIMER1_IRQn);
        NVIC_EnableIRQ(TIMER2_IRQn);
        NVIC_EnableIRQ(TIMER3_IRQn);   
    }

    ///
    // VHT extension
    ///

    static void IRQinitVhtTimer()
    {
        // setting PRS on channel 4 to capture RTC timer at COMP1 in CC[1]
        TIMER1->CC[1].CTRL=TIMER_CC_CTRL_ICEDGE_RISING
            | TIMER_CC_CTRL_FILT_DISABLE
            | TIMER_CC_CTRL_INSEL_PRS
            | TIMER_CC_CTRL_PRSSEL_PRSCH4
            | TIMER_CC_CTRL_MODE_INPUTCAPTURE;
        PRS->CH[4].CTRL = PRS_CH_CTRL_SOURCESEL_RTC | PRS_CH_CTRL_SIGSEL_RTCCOMP1;

        TIMER1->IEN |= TIMER_IEN_CC1;
    }

    static void IRQstartVhtTimer()
    {
        // start timer
        //TIMER1->CMD = TIMER_CMD_START; //  TIMER1 is already clocked
    }

    static inline unsigned int IRQgetVhtTimerCounter()
    {
        return (TIMER3->CNT<<16) | TIMER1->CC[1].CCV;
    }

    static inline bool IRQgetVhtMatchFlag()
    {
        return TIMER1->IF & TIMER_IF_CC1;
    }

    static inline void IRQclearVhtMatchFlag()
    {
        TIMER1->IFC = TIMER_IFC_CC1;
    }

    static inline void IRQsetVhtMatchReg(unsigned int v)
    {
        IRQclearVhtMatchFlag();

        // undocumented quirk, CC 1 tick after
        //unsigned int v_quirk = v == 0 ? 0 : v-1; // handling underflow
        // TODO: (s) finish
    }

    ///
    // Timeout extension 
    ///
    static inline unsigned int IRQgetTimeoutMatchReg()
    {
        return 0; //(TIMER3->CC[1].CCV<<16) | TIMER2->CC[1].CCV;
    }

    static inline void IRQsetTimeoutMatchReg(unsigned int v)
    {  
        // clear previous TIMER2 setting
        TIMER2->IFC = TIMER_IFC_CC2;
        //NVIC_ClearPendingIRQ(TIMER3_IRQn);

        // extracting lower and upper 16-bit parts from match value
        uint16_t lower_ticks = static_cast<uint16_t> (v & 0xFFFFUL); // lower part
        uint16_t upper_ticks = static_cast<uint16_t> (v >> 16) & 0xFFFFUL; // upper part
        
        // set output compare timer register
        // because of an undocumented quirk, the compare register is checked 1 tick late. 
        // By subtracting one tick, we have to make sure we do not underflow!
        // note: upper part is not decremented because it's only used to be checked
        // inside ISR and does not play am active role in CCV (where the quirk is)
        Hsc::nextCCtimeoutLower = lower_ticks == 0 ? 0 : lower_ticks-1; // underflow handling
        Hsc::nextCCtimeoutUpper = upper_ticks;

        // set and enable output compare interrupt on channel 1 for most significant timer
        TIMER2->CC[2].CCV = Hsc::nextCCtimeoutLower;
        TIMER2->IEN |= TIMER_IEN_CC2;
        TIMER2->CC[2].CTRL |= TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;
    }

    static inline bool IRQgetTimeoutFlag()
    {
        //return TIMER1->IF & TIMER_IF_CC1;
        return TIMER2->IF & TIMER_IF_CC2;
    }

    static inline void IRQclearTimeoutFlag()
    {
        // disable output compare interrupt
        TIMER2->IEN &= ~TIMER_IEN_CC2; // signal capture and compare register for OS interrupts
        //TIMER2->CC[1].CTRL &= ~TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;
        
        // clear output compare flag
        TIMER2->IFC = TIMER_IFC_CC2;

        // clear pending interrupt
        //NVIC_ClearPendingIRQ(TIMER2_IRQn);
    }

    static inline void IRQforcePendingTimeout()
    {
        NVIC_SetPendingIRQ(TIMER2_IRQn);
    }

    static inline void IRQsetTriggerMatchReg(unsigned int v, bool SFD_STXON)
    {
        int channelIndex = SFD_STXON ? 0 : 1;

        // clear previous TIMER setting
        if(SFD_STXON) // STXON
            TIMER2->IFC = TIMER_IFC_CC1;
        else // TIMESTAMP_IN/OUT
            TIMER1->IFC = TIMER_IFC_CC2;

        //NVIC_ClearPendingIRQ(TIMER2_IRQn);
        //NVIC_ClearPendingIRQ(TIMER1_IRQn);

        // extracting lower and upper 16-bit parts from match value
        uint16_t lower_ticks = static_cast<uint16_t> (v & 0xFFFFULL); // lower part
        uint16_t upper_ticks = static_cast<uint16_t> (v >> 16) & 0xFFFFUL; // upper part

        // set output compare timer register
        // because of an undocumented quirk, the compare register is checked 1 tick late. 
        // By subtracting one tick, we have to make sure we do not underflow!
        // note: upper part is not decremented because it's only used to be checked
        // inside ISR and does not play am active role in CCV (where the quirk is)
        Hsc::nextCCtriggerLower[channelIndex] = lower_ticks == 0 ? 0 : lower_ticks-1; // underflow handling
        Hsc::nextCCtriggerUpper[channelIndex] = upper_ticks;

        // set and enable output compare interrupt on right channel for least significant timer
        if(SFD_STXON) // STXON
        {
            TIMER2->CC[1].CCV = Hsc::nextCCtriggerLower[0];
            TIMER2->IEN |= TIMER_IEN_CC1;
            TIMER2->CC[1].CTRL |= TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;

            // TODO: (s) if too close, then abort, timeout
            // if upper part is matching already, enable CMOA right away (not in ISR)
            if(TIMER3->CNT == upper_ticks)
            {
                // connect TIMER2->CC1 to pin PA9 (stxon) on #0
                TIMER2->ROUTE |= TIMER_ROUTE_CC1PEN;
                TIMER2->CC[1].CTRL |= TIMER_CC_CTRL_CMOA_SET;
                TIMER2->ROUTE |= TIMER_ROUTE_LOCATION_LOC0;
            }
        }
        else // TIMESTAMP_IN/OUT
        {
            TIMER1->CC[2].CCV = Hsc::nextCCtriggerLower[1];
            TIMER1->IEN |= TIMER_IEN_CC2;
            TIMER1->CC[2].CTRL |= TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;

            // TODO: (s) if too close, then abort, timeout
            // if upper part is matching already, enable CMOA right away (not in ISR)
            if(TIMER3->CNT == upper_ticks)
            {
                // connect TIMER1->CC2 to pin PE12 (TIMESTAMP_OUT) on #1
                TIMER1->ROUTE |= TIMER_ROUTE_CC2PEN;
                TIMER1->CC[2].CTRL |= TIMER_CC_CTRL_CMOA_SET;
                TIMER1->ROUTE |= TIMER_ROUTE_LOCATION_LOC1;
            }
        }
    }

    // TODO: (s) fix
    static long long IRQgetEventTimestamp() { return 0; }


    ///
    // static variables getters
    ///
    static unsigned int IRQgetNextCCticksUpper() { return Hsc::nextCCticksUpper; }
    static unsigned int IRQgetNextCCticksLower() { return Hsc::nextCCticksLower; }

    static unsigned int IRQgetNextCCtimeoutUpper() { return Hsc::nextCCtimeoutUpper; }
    static unsigned int IRQgetNextCCtimeoutLower() { return Hsc::nextCCtimeoutLower; }

    static std::pair<unsigned int, unsigned int> IRQgetNextCCtriggerUpper() 
    { 
        return std::make_pair(Hsc::nextCCtriggerUpper[0], Hsc::nextCCtriggerUpper[1]); 
    }
    static std::pair<unsigned int, unsigned int>  IRQgetNextCCtriggerLower() 
    { 
        return std::make_pair(Hsc::nextCCtriggerLower[0], Hsc::nextCCtriggerLower[1]);
    }

private:
    /**
     * Constructor
     */
    Hsc() {};
    Hsc(const Hsc&)=delete;
    Hsc& operator=(const Hsc&)=delete;
    
    static inline unsigned int IRQread32Timer(){
        unsigned int high = TIMER3->CNT;
        unsigned int low = TIMER2->CNT;
        unsigned int high2 = TIMER3->CNT;
        
        if(high == high2) // No lower part overflow
        {
            return (high<<16) | low; // constructing 32 bit number
        }

        return high2<<16 | TIMER2->CNT;
    }

    // irq
    static unsigned int nextCCticksUpper;
    static unsigned int nextCCticksLower;
    // timeout
    static unsigned int nextCCtimeoutUpper;
    static unsigned int nextCCtimeoutLower;
    // trigger ([0] is STXON, [1] is TIMESTAMP_OUT)
    static unsigned int nextCCtriggerUpper[2];
    static unsigned int nextCCtriggerLower[2];

    static const unsigned int frequency = EFM32_HFXO_FREQ; //48000000 Hz if NOT prescaled!
};

}//end miosix namespace
