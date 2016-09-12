
#include <cstdio>
#include "miosix.h"
#include "transceiver.h"

using namespace std;
using namespace miosix;

int main()
{
	Transceiver& rtx=Transceiver::instance();
	TransceiverConfiguration tc(2450);
        std::tuple<bool,long long, int,int> result;
        char packet[100]={0};
	for(;;)
	{
		getchar();
		ledOn();
		puts("on");
		rtx.turnOn(tc);
		
                result=rtx.recv(packet,100,0);
                printf("RSSI: %d\n",std::get<2>(result));
                
		getchar();
		ledOff();
		puts("off");
		rtx.turnOff();
	}
}
