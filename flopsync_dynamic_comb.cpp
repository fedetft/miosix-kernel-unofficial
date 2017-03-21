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
#include "interfaces-impl/vht.h"
#include "interfaces-impl/virtual_clock.h"

#include "flopsync_v4/flooder_sync_node.h"
#include "flopsync_v4/flopsync2.h"
#include "flopsync_v4/flopsync1.h"
#include "flopsync_v4/flooder_factory.h"

using namespace std;
using namespace miosix;

const int hop=1;

void prova1(void*){
    for(;;){
        Thread::sleep(400);
        printf("y\n");
    }
}

void prova2(void*){
    for(;;){
        Thread::sleep(600);
        printf("x\n");
    }
}

void flopsyncRadio(void*){    
    printf("\tDynamic node, hop #%d\n",hop);
    
    VirtualClock& vt=VirtualClock::instance();
    FlooderSyncNode& flooder=FlooderFactory::instance();
    flooder.forceHop(hop);
    for(;;){
        bool resync=flooder.synchronize();
        if(resync){
            flooder.resynchronize();
        }else{                        
            Thread::sleep(500);
            Thread::sleep(500);
        }
    }
}

int main()
{
    Thread::create(flopsyncRadio,2048,PRIORITY_MAX-1);
    
//    Thread::create(prova1,2048,1);
//    Thread::create(prova2,2048,1);
    
    Thread::sleep(1000000000);
    
    return 0;
}
