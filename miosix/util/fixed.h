/***************************************************************************
 *   Copyright (C) 2022 by Sorrentino Alessandro                           *
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
 *   macros or functions from this file, or you compile this file   *
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

#ifndef FIXED_H
#define FIXED_H

#include <cstdint> // int32_t, int64_t, ...
#include <cstddef> // size_t
#include "time/timeconversion.h" // mul64x32d32
#include <cmath> // abs
#include "miosix.h"

namespace miosix
{

class notImplementedException : public std::logic_error
{
public:
    notImplementedException() : std::logic_error("Function not yet implemented") { };
}; // class notImplementedException

// TODO: (s) move everything in CPP to avoid linker error in multiple imports
// software handled unsigned 128-bit integer
struct uint128_t {  
    unsigned long long UPPER;
    unsigned long long LOWER;

    // default constructor
    constexpr uint128_t() : UPPER(0), LOWER(0) {};
    constexpr uint128_t(unsigned long long lower) : UPPER(0), LOWER(lower) {};
    constexpr uint128_t(unsigned long long upper, unsigned long long lower) : UPPER(upper), LOWER(lower) {};

    // Assign
    constexpr uint128_t& operator = (const uint128_t& other)
    {
        this->UPPER = other.UPPER;
        this->LOWER = other.LOWER;
        return *this;
    }

    constexpr uint128_t& operator = (unsigned long long a)
    {
        this->UPPER = 0;
        this->LOWER = a;
        return *this;
    }

    // Addition
    /*constexpr*/ uint128_t operator + (const uint128_t& other) const
    {
        uint128_t INTERNAL;
               
        #if defined(__ARM_EABI__)
        /* ARM 32-bit solution */
        unsigned long long out_upper;
        unsigned long long out_lower;
        _add_and_carry_uint128_t(this->UPPER, this->LOWER, other.UPPER, other.LOWER, &out_upper, &out_lower);

        INTERNAL.LOWER = out_lower;
        INTERNAL.UPPER = out_upper;

        #else
        
        #warning "uint128_t operator + (const uint128_t& other) const is not optimized for this architecture"
        /* portable c++ solution */
        INTERNAL.UPPER = this->UPPER + other.UPPER; // assume no overflow on upper part
        // sum lower part by predicting overflow a + b > max => b > max - a
        // TODO: (s) > o >=?
        if(this->LOWER > MAX_ULL - other.LOWER)
        {
            // overflow
            INTERNAL.UPPER += 1;
            
            // sum lower reamin without overflow
            INTERNAL.LOWER = (MAX_ULL - ((MAX_ULL - this->LOWER) + (MAX_ULL - other.LOWER))) - 1;
        }
        else INTERNAL.LOWER = this->LOWER + other.LOWER;
        #endif

        return INTERNAL;
    }

    inline /*constexpr*/ uint128_t& operator += (const uint128_t& other)
    {
        #if defined(__ARM_EABI__)
        /* ARM 32-bit solution */
        unsigned long long out_upper;
        unsigned long long out_lower;
        _add_and_carry_uint128_t(this->UPPER, this->LOWER, other.UPPER, other.LOWER, &out_upper, &out_lower);

        this->LOWER = out_lower;
        this->UPPER = out_upper;

        #else

        #warning "uint128_t& operator += (const uint128_t& other) is not optimized for this architecture"
        /* portable c++ solution */
        this->UPPER += other.UPPER; // assume no overflow on upper part 
        
        // sum lower part by predicting overflow a + b > max => b > max - a
        if(this->LOWER > MAX_ULL - other.LOWER)
        {
            // overflow
            this->UPPER += 1;
            
            // sum lower reamin without overflow
            this->LOWER = (MAX_ULL - ((MAX_ULL - this->LOWER) + (MAX_ULL - other.LOWER))) - 1;
        }
        else this->LOWER += other.LOWER;
        #endif

        return *this;
    }

    /*constexpr*/ uint128_t operator + (unsigned long long a) const
    {
        uint128_t INTERNAL;

        #if defined(__ARM_EABI__)
        /* ARM 32-bit solution */
        unsigned long long out_upper;
        unsigned long long out_lower;
        _add_and_carry_uint128_t(this->UPPER, this->LOWER, 0, a, &out_upper, &out_lower);

        INTERNAL.LOWER = out_lower;
        INTERNAL.UPPER = out_upper;

        #else

        #warning "uint128_t operator + (unsigned long long a) const is not optimized for this architecture"
        /* portable c++ solution */
        // sum lower part by predicting overflow a + b > max => b > max - a
        // TODO: (s) > o >=?
        if(this->LOWER > MAX_ULL - a)
        {
            // overflow
            INTERNAL.UPPER += 1;
            
            // sum lower reamin without overflow
            INTERNAL.LOWER = (MAX_ULL - ((MAX_ULL - this->LOWER) + (MAX_ULL - a))) - 1;
        }
        else INTERNAL.LOWER = this->LOWER + a;
        #endif

        return INTERNAL;
    }

    /*constexpr*/ uint128_t& operator += (unsigned long long a)
    {  
        #if defined(__ARM_EABI__)
        /* ARM 32-bit solution */
        unsigned long long out_upper;
        unsigned long long out_lower;
        _add_and_carry_uint128_t(this->UPPER, this->LOWER, 0, a, &out_upper, &out_lower);

        this->LOWER = out_lower;
        this->UPPER = out_upper;

        #else
        
        #warning "uint128_t& operator += (unsigned long long a) is not optimized for this architecture"
        /* portable c++ solution */
        // sum lower part by predicting overflow a + b > max => b > max - a
        if(this->LOWER > MAX_ULL - a)
        {
            // overflow
            this->UPPER += 1;
            
            // sum lower reamin without overflow
            this->LOWER = (MAX_ULL - ((MAX_ULL - this->LOWER) + (MAX_ULL - a))) - 1;
        }
        else this->LOWER += a;
        #endif

        return *this;
    }

    // Bit shift
    constexpr uint128_t operator >> (const size_t s) const
    {
        uint128_t INTERNAL;

        // right shift lower part
        INTERNAL.LOWER = this->LOWER >> s;

        // sum remain from upper shifting
        INTERNAL.LOWER |= (this->UPPER & ((1LLU<<s)-1))<<(64-s); // FIXME: (s) con s > 64 si rompe

        // right shift upper part
        INTERNAL.UPPER = this->UPPER >> s;

        return INTERNAL;
    }

    constexpr uint128_t operator << (const size_t s) const
    {
        uint128_t INTERNAL;

        // left shift upper part
        INTERNAL.UPPER = this->UPPER << s;

        // sum remain from lower shifting
        INTERNAL.UPPER |= (this->LOWER & ~((1LLU<<(64-s))-1))>>(64-s); // FIXME: (s) con s > 64 si rompe

        // left shift upper part
        INTERNAL.LOWER = this->LOWER << s;

        return INTERNAL;
    }

    // TODO: (s) provide general implementation, specify should never be called
    // from Fixed since * and / oeprators are custom in cpp spec (never call uint128_t * uint128_t, too slow!)

    // Multiplication (not used, avoid compilation error, overwritten in cpp)
    uint128_t operator * (const uint128_t& other) const
    {
        uint128_t INTERNAL;
        throw notImplementedException();
        return INTERNAL;
    }

    // Division (not used, avoid compilation error, overwritten in cpp)
    uint128_t operator / (const uint128_t& other) const
    {
        uint128_t INTERNAL;
        throw notImplementedException();
        return INTERNAL;
    }

    // conversions
    explicit constexpr operator int64_t() const
    {
        return this->LOWER;
    }

private:
    static constexpr unsigned long long MAX_ULL = std::numeric_limits<unsigned long long>::max();

    /**
     * @brief maybe better in c++ way with return std::pair?
     * 
     * @param first_in_upper 
     * @param first_in_lower 
     * @param second_in_upper 
     * @param second_in_lower 
     * @param out_upper 
     * @param out_lower 
     */
    inline void _add_and_carry_uint128_t(const uint64_t first_in_upper, const uint64_t first_in_lower,
                                    const uint64_t second_in_upper, const uint64_t second_in_lower,
                                    uint64_t * out_upper, uint64_t * out_lower) const
    {
        // decompose 64 bit variables into two 32-bit pair for first operand
        const uint32_t first_in_upper_hi = static_cast<uint32_t>(first_in_upper>>32);
        const uint32_t first_in_upper_low = static_cast<uint32_t>(first_in_upper);
        const uint32_t first_in_lower_hi = static_cast<uint32_t>(first_in_lower>>32); 
        const uint32_t first_in_lower_low = static_cast<uint32_t>(first_in_lower);

        // decompose 64 bit variables into two 32-bit pair for second operand
        const uint32_t second_in_upper_hi = static_cast<uint32_t>(second_in_upper>>32);
        const uint32_t second_in_upper_low = static_cast<uint32_t>(second_in_upper);
        const uint32_t second_in_lower_hi = static_cast<uint32_t>(second_in_lower>>32); 
        const uint32_t second_in_lower_low = static_cast<uint32_t>(second_in_lower);

        // decompose 64 bit variablee into two 32-bit pair for result
        uint32_t out_upper_hi = 0;
        uint32_t out_upper_low = 0;
        uint32_t out_lower_hi = 0;
        uint32_t out_lower_low = 0;

        /*iprintf("[(%lu, %lu) + (%lu, %lu)] + [(%lu, %lu) + (%lu, %lu)]\n", first_in_upper_hi, first_in_upper_low,
                                                                            second_in_upper_hi, second_in_upper_low,
                                                                            first_in_lower_hi, first_in_lower_low, 
                                                                            second_in_lower_hi, second_in_lower_low);*/

        // sum with carry // FIXME: (s) out_lower_hi = out_lower_low for some reason
        /*asm volatile("adds %[out_lower_low], %[first_in_lower_low], %[second_in_lower_low]\n\t"
                     "adcs %[out_lower_hi], %[first_in_lower_hi], %[second_in_lower_hi]\n\t"
                     "adcs %[out_upper_low], %[first_in_upper_low], %[second_in_upper_low]\n\t"
                     "adc  %[out_upper_hi], %[first_in_upper_hi], %[second_in_upper_hi]\n\t"
                        : [out_lower_low] "=r" (out_lower_low), [out_lower_hi] "=r" (out_lower_hi),
                            [out_upper_low] "=r" (out_upper_low), [out_upper_hi] "=r" (out_upper_hi)
                        : [first_in_lower_low] "r" (first_in_lower_low), [second_in_lower_low] "r" (second_in_lower_low),
                            [first_in_lower_hi] "r" (first_in_lower_hi), [second_in_lower_hi] "r" (second_in_lower_hi),
                            [first_in_upper_low] "r" (first_in_upper_low), [second_in_upper_low] "r" (second_in_upper_low),
                            [first_in_upper_hi] "r" (first_in_upper_hi), [second_in_upper_hi] "r" (second_in_upper_hi)
                        : "cc" //"r0"  // condition code flags are modified (NZCV)
                    );*/

        asm volatile("adds r0, %[first_in_lower_low], %[second_in_lower_low]\n\t"
                     "adcs r1, %[first_in_lower_hi],  %[second_in_lower_hi]\n\t"
                     "adcs r2, %[first_in_upper_low], %[second_in_upper_low]\n\t"
                     "adc  r3, %[first_in_upper_hi],  %[second_in_upper_hi]\n\t"
                     "mov  %[out_lower_low], r0\n\t"
                     "mov  %[out_lower_hi],  r1\n\t"
                     "mov  %[out_upper_low], r2\n\t"
                     "mov  %[out_upper_hi],  r3\n\t"
                        : [out_lower_low] "=r" (out_lower_low), [out_lower_hi] "=r" (out_lower_hi),
                            [out_upper_low] "=r" (out_upper_low), [out_upper_hi] "=r" (out_upper_hi)
                        : [first_in_lower_low] "r" (first_in_lower_low), [second_in_lower_low] "r" (second_in_lower_low),
                            [first_in_lower_hi] "r" (first_in_lower_hi), [second_in_lower_hi] "r" (second_in_lower_hi),
                            [first_in_upper_low] "r" (first_in_upper_low), [second_in_upper_low] "r" (second_in_upper_low),
                            [first_in_upper_hi] "r" (first_in_upper_hi), [second_in_upper_hi] "r" (second_in_upper_hi)
                        : "cc", "r0", "r1", "r2", "r3"  // condition code flags (NZCV)  and r[0-3] are modified 
                    );

        
        //iprintf("[(%lu, %lu)] + [(%lu, %lu)]\n", out_upper_hi, out_upper_low, out_lower_hi, out_lower_low);


        // composing result
        (*out_lower) = (static_cast<uint64_t>(out_lower_hi)<<32) | static_cast<uint64_t>(out_lower_low);
        (*out_upper) = (static_cast<uint64_t>(out_upper_hi)<<32) | static_cast<uint64_t>(out_upper_low);
    }
}; // struct uint128_t

//e.g. T = int32_t, T2 = int64_t
template<typename T, typename T2, size_t dp=sizeof(T)*4>
class Fixed
{
public:
    T value = T(0);

    constexpr Fixed() = default;
    constexpr Fixed(const Fixed& f) = default;

    explicit constexpr Fixed(const double d)
    { 
        value = static_cast<T>(d * static_cast<double>((dp >= 32 ? 1LL : 1) << dp) + (d >= 0 ? 0.5 : -0.5));
    }

    explicit constexpr operator double() const
    {
        return static_cast<double>(value) / static_cast<double>((dp >= 32 ? 1LL : 1) << dp);
    }

    explicit constexpr operator long long() const
    {
        return static_cast<long long>(value>>dp);
    }

    // parenthesis
    constexpr Fixed operator () (const double& d)
    {
        this->value = Fixed(d).value;
        return *this;
    }

    // Assign
    constexpr Fixed& operator = (const Fixed& f) = default;

    constexpr Fixed& operator = (double d)
    {
        this->value = Fixed(d).value;
        return *this;
    }

    constexpr Fixed& operator = (int32_t a)
    {
        this->value = a<<dp;
        return *this;
    }

    // Negation
    constexpr Fixed operator - () const
    {
        return form(-this->value);
    }

    // Addition
    constexpr Fixed operator + (const Fixed& f) const
    {
        return form(this->value + f.value);
    }

    constexpr Fixed operator + (long long a) const
    {
        return form(this->value + (a<<dp));
    }

    friend Fixed operator + (long long a, const Fixed& other)
    {
        return other + a;
    }

    constexpr Fixed& operator += (const Fixed& f)
    {
        this->value += f.value;
        return *this;
    }

    // Subtraction
    constexpr Fixed operator - (const Fixed& f) const
    {
        return form(this->value - f.value);
    }

    constexpr Fixed operator - (long long a) const
    {
        return form(this->value - (a<<dp));
    }

    friend Fixed operator - (long long a, const Fixed& other)
    {
        return a + (-other);
    }

    constexpr Fixed& operator -= (const Fixed& f)
    {
        this->value -= f.value;
        return *this;
    }

    // Multiplication
    constexpr Fixed operator * (const Fixed& f) const
    {
        // if constexpr form c++17, SFINAE used

        // default naive multiplication
        return form(static_cast<T>((static_cast<T2>(this->value) * static_cast<T2>(f.value)) >> dp));

    }

    long long operator * (long long a) const
    {
        // if constexpr form c++17, SFINAE used

        return form(static_cast<T>((static_cast<T2>(this->value) * static_cast<T2>(a<<dp)) >> dp));
    }
    long long operator * (unsigned long long a) const
    {
        // if constexpr form c++17, SFINAE used

        return form(static_cast<T>((static_cast<T2>(this->value) * static_cast<T2>(a<<dp)) >> dp));
    }
    friend long long operator * (long long a, const Fixed& other)
    {
        return other * a;
    }
    
    constexpr Fixed& operator *= (const Fixed& f)
    {
        // if constexpr form c++17, SFINAE used
        
        this->value = static_cast<T>((static_cast<T2>(this->value) * static_cast<T2>(f.value)) >> dp);
        return *this;
    }

    // Division
    constexpr Fixed operator / (const Fixed& f) const
    {
        // if constexpr form c++17, SFINAE used

        return form(static_cast<T>((static_cast<T2>(this->value) << dp) / static_cast<T2>(f.value)));
    }

    constexpr Fixed& operator /= (const Fixed& f)
    {
        // if constexpr form c++17, SFINAE used
        
        this->value = static_cast<T>((static_cast<T2>(this->value) << dp) / static_cast<T2>(f.value));
    }

    friend long long operator / (long long a, const Fixed& other)
    {
        return other.fastInverse() * a;
    }

    // inverse operator
    Fixed fastInverse() const
    {        
        Fixed f;
        throw notImplementedException();
        return f;
    }

    // comparison operator
    constexpr bool operator ==  (const Fixed& f)  { return this->value == f.value; }
    constexpr bool operator ==  (double other)    { return (*this) == Fixed(other); }
    
    constexpr bool operator !=  (const Fixed& f)  { return this->value != f.value; }
    constexpr bool operator !=  (double other)    { return (*this) != Fixed(other); }

    constexpr bool operator <   (const Fixed& f)  { return this->value < f.value; }
    constexpr bool operator <   (double other)    { return (*this) < Fixed(other); }

    constexpr bool operator <=  (const Fixed& f)  { return this->value <= f.value; }
    constexpr bool operator <=  (double other)    { return (*this) <= Fixed(other); }

    constexpr bool operator >   (const Fixed& f)  { return this->value > f.value; }
    constexpr bool operator >   (double other)    { return (*this) > Fixed(other); }

    constexpr bool operator >=  (const Fixed& f)  { return this->value >= f.value; }
    constexpr bool operator >=  (double other)    { return (*this) >= Fixed(other); }


private:
    static constexpr Fixed form(T v) { Fixed k; k.value = v; return k; }

    template <typename T3>
    static constexpr short signum(T x) {
        return (x > 0) - (x < 0);
    }
}; // class Fixed


///
// Specializations for 32.32 fixed point
///

// using compl2 for sign. Max integer part = 2^(32-1)-1 = 2147483647
using fp32_32 = Fixed<int64_t, uint128_t, 32UL>;

fp32_32 constexpr operator "" _fp32d32 (const long double d)
{
    return fp32_32(d);
}

template<>
inline fp32_32 fp32_32::operator * (const Fixed& f) const
{
    // integer and decimal decomposition
    int64_t thisAbsVal = std::abs(this->value);
    int32_t thisAbsDecimalVal = static_cast<int32_t>(thisAbsVal & 0x00000000FFFFFFFF);
    int32_t thisAbsIntegerVal = static_cast<int32_t>((thisAbsVal & 0xFFFFFFFF00000000) >> 32);
    
    int64_t otherAbsVal = std::abs(f.value);
    int32_t otherAbsDecimalVal = static_cast<int32_t>(otherAbsVal & 0x00000000FFFFFFFF);
    int32_t otherAbsIntegerVal = static_cast<int32_t>((otherAbsVal & 0xFFFFFFFF00000000) >> 32);

    // distributive property of multiplication is applied
    uint128_t res;
    res += static_cast<uint128_t>(mul32x32to64(thisAbsIntegerVal, otherAbsIntegerVal))<<64; // pure integer
    res += static_cast<uint128_t>(mul32x32to64(thisAbsIntegerVal, otherAbsDecimalVal))<<32; // integer.decimal
    res += static_cast<uint128_t>(mul32x32to64(thisAbsDecimalVal, otherAbsIntegerVal))<<32; // integer.decimal
    res += static_cast<uint128_t>(mul32x32to64(thisAbsDecimalVal, otherAbsDecimalVal));     // pure decimal
    
    short sign = signum<int64_t>(this->value) * signum<int64_t>(f.value);
    return form(sign * (static_cast<int64_t>(res >> 32)));
}

template<>
inline fp32_32& fp32_32::operator *= (const Fixed& f)
{
    this->value = ((*this) * f).value;
    return *this;
}

template<>
inline long long fp32_32::operator * (long long a) const
{
    int64_t absVal = std::abs(this->value);
    int32_t decimalVal = static_cast<int32_t>(absVal & 0x00000000FFFFFFFF);
    int32_t integerVal = static_cast<int32_t>((absVal & 0xFFFFFFFF00000000) >> 32);
    
    return signum<long long>(a) * signum<int32_t>(this->value) * static_cast<long long>(mul64x32d32(std::abs(a), integerVal, decimalVal));
}

// highly optimised function since it's used by the getTime()
template<>
inline long long fp32_32::operator * (unsigned long long a) const
{
    int64_t absVal = std::abs(this->value);
    int32_t decimalVal = static_cast<int32_t>(absVal & 0x00000000FFFFFFFF);
    int32_t integerVal = static_cast<int32_t>((absVal & 0xFFFFFFFF00000000) >> 32);
    
    return this->value > 0 ? mul64x32d32(a, integerVal, decimalVal) : -mul64x32d32(a, integerVal, decimalVal);
}

// up until this line, the warning is active

// https://www.fluentcpp.com/2019/08/30/how-to-disable-a-warning-in-cpp/
// PUSH disable warning (instruction specific to the compiler, see below)
#pragma GCC diagnostic push
// DISABLE the warning that a parameter is not used
#pragma GCC diagnostic ignored "-Wstrict-aliasing"

template<>
inline fp32_32 fp32_32::fastInverse() const
{    
    // Modified fast Inverse Square Root from Quake 3 Arena (not written by John Carmack) 
    // https://blog.timhutt.co.uk/fast-inverse-square-root/
    // https://whoisslimshady.medium.com/quake-iiis-or-fast-invsqrt-or-0x5f3759df-algorithm-47e6bf5bfa35
    // https://stackoverflow.com/questions/11644441/fast-inverse-square-root-on-x64
    
    double number = static_cast<double>(*this);
    double absNumber = std::abs(number);
    long long i;
    double x2, y;
    //const double threehalfs = 1.5;

    x2 = absNumber; // * 0.5;
    y  = absNumber;
    i  = * ( long long * ) &y;               
    i  = 0x7FE0000000000000 - i; // TODO: (s) optimize with something less
    y  = * ( double * ) &i;
    

    // f(y) = 1/(y^2) - x
    // y is our estimated inverse square root, and we’re trying to get a better estimate. 
    // To do that we want to get f(y) close to zero, which happens when y is correct and x equals the number that we’re taking the inverse-square-root of, 
    // ie the input named ‘number’. The x from the above formula isn’t being updated, we know it exactly already. 

    // The derivative of f(y) is -2/(y^3). This is because the x is constant, 1/y^2 is the same as y^-2, so multiply by -2 and subtract 1 from the exponent. 
    // ynew = y - f(y)/f’(y), which after we enter the function and its derivative is 
    // ynew = y - (1/(y^2) -x)/(-2/(y^3))
    // = y + y/2 - x*(y^3)/2
    // = y * (3/2 - y * y * x/2)
    //y  = y * ( threehalfs - ( x2 * y * y ) );   // 1st iteration

    // f(y) = 1/y - x, f(y) = 0 => y = 1/x
    // TODO: (s) change explaination
    y  = y * ( 2 - (y * x2) );   // 1st iteration
    y  = y * ( 2 - (y * x2) );   // 2nd iteration
    y  = y * ( 2 - (y * x2) );   // 3rd iteration

    return fp32_32(signum<int64_t>(this->value) * y);
}

// POP disable warning, the warning is now active again
#pragma GCC diagnostic pop

template<>
inline fp32_32 fp32_32::operator / (const fp32_32& f) const
{
    return (*this) * f.fastInverse();
}

// end of specializations for 32.32 fixed point


///
// Specializations for 16.16 fixed point
///

// using compl2 for sign. Max integer part = 2^(16-1)-1 = 32767
using fp16_16 = Fixed<int32_t, int64_t, 16UL>;

fp16_16 constexpr operator "" _fp16d16 (const long double d)
{
    return fp16_16(d);
}

template<>
inline fp16_16 fp16_16::operator * (const fp16_16& f) const
{
    long long res = signum<int32_t>(this->value) * signum<int32_t>(f.value) * static_cast<long long>(mul32x32to64(std::abs(this->value), std::abs(f.value)));
    return form(res >> 16);
}

template<>
inline long long fp16_16::operator * (long long a) const
{
    // mul64x32d32 is expecting a 32.32 fixed point number. Therefore, we convert the decimal part
    // from 2^16 to 2^32 by shifting left by 16 bit with inevitable loss of precision
    int32_t decimalVal = static_cast<int32_t>(std::abs(this->value) & 0x0000FFFF) << 16;
    int32_t integerVal = static_cast<int32_t>((std::abs(this->value) & 0xFFFF0000) >> 16);
    
    return signum<long long>(a) * signum<int32_t>(this->value) * mul64x32d32(a, integerVal, decimalVal);
}

template<>
inline /*constexpr*/ fp16_16& fp16_16::operator *= (const fp16_16& f)
{
    this->value = (signum<int32_t>(this->value) * signum<int32_t>(f.value) * mul32x32to64(std::abs(this->value), std::abs(f.value))) >> 16;
    return *this;
}

// end of specializations for 16.16 fixed point

} // namespace miosix



#endif