#include <cstdlib>
#include <cstdio>
#include "miosix.h"
#include "interfaces/arch_registers.h"
#include "kernel/kernel.h"
#include "kernel/scheduler/timer_interrupt.h"
#include "miosix/e20/e20.h"

using namespace std;
using namespace miosix;

int value=-1;
Thread *tWaiting=nullptr;

FixedEventQueue<100,12> queue;

int main(int argc, char** argv) {
    CMU->CALCTRL=CMU_CALCTRL_DOWNSEL_LFXO|CMU_CALCTRL_UPSEL_HFXO|CMU_CALCTRL_CONT;
    //due to hardware timer characteristic, the real counter trigger at value+1
    //tick of LFCO to yield the maximum from to up counter
    CMU->CALCNT=700; 
    //enable interrupt
    CMU->IEN=CMU_IEN_CALRDY;
    NVIC_SetPriority(CMU_IRQn,3);
    NVIC_ClearPendingIRQ(CMU_IRQn);
    NVIC_EnableIRQ(CMU_IRQn);
    CMU->CMD=CMU_CMD_CALSTART;
    queue.run();
    return 0;
}

void __attribute__((naked)) CMU_IRQHandler()
{
    saveContext();
    asm volatile("bl _Z10cmuhandlerv");
    restoreContext();
}

void __attribute__((used)) cmuhandler(){
    static float y;
    static bool first=true;
    if(CMU->IF & CMU_IF_CALRDY){
	if(first){
	    y=CMU->CALCNT;
	    first=false;
	}else{
	    y=0.8f*y+0.2f*CMU->CALCNT;
	}
        CMU->IFC=CMU_IFC_CALRDY;
	bool hppw;
	queue.IRQpost([&](){printf("%f\n",y);},hppw);
	if(hppw) Scheduler::IRQfindNextThread();
    }
}