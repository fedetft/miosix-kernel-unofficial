/***************************************************************************
 *   Copyright (C) 2022 by Alessandro Sorrentino                           *
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

#include "time/clock_sync.h"
#include "interfaces-impl/time_types_spec.h"
#include "e20/e20.h" // DELETEME: (s)
#include "thread" // DELETEME: (s)

namespace miosix
{
    /*static FixedEventQueue<100,12> queue; // DELETEME: (s)

    // DELETEME: (s)
    void startDBGthread()
    {
        std::thread t([]() { queue.run(); });
        t.detach();
    }*/

    Vht& Vht::instance()
    {
        static Vht vht;
        return vht;
    }

    long long Vht::IRQcorrect(long long ns)
    {
        if(!init) { return ns; }

        //long long tick = tc.ns2tick(ns) /*+ vhtClockOffset*/;
        //return tc.tick2ns(syncPointTheoreticalHsc + fastNegMul(tick - syncPointExpectedHsc, factorI, factorD));
        return a_km1 * ns + b_km1;
    }

    long long Vht::IRQuncorrect(long long ns)
    {
        if(!init) { return ns; }

        //long long tick = tc.ns2tick(ns);
        //return tc.tick2ns(syncPointExpectedHsc + fastNegMul(tick - syncPointTheoreticalHsc, inverseFactorI, inverseFactorD) /*- vhtClockOffset*/);
        return (ns - b_km1) / a_km1;
    }

    void Vht::IRQinit()
    {
        //startDBGthread(); // DELETEME: (s)

        this->flopsyncVHT = &FlopsyncVHT::instance();

        // setting up VHT timer
        Hsc::IRQinitVhtTimer();
        Rtc::IRQinitVhtTimer();

        // performing VHT initialization
        unsigned int nowRtc = Rtc::IRQgetTimerCounter() + 2; // TODO: (s) this is for undocumented quirk, generalize! + CHECK OVERFLOW
        Rtc::IRQsetVhtMatchReg(nowRtc);

        Rtc::IRQclearVhtMatchFlag();
        // WARNING: this while may break if connected to GDB while doing step-by-step execution from above
        while(!Rtc::IRQgetVhtMatchFlag()); // wait for first compare

        // Reading vht timestamp but replacing lower part of counter with RTC value
        unsigned int vhtTimestamp = Hsc::IRQgetVhtTimerCounter();
        if(vhtTimestamp > Hsc::IRQgetTimerCounter()){
            vhtTimestamp -= 1<<16; // equivalent to ((TIMER2->CNT-1)<<16) | TIMER3->CC[0].CCV;
        }

        Rtc::IRQclearVhtMatchFlag();
        Hsc::IRQclearVhtMatchFlag();

        #if EFM32_HFXO_FREQ != 48000000 || EFM32_LFXO_FREQ != 32768
        #error "Clock frequency assumption not satisfied"
        #endif

        // first VHT correction
        //conversion factor between RTC and HRT is 48e6/32768=1464+3623878656/2^32
        long long nowHsc = mul64x32d32(nowRtc, 1464, 3623878656);
        this->vhtClockOffset = nowHsc - vhtTimestamp;
        this->syncPointExpectedHsc = nowHsc;

        // resync done, set next resync
        this->nextSyncPointRtc = nowRtc + syncPeriodRtc;
        Rtc::IRQsetVhtMatchReg(nextSyncPointRtc);
        this->syncPointTheoreticalHsc = nowHsc;

        //first VHT correction
        IRQupdateImpl(syncPointTheoreticalHsc, syncPointExpectedHsc, 0);

        this->init = true;
    }

    void Vht::IRQupdate(long long syncPointActualHsc)
    {
        this->syncPointActualHsc = syncPointActualHsc + vhtClockOffset;

        // set next RTC trigger
        this->nextSyncPointRtc += this->syncPeriodRtc; // increments next RTC sync point as currentSyncPoint + syncPeriod
        this->syncPointTheoreticalHsc += this->syncPeriodHsc; // increments next HSC sync point as theoreticalSyncPoint + syncPeriod
        this->syncPointExpectedHsc += this->syncPeriodHsc + this->clockCorrectionFlopsync;

        Rtc::IRQclearVhtMatchFlag(); // clear RTC output channel
        Rtc::IRQsetVhtMatchReg(Rtc::IRQgetVhtTimerMatchReg() + syncPeriodRtc);

        // calculate required parameters
        
        this->error = this->syncPointActualHsc - syncPointExpectedHsc;
        this->clockCorrectionFlopsync = flopsyncVHT->computeCorrection(error);        

        // TODO: (s) extend error.h types and print everything inside error.cpp + recover
        // handle case in which error exceeds maximum theoretical error
        if(error < -maxTheoreticalError || error > maxTheoreticalError) 
            IRQhandleErrorException();

        IRQupdateImpl(syncPointTheoreticalHsc, syncPointExpectedHsc, clockCorrectionFlopsync);
    }

    void Vht::IRQupdateImpl(long long syncPointTheoreticalHsc, long long syncPointExpectedHsc, long long clockCorrectionFlopsync)
    {
        //efficient way to calculate the factor T/(T+u(k))
        long long temp = (syncPeriodHsc<<32) / (syncPeriodHsc + clockCorrectionFlopsync);
        //calculate inverse of previous factor (T+u(k))/T
        long long inverseTemp = ((syncPeriodHsc + clockCorrectionFlopsync)<<32) / syncPeriodHsc;
        
        factorI = static_cast<unsigned int>((temp & 0xFFFFFFFF00000000LLU)>>32);
        factorD = static_cast<unsigned int>(temp);
        
        inverseFactorI = static_cast<unsigned int>((inverseTemp & 0xFFFFFFFF00000000LLU)>>32);
        inverseFactorD = static_cast<unsigned int>(inverseTemp);

        // compute aX + b coefficient pairs
        fp32_32 a; a.value = (static_cast<int64_t>(factorI)<<32) | factorD;
        long long b = tc.tick2ns(syncPointTheoreticalHsc - (a * syncPointExpectedHsc));

        // update internal and vc coeff. at position N only if necessary
        if(a_km1 != a || b_km1 != b)
        {
            this->a_km1 = a;
            this->b_km1 = b;

            vc->IRQupdateCorrectionPair(std::make_pair(a, b), posCorrection);
        }
    }

    void Vht::IRQresyncClock()
    {
        long long nowRtc = Rtc::IRQgetTimerCounter();
        long long syncAtRtc = nowRtc + 2; // TODO: (s) generalize for random quirk
        //This is very important, we need to restore the previous value in COMP1, to gaurentee the proper wakeup
        long long prevCOMP1 = Rtc::IRQgetVhtTimerMatchReg();
        Rtc::IRQsetVhtMatchReg(syncAtRtc);

        //Virtual high resolution timer, init without starting the input mode!
        Hsc::IRQinitVhtTimer();
        Hsc::IRQstartVhtTimer();
        
        Rtc::IRQclearVhtMatchFlag();
        //Clean the buffer to avoid false reads
        Hsc::IRQgetVhtTimerCounter();
        Hsc::IRQgetVhtTimerCounter();
        while(!Rtc::IRQgetVhtMatchFlag()); // wait for first compare

        long long timestamp = Hsc::IRQgetVhtTimerCounter();
        //Got the values, now polishment of flags and register
        Rtc::IRQclearVhtMatchFlag();
        Hsc::IRQclearVhtMatchFlag();
        
        Rtc::IRQsetVhtMatchReg(prevCOMP1+1); // TODO: (s) generalize quirk
        
        long long syncAtHrt = mul64x32d32(syncAtRtc, 1464, 3623878656);
        vhtClockOffset = syncAtHrt - timestamp;
        syncPointExpectedHsc = syncAtHrt;
        nextSyncPointRtc = syncAtRtc + syncPeriodRtc;
        syncPointTheoreticalHsc = syncAtHrt;
        syncPointActualHsc = syncAtHrt;

        // clear flags not to resync immediately
        Hsc::IRQclearVhtMatchFlag();
        Hsc::IRQgetVhtTimerCounter();
    }

}