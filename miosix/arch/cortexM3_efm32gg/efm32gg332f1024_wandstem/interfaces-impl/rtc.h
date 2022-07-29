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

#ifndef RTC_H
#define RTC_H

#include "interfaces/os_timer.h"
#include "hwmapping.h"

namespace miosix
{

/**
 * Manages the hardware timer that runs also in low power mode.
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

class Rtc : public RTCTimerAdapter<Rtc, 24>
{
public:
    /**
     * \return a reference to the timer (singleton)
     */
    static Rtc& instance()
    {
        static Rtc timer;
        return timer;
    }

    /**
     * @brief RTC hardware timer counter
     * 
     * @return unsigned int time in ticks
     */
    static inline unsigned int IRQgetTimerCounter()
    {
        return RTC->CNT;
    }
    /**
     *  @brief Sets RTC hardware timer counter
     * 
     * @param v new timer value
     */
    static inline void IRQsetTimerCounter(unsigned int v)
    {
        RTC->CNT = v;
    }

    /**
     * @brief Gets register containing value to be used in capture and compare mode for the timer (same as CCR1 in stm32)
     * 
     * @return unsigned int register containing value to be compared
     */
    static inline unsigned int IRQgetTimerMatchReg()
    {
        return RTC->COMP0;
    }
    /**
     * @brief Sets register to be used in capture and compare mode for the timer
     * 
     * @param v time value to be comapred the timer with
     */
    static inline void IRQsetTimerMatchReg(unsigned int v)
    {
        RTC->COMP0 = v;
        while(RTC->SYNCBUSY & RTC_SYNCBUSY_COMP0) ;
        RTC->IEN |= RTC_IEN_COMP0; // TODO: (s) move into init?
    }

    /**
     * @brief Returns overflow flag of the RTC
     * 
     * @return true if we have an overflow
     * @return false if there is no overflow
     */
    static inline bool IRQgetOverflowFlag()
    {
        return RTC->IF & RTC_IFC_OF;
    }
    /**
     * @brief Clears the overflow flag
     * 
     */
    static inline void IRQclearOverflowFlag()
    {
        // missing NVIC clear pending?
        RTC->IFC |= RTC_IFC_OF;
    }

    /**
     * @brief Gets compare result flag of the timer with the campute and compare register
     * 
     * @return true if timer value matches compare register
     * @return false if timer value does not matche compare register
     */
    static inline bool IRQgetMatchFlag()
    {
        return RTC->IF & RTC_IF_COMP0;
    }
    /**
     * @brief Clears match flag
     * 
     */
    static inline void IRQclearMatchFlag()
    {
        RTC->IFC |= RTC_IFC_COMP0;
    }

    /**
     * @brief Forces pending interrupt request for the RTC timer
     * 
     */
    static inline void IRQforcePendingIrq()
    {
        NVIC_SetPendingIRQ(RTC_IRQn);
    }

    /**
     * @brief Stops RTC timer
     * 
     */
    static inline void IRQstopTimer()
    {
        RTC->CTRL = 0;
        while(RTC->SYNCBUSY & RTC_SYNCBUSY_CTRL) ;
    }
    /**
     * @brief Starts RTC timer
     * 
     */
    static inline void IRQstartTimer()
    {
        RTC->CTRL = RTC_CTRL_EN;
        while(RTC->SYNCBUSY & RTC_SYNCBUSY_CTRL) ;
    }

    /**
     * @brief Gets RTC timer frequency
     * 
     * @return unsigned int 
     */
    static unsigned int IRQTimerFrequency()
    {
        return frequency;
    }

    /**
     * @brief Inits RTC timer
     * 
     */
    static void IRQinitTimer()
    {
        //
        // Configure timer
        //
        
        //The LFXO is already started by the BSP
        CMU->HFCORECLKEN0 |= CMU_HFCORECLKEN0_LE; //Enable clock to LE peripherals
        CMU->LFACLKEN0 |= CMU_LFACLKEN0_RTC;
        while(CMU->SYNCBUSY & CMU_SYNCBUSY_LFACLKEN0) ;
        
        RTC->CNT=0;
        RTC->IEN |= RTC_IEN_OF;

        //In the EFM32GG332F1024 the RTC has two compare channels, used in this way:
        //COMP0 -> used for wait and trigger
        //COMP1 -> reserved for VHT resync and Power manager
        //NOTE: interrupt not yet enabled as we're not setting RTC->IEN
        NVIC_SetPriority(RTC_IRQn, 7); // 0 is the higest priority, 15 il the lowest
        NVIC_ClearPendingIRQ(RTC_IRQn);
        NVIC_EnableIRQ(RTC_IRQn);  
        
    }



private:
    /**
     * Constructor
     */
    Rtc() {};
    Rtc(const Rtc&)=delete;
    Rtc& operator=(const Rtc&)=delete;

    /// The internal RTC frequency in Hz
    static const unsigned int frequency = EFM32_LFXO_FREQ; // 32768 Hz
};

} /* end of namespace miosix */

#endif /* REAL_TIME_CLOCK */