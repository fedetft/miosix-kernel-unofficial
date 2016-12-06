#include <stdio.h>
#include "miosix.h"
#include "interfaces-impl/rtc.h"
#include "interfaces-impl/high_resolution_timer_base.h"
#include "interfaces-impl/gpio_timer.h"


using namespace miosix;

static volatile unsigned long long vhtBase=0;        ///< Vht time corresponding to rtc time: theoretical
static unsigned long long vhtSyncPointRtc=0;     ///< Rtc time corresponding to vht time : known with sum
static volatile unsigned long long vhtSyncPointVht=0;     ///< Vht time corresponding to rtc time :timestanped

int main(int argc, char** argv){
    PRS->CH[4].CTRL= PRS_CH_CTRL_SOURCESEL_RTC | PRS_CH_CTRL_SIGSEL_RTCCOMP1; 
    Rtc& rtc=Rtc::instance();

    TIMER2->CC[2].CTRL=TIMER_CC_CTRL_ICEDGE_RISING
			| TIMER_CC_CTRL_FILT_DISABLE
			| TIMER_CC_CTRL_INSEL_PRS
			| TIMER_CC_CTRL_PRSSEL_PRSCH4
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
	Thread::wait();
//	{
//	    FastInterruptDisableLock dLock;
//	    Thread::IRQwait();
//	    {
//		FastInterruptEnableLock eLock(dLock);
//		Thread::yield();
//	    }
//	}
	for (int i=4;i<14;i++){
	    printf("%lld ",HighResolutionTimerBase::diffs[i]);
	}
        printf("\n");
	//HighResolutionTimerBase::queue.runOne();
    }
    return 0;
}

