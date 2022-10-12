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

#pragma once

///
// imports
///

/* High Speed Clock */
#include "interfaces-impl/hsc.h" 

/* Real Time Clock */
#include "interfaces-impl/rtc.h" 

/* Virtual Clock */  
#include "interfaces/os_timer.h"

/* Flopsync3 e Virtual High Resolution Timer */  
#include "time/clock_sync.h"


///
// typedef(s)
/// 

namespace miosix {

/* High Speed Clock */
extern Hsc * hsc; //< HSC

/* Real Time Clock */
#if defined(WITH_DEEP_SLEEP) || defined(WITH_VHT)
extern Rtc * rtc; //< RTC
#endif

/* Virtual Clock */
#if defined(WITH_VHT) && defined(WITH_FLOPSYNC)
using VirtualClockSpec = VirtualClock<Hsc, 2>;
#elif !defined(WITH_VHT) && defined(WITH_FLOPSYNC)
using VirtualClockSpec = VirtualClock<Hsc, 1>;
#elif defined(WITH_VHT) && !defined(WITH_FLOPSYNC)
using VirtualClockSpec = VirtualClock<Hsc, 1>;
#else
using VirtualClockSpec = VirtualClock<Hsc, 0>;
#endif

extern VirtualClockSpec * vc; //< VC 

/* Virtual High Resolution Timer */
#if defined(WITH_VHT)
extern Vht * vht; //< VHT
#endif

}