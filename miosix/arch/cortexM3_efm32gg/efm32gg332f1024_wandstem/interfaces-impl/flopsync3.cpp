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

#include "time/clock_sync.h"
#include "time/timeconversion.h"
#include "kernel/logging.h"
#include <stdexcept>
#include "interfaces-impl/time_types_spec.h"

namespace miosix {

Flopsync3& Flopsync3::instance(){
    static Flopsync3 vt;
    return vt;
}

long long Flopsync3::IRQcorrect(long long tsnc)
{
    assertNonNegativeTime(tsnc);
    
    // correction as f = a*x + b
    return a_km1 * tsnc + b_km1;
}

inline long long Flopsync3::IRQuncorrect(long long vc_t)
{   
    assertNonNegativeTime(vc_t);
    
    // uncorrection as f^-1 = (x - b) / a
    return (vc_t - b_km1) / a_km1;
}

long long closestPowerOfTwo(unsigned long long); // forward declaration
void Flopsync3::update(long long vc_k, long long e_k)
{
    assertInit();
    assertNonNegativeTime(vc_k);

    /* DOUBLE VERSION */
    // static double vcdot_km1_double = 1.0;
    // static const double beta_double = 0.025;
    // double u_k_double = 0.015 * e_k;
    // double D_k_double = ( (vc_k - vc_km1) / vcdot_km1_double ) - syncPeriod;

    // double vcdot_k_double = (u_k_double * (beta_double - 1) + e_k * (1 - beta_double) + syncPeriod) / (D_k_double + syncPeriod);    
    // vcdot_km1_double = vcdot_k_double;

    // long long tsnc_km1;
    // {
    //     FastInterruptDisableLock dLock;
    //     tsnc_km1 = IRQuncorrect(vc_k);
    // }
     
    // fp32_32 a((double)vcdot_k_double);
    // long long b = vc_k - vcdot_km1_double * tsnc_km1;    

    // {
    //     FastInterruptDisableLock dLock;

    //     // next iteration values update
    //     this->vc_km1 = vc_k;

    //     if(this->a_km1 != a || this->b_km1 != b)
    //     {
    //         // update internal and vc coeff. at position N only if necessary
    //         this->a_km1 = a;
    //         this->b_km1 = b;

    //         vc->IRQupdateCorrectionPair(std::make_pair(a, b), posCorrection);
    //     }

    //     // calculating new receiving window (clamped between max and min)
    //     this->receiverWindow = std::max(
    //                         std::min(30 * e_k, static_cast<long long>(maxReceiverWindow)), 
    //                         static_cast<long long>(minReceiverWindow));
    // }

    // iprintf("[FP3] k:\t\t%lld\n", k);
    // iprintf("[FP3] D_k:\t\t%lld\n", (long long)D_k_double);
    // iprintf("[FP3] e_k:\t\t%lld\n", e_k);
    // printf("[FP3] vcdot_k_double:\t\t%.20f\n", vcdot_k_double);
    // printf("[FP3] a:\t\t\t%.20f\n", (double)a);
    // iprintf("[FP3] b:\t\t\t%lld\n", b);
    // ++k;

    // /* FP32_32 VERSION */
    // controller correction
    static constexpr fp32_32 factorP(0.15);
    fp32_32 e_kFP(e_k);
    fp32_32 u_k = factorP * e_kFP;

    // estimating clock skew
    long long D_k = ( (vc_k - vc_km1) / vcdot_km1 ) - syncPeriod;
    
    // this approach aims to scale down syncPeriod contribute in the formula since its value do npt
    // fit 32 signed bit in the fp32_32 type.
    // Since we have at the denumerator D_k + syncPeriod, that in fp32_32 is implemented as a
    // multiplication for the inverse, we want a power of two in order to avoid loss during inversion.
    // For this reason, we find the closest poewr of two to the sum D_k + syncperiod that would
    // fit in 32-bit signed integer part. Of course, since we rescaled part of the formula,
    // we need a rescaling factor to have things back to normal. This approach cannot avoid loss
    // in precision and long operations are prepared here with interrupt enabled.
    long long closest2pow = std::min(closestPowerOfTwo(D_k + syncPeriod), (1LL<<26));
    fp32_32 closest2powFP(closest2pow);
    fp32_32 prescaledDenum = closest2powFP;
    //fp32_32 rescaleFactor = fp32_32((D_k + syncPeriod) / closest2powFP).fastInverse();
    //       ↓↓        ↓↓       ↓↓       ↓↓       ↓↓          ↓↓          ↓↓      
    fp32_32 rescaleFactor = ( (fp32_32(D_k) / closest2powFP) + (syncPeriod / closest2powFP) ).fastInverse();
    
    // calculating vcdot_k with interrupts enabled, perform assignment with interrupts disabled
    //fp32_32 vcdot_k = (u_k * (beta - 1) + fp32_32(e_k) * (1 - beta) + syncPeriod) / fp32_32(D_k + syncPeriod);
    //        ↓↓        ↓↓       ↓↓       ↓↓       ↓↓       ↓↓       ↓↓       ↓↓       ↓↓       ↓↓
    //fp32_32 vcdot_k = ((u_k * (beta - 1) + fp32_32(e_k) * (1 - beta) + syncPeriod) / prescaledDenum) * rescaleFactor;
    //        ↓↓        ↓↓       ↓↓       ↓↓       ↓↓       ↓↓       ↓↓       ↓↓       ↓↓       ↓↓       ↓↓
    fp32_32 vcdot_k = ( ( (u_k * (beta - 1) + e_kFP * (1 - beta)) / prescaledDenum ) + ( syncPeriod / prescaledDenum ) ) * rescaleFactor;

    long long tsnc_km1;
    {
        FastInterruptDisableLock dLock;
        tsnc_km1 = IRQuncorrect(vc_k);
    }

    // calculate fast correction parameters
    fp32_32 a = vcdot_k;
    long long b = vc_k - vcdot_k * tsnc_km1;

    // performing virtual clock slope correction disabling interrupt
    {
        FastInterruptDisableLock dLock;
        this->vcdot_k = vcdot_k;

        // next iteration values update
        this->tsnc_km1 = tsnc_km1;
        this->vc_km1 = vc_k;
        this->vcdot_km1 = this->vcdot_k;

        if(this->a_km1 != a || this->b_km1 != b)
        {
            // update internal and vc coeff. at position N only if necessary
            this->a_km1 = a;
            this->b_km1 = b;

            vc->IRQupdateCorrectionPair(std::make_pair(a, b), posCorrection);
        }

        // calculating new receiving window (clamped between max and min)
        this->receiverWindow = std::max(
                            std::min(30 * e_k, static_cast<long long>(maxReceiverWindow)), 
                            static_cast<long long>(minReceiverWindow));
    }

    /* DEBUG PRINTS */
    // iprintf("[FP3] k:\t\t%lld\n", k);
    // iprintf("[FP3] D_k:\t\t%lld\n", D_k);
    // iprintf("[FP3] e_k:\t\t%lld\n", e_k);
    // printf("[FP3] a:\t\t%.20f\n", (double)a);
    // iprintf("[FP3] b:\t\t%lld\n", b);
    // iprintf("[FP3] recvWindow:\t%d\n", this->receiverWindow);
    // iprintf("[FP3] SyncPeriod:\t%lld\n", syncPeriod);
    // printf("[FP3] closest2pow:\t%lld\n", closest2pow);
    // printf("[FP3] prescaledDenum\t%.15f\n", static_cast<double>(prescaledDenum));
    // printf("[FP3] syncPeriod / prescaledDenum:\t%lld\n", syncPeriod / prescaledDenum);
    // printf("[FP3] (syncPeriod / prescaledDenum) * rescaleFactor:\t%.15f\n", static_cast<double>(fp32_32(syncPeriod / prescaledDenum) * rescaleFactor));
    // printf("[FP3] D_k:\t\t%.20f\n", (double)D_k);
    // printf("[FP3] vcdot:\t\t%.20f\n", (double)vcdot_km1);
    // printf("[FP3] a:\t\t\t%.20f\n", (double)a_km1);
    // printf("[FP3] inv_vcdot_km1:\t%.20f\n", (double)(vcdot_km1.fastInverse()));
    // iprintf("[FP3] vc_k - vc_km1:\t\t%lld\n", tmp);
    // printf("[FP3] b:\t\t\t%lld\n", b_km1);
    // printf("[FP3] corr of %lld = %lld\n", (long long)5e9, vc->IRQcorrectTimeNs((long long)5e9));
    // iprintf("[FP3] D_k:\t\t%lld\n", D_k);

    ++k;
}

void Flopsync3::setSyncPeriod(unsigned long long syncPeriod)
{
    FastInterruptDisableLock lck;
    IRQsetSyncPeriod(syncPeriod);
}

void Flopsync3::IRQsetSyncPeriod(unsigned long long syncPeriod)
{
    if(syncPeriod > Flopsync3::maxPeriod) { throw std::logic_error("Sync period cannot be more than maximum!"); };  
    if(syncPeriod == 0) { throw std::logic_error("Sync period cannot be zero!"); };  

    // init values with T or -T
    this->syncPeriod    = syncPeriod;
    this->vc_km1        = -syncPeriod;
    this->tsnc_km1      = -syncPeriod;

    this->init = true;
}

void Flopsync3::setInitialOffset(long long T0)
{
    FastInterruptDisableLock lck;
    IRQsetInitialOffset(T0);
}

void Flopsync3::IRQsetInitialOffset(long long T0)
{
    assertNonNegativeTime(T0);
    this->vc_km1 = T0;
}

void Flopsync3::assertNonNegativeTime(long long time)
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

void Flopsync3::assertInit()
{
    if(!init)
    {
        throw std::runtime_error(
            std::string("[ ") +  typeid(*this).name() +  std::string(" | ") + __FUNCTION__ + std::string(" ] Virtual clock has not been initialized yet.")
        );
    }
}

void Flopsync3::lostPacket(long long vc_k)
{
    this->vc_km1 = vc_k;
    this->receiverWindow = this->maxReceiverWindow;
}

void Flopsync3::reset()
{
    // casts force fp32_32 constructor to use fast the assignment operator with an integer
    this->vcdot_k        = static_cast<int32_t>(1); 
    this->vcdot_km1      = static_cast<int32_t>(1); 
    this->inv_vcdot_k    = static_cast<int32_t>(1); 
    this->inv_vcdot_km1  = static_cast<int32_t>(1);
    this->a_km1          = static_cast<int32_t>(1);
    this->b_km1          = 0;
    this->receiverWindow = maxReceiverWindow;
    this->k              = 1;

    // update virtual clock with default parameters a = 1 and b = 0 (no correction from FLOPSYNC)
    vc->IRQupdateCorrectionPair(std::make_pair(a_km1, b_km1), posCorrection);
}

Flopsync3::Flopsync3() : CorrectionTile(VirtualClockSpec::numCorrections-1), maxPeriod(1099511627775), 
                            syncPeriod(0), vcdot_k(1), vcdot_km1(1), inv_vcdot_k(1), inv_vcdot_km1(1), 
                            a(0.05), beta(0.025), k(0), init(false), tc(EFM32_HFXO_FREQ), a_km1(1LL), 
                            b_km1(0), receiverWindow(maxReceiverWindow), minReceiverWindow(50000), 
                            maxReceiverWindow(6000000) {}


///
// Utils
///

// Bit Twiddling Hacks
// calculates the closest power of two less or equal then the number v
long long closestPowerOfTwo(unsigned long long v)
{
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

} // namespace miosix
