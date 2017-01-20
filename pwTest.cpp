/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   pwTest.cpp
 * Author: fabiuz
 *
 * Created on December 20, 2016, 4:38 PM
 */

#include <stdio.h>
#include "miosix.h"
#include "interfaces-impl/rtc.h"
#include "interfaces-impl/gpio_timer.h"
#include "interfaces-impl/power_manager.h"


using namespace std;
using namespace miosix;

const int maxValue= 16777216; //2^24

void blink(void*){
    for(;;){
	redLed::high();
	redLed::low();
    }
}

int main(int argc, char** argv) {
    printf("\t\tTest DeepSleep\n\n");
    
    printf("Instantiation of PowerManager and Rtc...");
    PowerManager& pm=PowerManager::instance();
    Rtc& rtc=Rtc::instance();
    printf(" done!\n");
    
    //NB: in this phase HRTB is already instantiated by ContextSwitchTimer
    printf("Instantiation of GPIOtimer...");
    GPIOtimer& g=GPIOtimer::instance();
    printf(" done!\n");
    
    //Auxiliary var to make sure that the times are acquired as soon as possible
    long long tickH,tickL;
    printf("%.9f\n",1/3.14); // = 0,31847133757962
//    long long v=g.getValue();
//    long unsigned rtcV=RTC->CNT;
//    
//    for(int i=0;i<10;i++){
//    v=g.getValue();
//    rtcV=RTC->CNT;
//    printf("%lu %f %lld %lld %f %f\n", 
//	    rtcV,
//	    rtcV/(float)32768,
//	    HighResolutionTimerBase::clockCorrection,
//	    v+HighResolutionTimerBase::clockCorrection,
//	    (v+HighResolutionTimerBase::clockCorrection)/(float)48000000,
//	    (v+HighResolutionTimerBase::clockCorrection)/(float)48000000-rtcV/(float)32768);
//    }
    
    double timeH,timeL;
    
    for(int i=0;i<10;i++){
	//Fragment of code to check if HRt is consistent with RTC. 
	//On long running without resync, this code will fail due to the accumulating skew
	tickH=g.getValue();
	tickL=rtc.getValue();
	timeH=(double)(tickH+HighResolutionTimerBase::clockCorrection)/48000000;
	timeL=(double)tickL/32768;
	printf("%lld %.9f %lld %lld %.9f %.9f", 
	    tickL,
	    timeL,
	    HighResolutionTimerBase::clockCorrection,
	    tickH,
	    timeH,
	    timeH-timeL);
	if(timeH-timeL>0.000032){
	    printf("Sync error!\n");
	}
	printf("\n");
    }
    
    
    
    printf("\tTest of deepSleep multiple times:\n");
    for(int i=0;i < 7;i++){
	printf("[%lld] I'm going to sleep, #%d...\n",rtc.getValue(),i+1);
	pm.deepSleepUntil(32768*(i+1));
	printf("[%lld] Waken up!\n\n",rtc.getValue());
    }
    
    printf("\tTest of GPIOtimer after multiple sleep:\n");
    long long actualTime=rtc.getValue();
    for(int i=0;i<7;i++){
	printf("[%lld] I'm going to sleep, #%d at hrt:%lld...\n",rtc.getValue(),i+1,g.getValue()+HighResolutionTimerBase::clockCorrection);
	pm.deepSleepUntil(actualTime+32768*(i+1));
	printf("[%lld] Waken up at %lld!\n\n",rtc.getValue(),g.getValue()+HighResolutionTimerBase::clockCorrection);
    }
    
    printf("\tTest deepsleep until first and second Rtc overflow (2^24=16777216):\n");
    for(int i=0;i<2;i++){
	printf("[%lld] I'm going to sleep,#%d hrt:%lld...\n",rtc.getValue(),i+1,g.getValue()+HighResolutionTimerBase::clockCorrection);
	pm.deepSleepUntil(maxValue*(i+1));
	for(int i=0;i<10;i++){
	    tickL=rtc.getValue();
	    tickH=g.getValue()+HighResolutionTimerBase::clockCorrection;
	    printf("[%llu] Waken up at %lld\n\n",tickL,tickH);
	}
    }

    printf("\tTest deepsleep for 2 overflows:\n");
    for(int i=0;i<5;i++){
	printf("[%lld] I'm going to sleep, #%d hrt:%lld...\n",rtc.getValue(),i+1,g.getValue()+HighResolutionTimerBase::clockCorrection);
	pm.deepSleepUntil(maxValue*4+(maxValue*2)*i);
	for(int i=0;i<10;i++){
	    tickL=rtc.getValue();
	    tickH=g.getValue()+HighResolutionTimerBase::clockCorrection;
	    printf("[%llu] Waken up at %lld\n\n",tickL,tickH);
	}
    }
    
    printf("\tTest deepsleep for 3 overflows:\n");
    for(int i=0;i<5;i++){
	printf("[%lld] I'm going to sleep, #%d hrt:%lld...\n",rtc.getValue(),i+1,g.getValue());
	pm.deepSleepUntil(maxValue*15+(maxValue*3)*i);
	tickL=rtc.getValue();
	tickH=g.getValue()+HighResolutionTimerBase::clockCorrection;
	printf("[%llu] Waken up at %lld, aux1:%lld aux2;%lld aux3;%lld aux4;%lld\n\n",tickL,tickH,HighResolutionTimerBase::aux1,HighResolutionTimerBase::aux2,HighResolutionTimerBase::aux3,HighResolutionTimerBase::aux4);
    }
    
    
    
    printf("\nEnd of the test suite\n");
    
    return 0;
}

