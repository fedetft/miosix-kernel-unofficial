#include "interfaces/cstimer.h"
#include "interfaces/arch_registers.h"
#include "kernel/kernel.h"
#include "kernel/scheduler/timer_interrupt.h"
#include "kernel/timeconversion.h"
#include "cstimer_impl.h"
#include "high_resolution_timer_base.h"

using namespace miosix;

static TimeConversion *tc;

void __attribute__((naked)) TIMER2_IRQHandler()
{
    saveContext();
    asm volatile("bl _Z10cstirqhnd2v");
    restoreContext();
}

void __attribute__((used)) cstirqhnd2(){}

namespace miosix {
    
    ContextSwitchTimer& ContextSwitchTimer::instance()
    {
        static ContextSwitchTimer instance;
        return instance;
    }
    
    void ContextSwitchTimer::IRQsetNextInterrupt(long long ns){
        pImpl->b.IRQsetNextInterrupt1(tc->ns2tick(ns));
    }
    
    long long ContextSwitchTimer::getNextInterrupt() const{
	return tc->tick2ns(pImpl->b.IRQgetSetTimeCCV1());
    }
    
    long long ContextSwitchTimer::getCurrentTick() const{
        return tc->tick2ns(pImpl->b.getCurrentTick());
    }
    
    long long ContextSwitchTimer::IRQgetCurrentTick() const{
	return tc->tick2ns(pImpl->b.IRQgetCurrentTick());
    }
    
    ContextSwitchTimer::~ContextSwitchTimer(){}
    
    ContextSwitchTimer::ContextSwitchTimer(){
        timerFreq = 48000000;
        tc = new TimeConversion(timerFreq);
	
	pImpl=new ContextSwitchTimerImpl();
    }

}