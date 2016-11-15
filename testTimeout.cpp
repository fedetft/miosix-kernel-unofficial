/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   testTimeout.c
 * Author: fabiuz
 *
 * Created on November 11, 2016, 11:43 AM
 */
#include <cstdio>
#include "miosix.h"
#include "interfaces-impl/gpio_timer.h"
#include "debugpin.h"

using namespace std;
using namespace miosix;

/*
 * Connect the GPIO to the GND to avoid false high level
 */
int main() {
    initDebugPins();
    GPIOtimer& g=GPIOtimer::instance();
    
    for(long long i=96000000;;i+=480000000){
	printf("Inizio\n");
	bool w=g.waitTimeoutOrEvent(480000000);
	printf("%d, Fine\n",w);
    }
    return 0;
}

