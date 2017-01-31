
#include <cstdio>
#include "miosix.h"
#include "interfaces-impl/transceiver.h"
#include "interfaces-impl/transceiver_timer.h"

using namespace std;
using namespace miosix;

const static int N=5;
const static long long timeInterval=480000;

/*
 * This is a simple sender to test the transceiver and the precise timing of 
 * timer skew and transmission delay
 * To avoid the skew due to the temperature, use interval lower or equa 10ms.
 */
int main(){
    printf("\tSENDER Simple MASTER\n");
    Transceiver& rtx=Transceiver::instance();
    TransceiverConfiguration tc(2450); 
    rtx.configure(tc);
    //Rtc& timer=Rtc::instance();
    TransceiverTimer& tim=TransceiverTimer::instance();
    char packet[N]={0};
    for(int i=0;i<N;i++){
	packet[i]=i;
    }
    long long oldTimestamp=0;
    rtx.turnOn();
    for(long long i=96000000;;i+=timeInterval){
	ledOn();

	try{
            rtx.sendAt(packet,N,i);
        }catch(exception& e){
            puts(e.what());
        }
	ledOff();
	
	//printf("Sent at:%lld value return %d\n",i, HighResolutionTimerBase::aux);
	//NOTE: this sleep is very important, if we call turnOff and turnOn immediately, the system stops
	Thread::sleep(2);
    }
    rtx.turnOff();
}
