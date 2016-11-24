
#include <cstdio>
#include "miosix.h"
#include "interfaces-impl/transceiver.h"
#include "pingpong_const.h"

using namespace std;
using namespace miosix;

const static int N=100;

/**
 * Red led means that the device is waiting the packet
 */
int main(){
    printf("\tRECEIVER SLAVE\n");
    Transceiver& rtx=Transceiver::instance();
    TransceiverConfiguration tc(2450);
    TransceiverTimer& tim=TransceiverTimer::instance();
    
    rtx.configure(tc);
    bool firstTime=true;
    char packet[N]={0};
    long long oldTimestamp=0;
    rtx.turnOn();
    for(;;){
        try{
            ledOn();
            RecvResult result;
	    memset(packet,0,N);
            if(firstTime){
                result=rtx.recv(packet,N,tim.getValue()+firstTimeout);
                firstTime=false;
            }else{
                result=rtx.recv(packet,N,tim.getValue()+timeout);
            }
	    //Little change before retransmit
	    for(int i=0;i<N;i++){
		packet[i]+=10;
	    }
	    
	    if(result.error==RecvResult::OK){
		rtx.sendAt(packet,N,result.timestamp+delay);
	    }
	    //printf("%d\n",result.error);
            //memDump(packet,100);
            ledOff();
            
            //printf("%lld %d %d, diff=%lld\n", result.timestamp, result.size, result.timestampValid,result.timestamp-oldTimestamp);
            oldTimestamp=result.timestamp;

        }catch(exception& e){
            puts(e.what());
        }
    }
    rtx.turnOff();
}
