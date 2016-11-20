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
#include "interfaces-impl/gpio_timer.h"
#include "gpio_timer_test_const.h"

using namespace std;
using namespace miosix;

/*
 * Main of the master in Ping Pong test
 */
int main(int argc, char** argv) {
    GPIOtimer& g=GPIOtimer::instance();
    printf("Inizio test (master):\n\n");
    bool w;
    long long timestamp,oldtimestamp=0;
    for(long long i=startMaster;;i+=offsetBetweenPing){
	
        g.absoluteSyncWaitTrigger(i);
	
        w=g.waitTimeoutOrEvent(timeout);
	timestamp=g.getExtEventTimestamp();
	//printf("Send at: %lld Timestamp received: %lld diff=%lld diff between past event:%lld\n",i,timestamp,timestamp-i,timestamp-oldtimestamp);
        printf("%lld\n",timestamp-i);
	oldtimestamp=timestamp;
    }
    return 0;
}

