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
#include "kernel/logging.h"
#include <stdexcept>

namespace miosix {

VirtualClock& VirtualClock::instance(){
    static VirtualClock vt;
    return vt;
}

// TODO: (s) replace assert with various error handlers
long long VirtualClock::IRQgetVirtualTimeNs(long long tsnc) noexcept
{
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

inline long long VirtualClock::IRQgetUncorrectedTimeNs(long long vc_t)
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
    assertInit();
    assertNonNegativeTime(vc_k);

    long long kT = this->k * this->syncPeriod;

    // calculating sync error
    long long e_k = (kT/* + T0*/) - vc_k;

    VirtualClock::updateVC(vc_k, e_k);

    // next iteration values update
    this->k += 1;
}

// TODO: (s) pre-compute inv_vcdot_k(m1) + update it + use inv_vcdot_km1(m1)
void VirtualClock::updateVC(long long vc_k, long long e_k)
{
    // TODO: (s) use error handler with IRQ + reboot
    assertInit();
    assertNonNegativeTime(vc_k);

    // controller correction
    // TODO: (s) need saturation for correction!
    //double u_k = fsync->computeCorrection(e_k);
    fp32_32 u_k = fsync->computeCorrection(e_k);
    
    // estimating clock skew
    //fp32_32 D_k = ( (vc_k - vc_km1) / vcdot_km1 ) - syncPeriod;
    fp32_32 D_k = ( fp32_32(vc_k - vc_km1) / vcdot_km1 ) - syncPeriod;

    // performing virtual clock slope correction disabling interrupt
    {
        FastInterruptDisableLock dlck;

        this->vcdot_k = (u_k * (beta - 1) + fp32_32(e_k) * (1 - beta) + syncPeriod) / (D_k + syncPeriod);

        // next iteration values update
        this->tsnc_km1 = deriveTsnc(vc_k);
        this->vc_km1 = vc_k;
        this->vcdot_km1 = this->vcdot_k;
    }
}

void VirtualClock::setSyncPeriod(unsigned long long syncPeriod)
{
    {FastInterruptDisableLock lck;
    IRQsetSyncPeriod(syncPeriod);}
}

void VirtualClock::IRQsetSyncPeriod(unsigned long long syncPeriod)
{
    // TODO: (s) do not throw but use IRQlogerror instead
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

// TODO: (s) change to IRQ
inline long long VirtualClock::deriveTsnc(long long vc_t)
{
    assertInit();
    assertNonNegativeTime(vc_t);

    auto GDB_1 = vc_t - vc_km1;
    auto GDB_2 = tsnc_km1 * vcdot_km1;
    auto GDB_3 = GDB_1 + GDB_2;
    long long GDB_4 = static_cast<long long>(GDB_3);
    int64_t absVal = std::abs(vcdot_km1.value);
    int32_t absDecimalVal = static_cast<int32_t>(absVal & 0x00000000FFFFFFFF);
    int32_t absIntegerVal = static_cast<int32_t>((absVal & 0xFFFFFFFF00000000) >> 32);
    long long GDB_5 = GDB_4 * inv_vcdot_km1;

    // TODO: (s) maybe i need basic correction to set time taking into account time it takes to perform this operation??
    //return static_cast<long long>(fp32_32(vc_t - vc_km1 + tsnc_km1 * vcdot_km1) / vcdot_km1);
    long long GDB_6 = vc_t - GDB_5;
    if(vc_t >= 4e9)
    {
        long long GDB_20 = 10;
    }
    return GDB_5;
}

void VirtualClock::assertNonNegativeTime(long long time)
{
    if(time < 0) 
    {
        IRQerrorLog("\r\n***Exception: VC"); 
        IRQerrorLog("\r\ntsnc is negative");
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

void VirtualClock::IRQinit(){}

VirtualClock::VirtualClock() : maxPeriod(1099511627775), syncPeriod(0), vcdot_k(1), vcdot_km1(1), 
                                inv_vcdot_k(1), inv_vcdot_km1(1), a(0.05), beta(0.025), k(0), T0(0), init(false), 
                                fsync(nullptr), tc(EFM32_HFXO_FREQ) {}



} // namespace miosix
