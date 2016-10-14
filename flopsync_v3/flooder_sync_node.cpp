 
/***************************************************************************
 *   Copyright (C)  2013 by Terraneo Federico                              *
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
#include "flooder_sync_node.h"
#include <cstdio>
#include <cassert>
#include <stdexcept>
#include "myConfigRadio.h"

#define L 5

using namespace std;
using namespace miosix;


//
// class FlooderSyncNode
//

FlooderSyncNode::FlooderSyncNode(
    HardwareTimer& timer, Synchronizer& synchronizer, unsigned char hop)
    : timer(timer),
      transceiver(Transceiver::instance()),
      synchronizer(synchronizer),
      measuredFrameStart(0), computedFrameStart(0), clockCorrection(0),
      receiverWindow(w), missPackets(maxMissPackets+1), hop(hop) {}
        
bool FlooderSyncNode::synchronize()
{
    if(missPackets>maxMissPackets) return true;
    
    computedFrameStart+=nominalPeriod+clockCorrection;
    long long wakeupTime=computedFrameStart-(jitterAbsorption+rxTurnaroundTime+receiverWindow); 
    long long timeoutTime=computedFrameStart+receiverWindow+preambleFrameTime;
    assert(timer.getValue()<wakeupTime);
    timer.absoluteWait(wakeupTime);
    
    miosix::TransceiverConfiguration cfg(2450,0,true);
    transceiver.configure(cfg);
    transceiver.turnOn();
    unsigned char packet[L];
       
    assert(timer.getValue()<computedFrameStart-(rxTurnaroundTime+receiverWindow));
    bool timeout=false;
    ledOn();
    
    RecvResult result;
    for(;;)
    {
        try {
            result=transceiver.recv(packet,sizeof(packet),timeoutTime);
            
            if(   result.error==RecvResult::OK && result.timestampValid==true
               && result.size==L && (hop==packet[2]+1) && packet[0]==0xC2 && packet[1]==0x02 && packet[3]==static_cast<unsigned char>(PANID>>8) && packet[4]==static_cast<unsigned char>(PANID))
            {
                measuredFrameStart=result.timestamp;
                break;
            }
            
        } catch(exception& e) {
            puts(e.what());
            break;
        }
        if(timer.getValue()>=timeoutTime)
        {
            timeout=true;
            break;
        }
    }
    ledOff();
    
    if(packet[2]<0xff && hop==packet[2]+1){ //retransmit only once if my id is following the count
        packet[2]++;
        transceiver.sendAt(packet,result.size,result.timestamp+fullSyncPacketTime+rebroadcastGapTime);        
        printf("[my ID %d]Status: %d %d %d. Packet forwarded:\n",hop, result.error,result.timestampValid,result.size);
        memDump(packet,sizeof(packet));
    }
    
    transceiver.turnOff();
    
    pair<int,int> r;
    if(timeout)
    {
        if(++missPackets>maxMissPackets)
        {
            puts("Lost sync");
            return true;
        }
        r=synchronizer.lostPacket();
        measuredFrameStart=computedFrameStart;
    }
    else
    {
        int e = measuredFrameStart-computedFrameStart;
        r=synchronizer.computeCorrection(e);
        missPackets=0;
    }
    clockCorrection=r.first;
    receiverWindow=r.second;
    int e = measuredFrameStart-computedFrameStart;
    
    timeout? printf("e=%d u=%d w=%d ---> miss\n",e,clockCorrection,receiverWindow):
                    printf("e=%d u=%d w=%d\n",e,clockCorrection,receiverWindow);
    
    return false;
}

void FlooderSyncNode::resynchronize()
{
    puts("Resynchronize...");
    synchronizer.reset();
    miosix::TransceiverConfiguration cfg(2450,0,true);
    transceiver.configure(cfg);
    transceiver.turnOn();
    ledOn();
    unsigned char packet[L];
    for(;;)
    {
        try {
            auto result=transceiver.recv(packet,sizeof(packet),numeric_limits<long long>::max());
            if(   result.error==RecvResult::OK && result.timestampValid==true
               && result.size==L && packet[0]==0xC2 && packet[1]==0x02 && hop==packet[2]+1 && packet[3]==0x77 && packet[4]==0x77)
            {
                computedFrameStart=measuredFrameStart=result.timestamp;
                break;
            }
        } catch(exception& e) {
            puts(e.what());
        }
    }
    ledOff();
    clockCorrection=0;
    receiverWindow=w;
    missPackets=0;
}
