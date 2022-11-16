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

#include <stdexcept> // division by zero

namespace miosix
{


class Fraction
{
public:
    long long numerator;
    long long denominator;

    /* Constructors */
    explicit constexpr Fraction() : numerator(1), denominator(1) {}
    explicit constexpr Fraction(long long a, long long b) : numerator(a), denominator(b)
    {
        if(b == 0) throw std::runtime_error("Math error: division by zero");
        if(b < 0) { a = -a; b = -b; }
    }
    explicit constexpr Fraction(long long a) : numerator(a), denominator(1) {}
    explicit constexpr Fraction(double d) : numerator(1), denominator(1) {} // TODO: (s)

    // Fraction -> double
    constexpr operator double() const
    {
        return static_cast<double>(numerator) / static_cast<double>(denominator);
    }

    // Fraction -> long long
    constexpr operator long long() const
    {
        // returning rounded integer division to closest integer
        return ( numerator + ( numerator > 0 ? (denominator - 1) : -(denominator - 1) ) ) / denominator;
    }

    // long long -> Fraction
    constexpr Fraction operator () (const long long a)
    {
        numerator = a;
        denominator = 1;
        return *this;
    }

    // double -> Fraction
    constexpr Fraction operator () (const double a)
    {
        // TODO: (s)
        return *this;
    }

    // Assign
    constexpr Fraction& operator = (const long long a)
    {
        numerator = a;
        denominator = 1;
        return *this;
    }

    constexpr Fraction& operator = (const double d)
    {
        // TODO: (s)
        return *this;
    }

    // Negation
    constexpr Fraction operator - () const
    {
        return Fraction(-numerator, denominator);
    }

    // Addition
    constexpr Fraction operator + (const Fraction& other) const
    {
        long long CD = lcm(denominator, other.denominator);
        return Fraction(numerator * (CD / denominator) + other.numerator * (CD / other.denominator), CD);
    }

    constexpr Fraction operator + (long long a) const
    {
        return Fraction(numerator + denominator * a, denominator);
    }

    friend Fraction operator + (long long a, const Fraction& f)
    {
        return Fraction((f.denominator * a) + f.numerator, f.denominator);
    }

    constexpr Fraction& operator += (const Fraction& other)
    {
        long long CD = lcm(denominator, other.denominator);
        denominator = CD;
        numerator += numerator * (CD / denominator) + other.numerator * (CD / other.denominator);
        return *this;
    }

    constexpr Fraction& operator += (const long long a)
    {
        numerator += a * denominator;
        return *this;
    }

    // Subtraction
    constexpr Fraction operator - (const Fraction& other) const
    {
        long long CD = lcm(denominator, other.denominator);
        return Fraction(numerator * (CD / denominator) - other.numerator * (CD / other.denominator), CD);
    }

    constexpr Fraction operator - (long long a) const
    {
        return Fraction(numerator - denominator * a, denominator);
    }

    friend Fraction operator - (long long a, const Fraction& f)
    {
        return Fraction((f.denominator * a) - f.numerator, f.denominator);
    }

    constexpr Fraction& operator -= (const Fraction& other)
    {
        long long CD = lcm(denominator, other.denominator);
        denominator = CD;
        numerator += numerator * (CD / denominator) - other.numerator * (CD / other.denominator);
        return *this;
    }

    constexpr Fraction& operator -= (const long long a)
    {
        numerator -= a * denominator;
        return *this;
    }

    // Multiplication
    constexpr Fraction operator * (const Fraction& other) const
    {
        // cross simplification
        long long gdc1 = GCD(numerator, other.denominator);
        long long gdc2 = GCD(other.numerator, denominator);

        return Fraction( (numerator/gdc1) * (other.numerator/gdc2), (denominator/gdc2) * (other.denominator/gdc1) );
    }

    constexpr Fraction operator * (long long a) const
    {
        return Fraction(numerator * a, denominator);
    }

    friend Fraction operator * (long long a, const Fraction& other)
    {
        return Fraction(other.numerator * a, other.denominator);
    }

    constexpr Fraction& operator *= (const Fraction& other)
    {
        numerator *= other.numerator;
        denominator *= other.denominator;
        return *this;
    }

    constexpr Fraction& operator *= (const long long a)
    {
        numerator *= a;
        return *this;
    }

    // Division
    constexpr Fraction operator / (const Fraction& other) const
    {
        return Fraction(numerator * other.denominator, denominator * other.numerator);   
    }

    constexpr Fraction operator / (const long long a) const
    {
        if(a == 0) throw std::runtime_error("Math error: division by zero");
        return Fraction(numerator, denominator * a);   
    }

    constexpr Fraction& operator /= (const Fraction& other)
    {
        numerator *= other.denominator;
        denominator *= other.numerator;
        return *this;
    }

    constexpr Fraction& operator /= (const long long a)
    {
        if(a == 0) throw std::runtime_error("Math error: division by zero");
        denominator *= a;
        return *this;
    }

    friend Fraction operator / (long long a, const Fraction& f)
    {
        return Fraction(a * f.denominator, f.numerator);   
    }

    // inverse operator
    constexpr Fraction& invert()
    {        
        // note: std::swap is not constexpr for now
        long long tmp = denominator;
        denominator = numerator;
        numerator = tmp;
        return *this;
    }

    constexpr Fraction& simplify()
    {
        long long gdc = GCD(numerator, denominator);
        numerator /= gdc;
        denominator /= gdc;
        return *this;
    }

    // comparison operator
    // TODO: (s) reduce to minimum fraction first!
    /*constexpr bool operator ==  (const Fraction& other)  { return numerator == other.numerator &&
                                                                denominator == other.denominator; }
    constexpr bool operator ==  (double other)    { return (*this) == Fixed(other); }
    
    constexpr bool operator !=  (const Fraction& other)  { return numerator != other.numerator ||
                                                                denominator != other.denominator; }
    constexpr bool operator !=  (double other)    { return (*this) != Fixed(other); }

    constexpr bool operator <   (const Fraction& f)  { return this->value < f.value; }
    constexpr bool operator <   (double other)    { return (*this) < Fixed(other); }

    constexpr bool operator <=  (const Fraction& f)  { return this->value <= f.value; }
    constexpr bool operator <=  (double other)    { return (*this) <= Fixed(other); }

    constexpr bool operator >   (const Fraction& f)  { return this->value > f.value; }
    constexpr bool operator >   (double other)    { return (*this) > Fixed(other); }

    constexpr bool operator >=  (const Fraction& f)  { return this->value >= f.value; }
    constexpr bool operator >=  (double other)    { return (*this) >= Fixed(other); }*/

private:
    /* Helper functions */
    constexpr long long lcm(long long a, long long b) const
    {
        // calculating lcm as (a * b / GCD) where
        // GDC = greatest common divisor calculated using the Euclidean algorithm.
        long long gdc = GCD(a, b);
        return (a * b) / gdc;
    }

    constexpr long long GCD(long long a, long long b) const
    {
        while(b) b ^= a ^= b ^= a %= b;
        return a;
    }
};

}