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

#pragma once

#include "miosix.h"
#include "util/fixed.h"
#include "time/timeconversion.h"
#include "cassert"
#include "stdio.h"
#include <stdexcept>
#include "time/flopsync_vht.h"
#include "kernel/logging.h"
#include "interfaces-impl/time_types_spec.h"

namespace miosix {

// forward declaration


///
// Correction tile
///
class CorrectionTile
{
public:

    long long correct(long long ns)
    {
        FastInterruptDisableLock lck;
        return IRQcorrect(ns);
    }

    long long uncorrect(long long ns)
    {
        FastInterruptDisableLock lck;
        return IRQuncorrect(ns);
    }

    virtual long long IRQcorrect(long long ns)=0;
    virtual long long IRQuncorrect(long long ns)=0;

    //virtual void IRQinit()=0;
    
    // force a and b variables?
};

///
// VHT
///

// TODO: (s) refactor factorI and factorD using fp32_32 type + remove explicit mul64x32d32
template<typename Hsc_TA, typename Rtc_TA>
class Vht : public CorrectionTile
{
public:
    static Vht& instance()
    {
        static Vht vht;
        return vht;
    }

    /**
     * @brief 
     * 
     */
    const unsigned int posCorrection;

    /**
     * @brief 
     * 
     * @param ns 
     * @return long long 
     */
    long long IRQcorrect(long long ns)
    {
        if(!init) { return ns; }

        //long long tick = tc.ns2tick(ns) /*+ vhtClockOffset*/;
        //return tc.tick2ns(syncPointTheoreticalHsc + fastNegMul(tick - syncPointExpectedHsc, factorI, factorD));
        return a_km1 * ns + b_km1;
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

        //long long tick = tc.ns2tick(ns);
        //return tc.tick2ns(syncPointExpectedHsc + fastNegMul(tick - syncPointTheoreticalHsc, inverseFactorI, inverseFactorD) /*- vhtClockOffset*/);
        return (ns - b_km1) / a_km1;
    }

    /**
     * @brief sync between RTC and HSC every 20ms 
     * Since we execute instructions at 48Mhz, we execute multiple instructions before each RTC tick
     * 
     */
    void IRQinit()
    {
        this->flopsyncVHT = &FlopsyncVHT::instance();

        // setting up VHT timer
        Hsc_TA::IRQinitVhtTimer();
        Hsc_TA::IRQstartVhtTimer();

        Rtc_TA::IRQinitVhtTimer();
        Rtc_TA::IRQstartVhtTimer(); // read inside rtc.h

        // performing VHT initialization
        unsigned int nowRtc = Rtc_TA::IRQgetTimerCounter() + 2; // TODO: (s) this is for undocumented quirk, generalize! + CHECK OVERFLOW
        Rtc_TA::IRQsetVhtMatchReg(nowRtc);

        Rtc_TA::IRQclearVhtMatchFlag();
        // WARNING: this while may break if connected to GDB while doing step-by-step execution from above
        while(!Rtc_TA::IRQgetVhtMatchFlag()); // wait for first compare

        // Reading vht timestamp but replacing lower part of counter with RTC value
        long long vhtTimestamp = Hsc_TA::IRQgetVhtTimerCounter();

        if(vhtTimestamp > Hsc_TA::IRQgetTimerCounter()){
            vhtTimestamp -= 1<<16; // equivalent to ((TIMER2->CNT-1)<<16) | TIMER3->CC[0].CCV;
        }

        Rtc_TA::IRQclearVhtMatchFlag();
        Hsc_TA::IRQclearVhtMatchFlag();

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
        Rtc_TA::IRQsetVhtMatchReg(nextSyncPointRtc);
        this->syncPointTheoreticalHsc = nowHsc;

        // first VHT correction
        IRQupdateImpl(syncPointTheoreticalHsc, syncPointExpectedHsc, 0);

        this->init = true;
    }

    /**
     * @brief 
     * 
     * @param syncPointActualHsc 
     */
    void IRQupdate(long long syncPointActualHsc)
    {
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
    inline void IRQupdateImpl(long long syncPointTheoreticalHsc, long long syncPointExpectedHsc, long long clockCorrectionFlopsync)
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
        //static VirtualClockSpec * vc = &VirtualClockSpec::instance(); // FIXME: (s) remove instance
        if(a_km1 != a || b_km1 != b)
        {
            this->a_km1 = a;
            this->b_km1 = b;

            vc->IRQupdateCorrectionPair(std::make_pair(a, b), posCorrection);
        }
    }

    /**
     * @brief 
     * 
     */
    void IRQresyncClock()
    {
        long long nowRtc = Rtc_TA::IRQgetTimerCounter();
        long long syncAtRtc = nowRtc + 2; // TODO: (s) generalize for random quirk
        //This is very important, we need to restore the previous value in COMP1, to gaurentee the proper wakeup
        long long prevCOMP1 = Rtc_TA::IRQgetVhtTimerMatchReg();
        Rtc_TA::IRQsetVhtMatchReg(syncAtRtc);

        //Virtual high resolution timer, init without starting the input mode!
        Hsc_TA::IRQinitVhtTimer();
        Hsc_TA::IRQstartVhtTimer();
        
        Rtc_TA::IRQclearVhtMatchFlag();
        //Clean the buffer to avoid false reads
        Hsc_TA::IRQgetVhtTimerCounter();
        Hsc_TA::IRQgetVhtTimerCounter();
        while(!Rtc_TA::IRQgetVhtMatchFlag()); // wait for first compare

        long long timestamp = Hsc_TA::IRQgetVhtTimerCounter();
        //Got the values, now polishment of flags and register
        Rtc_TA::IRQclearVhtMatchFlag();
        Hsc_TA::IRQclearVhtMatchFlag();
        
        Rtc_TA::IRQsetVhtMatchReg(prevCOMP1+1); // TODO: (s) generalize quirk
        
        long long syncAtHrt = mul64x32d32(syncAtRtc, 1464, 3623878656);
        vhtClockOffset = syncAtHrt - timestamp;
        syncPointExpectedHsc = syncAtHrt;
        nextSyncPointRtc = syncAtRtc + syncPeriodRtc;
        syncPointTheoreticalHsc = syncAtHrt;
        syncPointActualHsc = syncAtHrt;

        // clear flags not to resync immediately
        Hsc_TA::IRQclearVhtMatchFlag();
        Hsc_TA::IRQgetVhtTimerCounter();
    }

    /**
     * @brief 
     * 
     */
    inline bool IRQisCorrectionEnabled() { return enabledCorrection; }

    /**
     * @brief 
     * 
     */
    inline void IRQdisableCorrection()   
    { 
        //RTC->IEN &= ~RTC_IEN_COMP1; // may be used for pre-wake up timer etc
        this->enabledCorrection = false; 
    }
    inline void IRQenableCorrection()    
    { 
        //RTC->IEN |= RTC_IEN_COMP1; // may be used for pre-wake up timer etc
        this->enabledCorrection = true; 
    }
    
private:
    ///
    // Constructors
    ///
    Vht() : posCorrection(0), tc(EFM32_HFXO_FREQ), syncPointTheoreticalHsc(0), syncPointExpectedHsc(0), 
            syncPointActualHsc(0), syncPeriodHsc(9609375), syncPointRtc(0), nextSyncPointRtc(0), syncPeriodRtc(6560),
            factorI(1), factorD(0), inverseFactorI(1), inverseFactorD(0), error(0), a_km1(1), b_km1(0), enabledCorrection(true), 
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

    // fast correction parameters
    fp32_32 a_km1;
    long long b_km1;

    bool enabledCorrection;
    const long long maxTheoreticalError; // max error less than 300ppm

    long long vhtClockOffset;
    long long clockCorrectionFlopsync;
    FlopsyncVHT* flopsyncVHT;

    bool init;
};

///
// Flopsync 3
///
/**
 * This class implements an instrument for correcting the clock based on
 * the FLOPSYNC-3 non-linear synchronization algorithm. It receives FLOPSYNC-3 data
 * and parameters as input, preserving a state capable of correcting an arbitrary
 * timestamp, using an equation of type Tcorrected = a * Tuncorrected + b where
 * a and b are coefficients derived from the VC formula.
 */
class Flopsync3  : public CorrectionTile
{
public:
    static Flopsync3& instance();

    /**
     * @brief 
     * 
     */
    const unsigned int posCorrection;

    /**
     * Converts an uncorrected time, expressed in nanoseconds, into a corrected one
     * @param tsnc uncorrected time (ns)
     * @return the corrected time (ticks) according to Flopsync3 correction
     */
    long long IRQcorrect(long long tsnc);

    /**
     * Converts a corrected time, expressed in ns, into an uncorrected one in ns
     * @param vc_t corrected time (ns)
     * @return the uncorrected time (ns)
     */
    long long IRQuncorrect(long long vc_t);

    // TODO: (s) change description + spiegare che non è fatto il IRQ tutto a parte operazioni critiche
    // perchè tanto update ogni 10s (IRQ se cambio variabili che rompono getTime())
    /**
     * Updates the internal value of the virtual clock
     * @param vc_k time of the virtual clock at the step t_0 + kT
     * - t_0 is the first synchronization time point in the local clock, in ns, corrected by FLOPSYNC-2 and VHT.
     * - k is the number of iterations of the synchronizer.
     * - T is the synchronization interval, as passed in the setSyncPeriod function
     * 
     * Please note that vc_k is in function of the tsnc_k (uncorrected time at current pase t_0 + kT)
     * vc_k(tsnc_k) := vc_km1 + (tsnc_k - tsnc_km1) * vcdot_km1;
     * So far, vc_k is computed by the transceiver timer (injecting tsnc_k) and forwarded to the dynamic_timesync_downlink
     */
    void IRQupdate(long long vc_k, long long e_k);

    /**
     * To be used only while initializing the time synchronization. Its purpose is initializing the T value
     * to be used in the correction as proportionality coefficient, thus performing compensation,
     * applying it as skew compensation but including also offset compensation, therefore obtaining
     * a monotonic clock.
     * @param syncPeriod the time T (ns), also known as synchronization interval
     */
    void IRQsetSyncPeriod(unsigned long long syncPeriod);
    void setSyncPeriod(unsigned long long syncPeriod);    

    /**
     * Set the initial offset for the virtual clock
     * 
     * @param T0 initial offset (ns)
     */
    void IRQsetInitialOffset(long long T0);
    void setInitialOffset(long long T0);
    
private:
    Flopsync3();
    Flopsync3(const Flopsync3&)=delete;
    Flopsync3& operator=(const Flopsync3&)=delete;

    /* utils functions */

    /**
     * Checks whether a passed time is negative or not. 
     * 
     * \throws invalid_argument if time is negative
     * @param time time to be checked
     */
    void assertNonNegativeTime(long long time);

    /**
     * This function verifies that the virtual clock has been initialized with
     * the sync period needed in order for the controller to work.
     * 
     * @throws runtime_error if the virtual clock is missing some initialization parameters
     */
    void assertInit();

    /* class variables */
    // NOTE: what we need for the correction is just a and b coefficients.
    // most of the other variables here are assigned just for debugging purposes.

    //Max period, necessary to guarantee the proper behaviour of runUpdate
    //They are 2^40=1099s
    const unsigned long long maxPeriod;
    unsigned long long syncPeriod;
    
    fp32_32 vcdot_k;        // slope of virtual clock at step k 
    fp32_32 vcdot_km1;      // slope of virtual clock at step k-1
    fp32_32 inv_vcdot_k;    // inverse of slope of virtual clock at step k 
    fp32_32 inv_vcdot_km1;  // inverse of slope of virtual clock at step k-1

    const fp32_32 a;
    const fp32_32 beta;    // denormalized pole for feedback linearization dynamics

    unsigned long long k;   // synchronizer step
    long long T0;           // initial offset

    // this variable verifies that the virtual clock has been initialized
    // with the sync period
    bool init;

    // lazy init since we get the sync period at runtime (see setSyncPeriod function)
    long long tsnc_km1; // -T
    long long vc_km1;   // -T

    TimeConversion tc;

    // fast correction parameters (ax + b)
    fp32_32 a_km1;
    long long b_km1;
};

#if defined(WITH_VHT) && !defined(WITH_FLOPSYNC)
using VirtualClockSpec = VirtualClock<Hsc, 1>;
#elif !defined(WITH_VHT) && defined(WITH_FLOPSYNC)
using VirtualClockSpec = VirtualClock<Hsc, 1>;
#elif defined(WITH_VHT) && defined(WITH_FLOPSYNC)
using VirtualClockSpec = VirtualClock<Hsc, 2>;
#else
using VirtualClockSpec = VirtualClock<Hsc, 0>;
#endif

}