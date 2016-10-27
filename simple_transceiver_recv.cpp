
#include <cstdio>
#include "miosix.h"
#include "interfaces-impl/transceiver.h"

using namespace std;
using namespace miosix;

int main()
{
	Transceiver& rtx=Transceiver::instance();
	TransceiverConfiguration tc(2450); 
	rtx.configure(tc);
        char packet[100]={0};
	for(;;)
	{
		ledOn();
		puts("on");
		rtx.turnOn();
		
                rtx.recv(packet,100,1000000000000LL);
                
		memDump(packet,100);
		
		ledOff();
		puts("off");
		rtx.turnOff();
	}
}
