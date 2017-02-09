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

const int N=50000;

static unsigned short diffs[N];

void getTickLen(){
    expansion::gpio13::mode(Mode::INPUT);
    unsigned short n,nprec;
    {
        FastInterruptDisableLock dLock;
	bool first=true;
        for(int i=0;i<N+1;i++){
	    TIMER2->CC[2].CTRL=0;
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
	    TIMER2->IFC=TIMER_IFC_CC2;
	    n=TIMER2->CC[2].CCV;
	    if(first){
		first=false;
	    }else{
		//printf("*%lu*\n",n-nprec);
		diffs[i-1]=n-nprec;
	    }
	    nprec=n;
	}
    }
}

int main(int argc, char** argv) {
    getTickLen();
    TIMER2->CC[2].CTRL=0;
    for(int i=0;i<N;i++){
	printf("%hu\n",diffs[i]);
    }
    
    printf("End\n");
    
    return 0;
}
