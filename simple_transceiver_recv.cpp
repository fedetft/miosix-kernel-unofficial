
#include <cstdio>
#include "miosix.h"
#include "interfaces-impl/transceiver.h"

using namespace std;
using namespace miosix;

const static int N=100;

/*
 * Just a simple receiver, useful to test the sender and the timing, like timer skews.
 */
int main(){
    printf("\tRECEIVER Simple SLAVE\n");
    Transceiver& rtx=Transceiver::instance();
    TransceiverConfiguration tc(2450);
    TransceiverTimer& tim=TransceiverTimer::instance();
    RecvResult result;
    rtx.configure(tc);
    char packet[N]={0};
    long long oldTimestamp=0;
    rtx.turnOn();
    for(;;){
	ledOn();
	try{
            result=rtx.recv(packet,N,tim.getValue()+48000000);
            //memDump(packet,100);
        }catch(exception& e){
            puts(e.what());
        }

	ledOff();
	//printf("%lld, diff=%lld\n", result.timestamp,result.timestamp-oldTimestamp);
        printf("%lld\n",result.timestamp-oldTimestamp);
	oldTimestamp=result.timestamp;
	//Thread::sleep(1);
    }
    rtx.turnOff();
}
