#include <cstdio>
#include "miosix.h"
#include "interfaces/arch_registers.h"
#include "kernel/kernel.h"
#include "kernel/scheduler/timer_interrupt.h"
#include "miosix/e20/e20.h"
#include "interfaces-impl/gpio_timer.h"
#include "interfaces-impl/rtc.h"

using namespace std;
using namespace miosix;


FixedEventQueue<100,12> queue;

static int error=0;
static double rate=1;
static int diffs[10000];
static long long hrtSyncPoint=0;
static unsigned int bf=0,bi=0;
static long long sum=0;


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
    time1=(time1 & 0xFFFFFFFFFFFF0000LL) | e ;
    long long time1Corrected = time1 - error;
    hrtIdeal=rtcTime*48000000/32768;
    //return time1Corrected-hrtIdeal;
    //hrtSyncPoint è il valore non corretto del timer nel sync oint
    return (hrtSyncPoint-error+(time1-hrtSyncPoint)*rate)-hrtIdeal;
    //return hrtSyncPoint - error + mul64x32d32(time1-hrtSyncPoint,bi,bf) - hrtIdeal;
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
    hrtSyncPoint=time2;
    error=time2-(c*48000000+16384)/32768;
}

template<typename C>
C divisionRounded(C a, C b){
    return (a+b/2)/b;
}

int main(int argc, char** argv) {
    CMU->CALCTRL=CMU_CALCTRL_DOWNSEL_LFXO|CMU_CALCTRL_UPSEL_HFXO|CMU_CALCTRL_CONT;
    //due to hardware timer characteristic, the real counter trigger at value+1
    //tick of LFCO to yield the maximum from to up counter
    CMU->CALCNT=699; 
    //enable interrupt
    CMU->IEN=CMU_IEN_CALRDY;
    NVIC_SetPriority(CMU_IRQn,3);
    NVIC_ClearPendingIRQ(CMU_IRQn);
    NVIC_EnableIRQ(CMU_IRQn);
    CMU->CMD=CMU_CMD_CALSTART;
    //queue.run();
    
    
    while(1){
	correct();
	/*const int cntNominal = 1025391;
	
	CMU->CMD=CMU_CMD_CALSTART;
	while(!(CMU->IF & CMU_IF_CALRDY));
	CMU->IFC=CMU_IFC_CALRDY;
	int y=CMU->CALCNT;
	int ecnt = y - cntNominal;
	bf = static_cast<unsigned int> (ecnt*4189);
	bi = ecnt>0 ? 1 : 0;
	rate=(double)(700LL*48000000/32768)/(double)y;
	*/ 
	printf("corrected rate %f\n",rate);
        
        //read a number of values
	long long now=getTime();
        for(int i=0;i<30;i++){
	    ledOn();
            diffs[i]=readRtc();
	    ledOff();
	    now+=1000000;
	    Thread::nanoSleepUntil(now);
        }
        for(int i=0;i<30;i++){
            printf("%d ",diffs[i]);
        }
	printf("\n");
    }
    
    return 0;
}

void __attribute__((naked)) CMU_IRQHandler()
{
    saveContext();
    asm volatile("bl _Z10cmuhandlerv");
    restoreContext();
}

void __attribute__((used)) cmuhandler(){
    static double y;
    static bool first = true;
    //Adjust the rate
    if(CMU->IF & CMU_IF_CALRDY){
	const int cntNominal = 1025391;
	if(first){
	    first=false;
	    y=CMU->CALCNT;
	}else{
	    y=0.8*y+0.2*CMU->CALCNT;
	}
	
	rate=(700LL*48000000/32768)/y;
        CMU->IFC=CMU_IFC_CALRDY;
	bool hppw;
	queue.IRQpost([&](){printf("%f\n",y);},hppw);
	if(hppw) Scheduler::IRQfindNextThread();
    }
}