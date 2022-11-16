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
#include <algorithm> // DELETEME: (s)
#include "interfaces-impl/time_types_spec.h"
#include "util/franction.h"

namespace miosix {

Flopsync3& Flopsync3::instance(){
    static Flopsync3 vt;
    return vt;
}

// TODO: (s) replace assert with various error handlers
long long Flopsync3::IRQcorrect(long long tsnc)
{
    assertNonNegativeTime(tsnc);
    return a_km1 * tsnc + b_km1;
}

inline long long Flopsync3::IRQuncorrect(long long vc_t)
{   
    //assertInit();
    assertNonNegativeTime(vc_t);
    return (vc_t - b_km1) / a_km1;
}

long long closestPowerOfTwo(unsigned long long); // forward declaration
void Flopsync3::update(long long vc_k, long long e_k)
{
    // TODO: (s) use error handler with IRQ + reboot
    assertInit();
    assertNonNegativeTime(vc_k);
    static long long k = 1;
    /* FRACTION VERSION */
    // static Fraction vcdot_km1_frac(1, 1);
    // static constexpr Fraction beta_frac = Fraction(25, 1000).simplify();
    // Fraction u_k_frac = (Fraction(15, 100) * e_k).simplify();
    // Fraction D_k_frac = ( ( (vc_k - vc_km1) / vcdot_km1_frac ) - (long long)syncPeriod ).simplify();
    
    // //Fraction vcdot_k_frac = (u_k_frac * (beta_frac - 1LL) + e_k * (1LL - beta_frac) + (long long)syncPeriod) / (D_k_frac + (long long)syncPeriod);
    // Fraction vcdot_k_frac = (u_k_frac * (beta_frac - 1LL)).simplify();
    //         vcdot_k_frac += (e_k * (1LL - beta_frac)).simplify(); 
    //         vcdot_k_frac.simplify();
    //         vcdot_k_frac += (long long)syncPeriod; vcdot_k_frac.simplify(); 
    //         vcdot_k_frac.simplify();
    //         vcdot_k_frac /= (D_k_frac + (long long)syncPeriod).simplify();
    //         vcdot_k_frac.simplify();

    // vcdot_km1_frac = vcdot_k_frac;
    
    // printf("[VC] vcdot_k_frac:\t\t%.20f\n", (double)vcdot_k_frac);
    // printf("[VC] vcdot_k_frac_fp:\t%.20f\n", (double)(fp32_32((double)vcdot_k_frac)));

    // long long tsnc_km1 = IRQuncorrect(vc_k);
    // // calculate fast correction parameters
    // fp32_32 a((double)vcdot_k_frac);


    // //long long b = vc_k - static_cast<long long>(vcdot_km1_frac * tsnc_km1);
    // long long alfa2 = vc_k - syncPeriod;
    // long long beta2 = tsnc_km1 - syncPeriod;
    // //static constexpr long long scaledSyncPeriod = 10000000000 / 10;
    // /*Fraction b_frac((long long)10);
    //         b_frac *= ( 1LL - vcdot_km1_frac ).simplify(); b_frac.simplify();
    //         b_frac += ( alfa2 - (vcdot_km1_frac * beta2) ) / 1000000000LL; b_frac.simplify();
    //         b_frac *= 1000000000LL; b_frac.simplify();
    // long long b = (long long)b_frac;*/
    // long long b = static_cast<long long>( ( (long long)syncPeriod * (1LL - vcdot_km1_frac).simplify() ).simplify() );
    //             b +=  static_cast<long long>( ( alfa2 - (vcdot_km1_frac * beta2).simplify() ).simplify() );

    // //long long b = 1000000000LL vc_k - static_cast<long long>(vcdot_km1_frac * tsnc_km1);
                


    //iprintf("vc_k: %lld\n", vc_k);
    //iprintf("vcdot_km1_frac * tsnc_km1: %lld\n", vcdot_km1_frac * tsnc_km1);

    // performing virtual clock slope correction disabling interrupt
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

    /* DOUBLE VERSION */
    static double vcdot_km1_double = 1.0;
    static const double beta_double = 0.025;
    double u_k_double = 0.015 * e_k;
    double D_k_double = ( (vc_k - vc_km1) / vcdot_km1_double ) - syncPeriod;

    double vcdot_k_double = (u_k_double * (beta_double - 1) + e_k * (1 - beta_double) + syncPeriod) / (D_k_double + syncPeriod);    
    vcdot_km1_double = vcdot_k_double;

    long long tsnc_km1;
    {
        FastInterruptDisableLock dLock;
        tsnc_km1 = IRQuncorrect(vc_k);
    }
     
    fp32_32 a((double)vcdot_k_double);
    long long b = vc_k - vcdot_km1_double * tsnc_km1;    

    {
        FastInterruptDisableLock dLock;

        // next iteration values update
        this->vc_km1 = vc_k;

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

    iprintf("[FP3] k:\t\t%lld\n", k);
    iprintf("[FP3] D_k:\t\t%lld\n", (long long)D_k_double);
    iprintf("[FP3] e_k:\t\t%lld\n", e_k);
    printf("[FP3] vcdot_k_double:\t\t%.20f\n", vcdot_k_double);
    printf("[FP3] a:\t\t\t%.20f\n", (double)a);
    iprintf("[FP3] b:\t\t\t%lld\n", b);
    ++k;

    // /* FP32_32 VERSION */
    // // controller correction
    // static constexpr fp32_32 factorP(0.15);
    // fp32_32 e_kFP(e_k);
    // fp32_32 u_k = factorP * e_kFP;

    // // estimating clock skew
    // long long D_k = ( (vc_k - vc_km1) / vcdot_km1 ) - syncPeriod;
    
    // // this approach aims to scale down syncPeriod contribute in the formula since its value do npt
    // // fit 32 signed bit in the fp32_32 type.
    // // Since we have at the denumerator D_k + syncPeriod, that in fp32_32 is implemented as a
    // // multiplication for the inverse, we want a power of two in order to avoid loss during inversion.
    // // For this reason, we find the closest poewr of two to the sum D_k + syncperiod that would
    // // fit in 32-bit signed integer part. Of course, since we rescaled part of the formula,
    // // we need a rescaling factor to have things back to normal. This approach cannot avoid loss
    // // in precision and long operations are prepared here with interrupt enabled.
    // long long closest2pow = std::min(closestPowerOfTwo(D_k + syncPeriod), (1LL<<26));
    // fp32_32 closest2powFP(closest2pow);
    // fp32_32 prescaledDenum = closest2powFP;
    // //fp32_32 rescaleFactor = fp32_32((D_k + syncPeriod) / closest2powFP).fastInverse();
    // //       ↓↓        ↓↓       ↓↓       ↓↓       ↓↓          ↓↓          ↓↓      
    // fp32_32 rescaleFactor = ( (fp32_32(D_k) / closest2powFP) + (syncPeriod / closest2powFP) ).fastInverse();
    
    // // calculating vcdot_k with interrupts enabled, perform assignment with interrupts disabled
    // //fp32_32 vcdot_k = (u_k * (beta - 1) + fp32_32(e_k) * (1 - beta) + syncPeriod) / fp32_32(D_k + syncPeriod);
    // //        ↓↓        ↓↓       ↓↓       ↓↓       ↓↓       ↓↓       ↓↓       ↓↓       ↓↓       ↓↓
    // //fp32_32 vcdot_k = ((u_k * (beta - 1) + fp32_32(e_k) * (1 - beta) + syncPeriod) / prescaledDenum) * rescaleFactor;
    // //        ↓↓        ↓↓       ↓↓       ↓↓       ↓↓       ↓↓       ↓↓       ↓↓       ↓↓       ↓↓       ↓↓
    // fp32_32 vcdot_k = ( ( (u_k * (beta - 1) + e_kFP * (1 - beta)) / prescaledDenum ) + ( syncPeriod / prescaledDenum ) ) * rescaleFactor;

    // printf("[VC] vcdot_k:\t\t\t%.20f\n", (double)vcdot_k);

    // // performing virtual clock slope correction disabling interrupt
    // {
    //     FastInterruptDisableLock dLock;

    //     // 1.000000009 -> 4294967335LL
    //     // 0.999999990 -> x
    //     //vcdot_k.value = std::max( std::min(vcdot_k.value, 4294967335LL), 4294963001LL );
    //     this->vcdot_k = vcdot_k;

    //     // next iteration values update
    //     this->tsnc_km1 = IRQuncorrect(vc_k);
    //     this->vc_km1 = vc_k;
    //     this->vcdot_km1 = this->vcdot_k;

    //     // calculate fast correction parameters
    //     fp32_32 a = vcdot_km1;
    //     long long b = vc_km1 - vcdot_km1 * tsnc_km1;
    //     //constexpr fp32_32 a(1.00000000009)
    //     //printf("[VC] D_k:\t\t%.20f\n", (double)D_k);
    //     //printf("[VC] a:\t\t\t%.20f\n", (double)a);
    //     //printf("[VC] inv_vcdot_km1:\t%.20f\n", (double)(a.fastInverse()));
    //     //printf("[VC] b:\t\t\t%lld\n", b);

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

    /* DEBUG PRINTS */
    // iprintf("[VC] SyncPeriod:\t%lld\n", syncPeriod);
    // printf("[VC] closest2pow:\t%lld\n", closest2pow);
    // printf("[VC] prescaledDenum\t%.15f\n", static_cast<double>(prescaledDenum));
    // printf("[VC] syncPeriod / prescaledDenum:\t%lld\n", syncPeriod / prescaledDenum);
    // printf("[VC] (syncPeriod / prescaledDenum) * rescaleFactor:\t%.15f\n", static_cast<double>(fp32_32(syncPeriod / prescaledDenum) * rescaleFactor));
    //printf("[VC] D_k:\t\t%.20f\n", (double)D_k);
    // printf("[VC] vcdot:\t\t%.20f\n", (double)vcdot_km1);
    //printf("[VC] a:\t\t\t%.20f\n", (double)a_km1);
    //printf("[VC] inv_vcdot_km1:\t%.20f\n", (double)(vcdot_km1.fastInverse()));
    // iprintf("[VC] vc_k - vc_km1:\t\t%lld\n", tmp);
    //printf("[VC] b:\t\t\t%lld\n", b_km1);
    //printf("[VC] corr of %lld = %lld\n", (long long)5e9, vc->IRQcorrectTimeNs((long long)5e9));
    //iprintf("D_k:\t\t%lld\n", D_k);
}

void Flopsync3::setSyncPeriod(unsigned long long syncPeriod)
{
    FastInterruptDisableLock lck;
    IRQsetSyncPeriod(syncPeriod);
}

void Flopsync3::IRQsetSyncPeriod(unsigned long long syncPeriod)
{
    // TODO: (s) do not throw but use IRQlogerror instead
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
    // // DELETEME: (s) T0 variable deve sparire
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

std::pair<int, int> Flopsync3::lostPacket()
{
    return std::make_pair(0, this->maxReceiverWindow);
}

void Flopsync3::reset()
{
    this->vcdot_k        = static_cast<int32_t>(1); 
    this->vcdot_km1      = static_cast<int32_t>(1); 
    this->inv_vcdot_k    = static_cast<int32_t>(1); 
    this->inv_vcdot_km1  = static_cast<int32_t>(1);
    this->k              = 0; // DELETEME: (s) delete k + method
    this->a_km1          = static_cast<int32_t>(1);
    this->b_km1          = 0;
    this->receiverWindow = maxReceiverWindow;

    vc->IRQupdateCorrectionPair(std::make_pair(a_km1, b_km1), posCorrection);
}

Flopsync3::Flopsync3() : posCorrection(VirtualClockSpec::numCorrections-1), maxPeriod(1099511627775), 
                            syncPeriod(0), vcdot_k(1), vcdot_km1(1), inv_vcdot_k(1), inv_vcdot_km1(1), 
                            a(0.05), beta(0.025), k(0), T0(0), init(false), tc(EFM32_HFXO_FREQ), 
                            a_km1(1LL), b_km1(0), receiverWindow(maxReceiverWindow) {}


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
