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

#include <stdio.h>
#include "miosix.h"
#include "interfaces-impl/rtc.h"
#include "interfaces-impl/power_manager.h"
#include "interfaces-impl/hrtb.h"
#include "interfaces-impl/vht.h"

using namespace std;
using namespace miosix;

/**
 * Get tick in low frequency and high frequency to check if the are really coherent
 * @param n number of time to test it
 * @param forceReturnOnError true means exit from function on first error found
 */
void testTickCorrectness(int n, bool verbose=0, bool forceReturnOnError=0){
    long long tickH,tickL;
    long long timeH,timeL;
    Rtc& rtc=Rtc::instance();
    HRTB& hrtb=HRTB::instance();
    
    
    for(int i=0;i<n;i++){
        // Fragment of code to check if HRt is consistent with RTC. 
        // On long running without resync, this code will fail due 
        // to the accumulating skew
        
        // Read before the RTC, then HRT because:
        // in this way, the HRT time should always be greater than RTC, 
        // I haven't to check negative value. If they occur, it is an error!
        // I observed that is necessary to disable to interrupt: 
        // even without any other thread, it's quite common the triggering of some interrupts
        {
            FastInterruptDisableLock dl;
            tickL=rtc.IRQgetValue();
            tickH=hrtb.IRQgetCurrentTickVht();
        }
        //Conversion precise as possible, performance aren't important
        timeH=(double)tickH*1000/48;
        timeL=(double)tickL*1953125/64;
        long long diff=timeH-timeL;
        // 1/32768=0,00003051757813 --> 31000ns approx
        bool error = diff>32000 || diff<-1000 ;
        if(error){
            printf("Sync error @iteration# %d --> \t\t",i);
        }
        if(verbose || error){
            printf("%lld %lld %lld %lld %lld %lld", 
                    tickL,
                    timeL,
                    HRTB::clockCorrection,
                    tickH,
                    timeH,
                    timeH-timeL);
        }
        if(verbose || error){
            printf("\n");
        }
        if(error && forceReturnOnError){
            return ;
        }
    }
}

int main(int argc, char** argv) {
    printf("Start testing...\n");
    testTickCorrectness(2500000,false,false);
    
    Rtc& rtc=Rtc::instance();
    PowerManager& pm=PowerManager::instance();
    long long wakeupTick;
    for(int i=0;i<10;i++){
        wakeupTick=rtc.getValue()+32768*2;
        printf("[%lld] I'm going to sleep until %lld, #%d...\n",rtc.getValue(),wakeupTick,i+1);
        pm.deepSleepUntil(wakeupTick,PowerManager::Unit::TICK);
        printf("[%lld] Waked up\n",rtc.getValue());
        
        testTickCorrectness(250000,false,false);
    }
    
    
    printf("End test\n\n");
    return 0;
}

