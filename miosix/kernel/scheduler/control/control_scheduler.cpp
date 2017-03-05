/***************************************************************************
 *   Copyright (C) 2010, 2011, 2012 by Terraneo Federico                   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   As a special exception, if other files instantiate templates or use   *
 *   macros or inline functions from this file, or you compile this file   *
 *   and link it with other works to produce a work based on this file,    *
 *   this file does not by itself cause the resulting work to be covered   *
 *   by the GNU General Public License. However the source code for this   *
 *   file must still be made available in accordance with the GNU General  *
 *   Public License. This exception does not invalidate any other reasons  *
 *   why a work based on this file might be covered by the GNU General     *
 *   Public License.                                                       *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, see <http://www.gnu.org/licenses/>   *
 ***************************************************************************/

#include "control_scheduler.h"
#include "kernel/error.h"
#include "kernel/process.h"
#include <limits>
#include "interfaces/cstimer.h"
#include "kernel/scheduler/scheduler.h"
#include <cmath>

using namespace std;

#ifdef SCHED_TYPE_CONTROL_BASED
#ifndef SCHED_CONTROL_MULTIBURST
namespace miosix {
//These are defined in kernel.cpp
extern volatile Thread *cur;
extern unsigned char kernel_running;
static ContextSwitchTimer *timer = nullptr;
extern IntrusiveList<SleepData> *sleepingList;
static long long burstStart = 0;
static long long nextPreemption = LONG_LONG_MAX;

//
// class ControlScheduler
//

bool ControlScheduler::PKaddThread(Thread *thread,
        ControlSchedulerPriority priority)
{
    #ifdef SCHED_CONTROL_FIXED_POINT
    if(threadListSize>=64) return false;
    #endif //SCHED_CONTROL_FIXED_POINT
    thread->schedData.priority=priority;
    {
        //Note: can't use FastInterruptDisableLock here since this code is
        //also called *before* the kernel is started.
        //Using FastInterruptDisableLock would enable interrupts prematurely
        //and cause all sorts of misterious crashes
        InterruptDisableLock dLock;
        thread->schedData.next=threadList;
        threadList=thread;
        threadListSize++;
        SP_Tr+=bNominal; //One thread more, increase round time
        IRQrecalculateAlfa();
    }
    return true;
}

bool ControlScheduler::PKexists(Thread *thread)
{
    if(thread==0) return false;
    for(Thread *it=threadList;it!=0;it=it->schedData.next)
    {
       if(it==thread)
       {
           if(it->flags.isDeleted()) return false; //Found, but deleted
           return true;
       }
    }
    return false;
}

void ControlScheduler::PKremoveDeadThreads()
{
    //Special case, threads at the head of the list
    while(threadList!=0 && threadList->flags.isDeleted())
    {
        Thread *toBeDeleted=threadList;
        {
            FastInterruptDisableLock dLock;
            threadList=threadList->schedData.next;
            threadListSize--;
            SP_Tr-=bNominal; //One thread less, reduce round time
        }
        void *base=toBeDeleted->watermark;
        toBeDeleted->~Thread();
        free(base); //Delete ALL thread memory
    }
    if(threadList!=0)
    {
        //General case, delete threads not at the head of the list
        for(Thread *it=threadList;it->schedData.next!=0;it=it->schedData.next)
        {
            if(it->schedData.next->flags.isDeleted()==false) continue;
            Thread *toBeDeleted=it->schedData.next;
            {
                FastInterruptDisableLock dLock;
                it->schedData.next=it->schedData.next->schedData.next;
                threadListSize--;
                SP_Tr-=bNominal; //One thread less, reduce round time
            }
            void *base=toBeDeleted->watermark;
            toBeDeleted->~Thread();
            free(base); //Delete ALL thread memory
        }
    }
    {
        FastInterruptDisableLock dLock;
        IRQrecalculateAlfa();
    }
}

void ControlScheduler::PKsetPriority(Thread *thread,
        ControlSchedulerPriority newPriority)
{
    thread->schedData.priority=newPriority;
    {
        FastInterruptDisableLock dLock;
        IRQrecalculateAlfa();
    }
}

void ControlScheduler::IRQsetIdleThread(Thread *idleThread)
{
    timer = &ContextSwitchTimer::instance();
    idleThread->schedData.priority=-1;
    idle=idleThread;
    //Initializing curInRound to end() so that the first time
    //IRQfindNextThread() is called the scheduling algorithm runs
    if(threadListSize!=1) errorHandler(UNEXPECTED);
    curInRound=0;
}

Thread *ControlScheduler::IRQgetIdleThread()
{
    return idle;
}

int bNominal=1000000*rescaled;// 1ms (hartstone)
int bMax=10000000*rescaled;// 10ms (hartstone)
void ControlScheduler::fixmeManualBurst(float burstLength)
{
    InterruptDisableLock dLock;
    
    bNominal=static_cast<int>(1000000000ll*burstLength)*rescaled;
    bMax=5*bNominal;
    SP_Tr=bNominal*threadListSize;
}

long long ControlScheduler::IRQgetNextPreemption()
{
    return nextPreemption;
}

// Should be called when the current thread is the idle thread
static inline void IRQsetNextPreemptionForIdle(){
    // In control scheduler without multiburst we don't care about other threads'
    // wake-up time unless the idle thread is about to run!
    // In this function since the idle thread has to run => we simply put the
    // wake-up time of the first thread in the sleeping list as the next
    // preemption point!
    if (sleepingList->empty())
        //normally should not happen unless an IRQ is already set and able to
        //preempt idle thread
        nextPreemption = LONG_LONG_MAX; 
    else
        nextPreemption = sleepingList->front()->wakeup_time;
    timer->IRQsetNextInterrupt(nextPreemption);
}

// Should be called for threads other than idle thread
static inline void IRQsetNextPreemption(long long burst){
    // In control scheduler without multiburst we don't care about other threads'
    // wake-up time unless the idle thread is about to run!
    // In this function since an active thread has to run => we simply
    // put its end-of-burst as the next preemption so that no other sleeping
    // threads would intervene
    burstStart = timer->IRQgetCurrentTime();
    nextPreemption = burstStart + burst;
    timer->IRQsetNextInterrupt(nextPreemption);
}

unsigned int ControlScheduler::IRQfindNextThread()
{
    // Warning: since this function is called within interrupt routines, it
    //is not possible to add/remove elements to threadList, since that would
    //require dynamic memory allocation/deallocation which is forbidden within
    //interrupts. Iterating the list is safe, though

    if(kernel_running!=0) return 0;//If kernel is paused, do nothing

    if(cur!=idle)
    {
        //Not preempting from the idle thread, store actual burst time of
        //the preempted thread
        //int Tp=miosix_private::AuxiliaryTimer::IRQgetValue(); //CurTime - LastTime = real burst
        int Tp = static_cast<int>(timer->IRQgetCurrentTime() - burstStart);
        cur->schedData.Tp=Tp;
        Tr+=Tp;
    }

    //Find next thread to run
    for(;;)
    {
        if(curInRound!=0) curInRound=curInRound->schedData.next;
        if(curInRound==0) //Note: do not replace with an else
        {
            //Check these two statements:
            //- If all threads are not ready, the scheduling algorithm must be
            //  paused and the idle thread is run instead
            //- If the inner integral regulator of all ready threads saturated
            //  then the integral regulator of the outer regulator must stop
            //  increasing because the set point cannot be attained anyway.
            bool allThreadNotReady=true;
            bool allReadyThreadsSaturated=true;
            for(Thread *it=threadList;it!=0;it=it->schedData.next)
            {
                if(it->flags.isReady())
                {
                    allThreadNotReady=false;
                    if(it->schedData.bo<bMax*multFactor)
                    {
                        allReadyThreadsSaturated=false;
                        //Found a counterexample for both statements,
                        //no need to scan the list further.
                        break;
                    }
                }
            }
            if(allThreadNotReady)
            {
                //No thread is ready, run the idle thread

                //This is very important: the idle thread can *remove* dead
                //threads from threadList, so it can invalidate iterators
                //to any element except theadList.end()
                curInRound=0;
                cur=idle;
                ctxsave=cur->ctxsave;
                #ifdef WITH_PROCESSES
                miosix_private::MPUConfiguration::IRQdisable();
                #endif
                //miosix_private::AuxiliaryTimer::IRQsetValue(bIdle); //curTime + burst
                IRQsetNextPreemptionForIdle();
                return 0;
            }
            //End of round reached, run scheduling algorithm
            curInRound=threadList;
            IRQrunRegulator(allReadyThreadsSaturated);
        }

        if(curInRound->flags.isReady() && curInRound->schedData.bo>0)
        {
            //Found a READY thread, so run this one
            cur=curInRound;
            #ifdef WITH_PROCESSES
            if(const_cast<Thread*>(cur)->flags.isInUserspace()==false)
            {
                ctxsave=cur->ctxsave;
                miosix_private::MPUConfiguration::IRQdisable();
            } else {
                ctxsave=cur->userCtxsave;
                //A kernel thread is never in userspace, so the cast is safe
                static_cast<Process*>(cur->proc)->mpu.IRQenable();
            }
            #else //WITH_PROCESSES
            ctxsave=cur->ctxsave;
            #endif //WITH_PROCESSES
            //miosix_private::AuxiliaryTimer::IRQsetValue(
            //        curInRound->schedData.bo/multFactor);
            IRQsetNextPreemption(curInRound->schedData.bo/multFactor);
            return 0;
        } else {
            //If we get here we have a non ready thread that cannot run,
            //so regardless of the burst calculated by the scheduler
            //we do not run it and set Tp to zero.
            curInRound->schedData.Tp=0;
        }
    }
}

void ControlScheduler::IRQwaitStatusHook(Thread* t)
{
    #ifdef ENABLE_FEEDFORWARD
    IRQrecalculateAlfa();
    #endif //ENABLE_FEEDFORWARD
}
void ControlScheduler::disableAutomaticAlfaChange()
 {
    InterruptDisableLock dLock;
    for(Thread *it=threadList;it!=0;it=it->schedData.next)
        it->schedData.alfaPrime=it->schedData.alfa;
    disableAutoAlfaChange=true;
    IRQrecalculateAlfa();
}

void ControlScheduler::enableAutomaticAlfaChange()
{
    InterruptDisableLock dLock;
    disableAutoAlfaChange=false;
    IRQrecalculateAlfa();
}
void ControlScheduler::IRQrecalculateAlfa()
{
    //IRQrecalculateAlfa rewritten from scratch to implement full workload
    //div deadline algorithm
    #if defined(FIXED_POINT_MATH) || !defined(ENABLE_FEEDFORWARD) || \
       !defined(ENABLE_REGULATOR_REINIT)
    #error "Workload div deadline preconditions not met"
    #endif
    if(disableAutoAlfaChange==false)
    {
        //Sum of all priorities of all threads
        //Note that since priority goes from 0 to PRIORITY_MAX-1
        //but priorities we need go from 1 to PRIORITY_MAX we need to add one
        unsigned int sumPriority=0;
        for(Thread *it=threadList;it!=0;it=it->schedData.next)
        {
            //Count only ready threads
            if(it->flags.isReady())
                sumPriority+=it->schedData.priority.get()+1;//Add one
        }
        //This can happen when ENABLE_FEEDFORWARD is set and no thread is ready
        if(sumPriority==0) return;
        float base=1.0f/((float)sumPriority);
        for(Thread *it=threadList;it!=0;it=it->schedData.next)
        {
            //Assign zero bursts to blocked threads
            if(it->flags.isReady())
            {
                it->schedData.alfa=base*((float)(it->schedData.priority.get()+1));
            } else {
                it->schedData.alfa=0;
            }
        }
    }else{
        float cpuLoad=0.0f;
        for(Thread *it=threadList;it!=0;it=it->schedData.next)
        {
            if(it->flags.isReady())
            {
                it->schedData.alfa=it->schedData.alfaPrime;
                cpuLoad+=it->schedData.alfaPrime;
            } else it->schedData.alfa=0.0f;    
         }
        if(cpuLoad<1.0f)
        {
            for(Thread *it=threadList;it!=0;it=it->schedData.next)
                it->schedData.alfa/=cpuLoad;
        } else {
            float rescale=0.0f;
            for(Thread *it=threadList;it!=0;it=it->schedData.next)
            {
                it->schedData.alfa=it->schedData.alfa*it->schedData.theta;
                rescale+=it->schedData.alfa;
            }
            for(Thread *it=threadList;it!=0;it=it->schedData.next)
                it->schedData.alfa/=rescale;
        }
    }
    reinitRegulator=true;
}

void ControlScheduler::IRQrunRegulator(bool allReadyThreadsSaturated)
{
    using namespace std;
    #ifdef SCHED_CONTROL_FIXED_POINT
    //The fixed point scheduler may overflow if Tr is higher than this
    Tr=min(Tr,524287);
    #endif //FIXED_POINT_MATH
    #ifdef ENABLE_REGULATOR_REINIT
    if(reinitRegulator==false)
    {
    #endif //ENABLE_REGULATOR_REINIT
    int eTr=SP_Tr-Tr;
    #ifndef SCHED_CONTROL_FIXED_POINT
    int bc=bco+static_cast<int>(krr*eTr-krr*zrr*eTro);
    #else //FIXED_POINT_MATH
    //Tr is clamped to 524287, so eTr uses at most 19bits. Considering
    //the 31bits of a signed int, we have 12bits free.
    const int fixedKrr=static_cast<int>(krr*2048);
    const int fixedKrrZrr=static_cast<int>(krr*zrr*1024);
    int bc=bco+(fixedKrr*eTr)/2048-(fixedKrrZrr*eTro)/1024;
    #endif //FIXED_POINT_MATH
    if(allReadyThreadsSaturated)
    {
        //If all inner regulators reached upper saturation,
        //allow only a decrease in the burst correction.
        if(bc<bco) bco=bc;
    } else bco=bc;

    bco=min<int>(max(bco,-Tr),bMax*threadListSize);
    #ifndef SCHED_CONTROL_FIXED_POINT
    float nextRoundTime=static_cast<float>(Tr+bco);
    #else //FIXED_POINT_MATH
    unsigned int nextRoundTime=Tr+bco; //Bounded to 20bits
    #endif //FIXED_POINT_MATH
    eTro=eTr;
    Trace::IRQaddToLog(31,SP_Tr);
    Trace::IRQaddToLog(31,Tr);
    Tr=0;//Reset round time
    int id = 0;
    for(Thread *it=threadList;it!=0;it=it->schedData.next)
    {
        //Recalculate per thread set point
        #ifndef SCHED_CONTROL_FIXED_POINT
        it->schedData.SP_Tp=static_cast<int>(
                it->schedData.alfa*nextRoundTime);
        #else //FIXED_POINT_MATH
        //nextRoundTime is bounded to 20bits, alfa to 12bits,
        //so the multiplication fits in 32bits
        it->schedData.SP_Tp=(it->schedData.alfa*nextRoundTime)/4096;
        #endif //FIXED_POINT_MATH

        //Run each thread internal regulator
//        int eTp=it->schedData.SP_Tp - it->schedData.Tp;
//        //note: since b and bo contain the real value multiplied by
//        //multFactor, this equals b=bo+eTp/multFactor.
//        int b=it->schedData.bo + eTp;
//        //saturation
//        it->schedData.bo=min(max(b,bMin*multFactor),bMax*multFactor);
        it->schedData.bo=it->schedData.SP_Tp*multFactor;
        Trace::IRQaddToLog(id,it->schedData.SP_Tp);
        Trace::IRQaddToLog(id,it->schedData.Tp);
        id++;
    }
    #ifdef ENABLE_REGULATOR_REINIT
    } else {
        reinitRegulator=false;
        Tr=0;//Reset round time
        //Reset state of the external regulator
        eTro=0;
        bco=0;

        int id = 0;
        for(Thread *it=threadList;it!=0;it=it->schedData.next)
        {
            //Recalculate per thread set point
            #ifndef SCHED_CONTROL_FIXED_POINT
            it->schedData.SP_Tp=static_cast<int>(it->schedData.alfa*SP_Tr);
            #else //FIXED_POINT_MATH
            //SP_Tr is bounded to 20bits, alfa to 12bits,
            //so the multiplication fits in 32bits
            it->schedData.SP_Tp=(it->schedData.alfa*SP_Tr)/4096;
            #endif //FIXED_POINT_MATH

            int b=it->schedData.SP_Tp*multFactor;
            it->schedData.bo=min(max(b,bMin*multFactor),bMax*multFactor);
            Trace::IRQaddToLog(id,it->schedData.SP_Tp);
            Trace::IRQaddToLog(id,it->schedData.Tp);
            id++;
        }
    }
    #endif //ENABLE_REGULATOR_REINIT
}

Thread *ControlScheduler::threadList=0;
unsigned int ControlScheduler::threadListSize=0;
Thread *ControlScheduler::curInRound=0;
Thread *ControlScheduler::idle=0;
int ControlScheduler::SP_Tr=0;
int ControlScheduler::Tr=bNominal;
int ControlScheduler::bco=0;
int ControlScheduler::eTro=0;
bool ControlScheduler::reinitRegulator=false;
volatile bool ControlScheduler::disableAutoAlfaChange;
}
#else
namespace miosix {

//These are defined in kernel.cpp
extern volatile Thread *cur;
extern unsigned char kernel_running;
extern IntrusiveList<SleepData> *sleepingList;
extern bool kernel_started;

//Internal
static ContextSwitchTimer *timer = nullptr;
static long long burstStart = 0;
static IntrusiveList<ThreadsListItem> activeThreads;
static IntrusiveList<ThreadsListItem>::iterator curInRound = activeThreads.end();
static long long nextPreemption = LONG_LONG_MAX;
static bool dontAdvanceCurInRound = false;

//
// class ControlScheduler
//

static inline void addThreadToActiveList(ThreadsListItem *atlEntry)
{
    
    switch (atlEntry->t->getPriority().getRealtime()){
        case REALTIME_PRIORITY_IMMEDIATE:
        {
            //In this case we should insert the woken thread before the current
            //item and put the pointer to the item behind it so that
            //IRQfindNextThread executes the woken thread and then come back
            //to this thread which is about to be preempted!!
            auto tmp = curInRound; tmp--;
            if (tmp==activeThreads.end()){ 
                // curInRound is the first Item and doing curInRound-- twice
                // raises an exceptional behavior in which puts the pointer
                // in the end of the list and IRQfindNextThread detects an
                // EndOfRound situation by mistake
                activeThreads.insert(curInRound,atlEntry);
                curInRound--;
                dontAdvanceCurInRound = true;
            }else{
                activeThreads.insert(curInRound,atlEntry);
                curInRound--;curInRound--;
            }
            //A preemption would occur right after this function
            break;
        }
        case REALTIME_PRIORITY_NEXT_BURST:
        {
            auto temp=curInRound;
            activeThreads.insert(++temp,atlEntry);
            //No preemption should occur after this function
            break;
        }
        default: //REALTIME_PRIORITY_END_OF_ROUND
        {
            activeThreads.push_back(atlEntry);
            //No preemption should occur after this function
        }
    }
}

static inline void remThreadfromActiveList(ThreadsListItem *atlEntry){
    // If the current thread in run has yielded and caused a call to this func.
    // the curInRound pointer must advance in the list so as not to lose the
    // track of the list and then we can delete the item
    if (*curInRound==atlEntry){
        curInRound++;
        // Since we are sure that IRQfindNextThread will be called afterwards,
        // it should be prevented to advance curInRound pointer again, otherwise
        // it will skip 1 thread's burst
        dontAdvanceCurInRound = true;
    }
    activeThreads.erase(IntrusiveList<ThreadsListItem>::iterator(atlEntry));
}

bool ControlScheduler::PKaddThread(Thread *thread,
        ControlSchedulerPriority priority)
{
    #ifdef SCHED_CONTROL_FIXED_POINT
    if(threadListSize>=64) return false;
    #endif //SCHED_CONTROL_FIXED_POINT
    thread->schedData.priority=priority;
    thread->schedData.atlEntry.t = thread;
    {
        //Note: can't use FastInterruptDisableLock here since this code is
        //also called *before* the kernel is started.
        //Using FastInterruptDisableLock would enable interrupts prematurely
        //and cause all sorts of misterious crashes
        InterruptDisableLock dLock;
        thread->schedData.next=threadList;
        threadList=thread;
        threadListSize++;
        SP_Tr+=bNominal; //One thread more, increase round time
        // Insert the thread in activeThreads list according to its real-time
        // priority
        if (thread->flags.isReady())
            addThreadToActiveList(&thread->schedData.atlEntry);
        if (disableAutoAlfaChange){
            disableAutoAlfaChange = false;
            IRQrecalculateAlfa();
            for(Thread *it=threadList;it!=0;it=it->schedData.next)
                it->schedData.alfaPrime=it->schedData.alfa;
            disableAutoAlfaChange=true;
            IRQrecalculateAlfa();
        }else
            IRQrecalculateAlfa();
        
    }
    return true;
}

bool ControlScheduler::PKexists(Thread *thread)
{
    if(thread==0) return false;
    for(Thread *it=threadList;it!=0;it=it->schedData.next)
    {
       if(it==thread)
       {
           if(it->flags.isDeleted()) return false; //Found, but deleted
           return true;
       }
    }
    return false;
}

void ControlScheduler::PKremoveDeadThreads()
{
    //Special case, threads at the head of the list
    while(threadList!=0 && threadList->flags.isDeleted())
    {
        Thread *toBeDeleted=threadList;
        {
            FastInterruptDisableLock dLock;
            threadList=threadList->schedData.next;
            threadListSize--;
            SP_Tr-=bNominal; //One thread less, reduce round time
        }
        void *base=toBeDeleted->watermark;
        toBeDeleted->~Thread();
        free(base); //Delete ALL thread memory
    }
    if(threadList!=0)
    {
        //General case, delete threads not at the head of the list
        for(Thread *it=threadList;it->schedData.next!=0;it=it->schedData.next)
        {
            if(it->schedData.next->flags.isDeleted()==false) continue;
            Thread *toBeDeleted=it->schedData.next;
            {
                FastInterruptDisableLock dLock;
                it->schedData.next=it->schedData.next->schedData.next;
                threadListSize--;
                SP_Tr-=bNominal; //One thread less, reduce round time
            }
            void *base=toBeDeleted->watermark;
            toBeDeleted->~Thread();
            free(base); //Delete ALL thread memory
        }
    }
    {
        FastInterruptDisableLock dLock;
        IRQrecalculateAlfa();
    }
}

void ControlScheduler::PKsetPriority(Thread *thread,
        ControlSchedulerPriority newPriority)
{
    thread->schedData.priority=newPriority;
    {
        FastInterruptDisableLock dLock;
        IRQrecalculateAlfa();
    }
}

void ControlScheduler::IRQsetIdleThread(Thread *idleThread)
{
    timer = &ContextSwitchTimer::instance();
    idleThread->schedData.priority=-1;
    idle=idleThread;
    //Initializing curInRound to end() so that the first time
    //IRQfindNextThread() is called the scheduling algorithm runs
    if(threadListSize!=1) errorHandler(UNEXPECTED);
    curInRound=activeThreads.end();
}

Thread *ControlScheduler::IRQgetIdleThread()
{
    return idle;
}

long long ControlScheduler::IRQgetNextPreemption()
{
    return nextPreemption;
}

// Should be called when the current thread is the idle thread
static inline void IRQsetNextPreemptionForIdle(){
    if (sleepingList->empty())
        //normally should not happen unless an IRQ is already set and able to
        //preempt idle thread
        nextPreemption = LONG_LONG_MAX; 
    else
        nextPreemption = sleepingList->front()->wakeup_time;
    timer->IRQsetNextInterrupt(nextPreemption);
}

// Should be called for threads other than idle thread
static inline void IRQsetNextPreemption(long long burst){
    long long firstWakeupInList;
    if (sleepingList->empty())
        firstWakeupInList = LONG_LONG_MAX;
    else
        firstWakeupInList = sleepingList->front()->wakeup_time;
    burstStart = timer->IRQgetCurrentTime();
    nextPreemption = min(firstWakeupInList,burstStart + burst);
    timer->IRQsetNextInterrupt(nextPreemption);
}

int bNominal=static_cast<int>(1000000)*rescaled;// 1ms (hartstone)
int bMax=static_cast<int>(10000000)*rescaled;// 10ms (hartstone)
void ControlScheduler::fixmeManualBurst(float burstLength)
{
    InterruptDisableLock dLock;
    bNominal=static_cast<int>(1000000000ll*burstLength)*rescaled;
    bMax=5*bNominal;
    SP_Tr=bNominal*threadListSize;
}

unsigned int ControlScheduler::IRQfindNextThread()
{
    // Warning: since this function is called within interrupt routines, it
    //is not possible to add/remove elements to threadList, since that would
    //require dynamic memory allocation/deallocation which is forbidden within
    //interrupts. Iterating the list is safe, though

    if(kernel_running!=0) return 0;//If kernel is paused, do nothing

    if(cur!=idle)
    {
        //Not preempting from the idle thread, store actual burst time of
        //the preempted thread
        int Tp = static_cast<int>(timer->IRQgetCurrentTime() - burstStart);
        cur->schedData.Tp+=Tp;
        //It's multiburst => deduct consumed burst so that if activated again in
        //this burst, the thread can not run for more than allotted time to it
        cur->schedData.bo-=(Tp*multFactor);
        Tr+=Tp;
    }

    //Find next thread to run
    for(;;)
    {
        //if(curInRound!=0) curInRound=curInRound->schedData.next;
        if(curInRound!=activeThreads.end()){
            if (dontAdvanceCurInRound)
                dontAdvanceCurInRound = false;
            else
                curInRound++;
        }
        if(curInRound==activeThreads.end()) //Note: do not replace with an else
        {
            //Check these two statements:
            //- If all threads are not ready, the scheduling algorithm must be
            //  paused and the idle thread is run instead
            //- If the inner integral regulator of all ready threads saturated
            //  then the integral regulator of the outer regulator must stop
            //  increasing because the set point cannot be attained anyway.
            bool allReadyThreadsSaturated=true;
            for (auto it=activeThreads.begin() ; it!=activeThreads.end() ; it++)
            {
                if((*it)->t->schedData.bo<bMax*multFactor){
                        allReadyThreadsSaturated=false;
                        //Found a counterexample for both statements,
                        //no need to scan the list further.
                        break;
                }
            }
            if(activeThreads.empty())
            {
                //No thread is ready, run the idle thread

                //This is very important: the idle thread can *remove* dead
                //threads from threadList, so it can invalidate iterators
                //to any element except theadList.end()
                curInRound=activeThreads.end();
                cur=idle;
                ctxsave=cur->ctxsave;
                #ifdef WITH_PROCESSES
                miosix_private::MPUConfiguration::IRQdisable();
                #endif
                //miosix_private::AuxiliaryTimer::IRQsetValue(bIdle); //curTime + burst
                IRQsetNextPreemptionForIdle();
                return 0;
            }

            //End of round reached, run scheduling algorithm
            //curInRound=threadList;
            curInRound = activeThreads.front();
//            long long eorTime = timer.IRQgetCurrentTime();
            IRQrunRegulator(allReadyThreadsSaturated);
        }

        if((*curInRound)->t->flags.isReady() && (*curInRound)->t->schedData.bo>0) 
        {
            //Found a READY thread, so run this one
            cur=(*curInRound)->t;
            #ifdef WITH_PROCESSES
            if(const_cast<Thread*>(cur)->flags.isInUserspace()==false)
            {
                ctxsave=cur->ctxsave;
                miosix_private::MPUConfiguration::IRQdisable();
            } else {
                ctxsave=cur->userCtxsave;
                //A kernel thread is never in userspace, so the cast is safe
                static_cast<Process*>(cur->proc)->mpu.IRQenable();
            }
            #else //WITH_PROCESSES
            ctxsave=cur->ctxsave;
            #endif //WITH_PROCESSES
            IRQsetNextPreemption(cur->schedData.bo/multFactor);
            return 0;
        } else {
            //The thread has no remaining burst time => just ignore it
            curInRound++;
        }
    }
}

void ControlScheduler::IRQwaitStatusHook(Thread* t)
{
    // Managing activeThreads list
    if (t->flags.isReady()){
        // The thread has became active -> put it in the list
        addThreadToActiveList(&t->schedData.atlEntry);
    }else {
        // The thread is no longer active -> remove it from the list
        remThreadfromActiveList(&t->schedData.atlEntry);
    }
}

void ControlScheduler::disableAutomaticAlfaChange()
 {
    InterruptDisableLock dLock;
    for(Thread *it=threadList;it!=0;it=it->schedData.next)
        it->schedData.alfaPrime=it->schedData.alfa;
    disableAutoAlfaChange=true;
    IRQrecalculateAlfa();
}

void ControlScheduler::enableAutomaticAlfaChange()
{
    InterruptDisableLock dLock;
    disableAutoAlfaChange=false;
    IRQrecalculateAlfa();
}

void ControlScheduler::IRQrecalculateAlfa()
{
    //IRQrecalculateAlfa rewritten from scratch to implement full workload
    //div deadline algorithm
    #if defined(FIXED_POINT_MATH) || !defined(ENABLE_FEEDFORWARD) || \
       !defined(ENABLE_REGULATOR_REINIT)
    #error "Workload div deadline preconditions not met"
    #endif
    if(disableAutoAlfaChange==false)
    { //Alfas should be computed automatically
        //Sum of all priorities of all threads
        //Note that since priority goes from 0 to PRIORITY_MAX-1
        //but priorities we need go from 1 to PRIORITY_MAX we need to add one
        unsigned int sumPriority=0;
        for (Thread *it=threadList;it!=0;it=it->schedData.next)
        {
            //Count only ready threads
            sumPriority+=it->schedData.priority.get()+1;//Add one
        }
        //This can happen when ENABLE_FEEDFORWARD is set and no thread is ready
        if(sumPriority==0) return;
        float base=1.0f/((float)sumPriority);
        for (Thread *it=threadList ; it!=0 ; it=it->schedData.next)
        {
            //Assign zero bursts to blocked threads
            it->schedData.alfa=base*((float)(it->schedData.priority.get()+1));
        }
    }else{ //Alfas should be as the user set them
        float cpuLoad=0.0f;
        for(Thread *it=threadList;it!=0;it=it->schedData.next)
        {
            it->schedData.alfa=it->schedData.alfaPrime;
            cpuLoad+=it->schedData.alfaPrime;            
        }
        if(cpuLoad<1.0f)
	{
            for(Thread *it=threadList;it!=0;it=it->schedData.next)
                it->schedData.alfa/=cpuLoad;
    	} else {
            float rescale=0.0f;
            for(Thread *it=threadList;it!=0;it=it->schedData.next)
            {
                it->schedData.alfa=it->schedData.alfa*it->schedData.theta;
                rescale+=it->schedData.alfa;
            }
            for(Thread *it=threadList;it!=0;it=it->schedData.next)
                it->schedData.alfa/=rescale;
	}
    }
    reinitRegulator=true;
}

void ControlScheduler::IRQrunRegulator(bool allReadyThreadsSaturated)
{
    using namespace std;
    #ifdef SCHED_CONTROL_FIXED_POINT
    //The fixed point scheduler may overflow if Tr is higher than this
    Tr=min(Tr,524287);
    #endif //FIXED_POINT_MATH
    #ifdef ENABLE_REGULATOR_REINIT
    if(reinitRegulator==false)
    {
    #endif //ENABLE_REGULATOR_REINIT
        int eTr=SP_Tr-Tr;
        #ifndef SCHED_CONTROL_FIXED_POINT
        int bc=bco+static_cast<int>(krr*eTr-krr*zrr*eTro);
        #else //FIXED_POINT_MATH
        //Tr is clamped to 524287, so eTr uses at most 19bits. Considering
        //the 31bits of a signed int, we have 12bits free.
        const int fixedKrr=static_cast<int>(krr*2048);
        const int fixedKrrZrr=static_cast<int>(krr*zrr*1024);
        int bc=bco+(fixedKrr*eTr)/2048-(fixedKrrZrr*eTro)/1024;
        #endif //FIXED_POINT_MATH
        if(allReadyThreadsSaturated)
        {
            //If all inner regulators reached upper saturation,
            //allow only a decrease in the burst correction.
            if(bc<bco) bco=bc;
        } else bco=bc;

        bco=min<int>(max(bco,-Tr),bMax*threadListSize);
        #ifndef SCHED_CONTROL_FIXED_POINT
        float nextRoundTime=static_cast<float>(Tr+bco);
        #else //FIXED_POINT_MATH
        unsigned int nextRoundTime=Tr+bco; //Bounded to 20bits
        #endif //FIXED_POINT_MATH
        eTro=eTr;
        Trace::IRQaddToLog(31,SP_Tr);
        Trace::IRQaddToLog(31,Tr);
        Tr=0;//Reset round time
        int id = 0;
        for(Thread *it=threadList;it!=0;it=it->schedData.next)
        {
            //Recalculate per thread set point
            #ifndef SCHED_CONTROL_FIXED_POINT
            it->schedData.SP_Tp=static_cast<int>(
                    it->schedData.alfa*nextRoundTime);
            #else //FIXED_POINT_MATH
            //nextRoundTime is bounded to 20bits, alfa to 12bits,
            //so the multiplication fits in 32bits
            it->schedData.SP_Tp=(it->schedData.alfa*nextRoundTime)/4096;
            #endif //FIXED_POINT_MATH

//            //Run each thread internal regulator
//            int eTp=it->schedData.SP_Tp - it->schedData.Tp;
//            //note: since b and bo contain the real value multiplied by
//            //multFactor, this equals b=bo+eTp/multFactor.
//            int b=it->schedData.bo + eTp;
            int b=it->schedData.SP_Tp*multFactor;
            //saturation
            it->schedData.bo=min(max(b,bMin*multFactor),bMax*multFactor);
            Trace::IRQaddToLog(id,it->schedData.SP_Tp);
            Trace::IRQaddToLog(id,it->schedData.Tp);
            id++;
        }
    #ifdef ENABLE_REGULATOR_REINIT
    } else {
        reinitRegulator=false;
        Tr=0;//Reset round time
        //Reset state of the external regulator
        eTro=0;
        bco=0;

        int id = 0;
        for(Thread *it=threadList;it!=0;it=it->schedData.next)
        {
            //Recalculate per thread set point
            #ifndef SCHED_CONTROL_FIXED_POINT
            it->schedData.SP_Tp=static_cast<int>(it->schedData.alfa*SP_Tr);
            #else //FIXED_POINT_MATH
            //SP_Tr is bounded to 20bits, alfa to 12bits,
            //so the multiplication fits in 32bits
            it->schedData.SP_Tp=(it->schedData.alfa*SP_Tr)/4096;
            #endif //FIXED_POINT_MATH

            int b=it->schedData.SP_Tp*multFactor;
            it->schedData.bo=min(max(b,bMin*multFactor),bMax*multFactor);
            Trace::IRQaddToLog(id,it->schedData.SP_Tp);
            Trace::IRQaddToLog(id,it->schedData.Tp);
            id++;
        }
    }
    #endif //ENABLE_REGULATOR_REINIT
}

Thread *ControlScheduler::threadList=0;
unsigned int ControlScheduler::threadListSize=0;
//Thread *ControlScheduler::curInRound=0;
Thread *ControlScheduler::idle=0;
int ControlScheduler::SP_Tr=0;
int ControlScheduler::Tr=bNominal;
int ControlScheduler::bco=0;
int ControlScheduler::eTro=0;
bool ControlScheduler::reinitRegulator=false;
volatile bool ControlScheduler::disableAutoAlfaChange=false;
} //namespace miosix
#endif // SCHED_CONTROL_MULTIBURST
#endif //SCHED_TYPE_CONTROL_BASED