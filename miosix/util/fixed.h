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

#ifndef FIXED_H
#define FIXED_H

#include <cstdint> // int32_t, int64_t, ...
#include <cstddef> // size_t
#include "time/timeconversion.h" // mul64x32d32
#include <cmath> // abs
#include "miosix.h"

namespace miosix
{

// TODO: (s) move into cpp for multiple imports + generalize
// TODO: (s) replace multiplication by 2^32 with shift
class fp32_32
{
public:
    // Default constructor
    constexpr fp32_32() = default;
    /**
     * @brief 
     * 
     */
    constexpr fp32_32(const double d)
    { 
        // extracting integer part (e.g. 456.45235 => 456 + (0.45235 / 2^32) )
        this->sign = signum<double>(d);
        
        double absD = std::abs(d); // just flipping sign bit in double, fast
        this->integerPart = static_cast<unsigned int>(absD);
        this->decimalPart = static_cast<unsigned int>(4294967296 * (absD - integerPart));
    }

    // Copy constructor
    /**
     * @brief Construct a new fp32 32 object
     * 
     * @param other 
     */
    inline fp32_32(const fp32_32& other)
    {
        this->integerPart = other.integerPart;
        this->decimalPart = other.decimalPart;
        this->sign = other.sign;
    }

    // functor
    /**
     * @brief 
     * 
     * @param other 
     * @return fp32_32 
     */
    inline fp32_32 operator () (const fp32_32& other) {
        this->integerPart = other.integerPart;
        this->decimalPart = other.decimalPart;
        this->sign = other.sign;
        return *this;
    }

    // Assign
    /**
     * @brief 
     * 
     * @param f 
     * @return fp32_32& 
     */
    inline fp32_32& operator = (const fp32_32& f) = default;

    // Negation
    /**
     * @brief 
     * 
     * @return fp32_32 
     */
    inline fp32_32 operator - () const & // lvalue ref-qualifier, -a
    {
        fp32_32 fp(*this);
        fp.sign = -this->sign;
        return fp;
    }

    // Addition
    /**
     * @brief 
     * 
     * @param other 
     * @return fp32_32 
     */
    // TODO: (s) check with -O3 or -O2 sometimes it is wrong...
    inline fp32_32 operator + (const fp32_32& other) const
    {
        // if you're reading this, god help you.
        fp32_32 fp;
        
        // integer and decimal signed operations
        long long decimalPart = ( this->sign * static_cast<long long>(this->decimalPart) ) + ( other.sign * static_cast<long long>(other.decimalPart) );
        long long integerPart = ( this->sign * static_cast<long long>(this->integerPart) ) + ( other.sign * static_cast<long long>(other.integerPart) );

        // global operation sign
        fp.sign = 1;
        if(integerPart < 0) { fp.sign = -1; };
        if(integerPart == 0 && decimalPart < 0) { fp.sign = -1; };

        // calculate remain to report
        long long decimalSum = static_cast<long long>(this->decimalPart) + static_cast<long long>(other.decimalPart);
        long long decimalRemain = std::abs(decimalSum)>>32;
        short decimalReportSign = signum(decimalPart) * fp.sign; // decimal remain sign
        integerPart = std::abs(integerPart) + decimalReportSign * decimalRemain; // decimal overflow
        decimalPart = (decimalRemain * (1LL<<32)) + decimalReportSign * (std::abs(decimalPart) % (1LL<<32));  // decimal remain

        fp.integerPart = static_cast<unsigned int>(integerPart);
        fp.decimalPart = static_cast<unsigned int>(decimalPart);
                
        return fp;
    }

    // Subtraction
    /**
     * @brief 
     * 
     * @param other 
     * @return fp32_32 
     */
    inline fp32_32 operator - (const fp32_32& other) const
    {
        fp32_32 fp ( (*this) + (-other) );
        return fp;
    }

    // Multiplication
    /**
     * @brief 
     * 
     * @param a 
     * @return long long 
     */
    inline long long operator * (long long a) const
    {
        // calculating sing and saving clock cycles if a is positive (no absolute value)
        // TODO: (s) which one is worse? multiplication or wrong branch prediction (both around 0.2us)
        return ( this->sign * signum<long long>(a) ) * mul64x32d32(std::abs(a), integerPart, decimalPart);
        //return this->sign * signum<long long>(a) * mul64x32d32(signum<long long>(a) * a, integerPart, decimalPart);
    }
    
    /**
     * @brief 
     * 
     * @param other 
     * @return fp32_32 
     */
    inline fp32_32 operator * (const fp32_32& other) const
    {
        fp32_32 fp;
        
        // e.g. 55.3 * 55.7 = 3080.21
        // integer part: 55 * 55 + 55 * 0.3 + 55 * 0.7 = 3080
        // decimal part: 0.3 * 0.7 = 0.21

        // TODO: (s) solve rounding problem in first 32 bits
        long long integerPart = mul32x32to64(this->integerPart, other.integerPart);
        integerPart += mul32x32to64(this->integerPart, other.decimalPart)>>32; // div 2^32
        integerPart += mul32x32to64(other.integerPart, this->decimalPart)>>32; // div 2^32

        long long decimalPart = mul32x32to64(this->decimalPart, other.decimalPart)>>32;  // div 2^32

        fp.integerPart = static_cast<unsigned int>(integerPart);
        fp.decimalPart = static_cast<unsigned int>(decimalPart);
        fp.sign = this->sign * other.sign;

        return fp;
    }

    /**
     * @brief 
     * 
     * @return double 
     */
    inline operator double() const
    {
        return this->sign * ( this->integerPart + (static_cast<double>(this->decimalPart) / 4294967296L) );
    }

    ///
    // getters and setters (getters for future extendibility)
    ///
    inline unsigned int getSign()                        { return this->sign; }
    inline void setSign(short sign)                      { this->sign = signum(sign); }

    inline unsigned int getIntegerPart()                 { return integerPart; }
    inline void setIntegerPart(unsigned int integerPart) { this->integerPart = integerPart; }

    inline unsigned int getDecimalPart()                 { return integerPart; }
    inline void setDecimalPart(double decimalPart)       { this->decimalPart = static_cast<unsigned int>(decimalPart * 4294967296); }
    inline void setDecimalPart(unsigned int decimalPart) { this->decimalPart = decimalPart; }

private:
    ///
    // Helper functions
    ///
    
    /**
     * @brief 
     * 
     * @tparam T 
     * @param x 
     * @return T 
     */
    template <typename T>
    constexpr short signum(T x) const {
        return (x > 0) - (x < 0);
    }

    ///
    // Class variables
    ///

    short sign = 1;
    unsigned int integerPart = 0;
    unsigned int decimalPart = 0; // 2^32
    //TODO: (s) save also inverted values for division
};

// TODO: (s) develop idea...
/*
// e.g. T = int32_t, T2 = int64_t
template<typename T, typename T2, size_t dp>
class fixed
{

public:
    T value = T(0);

    constexpr fixed() = default;
    constexpr fixed(const double d)
    { 
        value = static_cast<T>(d * static_cast<double>(1 << dp) + (d >= 0 ? 0.5 : -0.5));
    }

    constexpr operator double() const
    {
        return static_cast<double>(value) / static_cast<double>(1 << dp);
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

    constexpr fixed& operator += (const fixed& f) const
    {
        this->value += f.value;
        return *this;
    }

    // Subtraction
    constexpr fixed operator - (const fixed& f) const
    {
        return form(this->value - f.value);
    }

    constexpr fixed& operator -= (const fixed& f) const
    {
        this->value -= f.value;
        return *this;
    }

    // Multiplication
    // TODO: (s) banchmark with logic analyzer
    constexpr fixed operator * (const fixed& f) const
    {
        return form((static_cast<T2>(this->value) * static_cast<T2>(f.value)) >> dp);
    }
    
    constexpr fixed& operator *= (const fixed& f) const
    {
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

};*/

// example

/*using fp16_16 = fixed<int32_t, int64_t, 16>;
constexpr fp16_16 a(5.6);
constexpr fp16_16 a(2.7);
constexpr double z = static_cast<double>(a + b);
*/

}

#endif