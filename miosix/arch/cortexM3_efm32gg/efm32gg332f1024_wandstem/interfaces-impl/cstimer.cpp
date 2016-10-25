#include "interfaces/cstimer.h"
#include "kernel/timeconversion.h"
#include "cstimer_impl.h"

using namespace miosix;

static TimeConversion *tc;

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
    
    long long ContextSwitchTimer::getCurrentTime() const{
        return tc->tick2ns(pImpl->b.getCurrentTick());
    }
    
    long long ContextSwitchTimer::IRQgetCurrentTime() const{
	return tc->tick2ns(pImpl->b.IRQgetCurrentTick());
    }
    
    ContextSwitchTimer::~ContextSwitchTimer(){}
    
    ContextSwitchTimer::ContextSwitchTimer(){
        timerFreq = 48000000;
        tc = new TimeConversion(timerFreq);
	
	pImpl=new ContextSwitchTimerImpl();
    }

}