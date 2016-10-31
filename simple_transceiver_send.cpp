
#include <cstdio>
#include "miosix.h"
#include "interfaces-impl/transceiver.h"
#include "interfaces-impl/transceiver_timer.h"

using namespace std;
using namespace miosix;

const static int N=100;
const static long long timeInterval=48000000;

int main(){
    printf("\SENDER MASTER\n");
    Transceiver& rtx=Transceiver::instance();
    TransceiverConfiguration tc(2450); 
    rtx.configure(tc);
    //Rtc& timer=Rtc::instance();
    TransceiverTimer& tim=TransceiverTimer::instance();
    const int N=100;
    char packet[N]={0};
    for(int i=0;i<N;i++){
	packet[i]=i;
    }
    long long oldTimestamp=0;
    rtx.turnOn();
    for(long long i=96000000;;i+=timeInterval){
	ledOn();

	//rtx.sendNow(packet,100);
	rtx.sendAt(packet,N,i);
	ledOff();
	
	printf("Sent at:%lld\n",i);
	//NOTE: this sleep is very important, if we call turnOff and turnOn immediately, the system stops
	Thread::sleep(100);
    }
    rtx.turnOff();
}
