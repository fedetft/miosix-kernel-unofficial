/***************************************************************************
 *   Copyright (C) 2016 by Fabiano Riccardi                                *
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

#include <cstdio>
#include "miosix.h"
#include "interfaces-impl/gpio_timer.h"
#include "gpio_timer_test_p_const.h"

using namespace std;
using namespace miosix;

/*
 * Main of the slave in Ping Pong test
 */
int main(int argc, char** argv) {
    GPIOtimer& g=GPIOtimer::instance();
    printf("Start test (slave):\n\n");
    
    //in tick, stay below 0.1 second to avoid (great) dependencies on the temperature and intrinsic quartz skew.
    long long timestamp,oldtimestamp=0; 
    for(;;){
	g.waitTimeoutOrEvent(timeout);
	timestamp=g.getExtEventTimestamp();
	g.absoluteWaitTrigger(timestamp+delay);
    }
    return 0;
}

