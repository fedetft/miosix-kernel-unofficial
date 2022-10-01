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
#include "correction_types.h" 
#include <algorithm> // DELETEME: (s)

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
    else return IRQderiveTsnc(vc_t);
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

long long closestPowerOfTwo(unsigned long long v)
{
    /*if(n < 1) return 0;
    long long res = 1;
    // Try all powers starting from 2^1
    for (int i = 0; i < 8 * sizeof(unsigned long long); i++) {
        long long curr = 1 << i;
        // If current power is more than n, break
        if (curr > n)
            break;
        res = curr;
    }
    return res;*/
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v |= v >> 32;
    v++;

    return v>>1;
}

void VirtualClock::updateVC(long long vc_k, long long e_k)
{
    // TODO: (s) use error handler with IRQ + reboot
    assertInit();
    assertNonNegativeTime(vc_k);

    // controller correction
    // TODO: (s) need saturation for correction!
    fp32_32 u_k = fsync->computeCorrection(e_k);

    // estimating clock skew absorbing initial offset if starting from zero
    static bool offsetInit = false;
    static bool secondOffsetInit = false;
    long long deltaT = vc_k - vc_km1;
    if(offsetInit && !secondOffsetInit)
    {
        deltaT += T0;
        secondOffsetInit = true;
    }
    if (!offsetInit) 
    { 
        offsetInit = true; 
        setInitialOffset(vc_k); // sets T0
        deltaT -= T0;
    }
    long long D_k = ( deltaT * inv_vcdot_km1 ) - syncPeriod;

    //long long D_k = ( (vc_k - vc_km1) / vcdot_km1 ) - syncPeriod;
    //       ↓↓        ↓↓       ↓↓       ↓↓       ↓↓       ↓↓
    //long long D_k = ( (vc_k - vc_km1) * inv_vcdot_km1 ) - syncPeriod;

    // NOTE: this approach was discarded because of the poor rapresentation
    // we were getting when inverting prescaledDenum for the division
    // remember: in fp32_32, division is just multiplication by the reciprocal 
    // pre computing multiplicative factors
    // This was necessary because D_k + syncPeriod is a very big number and its inverse
    // was not rapresentable correctly on just 32 bit leading to an approximated and therefore
    // wrong vcdot_k factor.
    /*static const long long prescaleExp = 24; // scale exponent, 2^n

    // prescale denum by factor of 2^prescaleExp
    static const long long prescaledSyncPeriod = syncPeriod>>prescaleExp;
    // TODO: (s) usare prescaleOp alla potenza di due più vicina (spiegare che inverso di potenza di due invertibile senza approx)
    fp32_32 prescaledDenum = fp32_32((D_k>>prescaleExp) + prescaledSyncPeriod); 
    // rescale everything by factor of 256 (note inverse, evaluated at compile time)
    static constexpr fp32_32 rescaleFactor(1/static_cast<double>(1LL<<prescaleExp));*/

    // TODO: (s) should be numerator instead of just D_k + syncPeriod
    fp32_32 closest2pow = fp32_32(std::min(closestPowerOfTwo(D_k + syncPeriod), (1LL<<26)));
    fp32_32 prescaledDenum = closest2pow;
    fp32_32 rescaleFactor = fp32_32((D_k + syncPeriod) / closest2pow).fastInverse();

    static TimerProxySpec * timerProxy = &TimerProxySpec::instance();

    // performing virtual clock slope correction disabling interrupt
    {
        FastInterruptDisableLock dLock;

        // TODO: (s) don't scale all num but every single element because sum may overflow!
        // perchè fp32_32(syncPeriod) potrebbe non starci in 32 bit signed
        //this->vcdot_k = (u_k * (beta - 1) + fp32_32(e_k) * (1 - beta) + syncPeriod) / fp32_32(D_k + syncPeriod);
        //        ↓↓        ↓↓       ↓↓       ↓↓       ↓↓       ↓↓       ↓↓       ↓↓       ↓↓       ↓↓
        //this->vcdot_k = ((u_k * (beta - 1) + fp32_32(e_k) * (1 - beta) + syncPeriod) / prescaledDenum) * rescaleFactor;
        this->vcdot_k = fp32_32(syncPeriod / prescaledDenum) * rescaleFactor;
        this->inv_vcdot_k = vcdot_k.fastInverse();

        // next iteration values update
        this->tsnc_km1 = IRQderiveTsnc(vc_k);
        this->vc_km1 = vc_k;
        this->vcdot_km1 = this->vcdot_k;
        this->inv_vcdot_km1 = this->inv_vcdot_k;

        // calculate fast correction parameters
        fp32_32 a = vcdot_km1;
        long long b = vc_km1 - vcdot_km1 * tsnc_km1;
        this->a_km1 = a;
        this->b_km1 = b;

        timerProxy->IRQrecomputeFastCorrectionPair();
    }

    /* DEBUG PRINTS */
    // iprintf("SyncPeriod:\t%lld\n", syncPeriod);
    // printf("closest2pow:\t%lld\n", closest2pow.value>>32);
    // printf("prescaledDenum\t%.15f\n", static_cast<double>(prescaledDenum));
    // printf("syncPeriod / prescaledDenum:\t%lld\n", syncPeriod / prescaledDenum);
    // printf("(syncPeriod / prescaledDenum) * rescaleFactor:\t%.15f\n", static_cast<double>(fp32_32(syncPeriod / prescaledDenum) * rescaleFactor));
    // printf("vcdot:\t\t%.20f\n", (double)vcdot_km1);
    // printf("inv_vcdot_km1:\t%.20f\n", (double)inv_vcdot_km1);
    // iprintf("D_k:\t\t%lld\n", D_k);
}

void VirtualClock::setSyncPeriod(unsigned long long syncPeriod)
{
    FastInterruptDisableLock lck;
    IRQsetSyncPeriod(syncPeriod);
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
inline long long VirtualClock::IRQderiveTsnc(long long vc_t)
{
    assertInit();
    assertNonNegativeTime(vc_t);

    // (vc_t - vc_km1 + tsnc_km1 * vcdot_km1) / vcdot_km1;
    return (vc_t - vc_km1 + tsnc_km1 * vcdot_km1) * inv_vcdot_km1;
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
                                a_km1(1LL), b_km1(0), fsync(nullptr), tc(EFM32_HFXO_FREQ) {}



} // namespace miosix
