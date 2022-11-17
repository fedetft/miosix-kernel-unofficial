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

namespace miosix {

// forward declaration
//....

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

protected:
    CorrectionTile(unsigned int posCorrection) : posCorrection(posCorrection) {}
    const unsigned int posCorrection;

// Force derived classes to initialize position of the correction
private:
    CorrectionTile() = delete;
};

///
// VHT
///

class Vht : public CorrectionTile
{
public:
    static Vht& instance();

    /**
     * @brief 
     * 
     * @param ns 
     * @return long long 
     */
    long long IRQcorrect(long long ns);

    /**
     * @brief 
     * 
     * @param ns 
     * @return long long 
     */
    long long IRQuncorrect(long long ns);

    /**
     * @brief sync between RTC and HSC every 20ms 
     * Since we execute instructions at 48Mhz, we execute multiple instructions before each RTC tick
     * 
     */
    void IRQinit();

    /**
     * @brief 
     * 
     * @param syncPointActualHsc 
     */
    void IRQupdate(long long syncPointActualHsc);

    /**
     * @brief 
     * 
     * @param syncPointTheoreticalHsc
     * @param syncPointComputed 
     * @param vhtClockOffset 
     */
    void IRQupdateImpl(long long syncPointTheoreticalHsc, long long syncPointExpectedHsc, long long clockCorrectionFlopsync);

    /**
     * @brief 
     * 
     */
    void IRQresyncClock();

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
    Vht() : CorrectionTile(0), tc(EFM32_HFXO_FREQ), syncPointTheoreticalHsc(0), syncPointExpectedHsc(0), 
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
        auto tmp = RTC->CNT;
        IRQerrorLog("\r\n***Exception: VHT");
        IRQerrorLog("\r\nVHT exceeded maximum theoretical correction");
        IRQerrorLog("\r\nerror: "); IRQerrorLog(std::to_string(static_cast<int>(error)).c_str());
        IRQerrorLog("\r\nmaxTheoreticalError: "); IRQerrorLog(std::to_string(static_cast<int>(maxTheoreticalError)).c_str());
        IRQerrorLog("\r\nsyncPointActualHsc: "); IRQerrorLog(std::to_string(static_cast<int>(syncPointActualHsc)).c_str());
        IRQerrorLog("\r\nsyncPointExpectedHsc: "); IRQerrorLog(std::to_string(static_cast<int>(syncPointExpectedHsc)).c_str());
        IRQerrorLog("\r\nvhtClockOffset: "); IRQerrorLog(std::to_string(static_cast<int>(vhtClockOffset)).c_str());
        IRQerrorLog("\r\nRTC: "); IRQerrorLog(std::to_string(static_cast<int>(tmp)).c_str());
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
    void update(long long vc_k, long long e_k);

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

    /**
     * @brief 
     * 
     * @return std::pair<> 
     */
    void lostPacket(long long VCk);

    /**
     * @brief Resets a and b parameters among with all control variables
     * 
     */
    void reset();

    /**
     * @brief 
     * 
     */
    int getReceiverWindow() { return receiverWindow; }
    
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

    // fast correction parameters (ax + b), "a" DOES NOT refear to the flopsync controller parameter
    fp32_32 a_km1;
    long long b_km1;

    // reciever window parameters
    int receiverWindow;
    static constexpr int minReceiverWindow = 50000;
    static constexpr int maxReceiverWindow = 6000000;
};

}