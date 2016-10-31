
#include <cstdio>
#include "miosix.h"
#include "interfaces-impl/transceiver.h"
#include "interfaces-impl/transceiver_timer.h"

using namespace std;
using namespace miosix;

const static int N=100;
const static long long timeInterval=384000000;

int main(){
    printf("\SENDER MASTER\n");
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
	ledOn();

	//rtx.sendNow(packet,100);
	rtx.sendAt(packet,N,i);
	result=rtx.recv(packetAux,N,tim.getValue()+timeInterval);
	ledOff();
	
	printf("received at:%lld, roundtrip: %lld, temporal roundtrip: %lld\n",result.timestamp,result.timestamp-i, result.timestamp-oldTimestamp);
	oldTimestamp=result.timestamp;
	//NOTE: this sleep is very important, if we call turnOff and turnOn immediately, the system stops
	Thread::sleep(100);
    }
    rtx.turnOff();
}

