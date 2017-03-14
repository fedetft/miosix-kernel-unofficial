
#include <cstdio>
#include "miosix.h"
#include "kernel/timeconversion.h"
#include "debugpin.h"
using namespace std;
using namespace miosix;

void elaborateValue48(long long ns){
    TimeConversion tc(48000000);
    long long theor;
    long long converted;
    theor=ns*48/1000;
    converted=tc.ns2tick(ns);
    printf("%lld %lld %lld %lld\n",ns,converted,theor,converted-theor);
}

void elaborateValue32(long long ns){
    TimeConversion tc(32768);
    long long theor;
    long long converted;
    theor=(double)ns*0.000032768f;
    converted=tc.ns2tick(ns);
    printf("%lld %lld %lld %lld\n",ns,converted,theor,converted-theor);
}

int main()
{
    long long step=1000000000;
    long long base=0;
    for(int i=0;i<100;i++){
        elaborateValue48(base);
        base+=step;
    }
    
    volatile int c=43;
    volatile long long c1=43532324345343LL,c2=43543324345343LL,c3=746352;
    volatile double c6=4.54546,c5=34.5467,c7;
    initDebugPins();
    debug1::high();
    c3=c2*c1;
    debug1::low();
    
    debug1::high();
    c3=c2/c1;
    debug1::low();
    
    debug1::high();
    c3=c1*c;
    debug1::low();
    
    debug1::high();
    c3=c1/c;
    debug1::low();
    
    debug1::high();
    c7=c5*c6;
    debug1::low();
    
    debug1::high();
    c7=c5/c6;
    debug1::low();
    
    long long x=2;
    for(int i=0;i<64;i++,x*=2){
        elaborateValue48(x);
    }
    
//    x=5+1;
//    for(int i=0;i<40;i++,x*=2){
//        elaborateValue32(x);
//    }
    
    //iprintf("Hello world, write your application here\n");
}

