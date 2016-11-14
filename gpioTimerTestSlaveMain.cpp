/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   gpioTImerTestSlaveMain.cpp
 * Author: fabiuz
 *
 * Created on October 25, 2016, 11:34 AM
 */

#include <cstdlib>
#include <cstdio>
#include "miosix.h"
#include "debugpin.h"
#include "interfaces-impl/gpio_timer.h"

using namespace std;
using namespace miosix;

/*
 * Main of the slave in Ping Pong test
 */
int main(int argc, char** argv) {
    initDebugPins();
    GPIOtimer& g=GPIOtimer::instance();
    printf("Start test (slave):\n\n");
    const long long offset=1920000000LL; //40 seconds
    //in tick, stay below 0.1 second to avoid (great) dependencies on the temperature and intrinsic quartz skew.
    const long long delay= 480000;
    long long timestamp,oldtimestamp=0; 
    for(;;){
	printf("1. waiting for event...\n");
	g.waitTimeoutOrEvent(1000000000000LL);
	timestamp=g.getExtEventTimestamp();
	printf("2. waiting to trigger...\n");
	g.absoluteSyncWaitTrigger(timestamp+480000);
	printf("Now: %lld Received at: %lld (both in ticks)\n",g.getValue(),timestamp);
	printf("%lld\n",timestamp-oldtimestamp);
	oldtimestamp=timestamp;
    }
    return 0;
}

