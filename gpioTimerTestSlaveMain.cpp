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
#include "miosix/arch/cortexM3_efm32gg/efm32gg332f1024_wandstem/interfaces-impl/gpio_timer.h"

using namespace std;
using namespace miosix;

using namespace std;

/*
 * 
 */
int main(int argc, char** argv) {
    GPIOtimer& g=GPIOtimer::instance();
    printf("Inizio test slave:\n\n");
    const long long offset=192000000; //4 seconds
    for(long long i=offset;;i+=offset){
	printf("1 waiting for event...");
	g.waitTimeoutOrEvent(i);
	printf("2 wait to trigger...");
	g.absoluteSyncWaitTrigger(g.getExtEventTimestamp()+48000);
	printf("Now: %lld Timestamp: %lld (both in ticks)\n",g.getValue(),g.getExtEventTimestamp());
    }
    return 0;
}

