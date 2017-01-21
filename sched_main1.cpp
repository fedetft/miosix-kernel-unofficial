#include <cstdio>
#include "miosix.h"
#include "kernel/scheduler/control/control_scheduler.h"
using namespace std;
using namespace miosix;

bool status=false;

Thread *th1;
Thread *th2=nullptr;


void f2(void *arg){
    for (;;){
        
        Thread::nanoSleep(100 * 1000); // Sleep 100 uS
        ledOn();
        delayUs(400); //Work 400uS
        ledOff();
    }
}

int main()
{
    th1=Thread::getCurrentThread();
    th2=Thread::create(f2,STACK_MIN,ControlSchedulerPriority(MAIN_PRIORITY,REALTIME_PRIORITY_IMMEDIATE));
    {
        PauseKernelLock dLock;
        ControlScheduler::PKsetAlfa(th1,0.5f);
        ControlScheduler::PKsetAlfa(th2,0.5f);
    }
    ControlScheduler::fixmeManualBurst(0.001f); // Burst Time = 1ms
    for(;;) delayMs(500);
}
