#include <stdio.h>
#include "miosix.h"
#include "interfaces-impl/gpio_timer.h"
#include "interfaces-impl/transceiver_timer.h"

using namespace std;
using namespace miosix;

/**
 * Test to check the delay of input capture timer a.k.a. the stabilization time 
 * in input capture mode
 * ATTENTION: Connect GPIO13 to TP1, that is GPIO3 on CC2520
 */

void trigger(void*){
    TransceiverTimer& timer=TransceiverTimer::instance();
    printf("trigger\n");
    for(long long i=96000000;;i+=4800000){
	printf("Ready to trigger...\n");
	redLed::high();
	timer.absoluteWaitTrigger(i);
    }
}

void capture(void*){
    TransceiverTimer& timer=TransceiverTimer::instance();
    printf("capture\n");
    for(;;){
	bool result=timer.waitTimeoutOrEvent(96000000);
	printf("%lld %d\n",timer.getExtEventTimestamp(),result);
    }
}

int main(int argc, char** argv) {
    Thread::create(trigger,2048,3);
    //Thread::create(capture,2048,3);
    
    Thread::sleep(1000000000);
    
    return 0;
}

