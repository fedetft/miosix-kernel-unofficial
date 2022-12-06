/***************************************************************************
 *   Copyright (C) 2022 by Sorrentino Alessandro                           *
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

#include "interfaces/hw_eventstamping.h"
#include "hw_eventstamping_impl.h"
#include "time_types_spec.h"
#include "miosix.h"
#include <tuple>
#include <mutex>
#include <atomic>
#include "hwmapping.h"
#include "interfaces/gpio.h"
#include <exception>

namespace miosix
{

namespace events
{

// forward declaration
enum class Channel;

///
// Event and wait support variables
///

// [0] is for SFD_STXON, [1] is for TIMESTAMP_IN/OUT
static Thread * waitingThread[2] = { nullptr, nullptr };
static std::mutex eventMutex[2];
static std::atomic<bool> timeout(false);

///
// Actual interface 
///

// SFD, STXON, TIMESTAMP_IN_OUT //< channels
/*enum class Channel
{
    SFD,
    STXON,
    TIMESTAMP_IN_OUT
};*/

// [0] is for SFD_STXON, [1] is for TIMESTAMP_IN/OUT
static EventDirection currentDirection[2] = { EventDirection::DISABLED, EventDirection::DISABLED };


void configureEvent(Channel channel, EventDirection direction)
{
    if(channel == Channel::SFD_STXON)
    {
        switch (direction)
        {
        case EventDirection::INPUT:  // SFD
            // configure timers
            TIMER2->CC[1].CTRL = TIMER_CC_CTRL_ICEDGE_RISING | TIMER_CC_CTRL_FILT_DISABLE | TIMER_CC_CTRL_INSEL_PRS
                                    | TIMER_CC_CTRL_PRSSEL_PRSCH0 | TIMER_CC_CTRL_MODE_INPUTCAPTURE;
            TIMER3->CC[1].CTRL = TIMER_CC_CTRL_ICEDGE_RISING | TIMER_CC_CTRL_FILT_DISABLE | TIMER_CC_CTRL_INSEL_PRS
                                    | TIMER_CC_CTRL_PRSSEL_PRSCH0 | TIMER_CC_CTRL_MODE_INPUTCAPTURE;

            // connect timers to PRS
            PRS->CH[0].CTRL = PRS_CH_CTRL_SOURCESEL_GPIOH | PRS_CH_CTRL_SIGSEL_GPIOPIN8;

            break;
        case EventDirection::OUTPUT: // STXON
            // reset timers if we were in INPUT mode
            TIMER2->CC[1].CTRL = 0;
            PRS->CH[0].CTRL = 0;

            // configure timer
            TIMER2->CC[1].CTRL = TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;
            break;
        default: // EventDirection::DISABLED
            TIMER2->CC[1].CTRL = 0;
            TIMER3->CC[1].CTRL = 0;
            PRS->CH[0].CTRL = 0;
            break;
        }
        currentDirection[0] = direction;
    }
    else // Channel::TIMESTAMP_IN_OUT
    {
        switch (direction)
        {
        case EventDirection::INPUT: // TIMESTAMP_IN
            // configure timers
            TIMER1->CC[2].CTRL = TIMER_CC_CTRL_ICEDGE_RISING | TIMER_CC_CTRL_FILT_DISABLE | TIMER_CC_CTRL_INSEL_PRS
                                    | TIMER_CC_CTRL_PRSSEL_PRSCH1 | TIMER_CC_CTRL_MODE_INPUTCAPTURE;
            TIMER3->CC[2].CTRL = TIMER_CC_CTRL_ICEDGE_RISING | TIMER_CC_CTRL_FILT_DISABLE | TIMER_CC_CTRL_INSEL_PRS
                                    | TIMER_CC_CTRL_PRSSEL_PRSCH1 | TIMER_CC_CTRL_MODE_INPUTCAPTURE;

            // connect timers to PRS
            PRS->CH[1].CTRL = PRS_CH_CTRL_SOURCESEL_GPIOH | PRS_CH_CTRL_SIGSEL_GPIOPIN12;
            break;
        case EventDirection::OUTPUT: // TIMESTAMP_OUT
            // reset timers if we were in INPUT mode
            TIMER1->CC[2].CTRL = 0;
            TIMER3->CC[2].CTRL = 0;
            PRS->CH[1].CTRL = 0;

            // configure timer
            TIMER1->CC[2].CTRL = TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;
            break;
        default: // EventDirection::DISABLED
            TIMER1->CC[2].CTRL = 0;
            TIMER3->CC[2].CTRL = 0;
            PRS->CH[1].CTRL = 0;
            break;
        }
        currentDirection[1] = direction;
    }
}


///
// Event or timeout
///

std::pair<EventResult, long long> waitEvent(Channel channel)
{
    return waitEvent(channel, std::numeric_limits<long long>::max());
}
std::pair<EventResult, long long> waitEvent(Channel channel, long long timeoutNs) 
{ 
    //cap maximum absolute ns value to __LONG_LONG_MAX__
    long long now;
    {
        FastInterruptDisableLock dLock;

        now = IRQgetTime();
        if (timeoutNs > (std::numeric_limits<long long>::max() - now))
            timeoutNs = std::numeric_limits<long long>::max() - now;
    }
    // absolute wait 
    // note: if interrupts preempt this up to the point where the absoluteTime is 
    // in the past, absoluteWaitEvent will handle this
    return absoluteWaitEvent(channel, now + timeoutNs); 
}
std::pair<EventResult, long long> absoluteWaitEvent(Channel channel, long long absoluteTimeoutNs)
{
    // dispatch SFD or TIMESTAMP_IN
    int channelIndex = channel == Channel::SFD_STXON ? 0 : 1;

    // lock guard, this allows only one thread to access timeout timer per time for SFD_STXON and TIMESTAMP_IN_OUT
    std::unique_lock<std::mutex> lck(eventMutex[channelIndex], std::try_to_lock);
    bool gotLock = lck.owns_lock();
    if (!gotLock) return std::make_pair(EventResult::BUSY, 0);

    FastInterruptDisableLock dLock;

    // uncorrect time
    long long absoluteTimeoutNsTsnc = vc->IRQuncorrectTimeNs(absoluteTimeoutNs);

    // if-guard, event in the past
    if(hsc->IRQgetTimeNs() > absoluteTimeoutNsTsnc) return std::make_pair(EventResult::EVENT_TIMEOUT, 0);

    // set up timeout timer
    if(channel == Channel::SFD_STXON)
        hsc->IRQsetTimeoutNs(absoluteTimeoutNsTsnc);
    else // Channel::TIMESTAMP_IN_OUT
        ; // timeout for TIMESTAMP_IN not implemented. Future development

    // clean timestamp buffer (and BCCV)
    if(channel == Channel::SFD_STXON)
    {
        TIMER2->CC[1].CCV;  TIMER2->CC[1].CCV;
        TIMER3->CC[1].CCV;  TIMER3->CC[1].CCV;
    }
    else // Channel::TIMESTAMP_IN_OUT
    {
        TIMER1->CC[2].CCV;  TIMER1->CC[2].CCV;
        TIMER3->CC[2].CCV;  TIMER3->CC[2].CCV;
    }

    // enable timers interrupt (PRS triggers both at the same time, just one needed)
    if(channel == Channel::SFD_STXON)
        TIMER2->IEN |= TIMER_IEN_CC1;
    else // Channel::TIMESTAMP_IN_OUT
        TIMER3->IEN |= TIMER_IEN_CC2;

    // register current thread for wakeup
    waitingThread[channelIndex] = Thread::IRQgetCurrentThread();

    // wait for condition or timeout interrupt  (avoids spurious wakeups)
    while(hsc->IRQgetTimeNs() <= absoluteTimeoutNsTsnc && waitingThread[channelIndex] != nullptr)
    {
        // putting thread to sleep, woken up by either timeout or desired interrupt
        Thread::IRQwait();
        {
            FastInterruptEnableLock eLock(dLock);
            Thread::yield();
        }
    }

    if(waitingThread[channelIndex] != nullptr) timeout = true; // spurious wakeup and we got past timeout time
    waitingThread[channelIndex] = nullptr;

    // stop timeout in case of event
    TIMER2->IEN &= ~TIMER_IEN_CC2;
    TIMER2->CC[2].CTRL &= ~TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;

    // disable timer interrupts attached to PRS for timestamping 
    if(channel == Channel::SFD_STXON)
        TIMER2->IEN &= ~TIMER_IEN_CC1;
    else // Channel::TIMESTAMP_IN_OUT
        TIMER3->IEN &= ~TIMER_IEN_CC2;

    // if timeout, we got past time and thread is still not a nullptr
    //bool timeout = waitingThread[channelIndex] != nullptr;

    if(timeout) // timeout
    {
        timeout=false;
        waitingThread[0] = nullptr;
        return std::make_pair(EventResult::EVENT_TIMEOUT, 0);
    }
    
    // read captured timer value
    long long timestampLower;
    if(channel == Channel::SFD_STXON)
        timestampLower = (TIMER3->CC[1].CCV<<16) | TIMER2->CC[1].CCV;
    else // Channel::TIMESTAMP_IN_OUT
        timestampLower = (TIMER3->CC[2].CCV<<16) | TIMER1->CC[2].CCV;

    long long timestampTick = hsc->upperTimeTick | timestampLower;
    long long timestampNs = hsc->tc.tick2ns(timestampTick);

    // correct time
    long long correctedTimestampNs = vc->IRQcorrectTimeNs(timestampNs);

    return std::make_pair(EventResult::EVENT, correctedTimestampNs);

}

///
// Trigger
///

EventResult triggerEvent(Channel channel, long long ns) 
{ 
    return absoluteTriggerEvent(channel, getTime() + ns); 
}

EventResult absoluteTriggerEvent(Channel channel, long long absoluteNs)
{
    // dispatch STXON or TIMESTAMP_OUT
    int channelIndex = channel == Channel::SFD_STXON ? 0 : 1;

    // lock guard, this allows only one thread to access trigger one timer per time for SFD_STXON and TIMESTAMP_IN_OUT
    std::unique_lock<std::mutex> lck(eventMutex[channelIndex], std::try_to_lock);
    bool gotLock = lck.owns_lock();
    if (!gotLock) return EventResult::BUSY;
    
    FastInterruptDisableLock dLock;

    // uncorrect time
    long long absoluteNsTsnc = vc->IRQuncorrectTimeNs(absoluteNs);

    // register current thread for wakeup
    waitingThread[channelIndex] = Thread::IRQgetCurrentThread();

    // configure event on timer side
    long long absoluteTickTsnc = hsc->tc.ns2tick(absoluteNsTsnc);
    bool ok = hsc->IRQsetTriggerMatchReg(absoluteTickTsnc, channel == Channel::SFD_STXON);
    if(!ok) return EventResult::TRIGGER_IN_THE_PAST;

    // wait for trigger to be fired (avoids spurious wakeups)
    while(waitingThread[channelIndex] != nullptr)
    {
        // putting thread to sleep, woken up by either timeout or desired interrupt
        Thread::IRQwait();
        {
            FastInterruptEnableLock eLock(dLock);
            Thread::yield();
        }
    }
    //assert(hsc->IRQgetTimeNs() >= absoluteNsTsnc); // check correct wakeup time

    // (2) CMOA cleared, we can terminate trigger procedure 
    // disabling interrupts and output compare along with location reset is done here,
    // assuming (resonably) that the OUTPUT_COMPARE to clear the CMOA (scheduled 10 ticks after the match)
    // will be fired while exiting from interrupt and restoring the context.
    // (see os_timer.cpp TIMER1_IRQHandlerImpl or TIMER2_IRQHandlerImpl for (0) and (1))
    if(channel == Channel::SFD_STXON) // STXON
    {
        // disable output compare interrupt on channel 1 for least significant timer
        TIMER2->IEN &= ~TIMER_IEN_CC1;
        TIMER2->CC[1].CTRL &= ~TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;
        
        // disconnect TIMER2->CC1 from PEN completely
        TIMER2->CC[1].CTRL &= ~TIMER_CC_CTRL_CMOA_CLEAR;
        TIMER2->ROUTE &= ~TIMER_ROUTE_LOCATION_LOC0;
        TIMER2->ROUTE &= ~TIMER_ROUTE_CC1PEN;
    }
    else // Channel::TIMESTAMP_IN_OUT
    {
        TIMER1->IEN &= ~TIMER_IEN_CC2;
        TIMER1->CC[2].CTRL &= ~TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;
        
        // disconnect TIMER2->CC1 from PEN completely
        TIMER1->CC[2].CTRL &= ~TIMER_CC_CTRL_CMOA_CLEAR;
        TIMER1->ROUTE &= ~TIMER_ROUTE_LOCATION_LOC1;
        TIMER1->ROUTE &= ~TIMER_ROUTE_CC2PEN;
    }

    return EventResult::TRIGGER;
}

///
// Signals
///

void IRQsignalEventSFD()         
{ 
    if(waitingThread[0])
    {
        waitingThread[0]->IRQwakeup();  
        if(waitingThread[0]->IRQgetPriority()>Thread::IRQgetCurrentThread()->IRQgetPriority())
		    Scheduler::IRQfindNextThread();

        waitingThread[0] = nullptr;
    }
}

void IRQsignalEventTIMESTAMP_IN()         
{ 
    if(waitingThread[1])
    {
        waitingThread[1]->IRQwakeup();  
        if(waitingThread[1]->IRQgetPriority()>Thread::IRQgetCurrentThread()->IRQgetPriority())
		    Scheduler::IRQfindNextThread();
        
        waitingThread[1] = nullptr;
    }
}

void IRQsignalEventSTXON()         
{ 
    if(waitingThread[0])
    {
        waitingThread[0]->IRQwakeup();  
        if(waitingThread[0]->IRQgetPriority()>Thread::IRQgetCurrentThread()->IRQgetPriority())
		    Scheduler::IRQfindNextThread();

        waitingThread[0] = nullptr;
    }
}

void IRQsignalEventTIMESTAMP_OUT()         
{ 
    if(waitingThread[1])
    {
        waitingThread[1]->IRQwakeup();  
        if(waitingThread[1]->IRQgetPriority()>Thread::IRQgetCurrentThread()->IRQgetPriority())
		    Scheduler::IRQfindNextThread();
        
        waitingThread[1] = nullptr;
    }
}

void IRQsignalEventTimeout()  // only for SFD
{  
    if(waitingThread[0])
    {
        waitingThread[0]->IRQwakeup(); 
        if(waitingThread[0]->IRQgetPriority()>Thread::IRQgetCurrentThread()->IRQgetPriority())
		    Scheduler::IRQfindNextThread();

        waitingThread[0] = nullptr;
        timeout=true;
    } 
}

std::pair<EventDirection, EventDirection> getEventsDirection() 
{ 
    return std::make_pair(currentDirection[0], currentDirection[1]); 
}

} // namespace events

} // namespace miosix
