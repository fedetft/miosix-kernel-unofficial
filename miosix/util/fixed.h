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

namespace miosix
{

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

};

// example
/*
using fp16_16 = fixed<int32_t, int64_t, 16>;
constexpr fp16_16 a(5.6);
constexpr fp16_16 a(2.7);
constexpr double z = static_cast<double>(a + b);
*/

}

#endif