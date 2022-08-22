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
        long long tick = tc.ns2tick(ns);
        return baseTheoreticalHsc + fastNegMul(tick - baseExpectedHsc, factorI, factorD);
    }

    /**
     * @brief 
     * 
     * @param ns 
     * @return long long 
     */
    long long IRQuncorrect(long long ns)
    {
        long long tick = tc.ns2tick(ns);
        return baseExpectedHsc + fastNegMul((tick - baseTheoreticalHsc), inverseFactorI, inverseFactorD);
    }

    /**
     * @brief sync between RTC and HSC every 20ms 
     * 
     */
    void IRQinit()
    {
        // TODO: (s) develop
        this->flopsyncVHT = &(FlopsyncVHT::instance());

        // setting up VHT timer
        Hsc_TA::IRQinitVhtTimer();
        //Hsc_TA::IRQstartVhtTimer(); // TODO: (s) ??? start TIMER3 ma dopo mi chiede timestamp??

        Rtc_TA::IRQinitVhtTimer();
        //Rtc_TA::IRQstartVhtTimer(); // already started by deep sleep

        // performing VHT initialization
        unsigned int nowRtc = Rtc_TA::IRQgetTimerCounter() + 2; // TODO: (s) this is for undocumented quirk, generalize! + CHECK OVERFLOW
        Rtc_TA::IRQsetVhtMatchReg(nowRtc);

        // TODO: (s) set TIMER3 + PRS
        //Virtual high resolution timer, init starting the input mode!
        /*TIMER2->CC[2].CTRL=TIMER_CC_CTRL_ICEDGE_RISING
                    | TIMER_CC_CTRL_FILT_DISABLE
                    | TIMER_CC_CTRL_INSEL_PRS
                    | TIMER_CC_CTRL_PRSSEL_PRSCH4
                    | TIMER_CC_CTRL_MODE_INPUTCAPTURE;
        PRS->CH[4].CTRL= PRS_CH_CTRL_SOURCESEL_RTC | PRS_CH_CTRL_SIGSEL_RTCCOMP1;*/

        Rtc_TA::IRQclearVhtMatchFlag();
        while(!Rtc_TA::IRQgetVhtMatchFlag());

        /*long long timestamp=(TIMER3->CNT<<16) | TIMER2->CC[2].CCV;
        if(timestamp > IRQread32Timer()){
            timestamp=((TIMER3->CNT-1)<<16) | TIMER2->CC[2].CCV;
        }*/

        Rtc_TA::IRQclearVhtMatchFlag();
        Hsc_TA::IRQclearVhtMatchFlag(); // TODO: (s) TIMER2->IFC=TIMER_IFC_CC2;

        #if EFM32_HFXO_FREQ != 48000000 || EFM32_LFXO_FREQ != 32768 // TODO: (s) parametrize
        #error "Clock frequency assumption not satisfied"
        #endif

        long long nowHsc = mul64x32d32(nowRtc, 1464, 3623878656);
        this->clockCorrection = nowHsc; - timestamp; // TODO: (s) timestamp not calculated yet
        this->baseExpectedHsc = nowHsc;
        this->nextSyncPointRtc = nowRtc + syncPeriodRtc;
        Rtc_TA::IRQsetVhtMatchReg(nextSyncPointRtc); // TODO: (s) undocumented quirk, bring it to hsc and rtc class level, transparent
        this->baseTheoreticalHsc = nowHsc;

        // first VHT correction
        IRQupdate(baseTheoreticalHsc, baseExpectedHsc, 0);

        // start periodi VHT thead
        Hsc_TA::IRQstartVhtTimer();
        std::thread vhtThread = std::thread([this]{ this->vhtLoop(); }); //std::thread(&miosix::Vht<Hsc_TA, Rtc_TA>::vhtLoop, this);
        //Thread* vhtThread = Thread::create(&vhtLoop, 2048, 1, this);  // old definition with stack size
        static_cast<void>(vhtThread); // avoid unused variable warning
    }

    /**
     * @brief 
     * 
     * @param baseTheoreticalHsc
     * @param baseComputed 
     */
    void IRQoffsetUpdate(long long baseTheoreticalHsc, long long baseComputedHsc){
        this->baseTheoreticalHsc = baseTheoreticalHsc;
        this->baseExpectedHsc = baseComputedHsc;
    }

    /**
     * @brief 
     * 
     * @param baseTheoreticalHsc
     * @param baseComputed 
     * @param clockCorrection 
     */
    void IRQupdate(long long baseTheoreticalHsc, long long baseComputedHsc, long long clockCorrection){
        //efficient way to calculate the factor T/(T+u(k))
        long long temp = (syncPeriodHsc<<32) / (syncPeriodHsc + clockCorrection);
        //calculate inverse of previous factor (T+u(k))/T
        long long inverseTemp = ((syncPeriodHsc + clockCorrection)<<32) / syncPeriodHsc;
        
        //Save modification to make effective the update
        IRQoffsetUpdate(baseTheoreticalHsc, baseComputedHsc);
            
        factorI = static_cast<unsigned int>((temp & 0xFFFFFFFF00000000LLU)>>32);
        factorD = static_cast<unsigned int>(temp);
        
        inverseFactorI = static_cast<unsigned int>((inverseTemp & 0xFFFFFFFF00000000LLU)>>32);
        inverseFactorD = static_cast<unsigned int>(inverseTemp);
    }

    /**
     * @brief HSC setters
     * 
     */
    inline void IRQsetBaseTheoretical(long long basetheoreticalHsc) { this->baseTheoreticalHsc = baseTheoreticalHsc; }
    inline void IRQsetBaseExpected(long long baseExpectedHsc)       { this->baseExpectedHsc = baseExpectedHsc; }
    inline void IRQsetBaseActual(long long baseActualHsc)           { this->baseActualHsc = baseActualHsc; }
    inline void IRQsetSyncPeriodHsc(long long syncPeriodHsc)        { this->syncPeriodHsc = syncPeriodHsc; }

    /**
     * @brief RTC setters
     * 
     */
    inline void IRQsetSyncPointRtc(long long syncPointRtc)          { this->syncPointRtc = syncPointRtc; }
    inline void IRQsetNextSyncPointRtc(long long nextSyncPointRtc)  { this->nextSyncPointRtc = nextSyncPointRtc; }
    inline void IRQsetSyncPeriodRtc(long long syncPeriodRtc)        { this->syncPeriodRtc = syncPeriodRtc; }

    /**
     * @brief  Other setter and getters
     * 
     */
    inline bool IRQisCorrectionEnabled() { return enabledCorrection; }
    inline void IRQincrementPendingVhtSync() { ++pendingVhtSync; }
private:
    ///
    // Constructors
    ///
    Vht() : tc(EFM32_HFXO_FREQ), 
            baseTheoreticalHsc(0), baseExpectedHsc(0), baseActualHsc(0), syncPeriodHsc(9609375),
            syncPointRtc(0), nextSyncPointRtc(0), syncPeriodRtc(6560),
            factorI(1), factorD(0), inverseFactorI(1), inverseFactorD(0),
            error(0), pendingVhtSync(0), enabledCorrection(true),
            clockCorrection(0), flopsyncVHT(nullptr) {}
    Vht(const Vht&)=delete;
    Vht& operator=(const Vht&)=delete;

    ///
    // Helper functions
    ///
    static inline long long fastNegMul(long long a,unsigned int bi, unsigned int bf){
        return a < 0 ? -mul64x32d32(-a,bi,bf) : mul64x32d32(a,bi,bf);
    }

    /**
     * @brief 
     * 
     */
    void vhtLoop() {        
        this->pendingVhtSync = 0; // reset pending sync

        // check max error less than 300ppm
        const long long maxTheoreticalError = static_cast<double>(EFM32_HFXO_FREQ) * syncPeriodRtc / 32768 * 0.0003f;
        
        // VHT core correction
        // FIXME: (s) no loop ma chiamato più volte dal padre (update...) invece che IRQwakeup.
        for(;;)
        {
            Thread::wait(); //< blocking statement. Thread stops until another thread calls for wakeup
            // FIXME: (s) prima di fare wakeup, setta variabli corrispondenti
            error = baseActualHsc - baseExpectedHsc;
            clockCorrection = flopsyncVHT->computeCorrection(error);

            if(enabledCorrection)
            {
                //The correction should always less than 300ppm
                assert(error < maxTheoreticalError && error > -maxTheoreticalError);
                
                //HRTB::clockCorrectionFlopsync = u;
                {
                    FastInterruptDisableLock lck;
                    IRQupdate(baseTheoreticalHsc, baseExpectedHsc, clockCorrection);
                }                
            } 
        }
    }

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
    long long baseTheoreticalHsc;
    long long baseExpectedHsc;
    long long baseActualHsc;
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

    //It is incremented in the TMR2->CC2 routine and reset in the Thread 
    unsigned int pendingVhtSync;
    bool enabledCorrection;

    long long clockCorrection;
    FlopsyncVHT* flopsyncVHT;
};

}


#endif // VHT_H