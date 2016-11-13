
#include <cstdio>
#include "miosix.h"
#include "interfaces-impl/transceiver.h"

using namespace std;
using namespace miosix;

const static int N=100;
const static long long delay = 480000;
const static long long timeout = 24000000;          //< same as timeInterval in the master
const static long long firstTimeout = 1440000000LL; //< longer timout to allow the reset and the turning on of master 

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
            if(firstTime){
                result=rtx.recv(packet,N,tim.getValue()+firstTimeout);
                firstTime=false;
            }else{
                result=rtx.recv(packet,N,tim.getValue()+timeout);
            }
            //memDump(packet,100);
            ledOff();
            rtx.sendAt(packet,N,result.timestamp+delay);

            printf("%lld %d %d, diff=%lld\n", result.timestamp, result.size, result.timestampValid,result.timestamp-oldTimestamp);
            oldTimestamp=result.timestamp;

            Thread::sleep(1);
        }catch(exception& e){
            puts(e.what());
        }
    }
    rtx.turnOff();
}
