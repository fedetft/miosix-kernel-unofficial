
#include <cstdio>
#include "miosix.h"
#include "interfaces-impl/transceiver.h"
#include "interfaces-impl/transceiver_timer.h"

using namespace std;
using namespace miosix;

const static int N=100;
const static long long timeInterval = 24000000;
const static long long timeout = 96000000;

/**
 * Red led means that the device is waiting the packet
 */
int main(){
    printf("\tSENDER MASTER\n");
    Transceiver& rtx=Transceiver::instance();
    TransceiverConfiguration tc(2450); 
    rtx.configure(tc);
    //Rtc& timer=Rtc::instance();
    TransceiverTimer& tim=TransceiverTimer::instance();
    const int N=100;
    char packet[N]={0},packetAux[N];
    for(int i=0;i<N;i++){
	packet[i]=i;
    }
    RecvResult result;
    long long oldTimestamp=0;
    rtx.turnOn();
    for(long long i=96000000;;i+=timeInterval){
	try{
            rtx.sendAt(packet,N,i);
            ledOn();
            result=rtx.recv(packetAux,N,tim.getValue()+timeout);
            ledOff();

            //printf("received at:%lld, roundtrip: %lld, temporal roundtrip: %lld\n",result.timestamp,result.timestamp-i, result.timestamp-oldTimestamp);
            printf("%lld\n",result.timestamp-i);
            oldTimestamp=result.timestamp;
        }catch(exception& e){
            puts(e.what());
        }
        
	//NOTE: this sleep is very important, if we call turnOff and turnOn immediately, the system stops
	Thread::sleep(2);
    }
    rtx.turnOff();
}

