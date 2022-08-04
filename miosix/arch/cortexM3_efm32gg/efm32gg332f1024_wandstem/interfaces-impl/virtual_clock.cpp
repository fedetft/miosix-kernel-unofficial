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

#include "interfaces/virtual_clock.h"
#include "kernel/timeconversion.h"

namespace miosix {

VirtualClock& VirtualClock::instance(){
    static VirtualClock vt;
    return vt;
}

void VirtualClock::updateVC(long long vc_k)
{
    assertInit();
    assertNonNegativeTime(vc_k);
    
    if(syncPeriod == 0) { throw std::logic_error("No sync period has been set yet"); };  

    int kT = this->k * this->syncPeriod;

    // calculating sync error
    int e_k = (kT + T0) - vc_k; // TODO: (s) the error should be passed from timesync for a more abstraction level

    // controller correction
    int u_k = 1 + 0.15 * e_k; // TODO: (s) should call flopsync3 for correction!
    
    // performing virtual clock slope correction
    this->vcdot_k = u_k; 
    
    // next iteration values (k + 1 done in dynamic timesync downlink, ma non sono molto d'accordo)
    this->tsnc_km1 = deriveTsnc(vc_k);
    this->k += 1; // TODO: (s) move it to the synchronizer?
    this->vc_km1 = vc_k;
    this->vcdot_km1 = this->vcdot_k;
}

long long VirtualClock::getVirtualTimeNs(long long tsnc)
{
    //assertInit();
    assertNonNegativeTime(tsnc);

    if(!init) return tsnc; // needed when system is booting up and no sync period was recieved
    else return vc_km1 + vcdot_km1 * (tsnc - tsnc_km1);
}

long long VirtualClock::getVirtualTimeTicks(long long tsnc)
{
    return tc.ns2tick(getVirtualTimeNs(tsnc));
}

long long VirtualClock::getUncorrectedTimeNs(long long vc_t)
{
    assertInit();

    // inverting VC formula vc_k(tsnc_k) := vc_km1 + (tsnc_k - tsnc_km1) * vcdot_km1;
    return deriveTsnc(vc_t);
}

long long VirtualClock::getUncorrectedTimeTicks(long long vc_t)
{
    return tc.ns2tick(getUncorrectedTimeNs(vc_t));
}

void VirtualClock::setSyncPeriod(unsigned long long syncPeriod)
{
    if(syncPeriod > VirtualClock::maxPeriod) throw 0;

    // init values with T or -T
    this->syncPeriod    = syncPeriod;
    this->vc_km1        = -syncPeriod;
    this->tsnc_km1      = -syncPeriod;
    
    this->init = true;
}

void VirtualClock::setInitialOffset(long long T0)
{
    assertNonNegativeTime(T0);
    this->T0 = T0;
}

inline long long VirtualClock::deriveTsnc(long long vc_t)
{
    assertInit();
    assertNonNegativeTime(vc_t);

    return (vc_t - vc_km1 + tsnc_km1 * vcdot_km1)/ vcdot_km1;
}

void VirtualClock::assertNonNegativeTime(long long time)
{
    if(time < 0) 
    { 
        throw std::invalid_argument(
            std::string("[ ") +  typeid(*this).name() +  std::string(" | ") + __FUNCTION__ + std::string(" ] Time cannot be negative")
        ); 
    }
}

void VirtualClock::assertInit()
{
    if(!init)
    {
        throw std::runtime_error(
            std::string("[ ") +  typeid(*this).name() +  std::string(" | ") + __FUNCTION__ + std::string(" ] Virtual clock has not been initialized yet.")
        );
    }
}

}
