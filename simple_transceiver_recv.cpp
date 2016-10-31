
#include <cstdio>
#include "miosix.h"
#include "interfaces-impl/transceiver.h"

using namespace std;
using namespace miosix;

const static int N=100;

int main()
{
    printf("\tRECEIVER SLAVE\n");
    Transceiver& rtx=Transceiver::instance();
    TransceiverConfiguration tc(2450);
    TransceiverTimer& tim=TransceiverTimer::instance();
    RecvResult result;
    rtx.configure(tc);
    char packet[N]={0};
    long long oldTimestamp=0;
    for(;;){
	ledOn();
	puts("on");
	rtx.turnOn();

	result=rtx.recv(packet,N,tim.getValue()+1000000);
	//memDump(packet,100);
	
	ledOff();
	printf("%lld %d %d, diff=%lld off\n", result.timestamp,result.timestamp-oldTimestamp);
	oldTimestamp=result.timestamp;
	rtx.turnOff();
	Thread::sleep(1);
    }
}
