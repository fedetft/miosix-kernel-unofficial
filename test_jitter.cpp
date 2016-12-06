/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   newmain.cpp
 * Author: fabiuz
 *
 * Created on December 5, 2016, 11:39 AM
 */

#include <stdio.h>
#include "miosix.h"
#include "interfaces-impl/rtc.h"
#include "interfaces-impl/gpio_timer.h"
#include "math.h"

using namespace miosix;
using namespace std;

const unsigned int dataSize=40000;
signed char data[dataSize];

float stdev(){
    float mean=0;
    float meanSquared=0;
    for(unsigned int i=0;i<dataSize;i++){
	mean+=data[i];
	meanSquared+=data[i]*data[i];
    }
    mean/=dataSize;
    meanSquared/=dataSize;
    
    return sqrt(meanSquared-mean*mean);
}

// return the difference between Rtc and hrt
void jitterTest(){
    GPIOtimer g=GPIOtimer::instance();
    Rtc& rtc=Rtc::instance();
    {
        FastInterruptDisableLock dLock;
        unsigned int prev=loopback32KHzIn::value();
        //Wait for descend
        for(;;){
                int curr=loopback32KHzIn::value();
                if(curr==0 && prev==1) break;
                prev=curr;
        }
        TIMER2->CC[2].CTRL=TIMER_CC_CTRL_ICEDGE_RISING
                                        | TIMER_CC_CTRL_FILT_DISABLE
                                        | TIMER_CC_CTRL_INSEL_PIN
                                        | TIMER_CC_CTRL_MODE_INPUTCAPTURE;
        //Wait to raise
	ledOn();
	unsigned int upper=0,prevUpper;
	prev=TIMER2->CNT;
	prevUpper=0;
	for(unsigned int i=0;i<dataSize;i++)
	{
	    while((TIMER2->IF & TIMER_IF_CC2)==0) ;
	    TIMER2->IFC=TIMER_IFC_CC2;
	    unsigned int curr=TIMER2->CC[2].CCV;
	    if(curr<prev) upper+=0x10000;
	    data[i]=(curr | upper) - (prev | prevUpper) - 1465;
	    prev=curr;
	    prevUpper=upper;
	}
	ledOff();
        TIMER2->CC[2].CTRL=0;
    }
    
    //for(unsigned int i=0;i<dataSize;i++) printf("%d\n",data[i]);
    printf("%f\n",stdev());
}


int main(int argc, char** argv) {
    for(;;)
    {
	//getchar();
	jitterTest();
    }
}

