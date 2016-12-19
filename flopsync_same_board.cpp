#include <stdio.h>
#include "miosix.h"
#include "myflopsync1.h"
#include "interfaces-impl/rtc.h"
#include "interfaces-impl/high_resolution_timer_base.h"
#include "interfaces-impl/gpio_timer.h"

using namespace std;
using namespace miosix;

/*
 * This code aim to sync the 2 clock on wandstem. The master is HFXO and the slave is LFXO, the interesting part of code is in high_resolution_timer_base
 */

GPIOtimer* g; 
Rtc* rtc;

void task1(void* ){
    HighResolutionTimerBase::tWaiting=Thread::getCurrentThread();
    Flopsync1 f;
    for(;;){
	Thread::wait();
	pair<int,int> result=f.computeCorrection(HighResolutionTimerBase::error);
	HighResolutionTimerBase::clockCorrection=result.first;
	//printf("Timestamped=%lld Expected=%lld e=%lld %lld\n",HighResolutionTimerBase::syncPointHrtSlave,HighResolutionTimerBase::syncPointHrtMaster,HighResolutionTimerBase::error,HighResolutionTimerBase::clockCorrection);
	//printf("%lld\n",HighResolutionTimerBase::error);
	//greenLed::toggle();
    }
}

void task2(void* ){
    for(;;){
	//delayMs(1000);
	long long a=g->getValue();
	//long long b=rtc->getValue();  
	//printf("HRT:%lld RTC:%lld conv:%lld diff:%lld\n", a,b,b*48000000/32768,a-b*48000000/32768);
	printf("%lld\n",a);
	//Thread::sleep(200);
	
    }
}

void task3(void*){
    for(;;){
	greenLed::toggle();
	Thread::sleep(20);
    }
}

void task0(void*){
    HighResolutionTimerBase::queue.run();
}
/*
long long rtcTime,time1,hrtIdeal;
	HighResolutionTimerBase::diffs[1]=vhtSyncPointVht;
	HighResolutionTimerBase::diffs[2]=temp;
	HighResolutionTimerBase::diffs[3]=HighResolutionTimerBase::error;
	
	for(int i=0+4;i<20+4;i++){
	    int prev=loopback32KHzIn::value();
	    for(;;){
		int curr=loopback32KHzIn::value();
		if(curr==1 && prev==0) break;
		prev=curr;
	    }
	    //Approximated
	    time1=IRQgetTick();
	    rtcTime=RTC->CNT; //FIXME
	    HighResolutionTimerBase::diffs[i]=(time1-HighResolutionTimerBase::error)-(rtcTime*48000000+16384)/32768;
	    delayMs(1);
	}
*/

int main(int argc, char** argv) {
    printf("\t\tflopsync_same_board\n");
    
    rtc=&Rtc::instance();
    g=&GPIOtimer::instance();
    
    //Configuration of timers
    PRS->CH[4].CTRL= PRS_CH_CTRL_SOURCESEL_RTC | PRS_CH_CTRL_SIGSEL_RTCCOMP1; 
    TIMER2->CC[2].CTRL=TIMER_CC_CTRL_ICEDGE_RISING
			| TIMER_CC_CTRL_FILT_DISABLE
			| TIMER_CC_CTRL_INSEL_PRS
			| TIMER_CC_CTRL_PRSSEL_PRSCH4
			| TIMER_CC_CTRL_MODE_INPUTCAPTURE;
    
    //printf("Inizio %lu\n",RTC->CNT);
    RTC->COMP1=3000-1;
    RTC->IFC=RTC_IFC_COMP1;
    RTC->IEN|=RTC_IEN_COMP1;
    
    TIMER2->IEN |= TIMER_IEN_CC2;
    while(RTC->SYNCBUSY & RTC_SYNCBUSY_COMP1) ;
    //Thread::create(task0,2048,2);
    Thread::create(task1,2048,3);
    Thread::create(task2,2048,1);
    Thread::sleep(10);
    //Thread::create(task3,2048,0);
    Thread::wait();
    
    return 0;
}

