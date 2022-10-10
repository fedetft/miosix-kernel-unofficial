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
// TODO: (s) define here pointers to Hsc and Rtc!
///
// imports
///

#include "interfaces/os_timer.h"

// high speed clock
#include "interfaces-impl/hsc.h" 


// real time clock
#if defined(WITH_DEEP_SLEEP) || defined(WITH_VHT)
#include "interfaces-impl/rtc.h" 
#endif

//< deferred import for virtual clock and vht
                                                                
///
// typedef(s)
///

namespace miosix {

#if defined(WITH_VHT) && !defined(WITH_FLOPSYNC)
using VirtualClockSpec = VirtualClock<Hsc, 1>;
#elif !defined(WITH_VHT) && defined(WITH_FLOPSYNC)
using VirtualClockSpec = VirtualClock<Hsc, 1>;
#elif defined(WITH_VHT) && defined(WITH_FLOPSYNC)
using VirtualClockSpec = VirtualClock<Hsc, 2>;
#else
using VirtualClockSpec = VirtualClock<Hsc, 0>;
#endif

inline VirtualClockSpec * vc = &VirtualClockSpec::instance();

#if defined(WITH_VHT)
// forward declaration of Vht, body defined in clock_sync.h
template<typename Hsc_TA, typename Rtc_TA>
class Vht;

using VhtSpec = Vht<Hsc, Rtc>;
#endif

}

// Since Vht is a templated class, its implementation is fully contained in clock_sync.h.
// Therefore, to update coefficiencies a and b in the virtual clock, we need to use a
// specialization of the virtual clock. clock_sync.h cannot therefore be imported 
// before the definition of VirtualClockSpec.

// virtual clock and vht
#if defined(WITH_FLOPSYNC) || defined(WITH_VHT)
#include "time/clock_sync.h"
#endif
