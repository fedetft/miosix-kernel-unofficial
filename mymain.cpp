
#include <cstdio>
#include "miosix.h"
#include "transceiver.h"

using namespace std;
using namespace miosix;

int main()
{
	Transceiver& rtx=Transceiver::instance();
	TransceiverConfiguration tc(2450); 
        char packet[100]={0};
	for(;;)
	{
		getchar();
		ledOn();
		puts("on");
		rtx.turnOn(tc);
		
                rtx.recv(packet,100,0);
                
		getchar();
		ledOff();
		puts("off");
		rtx.turnOff();
	}
}
