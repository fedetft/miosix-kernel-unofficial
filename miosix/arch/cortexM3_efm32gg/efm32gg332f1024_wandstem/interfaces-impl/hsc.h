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

//#include "util/queue_gdb.h" // DELETEME: (s)

namespace miosix {
//extern QueueGDB * queue_gdb; // DELETEME: (s)
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

//> TIMERS CONFIGURATION <//
/**
 * Timers configuration: 
 * TIMER0 [Unclocked][free]
 * TIMER1 [Clocked 48MHz]
 * TIMER2 [Clocked 48MHz]
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

    /**
     * @brief returns hardware timer [ticks]. Because of the non-atomically readable
     * underlying chained 16-bit timers, it is necessary to read the upper part twice.
     * 
     * @return unsigned int hardware timer [ticks]
     */
    static inline unsigned int IRQgetTimerCounter()
    {
        return IRQread32Timer();
    }

    /**
     * @brief Sets the value for the two 16-bit hardware timers responsible
     * for keeping time (TIMER2 and TIMER3)
     * 
     * @param v new 32-bit value for the hardware timer
     */
    static inline void IRQsetTimerCounter(unsigned int v)
    {
        TIMER2->CNT = static_cast<uint16_t> (v & 0xFFFFUL); // lower part
        TIMER3->CNT = static_cast<uint16_t> ((v >> 16) & 0xFFFFUL); // upper part
    }

    /**
     * @brief returns the cached 32-bit value inside the capture compare register inside the
     * chained time keeping 16-bit timers (TIMER3 and TIMER2)
     * 
     * @return unsigned int capture compare value
     */
    static inline unsigned int IRQgetTimerMatchReg()
    {
        return matchValue;
    }

    /**
     * @brief Sets capture compare value inside the chained time keeping 16-bit timers 
     * (TIMER3 and TIMER2) to trigger an interrupt at a given value.
     * 
     * @param v time [ticks] to trigger the IRQ
     */
    static inline void IRQsetTimerMatchReg(unsigned int v)
    {
        // Before setting a new match register, the two timers needs to be set back to a known state. 
        // CORNER CASE 1: Timer2 needs to be disabled before setting the TIMER3 interrupt because it may be
        // tirggering the interrupt from a previous state 
        TIMER2->IEN &= ~TIMER_IEN_CC0;
        TIMER2->CC[0].CTRL &= ~TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;
        TIMER2->IFC = TIMER_IFC_CC0;

        matchValue = v;
        unsigned short upper16 = v >> 16;
        TIMER3->CC[0].CCV = upper16 - 1;
        TIMER3->IEN |= TIMER_IEN_CC0;
        TIMER3->CC[0].CTRL |= TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;

        // CORNER CASE 2: it may happen that another previous interrupt is triggered while we are 
        // setting the new TIMER3 interrupt. This will lead to a premature call of TIMER3 and therefore
        // TIMER2. Note that, if TIMER3 already matches the upper part, clearing the interrupt flac on the
        // CC this is not a problem because the underneath if handle it anyway.
        TIMER3->IFC = TIMER_IFC_CC0;

        if(TIMER3->CNT == upper16)
        {
            unsigned short lower16 = v & 0xffff;
            TIMER2->CC[0].CCV = lower16 - 1;
            TIMER2->IEN |= TIMER_IEN_CC0;
            TIMER2->CC[0].CTRL |= TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;
        }
    }

    /**
     * @brief checks if there's a pending overflow event flag
     * 
     * @return true if an overflow event is pending
     */
    static inline bool IRQgetOverflowFlag()
    {
        return TIMER3->IF & TIMER_IF_OF;
    }

    /**
     * @brief clears the overflow event flag
     * 
     */
    static inline void IRQclearOverflowFlag()
    {
        TIMER3->IFC = TIMER_IFC_OF;
    }

    /**
     * @brief checks if there's a pending capture compare regsister event flag.
     * 
     * @return true if the lower 16-bit capture compare register matched the timer value
     */
    static inline bool IRQgetMatchFlag()
    {
        return TIMER2->IF & TIMER_IF_CC0;
    }

    /**
     * @brief clears timer match register event flag
     * 
     */
    static inline void IRQclearMatchFlag()
    {
        TIMER2->IFC = TIMER_IFC_CC0;
    }

    /**
     * @brief Forces a pending IRQ in the NVIC
     * 
     */
    static inline void IRQforcePendingIrq()
    {
        NVIC_SetPendingIRQ(TIMER2_IRQn);
    }

    /**
     * @brief Stops chained timers
     * 
     */
    static inline void IRQstopTimer()
    {
        TIMER1->CMD=TIMER_CMD_STOP;
        TIMER2->CMD=TIMER_CMD_STOP;
        TIMER3->CMD=TIMER_CMD_STOP;
    }

    /**
     * @brief Starts two time-keeping 16-bit chained timers (TIMER2, TIMER3) along
     * with the TIMER1 used for the VHT timstamping
     * 
     */
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

    /**
     * @brief Returns the frequency powering the timers
     * 
     * @return unsigned int frequency of the hSC
     */
    static unsigned int IRQTimerFrequency()
    {
        return frequency;
    }

    /**
     * @brief Initialize underlying timers for time-keeping and VHT purposes.
     * TIMER2 and TIMER3 16-bit timers are chained togheter to form a 32-bit timer
     * while TIMER1 is reserved for the VHT.
     * 
     */
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

    /**
     * @brief Initializes related timer for the VHT
     * 
     */
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

    /**
     * @brief Starts the VHT relted timer if not running yet
     * 
     */
    static void IRQstartVhtTimer()
    {
        // start timer
        //TIMER1->CMD = TIMER_CMD_START; //  TIMER1 is already clocked
    }

    /**
     * @brief returns the captured timer value for the VHT correction.
     * We expect the lower 16-bit part to change more frequently than the upper 16-bit part.
     * For this reason, only the lower part is caputured while the upper part is just read
     * as current value and juxstapposed.
     * 
     * @return unsigned int 
     */
    static inline unsigned int IRQgetVhtTimerCounter()
    {
        return (TIMER3->CNT<<16) | TIMER1->CC[1].CCV;
    }

    /**
     * @brief checks if a VHT event flag is pending
     * 
     * @return true if a VHT event flag is pending
     */
    static inline bool IRQgetVhtMatchFlag()
    {
        return TIMER1->IF & TIMER_IF_CC1;
    }

    /**
     * @brief clears the VHT event flag
     * 
     */
    static inline void IRQclearVhtMatchFlag()
    {
        TIMER1->IFC = TIMER_IFC_CC1;
    }

    ///
    // Timeout extension 
    ///

    /**
     * @brief returns the cached timeout value [ticks]
     * 
     * @return unsigned int timeout value [ticks]
     */
    static inline unsigned int IRQgetTimeoutMatchReg()
    {
        return timeoutValue;
    }

    /**
     * @brief sets time for the timeout [ticks]
     * 
     * @param v timeout [ticks]
     */
    static inline void IRQsetTimeoutMatchReg(unsigned int v)
    {  
        // clear previous TIMER2 setting
        TIMER2->IFC = TIMER_IFC_CC2;

        timeoutValue = v;
        
        // set output compare timer register
        unsigned short lower16 = v & 0x0000ffff;

        // set and enable output compare interrupt on channel 1 for most significant timer
        TIMER2->CC[2].CCV = lower16 - 1;
        TIMER2->IEN |= TIMER_IEN_CC2;
        TIMER2->CC[2].CTRL |= TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;
    }

    /**
     * @brief checks if a timeout event is pending
     * 
     * @return true if a timeout event occurred
     */
    static inline bool IRQgetTimeoutFlag()
    {
        //return TIMER1->IF & TIMER_IF_CC1;
        return TIMER2->IF & TIMER_IF_CC2;
    }

    /**
     * @brief clears the timeout flag event
     * 
     */
    static inline void IRQclearTimeoutFlag()
    {
        // disable output compare interrupt
        TIMER2->IEN &= ~TIMER_IEN_CC2; // signal capture and compare register for OS interrupts
        
        // clear output compare flag
        TIMER2->IFC = TIMER_IFC_CC2;
    }

    /**
     * @brief forces pending event in the NVIC
     * 
     */
    static inline void IRQforcePendingTimeout()
    {
        NVIC_SetPendingIRQ(TIMER2_IRQn);
    }
    
    /**
     * @brief sets timer match register to trigger an event on a given channel at the given time.
     * (index = [0] is TIMESTAMP_OUT, index = [1] is STXON)
     * 
     * @param v 64-bit time to trigger the event [ticks]
     * @param channelIndex index of the event channel
     * @return false if too late to trigger the event being the trigger time too close
     * to the current time
     */
    inline bool IRQsetTriggerMatchReg(long long v, bool channelIndex)
    {
        // extracting lower and upper 16-bit parts from match value
        triggerValue[channelIndex] = v;

        // set and enable output compare interrupt on right channel for least significant timer
        unsigned short lower16 = v & 0xffff;
        long long upper48 = v & 0xffffffffffff0000;

        if(channelIndex) // STXON
        {
            TIMER2->CC[1].CCV = lower16 - 1;
            TIMER2->IEN |= TIMER_IEN_CC1;
            TIMER2->CC[1].CTRL |= TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;

            long long upperCounter = IRQgetUpper48();
            unsigned short lowerCounter = TIMER2->CNT;

            /*queue_gdb->post([=]{ iprintf("UC=%lld,\tLC=%u\n", upperCounter>>16, lowerCounter); }); // DELETEME: (s)
            queue_gdb->post([=]{ iprintf("M48=%lld,\tM16=%u\n", upper48>>16, lower16); }); // DELETEME: (s)
            queue_gdb->post([=]{ iprintf("U48=%lld,\tL16=%lu\n", IRQgetUpper48()>>16, TIMER2->CNT); }); // DELETEME: (s)
            queue_gdb->post([=]{ iprintf("U48=%lld,\tL16=%lu\n", IRQgetUpper48()>>16, TIMER2->CNT); }); // DELETEME: (s)
            queue_gdb->post([=]{ iprintf("U48=%lld,\tL16=%lu\n", IRQgetUpper48()>>16, TIMER2->CNT); }); // DELETEME: (s)
            queue_gdb->post([=]{ iprintf("U48=%lld,\tL16=%lu\n", IRQgetUpper48()>>16, TIMER2->CNT); }); // DELETEME: (s)*/

            // TODO: (s) document
            // 1. document...
            /*if(upperCounter > upper48)
            {
                // disable timer
                TIMER2->IEN &= ~TIMER_IEN_CC1;
                TIMER2->CC[1].CTRL &= ~TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;
                TIMER2->IFC = TIMER_IFC_CC1;
                return false;
            }
            // 2. document...
            else if(upperCounter == upper48)
            {
                if(static_cast<unsigned short>(lower16 - lowerCounter) <= 200) 
                {
                    // disable timer
                    TIMER2->IEN &= ~TIMER_IEN_CC1;
                    TIMER2->CC[1].CTRL &= ~TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;
                    TIMER2->IFC = TIMER_IFC_CC1;

                    return false;
                }
                // connect TIMER1->CC2 to pin PA9 (STX_ON) on #0
                TIMER2->ROUTE |= TIMER_ROUTE_CC1PEN;
                TIMER2->CC[1].CTRL |= TIMER_CC_CTRL_CMOA_SET;
                TIMER2->ROUTE |= TIMER_ROUTE_LOCATION_LOC0;
            }
            // 3. document...           
            // too late to send, TIMER3->CNT > upper16
            else if(upperCounter == (upper48-0x10000) && lowerCounter >= lower16)
            {
                // check if too late
                // difference between next interrupt is less then 200 ticks
                if(static_cast<unsigned short>(lower16 - lowerCounter) >= 0xffff - 200) 
                {
                    // disable timer
                    TIMER2->IEN &= ~TIMER_IEN_CC1;
                    TIMER2->CC[1].CTRL &= ~TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;
                    TIMER2->IFC = TIMER_IFC_CC1;
                    return false; 
                }
                // connect TIMER2->CC1 to pin PA9 (STX_ON) on #0
                TIMER2->ROUTE |= TIMER_ROUTE_CC1PEN;
                TIMER2->CC[1].CTRL |= TIMER_CC_CTRL_CMOA_SET;
                TIMER2->ROUTE |= TIMER_ROUTE_LOCATION_LOC0;
            }*/

            if(IRQgetUpper48() == (upper48-0x10000) && TIMER2->CNT >= lower16)
            {
                // check if too late
                // difference between next interrupt is less then 200 ticks
                if(static_cast<unsigned short>(lower16 - TIMER2->CNT) >= 0xffff - 200) 
                {
                    // disable timer
                    TIMER2->IEN &= ~TIMER_IEN_CC1;
                    TIMER2->CC[1].CTRL &= ~TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;
                    TIMER2->IFC = TIMER_IFC_CC1;
                    return false; 
                }
                // connect TIMER2->CC1 to pin PA9 (STX_ON) on #0
                TIMER2->ROUTE |= TIMER_ROUTE_CC1PEN;
                TIMER2->CC[1].CTRL |= TIMER_CC_CTRL_CMOA_SET;
                TIMER2->ROUTE |= TIMER_ROUTE_LOCATION_LOC0;
            }
            if(IRQgetUpper48() == upper48)
            {
                if(static_cast<unsigned short>(lower16 - TIMER2->CNT) <= 200) 
                {
                    // disable timer
                    TIMER2->IEN &= ~TIMER_IEN_CC1;
                    TIMER2->CC[1].CTRL &= ~TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;
                    TIMER2->IFC = TIMER_IFC_CC1;

                    // disconnect TIMER2->CC1 from PEN completely
                    TIMER2->CC[1].CTRL &= ~TIMER_CC_CTRL_CMOA_CLEAR;
                    TIMER2->ROUTE &= ~TIMER_ROUTE_LOCATION_LOC0;
                    TIMER2->ROUTE &= ~TIMER_ROUTE_CC1PEN;
                    return false;
                }
                // connect TIMER1->CC2 to pin PA9 (STX_ON) on #0
                TIMER2->ROUTE |= TIMER_ROUTE_CC1PEN;
                TIMER2->CC[1].CTRL |= TIMER_CC_CTRL_CMOA_SET;
                TIMER2->ROUTE |= TIMER_ROUTE_LOCATION_LOC0;
            }
            if(IRQgetUpper48() > upper48)
            {
                // disable timer
                TIMER2->IEN &= ~TIMER_IEN_CC1;
                TIMER2->CC[1].CTRL &= ~TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;
                TIMER2->IFC = TIMER_IFC_CC1;

                // disconnect TIMER2->CC1 from PEN completely
                TIMER2->CC[1].CTRL &= ~TIMER_CC_CTRL_CMOA_CLEAR;
                TIMER2->ROUTE &= ~TIMER_ROUTE_LOCATION_LOC0;
                TIMER2->ROUTE &= ~TIMER_ROUTE_CC1PEN;

                return false;
            }

            return true;
        }
        else // TIMESTAMP_OUT
        {
            TIMER1->CC[2].CCV = lower16 - 1;
            TIMER1->IEN |= TIMER_IEN_CC2;
            TIMER1->CC[2].CTRL |= TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;

            long long upperCounter = IRQgetUpper48();
            unsigned short lowerCounter = TIMER2->CNT;

            // TODO: (s) document
            // 1. document...
            if(upperCounter > upper48)
            {
                // disable timer
                TIMER1->IEN &= ~TIMER_IEN_CC2;
                TIMER1->CC[2].CTRL &= ~TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;
                TIMER1->IFC = TIMER_IFC_CC2;
                return false;
            }
            // 3. document...
            else if(upperCounter == upper48)
            {
                if(static_cast<unsigned short>(lower16 - lowerCounter) <= 200) 
                {
                    // disable timer
                    TIMER1->IEN &= ~TIMER_IEN_CC2;
                    TIMER1->CC[2].CTRL &= ~TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;
                    TIMER1->IFC = TIMER_IFC_CC2;

                    return false;
                }
                // connect TIMER1->CC2 to pin PA9 (STX_ON) on #0
                TIMER1->ROUTE |= TIMER_ROUTE_CC2PEN;
                TIMER1->CC[2].CTRL |= TIMER_CC_CTRL_CMOA_SET;
                TIMER1->ROUTE |= TIMER_ROUTE_LOCATION_LOC1;
            }
            // 2. document...           
            // too late to send, TIMER3->CNT > upper16
            else if(upperCounter == (upper48-0x10000) && lowerCounter >= lower16)
            {
                // check if too late
                // difference between next interrupt is less then 200 ticks
                if(static_cast<unsigned short>(lower16 - lowerCounter) >= 0xffff - 200) 
                {
                    // disable timer
                    TIMER1->IEN &= ~TIMER_IEN_CC2;
                    TIMER1->CC[2].CTRL &= ~TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;
                    TIMER1->IFC = TIMER_IFC_CC2;

                    return false; 
                }
                // connect TIMER1->CC2 to pin PE12 (TIMESTAMP_OUT) on #1
                TIMER1->ROUTE |= TIMER_ROUTE_CC2PEN;
                TIMER1->CC[2].CTRL |= TIMER_CC_CTRL_CMOA_SET;
                TIMER1->ROUTE |= TIMER_ROUTE_LOCATION_LOC1;
            }
            return true;
        }
    }

    /**
     * @brief implements a reduced version of the pendign bit trick to read the upper 48-bit part
     * of the 64-bit software extended timer
     * 
     * @return long long 
     */
    inline long long IRQgetUpper48()
    {
        unsigned int upper16 = TIMER3->CNT;
        long long upper48 = upperTimeTick | (upper16<<16);
        if(IRQgetOverflowFlag() && TIMER3->CNT >= upper16)
            upper48 += upperIncr;

        return upper48;
    }

    /**
     * @brief returns the trigger time [ticks] for a given event channel 
     * ([0] is STXON, [1] is TIMESTAMP_OUT)
     * 
     * @param index event channel index
     * @return long long time in which the trigger event will be fired
     */
    static inline long long IRQgetTriggerMatchReg(bool index) 
    { 
        return triggerValue[index]; 
    }

    /**
     * @brief returns captured timestamp value. Once read, the value is overwritten by the
     * backup register and therefore becomes meaningless.
     * (index = [0] is TIMESTAMP_OUT, index = [1] is STXON)
     * 
     * @param channelIndex event channel index to read the timestamp from
     * @return long long timestamp of the selected event channel
     */
    static long long IRQgetEventTimestamp(bool channelIndex = 1) 
    { 
        if(channelIndex) // SFD
            return (TIMER3->CC[1].CCV<<16) | TIMER2->CC[1].CCV;
        else // TIMESTAMP_IN
            return (TIMER3->CC[2].CCV<<16) | TIMER1->CC[2].CCV;

    }

    // FIXME: (s) its implementation should be here and not in os_timer.cpp
    inline void myIRQhandler();
    //{
        /*if(IRQgetMatchFlag() || lateIrq)
        {
            IRQclearMatchFlag();
            long long tick=IRQgetTimeTick();
            if(tick >= IRQgetIrqTick() || lateIrq)
            {
                lateIrq=false;
                IRQtimerInterrupt(vc->IRQcorrectTimeNs(tc.tick2ns(tick)));
            }
        }
        
        if(IRQgetOverflowFlag())
        {
            IRQclearOverflowFlag();
            upperTimeTick += upperIncr;
        }*/
    //}

private:
    /**
     * Constructor
     */
    Hsc() {}
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
    static unsigned int matchValue;
    // timeout
    static unsigned int timeoutValue;
    // trigger ([0] is TIMESTAMP_OUT, [1] is STXON)
    static long long triggerValue[2];

    static const unsigned int frequency = EFM32_HFXO_FREQ; //48000000 Hz if NOT prescaled!
};

}//end miosix namespace
