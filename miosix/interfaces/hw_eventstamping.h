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

#pragma once

#include <tuple>

namespace miosix
{

namespace events
{

// forward declaration
//enum class Channel;

// FIXME: (s) temporary solution, should go in cpp but cannot do forward declaration!
enum class Channel
{
    SFD_STXON,
    TIMESTAMP_IN_OUT
};

///
// Event and wait support structs
///

/**
 * @brief New error class type for badly configured channels
 * e.g. configuring an input-only channel as output will throw this error
 * 
 */
class BadEventTimerConfiguration : public std::logic_error
{
public:
    BadEventTimerConfiguration() : std::logic_error("Event timer was configured in wrong mode") { };
}; // class notImplementedException

// Possible event directions
enum class EventDirection
{
    DISABLED,
    INPUT,
    OUTPUT
};

// Possible event results
enum class EventResult 
{
    EVENT,
    EVENT_IN_THE_PAST,
    EVENT_TIMEOUT,
    TRIGGER,
    TRIGGER_IN_THE_PAST,
    BUSY,
};

///
// Actual interface 
///

/**
 * @brief this function configures a given event channel as input, output or disabled
 * 
 * @param channel event channel to be configured
 * @param direction direction chosen for the channel
 * @throw BadEventTimerConfiguration if event channel is configured wrongly
 */
void configureEvent(Channel channel, EventDirection direction);


/**
 * @brief this function allows to put the thread in wait and wait for an event to be
 * trigger on the chosen event channel until an event or timeout is reached.
 * The wait can be configured both in absolute or relative time.
 * 
 * @param channel channel to wait the event on
 * @param timeoutNs timeout for wait event
 * @return std::pair<EventResult, long long>  contains result for the waited event which can either
 * be a valid EventResult followed by its timestamp, or an invalid EventResult and the timestamp will
 * have a meaningless value of 0.
 */
std::pair<EventResult, long long> waitEvent(Channel channel, long long timeoutNs);
std::pair<EventResult, long long> absoluteWaitEvent(Channel channel, long long absoluteTimeoutNs);

/**
 * @brief his function allows to put the thread in wait and trigger an event on a given channel
 * at a given time. Once the event is triggered, the thread is woken up.
 * The wait can be configured both in absolute or relative time.
 * 
 * @param channel channel to trigger the event on
 * @param ns time for event to be triggered
 * @return EventResult result of the trigger event
 */
EventResult triggerEvent(Channel channel, long long ns); // watch for overflow! count number of times
EventResult absoluteTriggerEvent(Channel channel, long long absoluteNs); // watch for overflow! count number of times


/**
 * @brief Get the Event Direction object
 * 
 * @return EventDirection 
 */
std::pair<EventDirection, EventDirection> getEventsDirection();

} // namespace events

} // namespace miosix