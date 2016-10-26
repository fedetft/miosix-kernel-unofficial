/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   gpioTimerTestMasterMain.cpp
 * Author: fabiuz
 * 
 * Created on October 25, 2016, 11:33 AM
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
 * Main of the master in Ping Pong test
 */
int main(int argc, char** argv) {
    GPIOtimer& g=GPIOtimer::instance();
    printf("Inizio test master:\n\n");
    bool w;
    const long long offset=384000000; //4 seconds
    long long timestamp,oldtimestamp=0;
    for(long long i=offset;;i+=offset){
	//printf("1 wait to trigger...\n");
	g.absoluteSyncWaitTrigger(i);
	//printf("2 waiting event...\n");
	w=g.waitTimeoutOrEvent(i);
	timestamp=g.getExtEventTimestamp();
	printf("Send at: %lld Timestamp received: %lld diff=%lld diff between past event:%lld\n",i,timestamp,timestamp-i,timestamp-oldtimestamp);
	oldtimestamp=timestamp;
    }
    return 0;
}

