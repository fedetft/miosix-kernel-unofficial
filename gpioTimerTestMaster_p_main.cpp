/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   gpioTimerTestMasterMain.cpp
 * Author: fabiuz
 * 
 * Created on October 25, 2016, 11:33 AM
 */

#include <cstdlib>
#include <cstdio>
#include "miosix.h"
#include "interfaces-impl/gpio_timer.h"
#include "gpio_timer_test_p_const.h"

using namespace std;
using namespace miosix;

/*
 * Main of the master in Ping Pong test
 */
int main(int argc, char** argv) {
    GPIOtimer& g=GPIOtimer::instance();
    printf("Inizio test (master):\n\n");
    bool w;
    long long timestamp;
    /*for(long long i=0;i<sizeof(noticeableValues)/sizeof(noticeableValues[0]);i++){
        if(!g.absoluteSyncWaitTrigger(noticeableValues[i])){
            w=g.waitTimeoutOrEvent(timeout);
            timestamp=g.getExtEventTimestamp();
            //printf("Send at: %lld Timestamp received: %lld diff=%lld diff between past event:%lld\n",i,timestamp,timestamp-i,timestamp-oldtimestamp);
            printf("%lld\n",timestamp-noticeableValues[i]);
        }else{
            printf("Wake in the past\n");
        }
    }
    */
    printf("Second part\n");
    long long base=96000000;
    long long diff;
    for(long long i=0;i<65536;i++){
        if(!g.absoluteSyncWaitTrigger(base+i*240000+i)){
            w=g.waitTimeoutOrEvent(timeout);
            timestamp=g.getExtEventTimestamp();
            diff=timestamp-(base+i*240000+i);
            if(diff<t1ms-1||diff>t1ms+1){
                printf("%lld %lld\n",i,diff);
            }
        }else{
            printf("Wake in the past\n");
        }
    }
    
    printf("End test\n");
    return 0;
}

