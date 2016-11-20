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
#include "interfaces-impl/gpio_timer.h"
#include "gpio_timer_test_const.h"

using namespace std;
using namespace miosix;

/*
 * Main of the slave in Ping Pong test
 */
int main(int argc, char** argv) {
    GPIOtimer& g=GPIOtimer::instance();
    printf("Start test (slave):\n\n");
    //in tick, stay below 0.1 second to avoid (great) dependencies on the temperature and intrinsic quartz skew.
    long long timestamp,oldtimestamp=0; 
    for(;;){
	g.waitTimeoutOrEvent(timeout);
	timestamp=g.getExtEventTimestamp();
	g.absoluteSyncWaitTrigger(timestamp+t1ms);
    }
    return 0;
}

