#include <cstdio>
#include "miosix.h"
#include "kernel/timeconversion.h"
#include "debugpin.h"
#include "interfaces/cstimer.h"
#include "interfaces/arch_registers.h"
#include "debugpin.h"

using namespace std;
using namespace miosix;

int main(){
    printf("Hello World!\n");
    initDebugPins();

    for(long long t=getTick();;t+=4000000){
        
        Thread::nanoSleepUntil(t);      
    }

    
//    for (;;){
//        HighPin<debug2> hp;
//        Thread::sleep(4);
//    }
    
     
}