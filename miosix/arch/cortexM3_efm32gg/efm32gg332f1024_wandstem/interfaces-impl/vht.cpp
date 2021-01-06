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

#include "vht.h"
#include "flopsync_vht.h"
#include "cassert"

using namespace std;
using namespace miosix;

void VHT::stopResyncSoft(){
    softEnable=false;
}

void VHT::startResyncSoft(){
    softEnable=true;
}

VHT& VHT::instance(){
    static VHT vht;
    return vht;
}

void VHT::start(){
    TIMER2->IEN |= TIMER_IEN_CC2;
    
    // Thread that is waken up by the timer2 to perform the clock correction
    HRTB::flopsyncThread=Thread::create(&VHT::doRun,2048,1,this);
}

VHT::VHT() {
}

void VHT::doRun(void* arg)
{
    reinterpret_cast<VHT*>(arg)->loop();
}

void VHT::loop() {
    HRTB& hrtb=HRTB::instance();
    Rtc& rtc=Rtc::instance();

    initDebugPins();
    long long hrtT;
    long long rtcT;
    FlopsyncVHT f;
    
    int tempPendingVhtSync;				    ///< Number of sync acquired in a round
    const long long x=(double)48000000*HRTB::syncPeriodRtc/32768*0.0003f;
    while(1){
        Thread::wait();
        {
            //Read in atomic context the 2 SHARED values to run flopsync
            FastInterruptDisableLock dLock;
            //rtcT=HRTB::nextSyncPointRtc-HRTB::syncPeriodRtc;
            hrtT=HRTB::syncPointHrtActual;
            tempPendingVhtSync=VHT::pendingVhtSync;
            VHT::pendingVhtSync=0;
        }
        //Master è quello timestampato correttamente, il nostro punto di riferimento
        HRTB::error = hrtT - (HRTB::syncPointHrtExpected);
        int u=f.computeCorrection(HRTB::error);
        
        if(VHT::softEnable)
        {
            //The correction should always less than 300ppm
            assert(HRTB::error<x&&HRTB::error>-x);
            {
                PauseKernelLock pkLock;
                // Single instruction that update the error variable, 
                // interrupt can occur, but not thread preemption
                HRTB::clockCorrectionFlopsync=u;

                //efficient way to calculate the factor T/(T+u(k))
                long long temp=(HRTB::syncPeriodHrt<<32)/(HRTB::syncPeriodHrt+HRTB::clockCorrectionFlopsync);
                factorI = static_cast<unsigned int>((temp & 0xFFFFFFFF00000000LLU)>>32);
                factorD = static_cast<unsigned int>(temp);
                //calculate inverse of previous factor (T+u(k))/T
                temp = ((HRTB::syncPeriodHrt+HRTB::clockCorrectionFlopsync)<<32)/HRTB::syncPeriodHrt;
                inverseFactorI = static_cast<unsigned int>((temp & 0xFFFFFFFF00000000LLU)>>32);
                inverseFactorD = static_cast<unsigned int>(temp);
            }
            //printf("%lld\n",HRTB::clockCorrectionFlopsync);
            //This printf shouldn't be in here because is very slow 
//            printf( "HRT bare:%lld, RTC %lld, next:%lld, COMP1:%lu basicCorr:%lld\n\t"
//                    "Theor:%lld, Master:%lld, Slave:%lld\n\t"
//                    "Error:%lld, FSync corr:%lld, PendingSync:%d\n\n",
//                hrtb.IRQgetCurrentTick(),
//                rtc.getValue(),
//                HRTB::nextSyncPointRtc,
//                RTC->COMP1,
//                HRTB::clockCorrection,
//                HRTB::syncPointHrtTheoretical,
//                HRTB::syncPointHrtMaster,
//                HRTB::syncPointHrtSlave,
//                HRTB::error,
//                HRTB::clockCorrectionFlopsync,
//                tempPendingVhtSync);
        } 

        
         
        // If the event is enough in the future, correct the CStimer. NO!
        // Fact: this is a thread, so the scheduler set an a periodic interrupt until 
        //      all the threads go to sleep/wait. 
        // So, if there was a long wait/sleep already set in the register, 
        // this value is continuously overridden by this thread call. 
        // The VHT correction is done in the IRQsetInterrupt, 
        // that use always fresh window to correct the given time
        
        // Radio timer and GPIO timer should be corrected?
        // In first approximation yes, but usually I want to trigger after few times (few ms)
        // Doing some calculation, we found out that in the worst case of skew,
        // after 13ms we have 1tick of error. So we in this case we can disable 
        // the sync VHT but be sure that we won't lose precision. 
        // If you want trigger after a long time, let's say a year, 
        // you can call thread::sleep for that time and then call the trigger function
    }
}

int VHT::pendingVhtSync=0;
bool VHT::softEnable=true;
bool VHT::hardEnable=true;

//Multiplicative factor VHT
unsigned int VHT::factorI=1;
unsigned int VHT::factorD=0;
unsigned int VHT::inverseFactorI=1;
unsigned int VHT::inverseFactorD=0;



