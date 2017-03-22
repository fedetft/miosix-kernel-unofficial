
#include <stdio.h>
#include "interfaces-impl/gpio_timer_corr.h"
#include "interfaces-impl/vht.h"

using namespace std;
using namespace miosix;

/*
 * Test for GPIOtimerCorr
 */
int main(int argc, char** argv) {
    printf("\tMaster node\n");
    
    GPIOtimerCorr& g=GPIOtimerCorr::instance();
    VHT& vht=VHT::instance();
    
    printf("Loading vht..."); fflush(stdout);
    Thread::sleep(4000);
    printf("Done.\n");
    
    long long base=g.getValue()+1000*65536;
    long long diff,timestamp,delay=48000,timeout=48000000;
    bool w;
    for(long long i=0;i<65536;i++){
        //long long t=(base+i*(65536*100)+i);
        long long t=base+48000000*i;
        if(!g.absoluteWaitTrigger(t)){
            w=g.absoluteWaitTimeoutOrEvent(t+timeout);
            // Uncomment this lines together with the one in the GPIOtimerCorr class
            // to avoid the jitter
            //vht.startResyncSoft();
            if(w){
                printf("Reply not received\n");
            }else{
                timestamp=g.getExtEventTimestamp(HardwareTimer::Correct::CORR);
                diff=timestamp-t;
                printf("%lld\n",diff);
            }
        }else{
            printf("Send at too late\n");
        }
    }
    
    printf("End test\n");
    return 0;
}

