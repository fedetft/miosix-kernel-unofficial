#include <cstdio>
#include <miosix.h>
#include "interfaces-impl/transceiver_timer.h"
#include "interfaces-impl/rtc.h"
#include "interfaces-impl/gpio_timer.h"
#include "debugpin.h"

using namespace miosix;

static int error=0;
static int diffs[10000];

// return the difference between Rtc and hrt
int readRtc(){
    GPIOtimer g=GPIOtimer::instance();
    Rtc& rtc=Rtc::instance();
    long long rtcTime,time1,hrtIdeal;
    unsigned int e=0;
    {
        FastInterruptDisableLock dLock;
        int prev=loopback32KHzIn::value();
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
        while((TIMER2->IF & TIMER_IF_CC2)==0) ;
        TIMER2->CC[2].CTRL=0;
        TIMER2->IFC=TIMER_IFC_CC2;
        e=TIMER2->CC[2].CCV;
        rtcTime=rtc.IRQgetValue();
        time1=g.getValue();
    }
    time1=((time1 & 0xFFFFFFFFFFFF0000LL) | e) - error;
    hrtIdeal=rtcTime*48000000/32768;
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
    long long time2,c;
    unsigned int e=0;
    {
        FastInterruptDisableLock dLock;
        int prev=loopback32KHzIn::value();
        //Wait for falling edge
        for(;;)
        {
                int curr=loopback32KHzIn::value();
                if(curr==0 && prev==1) break;
                prev=curr;
        }
	//
        TIMER2->CC[2].CTRL=TIMER_CC_CTRL_ICEDGE_RISING
                            | TIMER_CC_CTRL_FILT_DISABLE
                            | TIMER_CC_CTRL_INSEL_PIN
                            | TIMER_CC_CTRL_MODE_INPUTCAPTURE;
        //Wait for rising edge
        while((TIMER2->IF & TIMER_IF_CC2)==0) ;
        TIMER2->CC[2].CTRL=0;
        TIMER2->IFC=TIMER_IFC_CC2;
        e=TIMER2->CC[2].CCV;
        c=rtc.IRQgetValue();
        time2=g.getValue(); //FIXME: IRQ
    }
    time2=(time2 & 0xFFFFFFFFFFFF0000LL) | e;
    error=time2-(c*48000000+16384)/32768;
}

void doAll(){
    while(1){
        correct();
        //read a number of values
	long long now=getTime();
        for(int i=0;i<10000;i++){
	    ledOn();
            diffs[i]=readRtc();
	    ledOff();
	    now+=1000000;
	    Thread::nanoSleepUntil(now);
        }
        for(int i=0;i<10000;i++){
            printf("%d ",diffs[i]);
        }
	printf("\n");
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
