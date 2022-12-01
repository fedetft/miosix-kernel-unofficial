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

/**
 * Correction Tile
 * This class defines methods and varaibles for synchronizers (VHT, FLOPSYNC-3, ...) to be compatible
 * with the VLCS. It forses the synchronizer to:
 * 1. Implement its correction function f
 * 2. Implement its uncorrection function f^-1
 * 3. Define its position on the VLCS
*/
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
     * @brief This function implements the time correction f of a generic uncorrected time
     * 
     * @param ns time to be corrected
     * @return long long corrected time as f(ns) that here is implemented as a*ns + b
     */
    long long IRQcorrect(long long ns);

    /**
     * @brief This function implements the time correction f^-1 of a generic corrected time
     * 
     * @param ns  time to be uncorrected
     * @return long long uncorrected time as f^-1(ns) that here is implemented as (ns - b) / a
     */
    long long IRQuncorrect(long long ns);

    /**
     * @brief configures uderlying timers to sync RTC and HSC every 20ms 
     * The first correction is forced manually reading timers value. Since the VHT
     * related HSC timer is started before the RTC, capturing the HSC value every 200ms
     * results in an offset on the captured timestamp. To overcome this problem, the variable
     * vhtOffset is initialized
     */
    void IRQinit();

    /**
     * @brief Method to update the VHT corretion
     * 
     * @param syncPointActualHsc time retrieved from the VHT timestamp, to be confronted with the
     * theoretical one
     */
    void IRQupdate(long long syncPointActualHsc);

    /**
     * @brief 
     * 
     * @param syncPointTheoreticalHsc theoretical time in which the VHT is expecting the correction (k*200ms + vhtClockOffset)
     * @param syncPointComputed actual timestamp captured by the HSC at the RTC trigger
     * @param clockCorrectionFlopsync correction value from the FLOPSYNC-VHT controller
     */
    void IRQupdateImpl(long long syncPointTheoreticalHsc, long long syncPointExpectedHsc, long long clockCorrectionFlopsync);

    /**
     * @brief This function is used to manually resync both HSC and RTC. It comes in handy
     * when exiting deep sleep where a manual resync is necessary.
     * 
     */
    void IRQresyncClock();

    /**
     * @brief Allows to check if the correction is enabled or not
     * 
     * @return true if correction is enabled
     * @return false if correction is not enabled
     */
    inline bool IRQisCorrectionEnabled() { return enabledCorrection; }

    /**
     * @brief Disables VHT correction
     * 
     */
    inline void IRQdisableCorrection()   
    { 
        this->enabledCorrection = false; 
    }

    /**
     * @brief Enables VHT correction
     * 
     */
    inline void IRQenableCorrection()    
    { 
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
     * @brief Performs fast signed multiplication using compile-time optimized functions as num * 32.32
     * 
     * @param a integer number
     * @param bi integer part of the 32.32 number
     * @param bf decimal part of the 32.32 number
     * @return long long value of the signed multiplication
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

/**
 * Flopsync 3
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
     * @brief Converts an uncorrected time, expressed in nanoseconds, into a corrected one using its
     * function f = a*ns + b
     * @param tsnc uncorrected time [ns]
     * @return the corrected time [ns]
     */
    long long IRQcorrect(long long tsnc);

    /**
     * @brief Converts a corrected time, expressed in nanoseconds, into an uncorrected one using its
     * function f^-1 = (ns - b) / a
     * @param vc_t (virtual) corrected time [ns]
     * @return uncorrected time [ns]
     */
    long long IRQuncorrect(long long vc_t);

    /**
     * @brief This function performs the update of the FLOPSYNC-3 controller and it's meant
     * to be called every synchronization period T. It calculates the new values for the 
     * clock rate 'a' and the clock offset 'b' and updates both inside the virtual clock.
     * This controller is meant to be called form the network module but its implementation
     * is independent from the network stack. It just needs an error and the actual virutal time
     * to perform the correction. 98% of this function works with interrupt enabled and only
     * disables them when needing to update 'a' and 'b' in the VC. Not only having interrupts
     * enabled do not weight on the operative system, but also this function is called presumably
     * every 10s
     * 
     * @param vc_k Virtual time, expected to be around kT
     * @param e_k error between actual time and the theoretical one
     */
    void update(long long vc_k, long long e_k);

    /**
     * @brief To be used only while initializing the time synchronization. 
     * Its purpose is initializing the T value necessary to initialize a few FLOPSYNC variables
     * as -T to perform the first correction.
     * @param syncPeriod the synchronization period T [ns]
     * @throw logic_error if synchronization period exceeds the maximum or equals zero
     */
    void setSyncPeriod(unsigned long long syncPeriod);    
    void IRQsetSyncPeriod(unsigned long long syncPeriod);
    
    /**
     * Set the initial offset for the virtual clock. It just works by setting vc_km1 to T0
     * in order to obtain a difference of vc_k - vc_km1 around T when calling the update function.
     * 
     * @param T0 initial offset [ns]
     */
    void setInitialOffset(long long T0);
    void IRQsetInitialOffset(long long T0);

    /**
     * @brief this function is called when a synchronization packet is lost.
     * Because the synchronization packet timestamp is not available, a placeholder is passed
     * correspondent to the theoretical time on which we were expecting the synch packet.
     * This practically makes the virtual clock continue blindly with the same correction rate as before
     * 
     * @param vc_k 
     */
    void lostPacket(long long vc_k);

    /**
     * @brief Resets all internal FLOPSYNC-3 variables
     * 
     */
    void reset();

    /**
     * @brief returns the receiver window value
     * 
     */
    int getReceiverWindow() { return receiverWindow; }
    
private:
    Flopsync3();
    Flopsync3(const Flopsync3&)=delete;
    Flopsync3& operator=(const Flopsync3&)=delete;

    /* utils functions */

    /**
     * @brief Checks whether a passed time is negative or not. 
     * 
     * \throws invalid_argument if time is negative
     * @param time time to be checked
     */
    void assertNonNegativeTime(long long time);

    /**
     * @brief This function verifies that the virtual clock has been initialized with
     * the sync period needed in order for the controller to work.
     * 
     * @throws runtime_error if the virtual clock is missing some initialization parameters
     */
    void assertInit();

    /* class variables */
    // NOTE: what we need for the correction is just a and b coefficients.
    // most of the other variables here are assigned just for debugging purposes.

    // Max period, necessary to guarantee the correct functioning of the FLOPSYNC-3
    // after this value, the temperature influences the skew so much that its contribution
    // cannot be ignored anymore
    const unsigned long long maxPeriod;
    unsigned long long syncPeriod;
    
    fp32_32 vcdot_k;        // slope of virtual clock at step k 
    fp32_32 vcdot_km1;      // slope of virtual clock at step k-1
    fp32_32 inv_vcdot_k;    // inverse of slope of virtual clock at step k 
    fp32_32 inv_vcdot_km1;  // inverse of slope of virtual clock at step k-1

    // FLOPSYNC-3 controller parameters
    const fp32_32 a;
    const fp32_32 beta;    // denormalized pole for feedback linearization dynamics

    unsigned long long k;   // synchronizer step

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
    const int minReceiverWindow;
    const int maxReceiverWindow;
};

}