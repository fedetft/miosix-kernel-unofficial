#include "interfaces/cstimer.h"
#include "interfaces/arch_registers.h"
#include "kernel/kernel.h"
#include "kernel/scheduler/timer_interrupt.h"
#include "kernel/timeconversion.h"
#include "../../../../../debugpin.h"
#include "CMSIS/Include/core_cm3.h"
#include "bsp_impl.h"
#include "high_resolution_timer_base.h"

using namespace miosix;

//
// class ContextSwitchTimer
//
namespace miosix {
    
    ContextSwitchTimer& ContextSwitchTimer::instance()
    {
        static ContextSwitchTimer instance;
        return instance;
    }
    
    void ContextSwitchTimer::IRQsetNextInterrupt(long long tick){
        b.IRQsetNextInterrupt1(tick);
    }
    
    long long ContextSwitchTimer::getNextInterrupt() const
    {
        return b.IRQgetSetTimeCCV1();
    }
    
    long long ContextSwitchTimer::getCurrentTick() const
    {
        return b.getCurrentTick();
    }
    
    long long ContextSwitchTimer::IRQgetCurrentTick() const
    {
        return b.IRQgetCurrentTick();
    }
    
    ContextSwitchTimer::~ContextSwitchTimer(){}
    
    ContextSwitchTimer::ContextSwitchTimer(): b(HighResolutionTimerBase::instance()){}
}