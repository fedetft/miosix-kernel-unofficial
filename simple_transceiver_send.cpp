
#include <cstdio>
#include "miosix.h"
#include "interfaces-impl/transceiver.h"

using namespace std;
using namespace miosix;

int main(){
    Transceiver& rtx=Transceiver::instance();
    TransceiverConfiguration tc(2450); 
    rtx.configure(tc);
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
	Thread::sleep(20);
	ledOff();
	puts("off");
	rtx.turnOff();
	
	Thread::sleep(2000);
    }
}
