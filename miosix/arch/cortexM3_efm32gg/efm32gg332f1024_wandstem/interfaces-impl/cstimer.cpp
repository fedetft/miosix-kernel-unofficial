#include "interfaces/cstimer.h"
#include "interfaces/arch_registers.h"
#include "kernel/kernel.h"
#include "kernel/scheduler/timer_interrupt.h"
#include "kernel/timeconversion.h"
#include "../../../../../debugpin.h"
#include "CMSIS/Include/core_cm3.h"
#include "bsp_impl.h"

using namespace miosix;

const unsigned int timerBits=32;
const unsigned long long overflowIncrement=(1LL<<timerBits);
const unsigned long long lowerMask=overflowIncrement-1;
const unsigned long long upperMask=0xFFFFFFFFFFFFFFFFLL-lowerMask;

static long long ms32time = 0; //most significant 32 bits of counter
static long long ms32chkp = 0; //most significant 32 bits of check point

static TimeConversion *tc;

static inline long long nextInterrupt()
{
    return ms32chkp | TIMER3->CC[1].CCV<<16 | TIMER1->CC[1].CCV;
}

static inline unsigned int IRQread32Timer(){
    unsigned int high=TIMER3->CNT;
    unsigned int low=TIMER1->CNT;
    unsigned int high2=TIMER3->CNT;
    if(high==high2){
        return (high<<16)|low;
    }
    return high2<<16|TIMER1->CNT;
}

static inline long long IRQgetTick(){
    //PENDING BIT TRICK
    unsigned int counter=IRQread32Timer();
    if((TIMER3->IF & _TIMER_IFC_OF_MASK) && IRQread32Timer()>=counter)
        return (ms32time | static_cast<long long>(counter)) + overflowIncrement;
    return ms32time | static_cast<long long>(counter);
}

static inline void callScheduler(){
    TIMER1->IEN &= ~TIMER_IEN_CC1;
    TIMER3->IEN &= ~TIMER_IEN_CC1;
    IRQtimerInterrupt(tc->tick2ns(IRQgetTick()));
}

void __attribute__((naked)) TIMER3_IRQHandler()
{
    saveContext();
    asm volatile("bl _Z10cstirqhnd3v");
    restoreContext();
}

void __attribute__((naked)) TIMER1_IRQHandler()
{
    saveContext();
    asm volatile("bl _Z10cstirqhnd1v");
    restoreContext();
}
void __attribute__((naked)) TIMER2_IRQHandler()
{
    saveContext();
    asm volatile("bl _Z10cstirqhnd2v");
    restoreContext();
}

static inline void setupTimers(long long curTick,long long nextIntTick){
    long long diffs = ms32chkp - ms32time;
    long int diff = TIMER3->CC[1].CCV - static_cast<unsigned int>((curTick & 0xFFFF0000)>>16);
    if (diffs == 0|| (diffs==1 && (TIMER3->IF & TIMER_IF_OF)) ){
        if (diff==0 || diff==-1){
            
            if (diff==-1){ //if TIM1 overflows before calculating diff
                callScheduler();
            }else{
                TIMER1->IFC = TIMER_IFC_CC1;
                TIMER1->IEN |= TIMER_IEN_CC1;
                if (TIMER1->CNT > TIMER1->CC[1].CCV || /* TIM1 overflows after calculating diff */
                        (TIMER3->CC[1].CCV - static_cast<unsigned int>((IRQgetTick() & 0xFFFF0000)>>16)==-1 &&
                        TIMER1->CNT < TIMER1->CC[1].CCV)){
                    callScheduler();
                }
            }
        }else{
            TIMER3->IFC = TIMER_IFC_CC1;
            TIMER3->IEN |= TIMER_IEN_CC1; 
        }
    }else{
        TIMER3->IFC = TIMER_IFC_CC1;
        TIMER3->IEN |= TIMER_IEN_CC1; 
    } 
}

void __attribute__((used)) cstirqhnd3(){
    //Checkpoint
    if (TIMER3->IF & TIMER_IF_CC1){
        TIMER3->IFC = TIMER_IFC_CC1;
        long long diffs = ms32chkp - ms32time;
        if (diffs == 0|| (diffs==1 && (TIMER3->IF & TIMER_IF_OF)) ){
            TIMER3->IEN &= ~TIMER_IEN_CC1;
            TIMER1->IFC = TIMER_IFC_CC1;
            TIMER1->IEN |= TIMER_IEN_CC1;
            if (TIMER1->CNT >= TIMER1->CC[1].CCV){
                callScheduler(); //WHat if CCV=0xFFFF
            }
        }
    }   
    //rollover
    if (TIMER3->IF & TIMER_IF_OF){
        TIMER3->IFC = TIMER_IFC_OF;
        ms32time += overflowIncrement;
    }
}
void __attribute__((used)) cstirqhnd1(){
    if (TIMER1->IF & TIMER_IF_CC1){
        callScheduler();
    }
}

void __attribute__((used)) cstirqhnd2(){

}

//
// class ContextSwitchTimer
//

namespace miosix {
    
    ContextSwitchTimer& ContextSwitchTimer::instance()
    {
        static ContextSwitchTimer instance;
        return instance;
    }
    
    void ContextSwitchTimer::IRQsetNextInterrupt(long long ns){
        // First off, clear and disable timers to prevent unnecessary IRQ
        // when IRQsetNextInterrupt is called multiple times consecutively.
        TIMER1->IEN &= ~TIMER_IEN_CC1;
        TIMER3->IEN &= ~TIMER_IEN_CC1;
        //
        long long tick = tc->ns2tick(ns);
        long long curTick = IRQgetTick();
        if(curTick >= tick)
        {
            // The interrupt is in the past => call timerInt immediately
            callScheduler(); //TODO: It could cause multiple invocations of sched.
        }else{
            // Apply the new interrupt on to the timer channels
            TIMER1->CC[1].CCV = static_cast<unsigned int>(tick & 0xFFFF);
            TIMER3->CC[1].CCV = static_cast<unsigned int>((tick & 0xFFFF0000)>>16);
            ms32chkp = tick & upperMask;
            setupTimers(curTick, tick);
        }
    }
    
    long long ContextSwitchTimer::getNextInterrupt() const
    {
        return tc->tick2ns(nextInterrupt());
    }
    
    long long ContextSwitchTimer::getCurrentTick() const
    {
        bool interrupts=areInterruptsEnabled();
        //TODO: optimization opportunity, if we can guarantee that no call to this
        //function occurs before kernel is started, then we can use
        //fastInterruptDisable())
        if(interrupts) disableInterrupts();
        long long result=tc->tick2ns(IRQgetTick());
        if(interrupts) enableInterrupts();
        return result;
    }
    
    long long ContextSwitchTimer::IRQgetCurrentTick() const
    {
        return tc->tick2ns(IRQgetTick());
    }
    
    ContextSwitchTimer::~ContextSwitchTimer(){}
    
    ContextSwitchTimer::ContextSwitchTimer()
    {
        //Power the timers up
        {
            InterruptDisableLock l;
            CMU->HFPERCLKEN0 |= CMU_HFPERCLKEN0_TIMER1 | CMU_HFPERCLKEN0_TIMER2
                    | CMU_HFPERCLKEN0_TIMER3;
        }
        //Configure Timers
        TIMER1->CTRL = TIMER_CTRL_MODE_UP | TIMER_CTRL_CLKSEL_PRESCHFPERCLK 
                | TIMER_CTRL_PRESC_DIV1 | TIMER_CTRL_SYNC;
        TIMER2->CTRL = TIMER_CTRL_MODE_UP | TIMER_CTRL_CLKSEL_PRESCHFPERCLK 
                | TIMER_CTRL_PRESC_DIV1 | TIMER_CTRL_SYNC;
        TIMER3->CTRL = TIMER_CTRL_MODE_UP | TIMER_CTRL_CLKSEL_TIMEROUF 
                | TIMER_CTRL_SYNC;
        
        //Enable necessary interrupt lines
        TIMER1->IEN = 0;
        TIMER3->IEN = TIMER_IEN_OF;

        TIMER1->CC[1].CTRL = TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;
        TIMER3->CC[1].CTRL = TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;

        NVIC_SetPriority(TIMER1_IRQn,3);
        NVIC_SetPriority(TIMER3_IRQn,3);
        NVIC_ClearPendingIRQ(TIMER3_IRQn);
        NVIC_ClearPendingIRQ(TIMER1_IRQn);
        NVIC_EnableIRQ(TIMER3_IRQn);
        NVIC_EnableIRQ(TIMER1_IRQn);
        //Start timers
        TIMER1->CMD = TIMER_CMD_START;
        //Synchronization is required only when timers are to start.
        //If the sync is not disabled after start, start/stop on another timer
        //(e.g. TIMER0) will affect the behavior of context switch timer!
        TIMER1->CTRL &= ~TIMER_CTRL_SYNC;
        TIMER2->CTRL &= ~TIMER_CTRL_SYNC;
        TIMER3->CTRL &= ~TIMER_CTRL_SYNC;
        //Setup tick2ns conversion tool
        timerFreq = 48000000;
        tc = new TimeConversion(timerFreq);
    }

}