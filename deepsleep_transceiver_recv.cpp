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

/*
 * Send some packet, while deepsleep for a while and then continue to send packets
 */
int main(int argc, char** argv) {
    printf("Receiver in Deep Sleep test\n\n");
    char packet[packetLen];
    long long last=-1,base=0;
    PowerManager& pw=PowerManager::instance();
    Rtc& rtc=Rtc::instance();
    Transceiver& t=Transceiver::instance();
    TransceiverTimer& tt=TransceiverTimer::instance();
    VHT& vht=VHT::instance();
    
    //vht.stopResyncSoft();
    t.turnOn();
    for(;;){
        RecvResult result=t.recv(packet,packetLen,base+96000000,Transceiver::Unit::TICK);
        if(packet[0]==2){
            pw.deepSleepUntil(rtc.getValue()+32768*2,PowerManager::Unit::TICK);
            base=tt.getValue();
            printf("Waken up\n");
            last=-1;
        }else{
            if(result.timestampValid&&result.error==RecvResult::ErrorCode::OK){
                if(last!=-1){
                    printf("%lld\n",result.timestamp-last);
                }
                last=result.timestamp;
            }else{
                printf("Error timestamp\n");
            }
        }
    }
    t.turnOff();
    return 0;
}