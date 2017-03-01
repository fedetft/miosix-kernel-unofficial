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
#include <stdexcept>
#include <list>
#include "miosix.h"
#include "interfaces-impl/power_manager.h"
#include "interfaces-impl/spi.h"
#include "interfaces-impl/transceiver.h"
#include "interfaces-impl/timer_interface.h"

using namespace std;
using namespace miosix;

const unsigned int maxNumOfPackets=10;
list<pair<unsigned char *,RecvResult>> packets;
FastMutex mutex;
ConditionVariable cv;

void printThread(void *argv)
{
    for(;;)
    {
        unsigned char *packet;
        RecvResult result;
        {
            Lock<FastMutex> l(mutex);
            while(packets.empty()) cv.wait(l);
            packet=packets.front().first;
            result=packets.front().second;
            packets.pop_front();
        }
        ledOn();
        printf("RSSI=%d size=%d ",result.rssi,result.size);
        switch(result.error)
        {
            case RecvResult::OK:
                printf("Ok\n");
                break;
            case RecvResult::TIMEOUT:
                printf("Timeout\n");
                break;
            case RecvResult::TOO_LONG:
                printf("Too long\n");
                break;
            case RecvResult::CRC_FAIL:
                printf("CRC fail\n");
                break;
        }
        memDump(packet,result.size);
        printf("\n");
        delete[] packet;
        ledOff();
    }
}

int main()
{
    Thread::create(printThread,2048);
    Transceiver& rtx=Transceiver::instance();
//     TransceiverConfiguration cfg(2450,0,true); //Check CRC
    TransceiverConfiguration cfg(2450,0,false); //Do not check CRC
    rtx.configure(cfg);
    rtx.turnOn();
    auto packet=new unsigned char[127];
    RecvResult result;
    for(;;)
    {
        try {
            result=rtx.recv(packet,127,numeric_limits<long long>::max());
            Lock<FastMutex> l(mutex);
            if(packets.size()<maxNumOfPackets)
            {
                packets.push_back(make_pair(packet,result));
                cv.signal();
                packet=new unsigned char[127];
            }
        } catch(exception& e) {
            printf("exception thrown %s\n",e.what());
        }
    }
}

