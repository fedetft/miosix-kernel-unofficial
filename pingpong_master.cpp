
#include <cstdio>
#include "miosix.h"
#include "interfaces-impl/transceiver.h"
#include "interfaces-impl/transceiver_timer.h"
#include "pingpong_const.h"

using namespace std;
using namespace miosix;

const static int N=100;
const static long long timeInterval = 24000000;

/**
 * Red led means that the device is waiting the packet
 */
int main(){
    printf("\tSENDER MASTER\n");
    Transceiver& rtx=Transceiver::instance();
    TransceiverConfiguration tc(2450); 
    rtx.configure(tc);
    
    TransceiverTimer& tim=TransceiverTimer::instance();
    const int N=100;
    char packet[N]={0},packetAux[N];
    for(int i=0;i<N;i++){
	packet[i]=i;
    }
    RecvResult result;
    long long oldTimestamp=0;
    rtx.turnOn();
    long long base=65536*1000;
    for(long long i=0;i<65536;i++){
	long long t=base+i*(20*65536)+i;
	try{
	    memset(packetAux,0,N);
            rtx.sendAt(packet,N,t);
            ledOn();
            result=rtx.recv(packetAux,N,t + delay + 300000);
	    if(result.error==RecvResult::TIMEOUT){
		printf("Timeout\n");
	    }else{
		//printf("received at:%lld, roundtrip: %lld, temporal roundtrip: %lld\n",result.timestamp,result.timestamp-i, result.timestamp-oldTimestamp);
		printf("%lld\n",result.timestamp-t);
		oldTimestamp=result.timestamp;
	    }
            ledOff();
        }catch(exception& e){
            puts("Exc");
        }
        
    }
    rtx.turnOff();
}

