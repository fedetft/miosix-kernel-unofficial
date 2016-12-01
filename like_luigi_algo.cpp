#include <stdio.h>
#include "miosix.h"
#include "interfaces-impl/rtc.h"
#include "interfaces-impl/high_resolution_timer_base.h"
#include "interfaces-impl/gpio_timer.h"
#include "interfaces-impl/power_manager.h"


using namespace miosix;

static volatile unsigned long long vhtBase=0;        ///< Vht time corresponding to rtc time: theoretical
static unsigned long long vhtSyncPointRtc=0;     ///< Rtc time corresponding to vht time : known with sum
static volatile unsigned long long vhtSyncPointVht=0;     ///< Vht time corresponding to rtc time :timestanped

int main(int argc, char** argv){
    PowerManager& pm=PowerManager::instance();
    printf("%lu",TIMER2->IEN);
    TIMER2->IEN=TIMER_IEN_CC0|TIMER_IEN_CC1;
    printf("%lu",TIMER2->IEN);
    pm.deepSleepUntil(80000);
    printf("%lu",TIMER2->IEN);
    
    
    
    
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
    RTC->COMP1=99999;
    RTC->IFC=RTC_IFC_COMP1;
    RTC->IEN=RTC_IEN_COMP1;
    
    TIMER2->IEN |= TIMER_IEN_CC2;
    while(RTC->SYNCBUSY & RTC_SYNCBUSY_COMP1) ;
   
    //printf("Fine %lu %lu %lu\n",RTC->CNT,TIMER2->CC[2].CCV,TIMER2->CC[2].CCV);
    HighResolutionTimerBase::tWaiting=Thread::getCurrentThread();
    
    while(1){
	{
	    FastInterruptDisableLock dLock;
	    Thread::IRQwait();
	    {
		FastInterruptEnableLock eLock(dLock);
		Thread::yield();
	    }
	}
	for (int i=0;i<24;i++){
	    printf("%lld ",HighResolutionTimerBase::diffs[i]);
	}
        printf("\n");
	//HighResolutionTimerBase::queue.runOne();
    }
    return 0;
}

