

#include <stdio.h>
#include "miosix.h"
#include "interfaces-impl/rtc.h"
#include "interfaces-impl/gpio_timer.h"
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

void loop(void*){
    for(;;){
        
    }
}

/**
 * Test the vht algorithm, with enable/disable, sleep and deep sleep
 */

int main(int argc, char** argv) {
    printf("\tTest VHT 2\n");
    
    HRTB& h=HRTB::instance();
    Rtc& rtc=Rtc::instance();
    VHT& vht=VHT::instance();
    PowerManager& pm=PowerManager::instance();
    
    Thread::create(loop,512);
    
    testTickCorrectness(1000,false,false);
    
    long long b=0;
    for(long long i=800000;;i+=800000,b+=24414){
        Thread::sleepUntil(b+9000);
        testTickCorrectness(1000,false,false);
//        for(int j=0;j<3;j++){
//            long long tick=h.IRQgetCurrentTickCorrected();
//            long long x=vht.uncorrected2corrected(tick);
//            long long original=vht.corrected2uncorrected(x);
//            printf("Orig: %lld, VHT:%lld, Calc:%lld E:%d\n",tick,x,original,tick-original);
//            
//        }
        Thread::sleepUntil(b+12000);
        vht.stopResyncSoft();
        printf("Start lock..");
        fflush(stdout);
        testTickCorrectness(1000,false,false);
        Thread::sleepUntil(b+16000);
        printf("End lock\n");
        vht.startResyncSoft();
        //This sleep is very important, if remove i will start to see some sync error
        Thread::sleep(250);
        testTickCorrectness(1000,false,false);
        Thread::sleepUntil(b+20000);
        testTickCorrectness(1000,false,false);
        printf("[%lld] Sleep until %lld\n",rtc.getValue(),i);
        pm.deepSleepUntil(i,PowerManager::Unit::TICK);
        printf("[%lld] Wake up!\n\n",rtc.getValue());
        testTickCorrectness(1000,false,false);
    }
    
    return 0;
}

