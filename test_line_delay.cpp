#include <stdio.h>
#include "miosix.h"
#include "interfaces-impl/gpio_timer.h"
#include "interfaces-impl/transceiver_timer.h"

using namespace std;
using namespace miosix;

/**
 * Test to check the stabilization time in input capture mode
 * ATTENTION:Connect GPIO13 to TP3 or TP5 with a simple wire
 */

void trigger(void*){
    GPIOtimer& timer=GPIOtimer::instance();
    printf("trigger\n");
    for(long long i=96000000;;i+=96000000){
	printf("Ready to trigger...\n");
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
    Thread::create(capture,2048,3);
    
    Thread::sleep(1000000000);
    
    return 0;
}

