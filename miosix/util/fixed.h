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

/* This class needs A LOT of re-work. Most of operations are just unefficients */
class fp32_32
{
public:
    // Default constructor
    fp32_32();
    /**
     * @brief 
     * 
     */
    explicit fp32_32(const double d);

    // Copy constructor
    /**
     * @brief Construct a new fp32 32 object
     * 
     * @param other 
     */
    fp32_32(const fp32_32& other);

    // functor
    /**
     * @brief 
     * 
     * @param other 
     * @return fp32_32 
     */
    fp32_32 operator () (const fp32_32& other);

    /**
     * @brief 
     * 
     * @param d 
     * @return fp32_32 
     */
    fp32_32 operator () (double d);

    // Assign
    /**
     * @brief 
     * 
     * @param f 
     * @return fp32_32& 
     */
    fp32_32& operator = (const fp32_32& f);

    // Negation
    /**
     * @brief 
     * 
     * @return fp32_32 
     */
    fp32_32 operator - () const &;

    // Addition
    /**
     * @brief 
     * 
     * @param other 
     * @return fp32_32 
     */
    // TODO: (s) check with -O3, sometimes it is wrong for aggressive compilation optimization...
    fp32_32 operator + (const fp32_32& other) const;
    fp32_32 operator + (long long a) const;

    // Subtraction
    /**
     * @brief 
     * 
     * @param other 
     * @return fp32_32 
     */
    fp32_32 operator - (const fp32_32& other) const;
    fp32_32 operator - (long long a) const;
    friend fp32_32 operator - (long long a, const fp32_32& other); 

    // Multiplication
    // TODO: (s) can avoid using long long and shift but losing precision!
    /**
     * @brief fp32_32 * long long
     * 
     * @param a 
     * @return long long 
     */
    long long operator * (long long a) const;
    // should be a free function, little abuse of the friend keyword but those
    // are common and recommended for operator overloading
    // long long * fp32_32
    friend long long operator * (long long a, const fp32_32& other);
    
    /**
     * @brief 
     * 
     * @param other 
     * @return fp32_32 
     */
    fp32_32 operator * (const fp32_32& other) const;

    // Division
    /**
     * @brief 
     * 
     * @param other 
     * @return fp32_32 
     */
    fp32_32 operator / (const fp32_32& other) const;

    /**
     * @brief 
     * 
     * @param a 
     * @return fp32_32 
     */
    fp32_32 operator / (long long a) const;
    // should be a free function, little abuse of the friend keyword but those
    // are common and recommended for operator overloading
    // long long / fp32_32 => long long * (fp32_32)^-1
    friend long long operator / (long long a, const fp32_32& other);

    // reciprocal operator
    void reciprocal();

    // fixed2double cast
    /**
     * @brief 
     * 
     * @return double 
     */
    /*operator double() const;*/

    /**
     * @brief Get the Double object
     * 
     * @return double 
     */
    double getDouble() const;

    /**
     * @brief 
     * 
     * @return double 
     */
    double getRDouble() const;

    ///
    // getters and setters (getters for future extendibility)
    ///
    unsigned int getSign() const;
    void setSign(short sign);

    unsigned int getIntegerPart() const;
    unsigned int getRIntegerPart() const;
    void setIntegerPart(unsigned int integerPart);

    unsigned int getDecimalPart() const;
    unsigned int getRDecimalPart() const;
    void setDecimalPart(double decimalPart);
    void setDecimalPart(unsigned int decimalPart);

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
    static constexpr short signum(T x) {
        return (x > 0) - (x < 0);
    }

    ///
    // Class variables
    ///

    short sign = 1;
    unsigned int integerPart = 0;
    unsigned int decimalPart = 0; // 2^32
    unsigned int rIntegerPart = 0; // reciprocal integer part
    unsigned int rDecimalPart = 0; // reciprocal decimal part
};

// TODO: (s) use mul + sum and mul per long long...
// e.g. T = int32_t, T2 = int64_t
template<typename T, typename T2, size_t dp>
class fixed
{

public:
    T value = T(0);

    constexpr fixed() = default;
    constexpr fixed(const fixed& f) = default;

    constexpr fixed(const double d)
    { 
        value = static_cast<T>(d * static_cast<double>(1 << dp) + (d >= 0 ? 0.5 : -0.5));
    }

    constexpr operator double() const
    {
        return static_cast<double>(value) / static_cast<double>(1 << dp);
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
    /*constexpr*/ fixed operator * (const fixed& f) const
    {
        // if constexpr form c++17, TODO: (s) change to SFINAE
        if (std::is_same<T, int32_t>::value && std::is_same<T2, int64_t>::value)
            return  form(signum<int32_t>(this->value) * signum<int32_t>(f.value) * mul32x32to64(std::abs(this->value), std::abs(f.value)) >> dp);

        return form((static_cast<T2>(this->value) * static_cast<T2>(f.value)) >> dp);
    }
    
    /*constexpr*/ fixed& operator *= (const fixed& f)
    {
        // if constexpr form c++17, TODO: (s) change to SFINAE
        if (std::is_same<T, int32_t>::value && std::is_same<T2, int64_t>::value)
            this->value = (signum<int32_t>(this->value) * signum<int32_t>(f.value) * mul32x32to64(std::abs(this->value), std::abs(f.value))) >> dp;
        else
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

// using compl2 for sign. Max integer part = 2^(16-1) = 32768
using fixed_16_16 = fixed<int32_t, int64_t, 16>;


}

#endif