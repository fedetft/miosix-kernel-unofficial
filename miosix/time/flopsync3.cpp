/***************************************************************************
 *   Copyright (C)  2022 by Sorrentino Alessandro                          *
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

#include "flopsync3.h"

using namespace std;

namespace miosix {

Flopsync3& Flopsync3::instance(){
    static Flopsync3 fsync;
    return fsync;
}

long long Flopsync3::computeCorrection(long long e_k)
{
    // computing controller output
    //this->u_k = 0.15 * e_k; // * 1e-9;
    this->u_k = factorP * e_k;

    // updating internal status for next iteration
    this->e_km2 = this->e_km1;
    this->e_km1 = this->e_k;
    this->e_k = e_k;
    
    this->u_km2 = this->u_km1;
    this->u_km1 = this->u_k;

    return this->u_k;
}

void Flopsync3::reset()
{
    // errors
    this->e_k   = 0;
    this->e_km1 = 0;
    this->e_km2 = 0;

    // corrections
    this->u_k   = 0;
    this->u_km1 = 0;
    this->u_km2 = 0;

    // controller
    this->factorP(0.15);
}

long long Flopsync3::getClockCorrection()
{
    return this->u_k;
}

long long Flopsync3::getSyncError()
{
    return this->e_k;
}

Flopsync3::Flopsync3()
{
    Flopsync3::reset();
}

}