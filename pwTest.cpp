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
    
    //Auxiliary var to make sure that the times are acquired as soon as possible
    long long timeH,timeL;
    
    
    PowerManager& pm=PowerManager::instance();
    Rtc& rtc=Rtc::instance();
    printf("Instantiation of PowerManager and Rtc done!\n");
    
    printf("\tTest of deepSleep multiple times:\n");
    for(int i=0;i < 7;i++){
	printf("[%lld] I'm going to sleep, #%d...\n",rtc.getValue(),i+1);
	pm.deepSleepUntil(32768*(i+1));
	printf("[%lld] Waken up!\n\n",rtc.getValue());
    }
    
    printf("\tTest of GPIOtimer after multiple sleep:\n");
    GPIOtimer& g=GPIOtimer::instance();
    printf("Instantiation of GPIOtimer done!\n");
    long long actualTime=rtc.getValue();
    for(int i=0;i<7;i++){
	printf("[%lld] I'm going to sleep, #%d at hrt:%lld...\n",rtc.getValue(),i+1,g.getValue());
	pm.deepSleepUntil(actualTime+32768*(i+1));
	printf("[%lld] Waken up at %lld!\n\n",rtc.getValue(),g.getValue());
    }
    
    printf("\tTest deepsleep until first and second Rtc overflow (2^24=16777216):\n");
    for(int i=0;i<2;i++){
	printf("[%lld] I'm going to sleep,#%d hrt:%lld...\n",rtc.getValue(),i+1,g.getValue());
	pm.deepSleepUntil(maxValue*(i+1));
	timeL=rtc.getValue();
	timeH=g.getValue();
	printf("[%llu] Waken up at %lld\n\n",timeL,timeH);
    }

    printf("\tTest deepsleep for 2 overflows:\n");
    for(int i=0;i<5;i++){
	printf("[%lld] I'm going to sleep, #%d hrt:%lld...\n",rtc.getValue(),i+1,g.getValue());
	pm.deepSleepUntil(maxValue*4+(maxValue*2)*i);
	timeL=rtc.getValue();
	timeH=g.getValue();
	printf("[%llu] Waken up at %lld, aux1:%lld aux2;%lld aux3;%lld aux4;%lld\n\n",timeL,timeH,HighResolutionTimerBase::aux1,HighResolutionTimerBase::aux2,HighResolutionTimerBase::aux3,HighResolutionTimerBase::aux4);
    }
    
    printf("\tTest deepsleep for 3 overflows:\n");
    for(int i=0;i<5;i++){
	printf("[%lld] I'm going to sleep, #%d hrt:%lld...\n",rtc.getValue(),i+1,g.getValue());
	pm.deepSleepUntil(maxValue*15+(maxValue*3)*i);
	timeL=rtc.getValue();
	timeH=g.getValue();
	printf("[%llu] Waken up at %lld, aux1:%lld aux2;%lld aux3;%lld aux4;%lld\n\n",timeL,timeH,HighResolutionTimerBase::aux1,HighResolutionTimerBase::aux2,HighResolutionTimerBase::aux3,HighResolutionTimerBase::aux4);
    }
    
    
    
    printf("\nEnd of the test suite\n");
    
    return 0;
}

