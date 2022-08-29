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
#include "time/timeconversion.h"

namespace miosix {

VirtualClock& VirtualClock::instance(){
    static VirtualClock vt;
    return vt;
}

long long VirtualClock::IRQgetVirtualTimeNs(long long tsnc) noexcept
{
    //assertInit();
    assertNonNegativeTime(tsnc);
    
    if(!init) return tsnc; // needed when system is booting up and until no sync period is set
    else return /*T0 +*/ vc_km1 + vcdot_km1 * (tsnc - tsnc_km1);
}

long long VirtualClock::getVirtualTimeNs(long long tsnc) noexcept
{
    FastInterruptDisableLock lck;
    return IRQgetVirtualTimeNs(tsnc);
}

long long VirtualClock::getVirtualTimeTicks(long long tsnc) noexcept
{
    return tc.ns2tick(getVirtualTimeNs(tsnc));
}

long long VirtualClock::IRQgetUncorrectedTimeNs(long long vc_t)
{   
    //assertInit();
    assertNonNegativeTime(vc_t);

    if(!init) return vc_t; // needed when system is booting up and until no sync period is set
    // inverting VC formula vc_k(tsnc_k) := vc_km1 + (tsnc_k - tsnc_km1) * vcdot_km1;
    else return deriveTsnc(vc_t);
}

long long VirtualClock::getUncorrectedTimeNs(long long vc_t)
{
    FastInterruptDisableLock lck;
    return IRQgetUncorrectedTimeNs(vc_t);
}

long long VirtualClock::getUncorrectedTimeTicks(long long vc_t)
{
    return tc.ns2tick(getUncorrectedTimeNs(vc_t));
}

// TODO: (s) the error should be passed from timesync for a more abstraction level
void VirtualClock::updateVC(long long vc_k)
{
    FastInterruptDisableLock lck;
    IRQupdateVC(vc_k);
}

void VirtualClock::IRQupdateVC(long long vc_k)
{
    assertInit();
    assertNonNegativeTime(vc_k);

    long long kT = this->k * this->syncPeriod;

    // calculating sync error
    long long e_k = (kT/* + T0*/) - vc_k;

    VirtualClock::IRQupdateVC(vc_k, e_k);

    // next iteration values update
    this->k += 1;
}

void VirtualClock::updateVC(long long vc_k, long long e_k)
{
    FastInterruptDisableLock lck;
    IRQupdateVC(vc_k, e_k);
}

void VirtualClock::IRQupdateVC(long long vc_k, long long e_k)
{
    assertInit();
    assertNonNegativeTime(vc_k);

    //FIXME: (s) use fixed point 32x32

    // controller correction
    // TODO: (s) need saturation for correction!
    double u_k = fsync->computeCorrection(e_k);

    // estimating clock skew
    double D_k = ( (vc_k - vc_km1) / vcdot_km1 ) - syncPeriod;

    // performing virtual clock slope correction
    this->vcdot_k = (u_k * (beta - 1) + e_k * (1 - beta) + syncPeriod) / (D_k + syncPeriod);

    // next iteration values update
    this->tsnc_km1 = deriveTsnc(vc_k);
    this->vc_km1 = vc_k;
    this->vcdot_km1 = this->vcdot_k;
}

void VirtualClock::setSyncPeriod(unsigned long long syncPeriod)
{
    FastInterruptDisableLock lck;
    IRQsetSyncPeriod(syncPeriod);
}

void VirtualClock::IRQsetSyncPeriod(unsigned long long syncPeriod)
{
    if(syncPeriod > VirtualClock::maxPeriod) { throw std::logic_error("Sync period cannot be more than maximum!"); };  
    if(syncPeriod == 0) { throw std::logic_error("Sync period cannot be zero!"); };  

    // init values with T or -T
    this->syncPeriod    = syncPeriod;
    this->vc_km1        = -syncPeriod;
    this->tsnc_km1      = -syncPeriod;
    
    this->fsync = &Flopsync3::instance();

    this->init = true;
}

void VirtualClock::setInitialOffset(long long T0)
{
    FastInterruptDisableLock lck;
    IRQsetInitialOffset(T0);
}

void VirtualClock::IRQsetInitialOffset(long long T0)
{
    assertNonNegativeTime(T0);
    this->T0 = T0;
}

long long VirtualClock::deriveTsnc(long long vc_t)
{
    assertInit();
    assertNonNegativeTime(vc_t);

    return (vc_t - vc_km1 + tsnc_km1 * vcdot_km1) / vcdot_km1;
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

VirtualClock::VirtualClock() : maxPeriod(1099511627775), syncPeriod(0), baseTheoretical(0),
                            baseComputed(0), vcdot_k(1), vcdot_km1(1), a(0.05), beta(a/2),
                            k(0), T0(0), init(false), fsync(nullptr), tc(EFM32_HFXO_FREQ) {}


void VirtualClock::IRQinit(){}
} // namespace miosix
