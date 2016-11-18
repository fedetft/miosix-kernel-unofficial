
#include <cstdio>
#include <miosix.h>
#include "interfaces-impl/transceiver_timer.h"
#include "interfaces-impl/rtc.h"
#include "interfaces-impl/gpio_timer.h"
#include "debugpin.h"

using namespace miosix;

void test()
{
	Rtc& rtc=Rtc::instance();
	GPIOtimer g=GPIOtimer::instance();
	long long rtcTime,time1,time2;
	int a=0,b=0,c=0,d=0,e=0;
	{
		FastInterruptDisableLock dLock;
		int prev=loopback32KHzIn::value();
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
		rtcTime=rtc.IRQgetValue();
 		c=RTC->CNT;
		time1=g.getValue();
	}
	printf("a=%d b=%d c=%d d=%d e=%x %llx e-d=%d\n",a,b,c,d,e,time1,e>=d ? e-d : e+65536-d);
	time1=(time1 & 0xFFFFFFFFFFFF0000LL) | e;
	
	Thread::sleep(30000);
	
	{
		FastInterruptDisableLock dLock;
		int prev=loopback32KHzIn::value();
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
		rtcTime=rtc.IRQgetValue();
 		c=RTC->CNT;
		time2=g.getValue();
	}
	printf("a=%d b=%d c=%d d=%d e=%x %llx e-d=%d\n",a,b,c,d,e,time2,e>=d ? e-d : e+65536-d);
	time2=(time2 & 0xFFFFFFFFFFFF0000LL) | e;
	printf("%llx %llx %llx\n\n",time1,time2,time2-time1);
}

int main()
{
	initDebugPins();
		
	for(;;)
	{
		puts("----");
		test();
		
		Thread::sleep(2000);
	}
}
