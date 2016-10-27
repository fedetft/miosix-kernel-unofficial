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
    GPIOtimer& g=GPIOtimer::instance();
    printf("Start test (slave):\n\n");
    const long long offset=192000000; //4 seconds
    long long timestamp,oldtimestamp=0; 
    for(;;){
	//printf("1. waiting for event...");
	g.waitTimeoutOrEvent(offset);
	timestamp=g.getExtEventTimestamp();
	//printf("Sleeping...\n");
	//delayMs(1000);
	//printf("2. wait to trigger...");
	g.absoluteSyncWaitTrigger(timestamp+24000000);
	printf("Now: %lld Received at: %lld (both in ticks)\n",g.getValue(),timestamp);
	printf("%lld\n",timestamp-oldtimestamp);
	oldtimestamp=timestamp;
    }
    return 0;
}

