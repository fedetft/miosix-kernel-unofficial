/***************************************************************************
 *   Copyright (C) 2016 by Fabiano Riccardi                                *
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
#include "miosix.h"
#include "interfaces-impl/transceiver.h"
#include "interfaces-impl/transceiver_timer.h"
#include "pingpong_const.h"

using namespace std;
using namespace miosix;

/**
 * Red led means that the device is waiting the packet
 */
int main(){
    printf("\tRECEIVER SLAVE\n");
    Transceiver& rtx=Transceiver::instance();
    TransceiverConfiguration tc(2450);
    TransceiverTimer& tim=TransceiverTimer::instance();
    
    rtx.configure(tc);
    char packet[N]={0};
    long long oldTimestamp=0;
    rtx.turnOn();
    for(;;){
        try{
            ledOn();
            RecvResult result;
	    memset(packet,0,N);
            result=rtx.recv(packet,N,tim.getValue()+offsetBetweenPing*2);
	    
	    //Modify data change before retransmit
	    for(int i=0;i<N;i++){
		packet[i]+=10;
	    }
	    
	    if(result.error==RecvResult::OK && result.timestampValid){
		rtx.sendAt(packet,N,result.timestamp+delay);
	    }else{
		//printf("Error\n");
	    }
	    //printf("%d\n",result.error);
            //memDump(packet,100);
            ledOff();
            
            //printf("%lld %d %d, diff=%lld\n", result.timestamp, result.size, result.timestampValid,result.timestamp-oldTimestamp);
            oldTimestamp=result.timestamp;

        }catch(exception& e){
            puts(e.what());
        }
    }
    rtx.turnOff();
}
