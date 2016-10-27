
#include <cstdio>
#include "miosix.h"
#include "interfaces-impl/transceiver.h"
#include "interfaces-impl/transceiver_timer.h"

using namespace std;
using namespace miosix;

int main(){
    Transceiver& rtx=Transceiver::instance();
    TransceiverConfiguration tc(2450); 
    rtx.configure(tc);
    Rtc& timer=Rtc::instance();
    TransceiverTimer& tim=TransceiverTimer::instance();
    const int N=100;
    char packet[N]={0};
    for(int i=0;i<N;i++){
	packet[i]=i;
    }

    for(;;){
	ledOn();
	puts("on");
	rtx.turnOn();

	rtx.sendNow(packet,100);
	//rtx.sendAt(packet,100,timer.getValue()+32768);
	
	Thread::sleep(20);
	ledOff();
	puts("off");
	rtx.turnOff();
	
	Thread::sleep(2000);
    }
}
