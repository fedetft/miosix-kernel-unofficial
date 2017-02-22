
#include <cstdio>
#include "miosix.h"
#include "interfaces-impl/transceiver.h"
#include "interfaces-impl/transceiver_timer.h"


using namespace std;
using namespace miosix;

const static int N=5;

/*
 * Just a simple receiver, useful to test the sender and the timing, like timer skews.
 */
int main(){
    printf("\tRECEIVER Simple SLAVE\n");
    Transceiver& rtx=Transceiver::instance();
    TransceiverConfiguration tc(2450);
    TransceiverTimer& tim=TransceiverTimer::instance();
    TimeConversion conv(48000000);
    RecvResult result;
    rtx.configure(tc);
    char packet[N]={0};
    long long oldTimestamp=0;
    rtx.turnOn();
    for(int i=0;;i++){
        ledOn();
        try{
            result=rtx.recv(packet,N,conv.tick2ns(tim.getValue()+48000000),Transceiver::Unit::TICK);
            //memDump(packet,100);
        }catch(exception& e){
            puts(e.what());
        }

        ledOff();
        if(result.timestampValid){
            //printf("%lld, diff=%lld\n", result.timestamp,result.timestamp-oldTimestamp);
            printf("%lld",result.timestamp-oldTimestamp);
            oldTimestamp=result.timestamp;
        }else{
            printf("Not valid");
        }
	
        if(result.error!=RecvResult::OK){
            printf(" Error #%d",result.error);
        }
        printf("\n");
        if(i<10) Thread::sleep(100);
    }
    rtx.turnOff();
}
