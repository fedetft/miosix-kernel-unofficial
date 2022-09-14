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

#include "fixed.h"

namespace miosix
{

// Default constructor
fp32_32::fp32_32() = default;

fp32_32::fp32_32(const double d)
{ 
    // extracting integer part (e.g. 456.45235 => 456 + (0.45235 / 2^32) )
    this->sign = signum<double>(d);
    
    double absD = std::abs(d); // just flipping sign bit in double, fast
    double invD = 1 / absD;

    this->integerPart = static_cast<unsigned int>(absD);
    this->decimalPart = static_cast<unsigned int>(4294967296 * (absD - integerPart));

    this->rIntegerPart = static_cast<unsigned int>(invD);
    this->rDecimalPart = static_cast<unsigned int>(4294967296 * (invD - rIntegerPart));
}

// Copy constructor
fp32_32::fp32_32(const fp32_32& other)
{
    // integer part
    this->integerPart = other.integerPart;
    this->decimalPart = other.decimalPart;
    // reciprocal part
    this->rIntegerPart = other.rIntegerPart;
    this->rDecimalPart = other.rDecimalPart;
    // sign
    this->sign = other.sign;
}

// functor
fp32_32 fp32_32::operator () (const fp32_32& other) {
    this->integerPart = other.integerPart;
    this->decimalPart = other.decimalPart;
    this->sign = other.sign;
    return *this;
}
fp32_32 fp32_32::operator () (double d) {
    fp32_32 fp(d);
    (*this)(fp);
    return *this;
}

// Assign
fp32_32& fp32_32::operator = (const fp32_32& f) = default;

// Negation
fp32_32 fp32_32::operator - () const & // lvalue ref-qualifier, -a
{
    fp32_32 fp(*this);
    fp.sign = -this->sign;
    return fp;
}

// Addition
// TODO: (s) check with -O3, sometimes it is wrong for aggressive compilation optimization...
fp32_32 fp32_32::operator + (const fp32_32& other) const
{
    // if you're reading this, god help you.
    fp32_32 fp;
    
    // integer and decimal signed operations
    long long decimalPart = ( this->sign * static_cast<long long>(this->decimalPart) ) + ( other.sign * static_cast<long long>(other.decimalPart) );
    long long integerPart = ( this->sign * static_cast<long long>(this->integerPart) ) + ( other.sign * static_cast<long long>(other.integerPart) );

    // global operation sign
    fp.sign = 1;
    if(integerPart < 0 || (integerPart == 0 && decimalPart < 0)) { fp.sign = -1; };

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
fp32_32 fp32_32::operator + (long long a) const
{
    fp32_32 fp(*this);
    return fp + fp32_32(a);
}

// Subtraction

fp32_32 fp32_32::operator - (const fp32_32& other) const
{
    fp32_32 fp ( (*this) + (-other) );
    return fp;
}
fp32_32 fp32_32::operator - (long long a) const
{
    fp32_32 fp(*this);
    return fp - fp32_32(a);
}
fp32_32 operator - (long long a, const fp32_32& other) { return (-other) + a; }    

// Multiplication
// TODO: (s) can avoid using long long and shift but losing precision!

long long fp32_32::operator * (long long a) const
{
    return ( this->sign * signum<long long>(a) ) * mul64x32d32(std::abs(a), integerPart, decimalPart);
}
long long operator * (long long a, const fp32_32& other) { return other * a; }    


fp32_32 fp32_32::operator * (const fp32_32& other) const
{
    fp32_32 fp;
    
    // e.g. 55.3 * 55.7 = 3080.21
    // integer part: 55 * 55 + 55 * 0.3 + 55 * 0.7 = 3080
    // decimal part: 0.3 * 0.7 = 0.21
    long long integerPart = mul32x32to64(this->integerPart, other.integerPart);
    long long decimalPart = mul32x32to64(this->decimalPart, other.decimalPart)>>32;

    // cross product
    long long tmpMul = mul32x32to64(this->integerPart, other.decimalPart);
    integerPart += tmpMul>>32; // div 2^32
    decimalPart += tmpMul % (1LL<<32);

    tmpMul = mul32x32to64(other.integerPart, this->decimalPart);
    integerPart += tmpMul>>32; // div 2^32
    decimalPart += tmpMul % (1LL<<32);

    // remain
    integerPart += decimalPart>>32;
    decimalPart = decimalPart % (1LL<<32);

    fp.integerPart = static_cast<unsigned int>(integerPart);
    fp.decimalPart = static_cast<unsigned int>(decimalPart);
    fp.sign = this->sign * other.sign;

    return fp;
}

// Division

fp32_32 fp32_32::operator / (const fp32_32& other) const
{
    fp32_32 fp;
    fp.sign = this->sign * other.sign;

    // (denum)^-1
    fp32_32 denum(other);
    denum.reciprocal();

    fp = fp32_32(this->integerPart) * denum + fp32_32(getDouble() - this->integerPart) * denum;

    return fp;
    //return fp32_32((*this).getDouble() / other.getDouble());
}


fp32_32 fp32_32::operator / (long long a) const
{
    fp32_32 fp;

    fp = fp32_32(a) / (*this);
    fp.reciprocal();

    return fp;
}
long long operator / (long long a, const fp32_32& other) 
{
    return ( other.sign * fp32_32::signum<long long>(a) ) * mul64x32d32(std::abs(a), other.getRIntegerPart(), other.getRDecimalPart());
}  

// reciprocal operator
void fp32_32::reciprocal()
{
    std::swap(this->integerPart, this->rIntegerPart);
    std::swap(this->decimalPart, this->rDecimalPart);
}

// fixed2double cast
/*operator double() const
{
    return this->sign * ( this->integerPart + (static_cast<double>(this->decimalPart) / 4294967296L) );
}*/

double fp32_32::getDouble() const
{
    return this->sign * ( this->integerPart + (static_cast<double>(this->decimalPart) / 4294967296L) );
    //static_cast<double>(*this);
}

double fp32_32::getRDouble() const
{
    return this->sign * ( this->rIntegerPart + (static_cast<double>(this->rDecimalPart) / 4294967296L) );
}

///
// getters and setters (getters for future extendibility)
///
unsigned int fp32_32::getSign() const                  { return this->sign; }
void fp32_32::setSign(short sign)                      { this->sign = signum(sign); }

unsigned int fp32_32::getIntegerPart() const           { return integerPart; }
unsigned int fp32_32::getRIntegerPart() const          { return rIntegerPart; }
void fp32_32::setIntegerPart(unsigned int integerPart) { this->integerPart = integerPart; }

unsigned int fp32_32::getDecimalPart() const           { return decimalPart; }
unsigned int fp32_32::getRDecimalPart() const          { return rDecimalPart; }
void fp32_32::setDecimalPart(double decimalPart)       { this->decimalPart = static_cast<unsigned int>(decimalPart * 4294967296); }
void fp32_32::setDecimalPart(unsigned int decimalPart) { this->decimalPart = decimalPart; }

}