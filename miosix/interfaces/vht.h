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

#ifndef VHT_H
#define VHT_H

#include <thread>
#include "time/correction_tile.h"
#include "time/timeconversion.h"
#include "time/flopsync_vht.h"
#include "kernel/logging.h"

namespace miosix
{

template<typename Hsc_TA, typename Rtc_TA>
class Vht : public CorrectionTile
{
public:
    static Vht& instance(){
        static Vht vht;
        return vht;
    }

    /**
     * @brief 
     * 
     * @param ns 
     * @return long long 
     */
    long long IRQcorrect(long long ns)
    {
        if(!init) { return ns; }

        long long tick = tc.ns2tick(ns) /*+ vhtClockOffset*/;
        return tc.tick2ns(syncPointTheoreticalHsc + fastNegMul(tick - syncPointExpectedHsc, factorI, factorD));
    }

    /**
     * @brief 
     * 
     * @param ns 
     * @return long long 
     */
    long long IRQuncorrect(long long ns)
    {
        if(!init) { return ns; }

        long long tick = tc.ns2tick(ns);
        return tc.tick2ns(syncPointExpectedHsc + fastNegMul(tick - syncPointTheoreticalHsc, inverseFactorI, inverseFactorD) /*- vhtClockOffset*/);
    }

    /**
     * @brief sync between RTC and HSC every 20ms 
     * Since we execute instructions at 48Mhz, we execute multiple instructions before each RTC tick
     * 
     */
    void IRQinit()
    {
        // TODO: (s) develop
        this->flopsyncVHT = &(FlopsyncVHT::instance());

        // setting up VHT timer
        Hsc_TA::IRQinitVhtTimer();
        Hsc_TA::IRQstartVhtTimer();

        Rtc_TA::IRQinitVhtTimer();
        Rtc_TA::IRQstartVhtTimer(); // TODO: (s) read inside rtc.h

        // performing VHT initialization
        unsigned int nowRtc = Rtc_TA::IRQgetTimerCounter() + 2; // TODO: (s) this is for undocumented quirk, generalize! + CHECK OVERFLOW
        Rtc_TA::IRQsetVhtMatchReg(nowRtc);

        Rtc_TA::IRQclearVhtMatchFlag();
        while(!Rtc_TA::IRQgetVhtMatchFlag()); // wait for first compare

        // Reading vht timestamp but replacing lower part of counter with RTC value
        long long vhtTimestamp = Hsc_TA::IRQgetVhtTimerCounter();

        if(vhtTimestamp > Hsc_TA::IRQgetTimerCounter()){
            vhtTimestamp -= 1<<16; // equivalent to ((TIMER2->CNT-1)<<16) | TIMER3->CC[0].CCV;
        }

        Rtc_TA::IRQclearVhtMatchFlag();
        Hsc_TA::IRQclearVhtMatchFlag();

        #if EFM32_HFXO_FREQ != 48000000 || EFM32_LFXO_FREQ != 32768 // TODO: (s) parametrize
        #error "Clock frequency assumption not satisfied"
        #endif

        // first VHT correction
        //conversion factor between RTC and HRT is 48e6/32768=1464+3623878656/2^32
        long long nowHsc = mul64x32d32(nowRtc, 1464, 3623878656);
        this->vhtClockOffset = nowHsc - vhtTimestamp;
        this->syncPointExpectedHsc = nowHsc;

        // resync done, set next resync
        this->nextSyncPointRtc = nowRtc + syncPeriodRtc;
        Rtc_TA::IRQsetVhtMatchReg(nextSyncPointRtc);
        this->syncPointTheoreticalHsc = nowHsc;

        // first VHT correction
        IRQupdateImpl(syncPointTheoreticalHsc, syncPointExpectedHsc, 0);

        // start periodic VHT thead ()
        //std::thread vhtThread = std::thread([this]{ this->vhtLoop(); }); //std::thread(&miosix::Vht<Hsc_TA, Rtc_TA>::vhtLoop, this);
        //Thread* vhtThread = Thread::create(&vhtLoop, 2048, 1, this);  // old definition with stack size
        //static_cast<void>(vhtThread); // avoid unused variable warning

        this->init = true;
    }

    /**
     * @brief 
     * 
     * @param syncPointTheoreticalHsc
     * @param syncPointComputed 
     */
    /*void IRQoffsetUpdate(long long syncPointTheoreticalHsc, long long syncPointExpectedHsc){
        this->syncPointTheoreticalHsc = syncPointTheoreticalHsc;
        this->syncPointExpectedHsc = syncPointExpectedHsc;
    }*/

    /**
     * @brief 
     * 
     * @param syncPointActualHsc 
     */
    void IRQupdate(long long syncPointActualHsc){
        this->syncPointActualHsc = syncPointActualHsc + vhtClockOffset;

        // set next RTC trigger
        this->nextSyncPointRtc += this->syncPeriodRtc; // increments next RTC sync point as currentSyncPoint + syncPeriod
        this->syncPointTheoreticalHsc += this->syncPeriodHsc; // increments next HSC sync point as theoreticalSyncPoint + syncPeriod
        this->syncPointExpectedHsc += this->syncPeriodHsc + this->clockCorrectionFlopsync;

        Rtc_TA::IRQclearVhtMatchFlag(); // clear RTC output channel
        Rtc_TA::IRQsetVhtMatchReg(Rtc_TA::IRQgetVhtTimerMatchReg() + syncPeriodRtc);

        // calculate required parameters
        this->error = this->syncPointActualHsc - syncPointExpectedHsc;
        this->clockCorrectionFlopsync = flopsyncVHT->computeCorrection(error);

        // TODO: (s) extend error.h types and print everything inside error.cpp + recover
        // handle case in which error exceeds maximum theoretical error
        if(error < -maxTheoreticalError || error > maxTheoreticalError) 
            IRQhandleErrorException();

        IRQupdateImpl(syncPointTheoreticalHsc, syncPointExpectedHsc, clockCorrectionFlopsync);
    }

    /**
     * @brief 
     * 
     * @param syncPointTheoreticalHsc
     * @param syncPointComputed 
     * @param vhtClockOffset 
     */
    void IRQupdateImpl(long long syncPointTheoreticalHsc, long long syncPointExpectedHsc, long long clockCorrectionFlopsync){
        
        //efficient way to calculate the factor T/(T+u(k))
        long long temp = (syncPeriodHsc<<32) / (syncPeriodHsc + clockCorrectionFlopsync);
        //calculate inverse of previous factor (T+u(k))/T
        long long inverseTemp = ((syncPeriodHsc + clockCorrectionFlopsync)<<32) / syncPeriodHsc;
        
        //Save modification to make effective the update
        //IRQoffsetUpdate(syncPointTheoreticalHsc, syncPointExpectedHsc); // TODO: (s) correct?
            
        factorI = static_cast<unsigned int>((temp & 0xFFFFFFFF00000000LLU)>>32);
        factorD = static_cast<unsigned int>(temp);
        
        inverseFactorI = static_cast<unsigned int>((inverseTemp & 0xFFFFFFFF00000000LLU)>>32);
        inverseFactorD = static_cast<unsigned int>(inverseTemp);
    }

    /**
     * @brief 
     * 
     */
    inline bool IRQisCorrectionEnabled() { return enabledCorrection; }
    
private:
    ///
    // Constructors
    ///
    Vht() : tc(EFM32_HFXO_FREQ), 
            syncPointTheoreticalHsc(0), syncPointExpectedHsc(0), syncPointActualHsc(0), 
            syncPeriodHsc(9609375), syncPointRtc(0), nextSyncPointRtc(0), syncPeriodRtc(6560),
            factorI(1), factorD(0), inverseFactorI(1), inverseFactorD(0), error(0), enabledCorrection(true), 
            maxTheoreticalError(static_cast<long long>(static_cast<double>(EFM32_HFXO_FREQ) * syncPeriodRtc / 32768 * 0.0003f)),
            vhtClockOffset(0), flopsyncVHT(nullptr), init(false) {}
    Vht(const Vht&)=delete;
    Vht& operator=(const Vht&)=delete;

    ///
    // Helper functions
    ///
    
    /**
     * @brief 
     * 
     * @param a 
     * @param bi 
     * @param bf 
     * @return long long 
     */
    static inline long long fastNegMul(long long a, unsigned int bi, unsigned int bf){
        return a < 0 ? -mul64x32d32(-a,bi,bf) : mul64x32d32(a,bi,bf);
    }

    /**
     * @brief 
     * 
     */
    void IRQhandleErrorException()
    {
        IRQerrorLog("\r\n***Exception: VHT");
        IRQerrorLog("\r\nVHT exceeded maximum theoretical correction");
        IRQerrorLog("\r\nerror: "); IRQerrorLog(std::to_string(static_cast<int>(error)).c_str());
        IRQerrorLog("\r\nmaxTheoreticalError: "); IRQerrorLog(std::to_string(static_cast<int>(maxTheoreticalError)).c_str());
        IRQerrorLog("\r\n\n");
        miosix_private::IRQsystemReboot();
    }

    /**
     * @brief 
     * 
     */
    /*
    void IRQresyncClockVht(){
        long long nowRtc=rtc.IRQgetValue();
        long long syncAtRtc=nowRtc+2;
        //This is very important, we need to restore the previous value in COMP1, to gaurentee the proper wakeup
        long long prevCOMP1=RTC->COMP1;
        RTC->COMP1=syncAtRtc-1;
        //Virtual high resolution timer, init without starting the input mode!
        TIMER2->CC[2].CTRL=TIMER_CC_CTRL_ICEDGE_RISING
                | TIMER_CC_CTRL_FILT_DISABLE
                | TIMER_CC_CTRL_INSEL_PRS
                | TIMER_CC_CTRL_PRSSEL_PRSCH4
                | TIMER_CC_CTRL_MODE_INPUTCAPTURE;
        PRS->CH[4].CTRL= PRS_CH_CTRL_SOURCESEL_RTC | PRS_CH_CTRL_SIGSEL_RTCCOMP1;
        RTC->IFC=RTC_IFC_COMP1;
        
        //Clean the buffer to avoid false reads
        TIMER2->CC[2].CCV;
        TIMER2->CC[2].CCV;
        
        while(RTC->SYNCBUSY & RTC_SYNCBUSY_COMP1);

        while(!(RTC->IF & RTC_IF_COMP1));
        long long timestamp=b.IRQgetVhtTimestamp();
        //Got the values, now polishment of flags and register
        RTC->IFC=RTC_IFC_COMP1;
        TIMER2->IFC=TIMER_IFC_CC2;
        
        RTC->COMP1=prevCOMP1;
        while(RTC->SYNCBUSY & RTC_SYNCBUSY_COMP1);
        long long syncAtHrt=mul64x32d32(syncAtRtc, 1464, 3623878656);
        HRTB::clockCorrection=syncAtHrt-timestamp;
        HRTB::syncPointHrtExpected=syncAtHrt;
        HRTB::nextSyncPointRtc=syncAtRtc+HRTB::syncPeriodRtc;
        HRTB::syncPointHrtTheoretical=syncAtHrt;
        HRTB::syncPointHrtActual=syncAtHrt;
        vht.IRQoffsetUpdate(HRTB::syncPointHrtTheoretical,HRTB::syncPointHrtExpected);
    }
    */

    TimeConversion tc;

    // HSC parameters
    long long syncPointTheoreticalHsc;
    long long syncPointExpectedHsc;
    long long syncPointActualHsc;
    long long syncPeriodHsc; //9609375; //11015625;//21984375;//4406250;//9609375;

    // RTC parameters
    long long syncPointRtc;
    long long nextSyncPointRtc;
    long long syncPeriodRtc; //6560; //7520; //15008; //3008; //6560;

    // Control parameters
    unsigned int factorI;
    unsigned int factorD;
    unsigned int inverseFactorI;
    unsigned int inverseFactorD;
    long long error;

    bool enabledCorrection;
    const long long maxTheoreticalError; // max error less than 300ppm

    long long vhtClockOffset;
    long long clockCorrectionFlopsync;
    FlopsyncVHT* flopsyncVHT;

    bool init;
};

}


#endif // VHT_H