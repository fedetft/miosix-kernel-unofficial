#include <cstdio>
#include <miosix.h>
#include "interfaces-impl/transceiver_timer.h"
#include "interfaces-impl/rtc.h"
#include "interfaces-impl/gpio_timer.h"
#include "debugpin.h"

using namespace miosix;

int error=0;
int diffs[10000];

// return the difference between Rtc and hrt
int readRtc(){
    GPIOtimer g=GPIOtimer::instance();
    long long rtcTime,time1,hrtIdeal;
    int a=0,b=0,d=0,e=0;
    int prev;
    Rtc& rtc=Rtc::instance();
   
    {
        FastInterruptDisableLock dLock;
        prev=loopback32KHzIn::value();
        a=RTC->CNT;
        //Wait for descend
        for(;;){
                int curr=loopback32KHzIn::value();
                if(curr==0 && prev==1) break;
                prev=curr;
        }
        d=TIMER2->CNT;
        b=RTC->CNT;
        TIMER2->CC[2].CTRL=TIMER_CC_CTRL_ICEDGE_RISING
                                        | TIMER_CC_CTRL_FILT_DISABLE
                                        | TIMER_CC_CTRL_INSEL_PIN
                                        | TIMER_CC_CTRL_MODE_INPUTCAPTURE;
        //Wait to raise
        while((TIMER2->IF & TIMER_IF_CC2)==0) ;
        TIMER2->CC[2].CTRL=0;
        TIMER2->IFC=TIMER_IFC_CC2;
        e=TIMER2->CC[2].CCV;
        rtcTime=rtc.IRQgetValue();
        time1=g.getValue();
        time1=((time1 & 0xFFFFFFFFFFFF0000LL) | e) - error;
        hrtIdeal=rtcTime*48000000/32768;
    }
    //printf("RTC: %lld, HRT: %lld, HRT ideal: %lld, diff: %lld\n",rtcTime,time1,hrtIdeal,time1-hrtIdeal);
        
    return time1-hrtIdeal;
}

void loopReadRtc(void*){
    long long diff;
    while(1){
        diff=readRtc();
        printf("%lld\n",diff);
        Thread::sleep(10);
    }
}

void correct()
{
    Rtc& rtc=Rtc::instance();
    GPIOtimer g=GPIOtimer::instance();
    long long rtcTime,time1,time2,t1,t2;
    int a=0,b=0,c=0,d=0,e=0;
    int prev;
    {
        FastInterruptDisableLock dLock;

        prev=loopback32KHzIn::value();
        a=RTC->CNT;
        //Wait for descend
        for(;;)
        {
                int curr=loopback32KHzIn::value();
                if(curr==0 && prev==1) break;
                prev=curr;
        }
        d=TIMER2->CNT;
        b=RTC->CNT;
        TIMER2->CC[2].CTRL=TIMER_CC_CTRL_ICEDGE_RISING
                            | TIMER_CC_CTRL_FILT_DISABLE
                            | TIMER_CC_CTRL_INSEL_PIN
                            | TIMER_CC_CTRL_MODE_INPUTCAPTURE;
        //Wait to raise
        while((TIMER2->IF & TIMER_IF_CC2)==0) ;
        TIMER2->CC[2].CTRL=0;
        TIMER2->IFC=TIMER_IFC_CC2;
        e=TIMER2->CC[2].CCV;
        c=rtc.IRQgetValue();
        time2=g.getValue();
        t2=c;
        time2=(time2 & 0xFFFFFFFFFFFF0000LL) | e;
        error=time2-t2*48000000/32768;
    }
    printf("Correzione\n\n");
}

void doAll(){
    while(1){
        correct();
        //read a number of values
        for(int i=0;i<1000;i++){
            diffs[i]=readRtc();
        }
        for(int i=0;i<1000;i++){
            printf("%d\n",diffs[i]);
        }
        Thread::sleep(10000);
    }
}

int main(){
    initDebugPins();
    
    //Read until the correction has to start! Unique thread
    doAll();
    
    correct();
    Thread::create(loopReadRtc,2048,0,0);
    for(;;){
        Thread::sleep(1000);
        correct();
    }
}
