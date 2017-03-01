#include <stdio.h>
#include "miosix.h"
#include "interfaces-impl/transceiver.h"
#include "interfaces-impl/power_manager.h"
#include "interfaces-impl/rtc.h"
#include "interfaces-impl/transceiver_timer.h"
#include "interfaces-impl/vht.h"


using namespace std;
using namespace miosix;

const int n=10;
const int packetLen=1;
const int interval=48000;   ///< in tick 
/*
 * Send some packet, while deepsleep for a while and then continue to send packets
 */
int main(int argc, char** argv) {
    printf("Sender in deep sleep test\n\n");
    char packet[packetLen];
    Transceiver& t=Transceiver::instance();
    TransceiverTimer& tt=TransceiverTimer::instance();
    PowerManager& pw=PowerManager::instance();
    Rtc& rtc=Rtc::instance();
    int i=0;
    VHT& vht=VHT::instance();
    
    //vht.stopResyncSoft();
    t.turnOn();
    for(long long time=96000000;;time+=interval){
        
        if(i<10){
            packet[0]=0;
        }else if(i==10){
            packet[0]=2;
        }else{
            i=-1;
        }
        if(i!=-1){
            //printf("Send #%i\n",i);
            t.sendAt(packet,packetLen,time,Transceiver::Unit::TICK);
        }else{
            printf("Deep sleep...");
            pw.deepSleepUntil(rtc.getValue()+32768*2,PowerManager::Unit::TICK);
            printf("Wake up at %lld\n",getTime());
            
            //Delay to give the possibility to the receiver to wake up
            Thread::sleep(100);
            time=tt.getValue();
        }
        i++;
    }
    t.turnOff();
    return 0;
}

