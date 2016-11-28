/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   like_luigi_algo.cpp
 * Author: fabiuz
 *
 * Created on November 28, 2016, 10:51 AM
 */

#include <stdio.h>
#include "miosix.h"
#include "interfaces-impl/rtc.h"
#include "interfaces-impl/high_resolution_timer_base.h"
#include "interfaces-impl/gpio_timer.h"

using namespace miosix;

static volatile unsigned long long vhtBase=0;        ///< Vht time corresponding to rtc time: theoretical
static unsigned long long vhtSyncPointRtc=0;     ///< Rtc time corresponding to vht time : known with sum
static volatile unsigned long long vhtSyncPointVht=0;     ///< Vht time corresponding to rtc time :timestanped

// return the difference between Rtc and hrt
int readRtc(){
    GPIOtimer g=GPIOtimer::instance();
    Rtc& rtc=Rtc::instance();
    long long rtcTime,time1,hrtIdeal;
    unsigned int e=0;
    {
        FastInterruptDisableLock dLock;
        int prev=loopback32KHzIn::value();
        for(;;){
                int curr=loopback32KHzIn::value();
                if(curr==1 && prev==0) break;
                prev=curr;
        }
        time1=g.getValue();
        rtcTime=rtc.IRQgetValue();
    }
    //return time1Corrected-hrtIdeal;
    //hrtSyncPoint è il valore non corretto del timer nel sync oint
    return time1-hrtIdeal;
    //return hrtSyncPoint - error + mul64x32d32(time1-hrtSyncPoint,bi,bf) - hrtIdeal;
}

int main(int argc, char** argv) {
    
    loopback32KHzOut::mode(Mode::DISABLED);
    loopback32KHzIn::mode(Mode::INPUT);
    expansion::gpio18::mode(Mode::OUTPUT);
    
    PRS->CH[3].CTRL= PRS_CH_CTRL_SOURCESEL_RTC | PRS_CH_CTRL_SIGSEL_RTCCOMP1; 
    PRS->ROUTE=PRS_ROUTE_CH3PEN | PRS_ROUTE_LOCATION_LOC1; //gpio18
    Rtc& rtc=Rtc::instance();
    TIMER2->ROUTE|=TIMER_ROUTE_CC2PEN
            |TIMER_ROUTE_LOCATION_LOC0;
    TIMER2->CC[2].CTRL=TIMER_CC_CTRL_ICEDGE_RISING
			| TIMER_CC_CTRL_FILT_DISABLE
			| TIMER_CC_CTRL_INSEL_PIN
			| TIMER_CC_CTRL_MODE_INPUTCAPTURE;
    
    //printf("Inizio %lu\n",RTC->CNT);
    RTC->COMP1=100000;
    RTC->IFC=RTC_IFC_COMP1;
    RTC->IEN=RTC_IEN_COMP1;
    
    TIMER2->IEN |= TIMER_IEN_CC2;
    while(RTC->SYNCBUSY & RTC_SYNCBUSY_COMP1) ;
   
    //printf("Fine %lu %lu %lu\n",RTC->CNT,TIMER2->CC[2].CCV,TIMER2->CC[2].CCV);
    
    //HighResolutionTimerBase::queue.run();
    while(1){
        Thread::sleep(1);
        printf("%d\n",readRtc());
    }
    return 0;
}

