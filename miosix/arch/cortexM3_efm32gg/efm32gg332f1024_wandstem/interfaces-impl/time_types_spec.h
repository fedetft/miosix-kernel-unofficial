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

#include "interfaces/os_timer.h"

// high speed clock
#include "interfaces-impl/hsc.h" 


// real time clock
#if defined(WITH_DEEP_SLEEP) || defined(WITH_VHT)
#include "interfaces-impl/rtc.h" 
#endif

// virtual clock and vht
#ifdef WITH_FLOPSYNC
#include "interfaces/flopsync3.h"
#endif

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

}

// Because the vht class it templates, all its implementation is contianed in the
// header file. Therefore, we need to import it after having defined VirtualClockSpec.
#ifdef WITH_VHT
#include "interfaces/vht.h"
#endif

namespace miosix
{
    #if defined(WITH_VHT)
    using VhtSpec = Vht<Hsc, Rtc>;
    #endif
}
