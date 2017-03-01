/***************************************************************************
 *   Copyright (C) 2013 by Terraneo Federico                               *
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

#include <cstdio>
#include <miosix.h>
#include "interfaces-impl/timer_interface.h"
#include "interfaces-impl/transceiver_timer.h"

#include "flopsync_v4/flooder_sync_node.h"
#include "flopsync_v4/flopsync2.h"
#include "flopsync_v4/flopsync1.h"

using namespace std;
using namespace miosix;

void dosome(void*){
    for(;;){
        Thread::sleep(10);
        greenLed::toggle();
    }
}

int main()
{
    printf("Dynamic node\n");
    
    Thread::create(dosome,512,1);
    
    HardwareTimer& timer=TransceiverTimer::instance();
    //Synchronizer *sync=new Flopsync2; //The new FLOPSYNC, FLOPSYNC 2
    Synchronizer* sync=new Flopsync1;
    //the third parameter is the node ID
    FlooderSyncNode flooder(sync,10000000000LL,2450,1,1);


//     Clock *clock=new MonotonicClock(*sync,flooder);
//     else clock=new NonMonotonicClock(*sync,flooder);
    
    for(;;){
        if(flooder.synchronize()){
            flooder.resynchronize();
        }
    }
}
