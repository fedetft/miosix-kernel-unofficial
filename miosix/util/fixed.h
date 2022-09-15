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
#include <type_traits> // std::is_same

namespace miosix
{

class notImplementedException : public std::logic_error
{
public:
    notImplementedException() : std::logic_error("Function not yet implemented") { };
};

/* This class needs A LOT of re-work. Most of operations are just unefficients */
// class fp32_32
// {
// public:
//     // Default constructor
//     fp32_32();
//     /**
//      * @brief 
//      * 
//      */
//     explicit fp32_32(const double d);

//     // Copy constructor
//     /**
//      * @brief Construct a new fp32 32 object
//      * 
//      * @param other 
//      */
//     fp32_32(const fp32_32& other);

//     // functor
//     /**
//      * @brief 
//      * 
//      * @param other 
//      * @return fp32_32 
//      */
//     fp32_32 operator () (const fp32_32& other);

//     /**
//      * @brief 
//      * 
//      * @param d 
//      * @return fp32_32 
//      */
//     fp32_32 operator () (double d);

//     // Assign
//     /**
//      * @brief 
//      * 
//      * @param f 
//      * @return fp32_32& 
//      */
//     fp32_32& operator = (const fp32_32& f);

//     // Negation
//     /**
//      * @brief 
//      * 
//      * @return fp32_32 
//      */
//     fp32_32 operator - () const &;

//     // Addition
//     /**
//      * @brief 
//      * 
//      * @param other 
//      * @return fp32_32 
//      */
//     // TODO: (s) check with -O3, sometimes it is wrong for aggressive compilation optimization...
//     fp32_32 operator + (const fp32_32& other) const;
//     fp32_32 operator + (long long a) const;

//     // Subtraction
//     /**
//      * @brief 
//      * 
//      * @param other 
//      * @return fp32_32 
//      */
//     fp32_32 operator - (const fp32_32& other) const;
//     fp32_32 operator - (long long a) const;
//     friend fp32_32 operator - (long long a, const fp32_32& other); 

//     // Multiplication
//     // TODO: (s) can avoid using long long and shift but losing precision!
//     /**
//      * @brief fp32_32 * long long
//      * 
//      * @param a 
//      * @return long long 
//      */
//     long long operator * (long long a) const;
//     // should be a free function, little abuse of the friend keyword but those
//     // are common and recommended for operator overloading
//     // long long * fp32_32
//     friend long long operator * (long long a, const fp32_32& other);
    
//     /**
//      * @brief 
//      * 
//      * @param other 
//      * @return fp32_32 
//      */
//     fp32_32 operator * (const fp32_32& other) const;

//     // Division
//     /**
//      * @brief 
//      * 
//      * @param other 
//      * @return fp32_32 
//      */
//     fp32_32 operator / (const fp32_32& other) const;

//     /**
//      * @brief 
//      * 
//      * @param a 
//      * @return fp32_32 
//      */
//     fp32_32 operator / (long long a) const;
//     // should be a free function, little abuse of the friend keyword but those
//     // are common and recommended for operator overloading
//     // long long / fp32_32 => long long * (fp32_32)^-1
//     friend long long operator / (long long a, const fp32_32& other);

//     // reciprocal operator
//     void reciprocal();

//     // fixed2double cast
//     /**
//      * @brief 
//      * 
//      * @return double 
//      */
//     /*operator double() const;*/

//     /**
//      * @brief Get the Double object
//      * 
//      * @return double 
//      */
//     double getDouble() const;

//     /**
//      * @brief 
//      * 
//      * @return double 
//      */
//     double getRDouble() const;

//     ///
//     // getters and setters (getters for future extendibility)
//     ///
//     unsigned int getSign() const;
//     void setSign(short sign);

//     unsigned int getIntegerPart() const;
//     unsigned int getRIntegerPart() const;
//     void setIntegerPart(unsigned int integerPart);

//     unsigned int getDecimalPart() const;
//     unsigned int getRDecimalPart() const;
//     void setDecimalPart(double decimalPart);
//     void setDecimalPart(unsigned int decimalPart);

// private:
//     ///
//     // Helper functions
//     ///
    
//     /**
//      * @brief 
//      * 
//      * @tparam T 
//      * @param x 
//      * @return T 
//      */
//     template <typename T>
//     static constexpr short signum(T x) {
//         return (x > 0) - (x < 0);
//     }

//     ///
//     // Class variables
//     ///

//     short sign = 1;
//     unsigned int integerPart = 0;
//     unsigned int decimalPart = 0; // 2^32
//     unsigned int rIntegerPart = 0; // reciprocal integer part
//     unsigned int rDecimalPart = 0; // reciprocal decimal part
// };

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
    constexpr uint128_t operator + (const uint128_t& other) const
    {
        uint128_t INTERNAL;

        // assume no overflow on upper part
        INTERNAL.UPPER = this->UPPER + other.UPPER;

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

        return INTERNAL;
    }

    constexpr uint128_t& operator += (const uint128_t& other)
    {
        // assume no overflow on upper part
        this->UPPER += other.UPPER;

        // sum lower part by predicting overflow a + b > max => b > max - a
        if(this->LOWER > MAX_ULL - other.LOWER)
        {
            // overflow
            this->UPPER += 1;
            
            // sum lower reamin without overflow
            this->LOWER = (MAX_ULL - ((MAX_ULL - this->LOWER) + (MAX_ULL - other.LOWER))) - 1;
        }
        else this->LOWER += other.LOWER;

        return *this;
    }

    constexpr uint128_t operator + (unsigned long long a) const
    {
        uint128_t INTERNAL;

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

        return INTERNAL;
    }

    constexpr uint128_t& operator += (unsigned long long a)
    {
        // sum lower part by predicting overflow a + b > max => b > max - a
        if(this->LOWER > MAX_ULL - a)
        {
            // overflow
            this->UPPER += 1;
            
            // sum lower reamin without overflow
            this->LOWER = (MAX_ULL - ((MAX_ULL - this->LOWER) + (MAX_ULL - a))) - 1;
        }
        else this->LOWER += a;

        return *this;
    }

    // Bit shift
    constexpr uint128_t operator >> (const size_t s) const
    {
        uint128_t INTERNAL;

        // right shift lower part
        INTERNAL.LOWER = this->LOWER >> s;

        // sum remain from upper shifting
        INTERNAL.LOWER |= (this->UPPER & ((1LLU<<s)-1))<<(64-s);

        // right shift upper part
        INTERNAL.UPPER = this->UPPER >> s;

        return INTERNAL;
    }

    // FIXME: (s) when s = 32, not reporting integer part!
    constexpr uint128_t operator << (const size_t s) const
    {
        uint128_t INTERNAL;

        // left shift upper part
        INTERNAL.UPPER = this->UPPER << s;

        // sum remain from lower shifting
        INTERNAL.UPPER |= (this->LOWER & ~((1LLU<<(64-s))-1))>>(64-s);

        // left shift upper part
        INTERNAL.LOWER = this->LOWER << s;

        return INTERNAL;
    }

    // Multiplication (not used, avoid compilation error)
    uint128_t operator * (const uint128_t& other) const
    {
        uint128_t INTERNAL;
        throw notImplementedException();
        return INTERNAL;
    }

    // conversions
    constexpr operator int64_t() const
    {
        return this->LOWER;
    }

private:
    static constexpr unsigned long long MAX_ULL = std::numeric_limits<unsigned long long>::max();
};

//e.g. T = int32_t, T2 = int64_t
template<typename T, typename T2, size_t dp=sizeof(T)*4>
class fixed
{

public:
    T value = T(0);

    constexpr fixed() = default;
    constexpr fixed(const fixed& f) = default;

    constexpr fixed(const double d)
    { 
        value = static_cast<T>(d * static_cast<double>((dp >= 32 ? 1LL : 1) << dp) + (d >= 0 ? 0.5 : -0.5));
    }

    constexpr operator double() const
    {
        return static_cast<double>(value) / static_cast<double>((dp >= 32 ? 1LL : 1) << dp);
    }

    // parenthesis
    constexpr fixed operator () (const double& d)
    {
        this->value = fixed(d).value;
        return *this;
    }

    // Assign
    constexpr fixed& operator = (const fixed& f) = default;

    // Negation
    constexpr fixed operator - () const
    {
        return form(-this->value);
    }

    // Addition
    constexpr fixed operator + (const fixed& f) const
    {
        return form(this->value + f.value);
    }

    constexpr fixed& operator += (const fixed& f)
    {
        this->value += f.value;
        return *this;
    }

    // Subtraction
    constexpr fixed operator - (const fixed& f) const
    {
        return form(this->value - f.value);
    }

    constexpr fixed& operator -= (const fixed& f)
    {
        this->value -= f.value;
        return *this;
    }

    // Multiplication
    constexpr fixed operator * (const fixed& f) const
    {
        // if constexpr form c++17, SFINAE used

        // default naive multiplication
        return form((static_cast<T2>(this->value) * static_cast<T2>(f.value)) >> dp);

    }

    long long operator * (long long a) const
    {
        // if constexpr form c++17, SFINAE used

        throw notImplementedException();
        return 0; //form((static_cast<T2>(this->value) * static_cast<T2>(f.value)) >> dp);
    }

    friend long long operator * (long long a, const fixed& other)
    {
        return other * a;
    }
    
    constexpr fixed& operator *= (const fixed& f)
    {
        // if constexpr form c++17, SFINAE used
        
        this->value = (static_cast<T2>(this->value) * static_cast<T2>(f.value)) >> dp;
        return *this;
    }

    // Division
    constexpr fixed operator / (const fixed& f) const
    {
        return form((static_cast<T2>(this->value) << dp) / static_cast<T2>(f.value));
    }

    constexpr fixed& operator /= (const fixed& f)
    {
        this->value = (static_cast<T2>(this->value) << dp) / static_cast<T2>(f.value);
    }

private:
    static constexpr fixed form(T v) { fixed k; k.value = v; return k; }

    template <typename T3>
    static constexpr short signum(T x) {
        return (x > 0) - (x < 0);
    }

};

///
// Special specializations for 32.32 fixed point
///

// using compl2 for sign. Max integer part = 2^(32-1)-1 = 2147483647
using fp32_32 = fixed<int64_t, uint128_t, 32UL>;

template<>
fp32_32 fp32_32::operator * (const fixed& f) const
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

///
// Special specializations for 16.16 fixed point
///

// using compl2 for sign. Max integer part = 2^(16-1)-1 = 32767
using fp16_16 = fixed<int32_t, int64_t, 16UL>;

template<>
fp16_16 fp16_16::operator * (const fixed& f) const
{
    long long res = signum<int32_t>(this->value) * signum<int32_t>(f.value) * static_cast<long long>(mul32x32to64(std::abs(this->value), std::abs(f.value)));
    return form(res >> 16);
}

template<>
long long fp16_16::operator * (long long a) const
{
    // mul64x32d32 is expecting a 32.32 fixed point number. Therefore, we convert the decimal part
    // from 2^16 to 2^32 by shifting left by 16 bit with inevitable loss of precision
    int32_t decimalVal = static_cast<int32_t>(std::abs(this->value) & 0x0000FFFF) << 16;
    int32_t integerVal = static_cast<int32_t>((std::abs(this->value) & 0xFFFF0000) >> 16);
    
    return signum<long long>(a) * signum<int32_t>(this->value) * mul64x32d32(a, integerVal, decimalVal);
}

template<>
/*constexpr*/ fp16_16& fp16_16::operator *= (const fixed& f)
{
    this->value = (signum<int32_t>(this->value) * signum<int32_t>(f.value) * mul32x32to64(std::abs(this->value), std::abs(f.value))) >> 16;
    return *this;
}

}

#endif