/***************************************************************************
 *   Copyright (C)  2013,2018 by Terraneo Federico                         *
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

#ifndef FLOPSYNC_3_H
#define	FLOPSYNC_3_H

//#include "synchronizer.h"
#include <cstdio>
#include "util/fixed.h"

namespace miosix {

/**
 * A new flopsync controller that can reach zero steady-state error both
 * with step-like and ramp-like disturbances.
 * It provides better synchronization under temperature changes, that occur
 * so slowly with respect to the controller operation to look like ramp
 * changes in clock skew.
 */
// TODO: (s) redefine Syncrhonizer interface according to new Flopsync3 implementation
class Flopsync3 //: public Synchronizer
{
public:
    static Flopsync3& instance();
    
    /**
     * Compute clock correction and receiver window given synchronization error
     * \param error synchronization error
     * \return a pair with the clock correction, and the receiver window
     */
    double computeCorrection(long long e_k);
    
    /**
     * Used after a resynchronization to reset the controller state
     */
    void reset();
    
    /**
     * \return the synchronization error e(k)
     */
    long long getSyncError();
    
    /**
     * \return getter for the clock correction u(k)
     */
    double getClockCorrection();
    
private:
    /**
     * Base constructor
     */
    Flopsync3();
    Flopsync3(const Flopsync3&)=delete;
    Flopsync3& operator=(const Flopsync3&)=delete;

    // errors
    long long e_k;
    long long e_km1;
    long long e_km2;

    // corrections
    fp32_32 u_k;
    fp32_32 u_km1;
    fp32_32 u_km2;
};

}

#endif //FLOPSYNC_3_H

