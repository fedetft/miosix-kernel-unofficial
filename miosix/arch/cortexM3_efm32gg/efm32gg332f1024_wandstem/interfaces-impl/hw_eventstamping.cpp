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

static std::mutex eventMutex;
static Thread* eventThread = nullptr;
static std::atomic<bool>event(false);
static std::atomic<bool>eventTimeout(false);

///
// Actual interface 
///

//static VirtualClockSpec * vc = &VirtualClockSpec::instance();
static Hsc * hsc = &Hsc::instance();
static TimeConversion tc(Hsc::IRQTimerFrequency()); // FIXME: (s) if i use hsc->tc doesn't work!

// SFD, STXON, TIMESTAMP_IN_OUT //< channels
/*enum class Channel
{
    SFD,
    STXON,
    TIMESTAMP_IN_OUT
};*/

static EventDirection currentDirection = EventDirection::DISABLED;

inline void configureEventHelper(GpioPin pinChannel, EventDirection direction)
{
    // TODO: (s) svegliare trigger con sempre gpioirq e non timeout
    if(direction == EventDirection::INPUT)
    {
        try // register GPIO irq
        {
            registerGpioIrq(pinChannel, GpioIrqEdge::RISING, [](){  
                    FastInterruptDisableLock dLock;
                    events::IRQsignalEvent();
            });
        }
        catch(const std::runtime_error& error)
        {
            // GPIO irq already previosly registered, no need to register it again
        }
    }
    else if(direction == EventDirection::DISABLED)
        unregisterGpioIrq(pinChannel);
    else ; // OUTPUT direction, wired using alternate functions (see routing table in RM) (uses timeout to wake up)
}

void configureEvent(Channel channel, EventDirection direction)
{
    // check channel compatibility
    if(channel == Channel::STXON && direction == EventDirection::INPUT)
        throw BadEventTimerConfiguration();
    else if(channel == Channel::SFD && direction == EventDirection::OUTPUT)
        throw BadEventTimerConfiguration();
    else ; // other...

    // register channel GPIO irq to wake up waiting thread
    switch (channel)
    {
    case Channel::SFD: // PA8, excChB including SFD and FRM_DONE
        configureEventHelper(transceiver::excChB::getPin(), direction);
        break;
    case Channel::STXON: // PA9
        configureEventHelper(transceiver::stxon::getPin(), direction);
        break;
    default: // PE12, Channel::TIMESTAMP_IN_OUT: 
        configureEventHelper(expansion::gpio10::getPin(), direction);
        break;
    }

    // save event direction state
    currentDirection = direction;

    {
        FastInterruptDisableLock dLock;

        // default is disabled state
        uint32_t TIMER1_FLAGS = 0;
        uint32_t TIMER2_FLAGS = 0;
        
        // INPUT_CAPTURE
        if(direction == EventDirection::INPUT)
        {
            // configure timer as input capture triggered by PRS
            TIMER1_FLAGS = TIMER_CC_CTRL_ICEDGE_RISING | TIMER_CC_CTRL_FILT_DISABLE | TIMER_CC_CTRL_INSEL_PRS
                                    | TIMER_CC_CTRL_PRSSEL_PRSCH0 | TIMER_CC_CTRL_MODE_INPUTCAPTURE;
            TIMER2_FLAGS = TIMER_CC_CTRL_ICEDGE_RISING | TIMER_CC_CTRL_FILT_DISABLE | TIMER_CC_CTRL_INSEL_PRS 
                                    | TIMER_CC_CTRL_PRSSEL_PRSCH0 | TIMER_CC_CTRL_MODE_INPUTCAPTURE;
        }
        // OUTPUT_COMPARE
        else if(direction == EventDirection::OUTPUT)
        {
            // configure timer as output compare
            TIMER1_FLAGS = TIMER_CC_CTRL_MODE_OUTPUTCOMPARE;
            TIMER2_FLAGS = TIMER_CC_CTRL_MODE_OUTPUTCOMPARE; 

            // connect TIMER2 output using routing table //< set and reset moved into TIMER2_ISR
            //TIMER2->ROUTE |= TIMER_ROUTE_CC1PEN;
            //TIMER2->CC[1].CTRL |= TIMER_CC_CTRL_CMOA_SET;
            //TIMER2->ROUTE |= TIMER_ROUTE_LOCATION_LOC0;
        }
        else ; // DISABLED

        TIMER1->CC[2].CTRL = TIMER1_FLAGS;
        TIMER2->CC[2].CTRL = TIMER2_FLAGS;
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
    return absoluteWaitEvent(channel, getTime() + timeoutNs); 
}
std::pair<EventResult, long long> absoluteWaitEvent(Channel channel, long long absoluteTimeoutNs)
{
    // check if channel has been configured correctly (INPUT_CAPTURE)
    if (currentDirection != EventDirection::INPUT) throw BadEventTimerConfiguration();

    // lock guard, this allows only one thread to access timeout timer per time
    std::unique_lock<std::mutex> lck(eventMutex, std::try_to_lock);
    bool gotLock = lck.owns_lock();
    if (!gotLock) return std::make_pair(EventResult::BUSY, 0);

    FastInterruptDisableLock dLock;

    // connect correct channel
    uint32_t PRS_FLAGS = 0;

    // connect TIMER to GPIO using PRS (GPIOH--PRS-->TIMER)
    PRS_FLAGS = PRS_CH_CTRL_SOURCESEL_GPIOH;
    
    // selecting right channel for PRS binding and enable IRQ on channel
    switch (channel)
    {
    case Channel::SFD: // PA8, excChB including SFD and FRM_DONE
        enableGpioIrq(transceiver::excChB::getPin());
        PRS_FLAGS |= PRS_CH_CTRL_SIGSEL_GPIOPIN8;
        break;
    case Channel::STXON: // PA9, STXON
        enableGpioIrq(transceiver::stxon::getPin());
        PRS_FLAGS |= PRS_CH_CTRL_SIGSEL_GPIOPIN9;
        break;
    default: // PE12, Channel::TIMESTAMP_IN_OUT: 
        enableGpioIrq(expansion::gpio10::getPin());
        PRS_FLAGS |= PRS_CH_CTRL_SIGSEL_GPIOPIN12;
        break;
    }

    PRS->CH[0].CTRL = PRS_FLAGS;

    // uncorrect time
    long long absoluteTimeoutNsTsnc = vc->IRQuncorrectTimeNs(absoluteTimeoutNs);

    // if-guard, event in the past
    if(hsc->IRQgetTimeNs() > absoluteTimeoutNsTsnc) return std::make_pair(EventResult::EVENT_TIMEOUT, 0);

    // set up timeout timer
    hsc->IRQsetEventNs(absoluteTimeoutNsTsnc);

    // clean timestamp buffer
    Hsc::clearTimestampBuffer();

    // enable timers interrupt (PRS triggers both at the same time, just one needed)
    TIMER2->IEN |= TIMER_IEN_CC2;
    //TIMER1->IEN |= TIMER_IEN_CC2;

    // register current thread for wakeup
    eventThread = Thread::IRQgetCurrentThread();

    event = false;
    eventTimeout = false;
    // wait for condition or timeout interrupt  (avoids spurious wakeups)
    while(hsc->IRQgetTimeNs() <= absoluteTimeoutNsTsnc && !event && !eventTimeout)
    {
        // putting thread to sleep, woken up by either timeout or desired interrupt
        Thread::IRQwait();
        {
            FastInterruptEnableLock eLock(dLock);
            Thread::yield();
        }
    }
    
    // disable timer interrupts
    TIMER2->IEN &= ~TIMER_IEN_CC2;
    //TIMER1->IEN &= ~TIMER_IEN_CC2;

    // reset timeout timer
    Hsc::IRQclearEventFlag();

    // reset event status
    eventThread = nullptr;

    // disconnect GPIOH--PRS-->TIMER connection
    PRS->CH[0].CTRL = 0;

    // disable IRQ on channel
    switch (channel)
    {
    case Channel::SFD: // PA8, excChB including SFD and FRM_DONE
        disableGpioIrq(transceiver::excChB::getPin());
        break;
    case Channel::STXON: // PA9, STXON
        disableGpioIrq(transceiver::stxon::getPin());
        break;
    default: // PE12, Channel::TIMESTAMP_IN_OUT: 
        disableGpioIrq(expansion::gpio10::getPin());
        break;
    }

    // event received before timeout
    if(event)
    {
        // read captured timer value
        long long timestampTick = hsc->upperTimeTick | Hsc::IRQgetEventTimestamp(); //(TIMER2->CC[2].CCV<<16) | TIMER1->CC[2].CCV;
        long long timestampNs = tc.tick2ns(timestampTick);

        // correct time
        long long correctedTimestampNs = vc->IRQcorrectTimeNs(timestampNs);

        return std::make_pair(EventResult::EVENT, correctedTimestampNs);
    }

    // timeout
    return std::make_pair(EventResult::EVENT_TIMEOUT, 0);
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
    // check if channel has been configured correctly (OUTPUT_COMPARE)
    if (currentDirection != EventDirection::OUTPUT) throw BadEventTimerConfiguration();

    // lock guard, this allows only one thread to access timeout timer per time
    std::unique_lock<std::mutex> lck(eventMutex, std::try_to_lock);
    bool gotLock = lck.owns_lock();
    if (!gotLock) return EventResult::BUSY;
    
    FastInterruptDisableLock dLock;

    // uncorrect time
    long long absoluteNsTsnc = vc->IRQuncorrectTimeNs(absoluteNs);

    // if-guard, event in the past
    if(hsc->IRQgetTimeNs() > absoluteNsTsnc) return EventResult::TRIGGER_IN_THE_PAST;

    // register current thread for wakeup
    eventThread = Thread::IRQgetCurrentThread();

    // configure event on timer side
    hsc->IRQsetEventNs(absoluteNsTsnc);
    greenLedOff();
    eventTimeout = false;
    // wait for trigger to be fired (avoids spurious wakeups)
    while(hsc->IRQgetTimeNs() <= absoluteNsTsnc && !eventTimeout)
    {
        // putting thread to sleep, woken up by either timeout or desired interrupt
        Thread::IRQwait();
        {
            FastInterruptEnableLock eLock(dLock);
            Thread::yield();
        }
    }

    // reset timeout timer
    Hsc::IRQclearEventFlag();

    // reset event status
    eventThread = nullptr;

    // disable timer interrupts
    TIMER2->IEN &= ~TIMER_IEN_CC1;
    TIMER1->IEN &= ~TIMER_IEN_CC1;

    return EventResult::TRIGGER;
}

///
// Signals
///

void IRQsignalEvent()         
{ 
    if(eventThread)
    {
        event=true; 
        eventThread->IRQwakeup();  
        if(eventThread->IRQgetPriority()>Thread::IRQgetCurrentThread()->IRQgetPriority())
		    Scheduler::IRQfindNextThread();
    }
}
void IRQsignalEventTimeout()  
{  
    //iprintf("X\n");
    if(eventThread)
    {
        eventTimeout=true; 
        eventThread->IRQwakeup(); 
        if(eventThread->IRQgetPriority()>Thread::IRQgetCurrentThread()->IRQgetPriority())
		    Scheduler::IRQfindNextThread();
    } 
}

EventDirection getEventDirection() { return currentDirection; }

} // namespace events

} // namespace miosix