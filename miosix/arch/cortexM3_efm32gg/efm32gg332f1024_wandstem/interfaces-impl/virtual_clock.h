/***************************************************************************
 *   Copyright (C) 2016 by Fabiano Riccardi                                *
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

#ifndef VIRTUAL_CLOCK_H
#define VIRTUAL_CLOCK_H

#include "kernel/timeconversion.h"
#include "cassert"
#include "stdio.h"
#include "miosix.h"
#include <stdexcept>

namespace miosix {
// TODO: (s) fix description in the future
/**
 * This class implements an instrument for correcting the clock based on
 * the FLOPSYNC-3 synchronization algorithm. It receives FLOPSYNC-3 data
 * and parameters as input, preserving a state capable of correcting an arbitrary
 * timestamp, using an equation of type texpected = tstart + coeff * skew
 */
class VirtualClock 
{
public:
    static VirtualClock& instance();
    
    //FIXME: (s) does it make sense to have uncorrected time here? I don't want propagazion of tsnc!! 
    /**
     * Converts a corrected time, expressed in ticks, into an uncorrected one
     * @param t_corr corrected time (ns)
     * @return the uncorrected time (ticks)
     */
    long long corrected2uncorrected(long long vc_t);
        
    /**
     * Converts an uncorrected time, expressed in nanoseconds, into a corrected one
     * @param tsnc uncorrected time (ns)
     * @return the corrected time (ns) according to Flopsync3 correction
     */
    long long getVirtualTime(long long tsnc);
    
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
    void updateVC(long long vc_k);

    /**
     * To be used only while initializing the time synchronization. Its purpose is initializing the T value
     * to be used in the correction as proportionality coefficient, thus performing compensation,
     * applying it as skew compensation but including also offset compensation, therefore obtaining
     * a monotonic clock.
     * @param syncPeriod the time T (ns), also known as synchronization interval
     */
    void setSyncPeriod(unsigned long long syncPeriod);

    /**
     * Set the initial offset for the virtual clock
     * 
     * @param T0 initial offset (ns)
     */
    void setInitialOffset(long long T0);
    
private:
    VirtualClock(){};
    VirtualClock(const VirtualClock&)=delete;
    VirtualClock& operator=(const VirtualClock&)=delete;

    /* utils functions */

    /**
     * calculates the tsnc associated with a virtual clock time by inverting the clock formula.
     * tsnc = (vc_t - vc_km1 + tsnc_km1 * vcdot_km1)/ vcdot_km1)
     * 
     * @param vc_t virtual clock time (ns)
     * @return long long uncorrected time (ns)
     */
    long long deriveTsnc(long long vc_t);

    /**
     * Checks whether a passed time is negative or not. 
     * 
     * \throws invalid_argument if time is negative
     * @param time time to be checked
     */
    void assertNonNegativeTime(long long time);

    /* class variables */

    //Max period, necessary to guarantee the proper behaviour of runUpdate
    //They are 2^40=1099s
    const unsigned long long maxPeriod = 1099511627775;
    
    unsigned long long syncPeriod = 0;
    
    long long baseTheoretical = 0;
    long long baseComputed    = 0;

    long long vcdot_k   = 1;    // slope of virtual clock at step k 
    long long vcdot_km1 = 1;    // slope of virtual clock at step k-1

    unsigned long long k = 0;   // synchronizer step
    long long T0 = 0;           // initial offset

    // lazy init since we get the sync period at runtime (see setSyncPeriod function)
    long long tsnc_km1; // -T
    long long vc_km1;   // -T
};
}

#endif /* VIRTUAL_CLOCK_H */

