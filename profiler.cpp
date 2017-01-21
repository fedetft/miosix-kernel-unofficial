
#include "profiler.h"
#include "kernel/sync.h"
#include "board_settings.h"
#include <cstdio>

//#define ENABLE_TRACE

using namespace std;

//
// class Profiler
//

void Profiler::IRQoneContextSwitchHappened()
{
    if(profile) numContextSwitches++;
}

volatile bool Profiler::profile=false;
volatile unsigned int Profiler::numContextSwitches=0;


//
// class Trace
//

void Trace::start()
{
    #ifdef ENABLE_TRACE
    if(log==0) log=new DebugValue[logSize];
    debugCount=0;
    doLog=true;
    #endif //ENABLE_TRACE
}

void Trace::print()
{
    #ifdef ENABLE_TRACE
    if(doLog==true || log==0) return;
    if(debugCount>=logSize) printf("Bufffer too small, some data not stored\n");

    FILE *f;
    if((f=fopen("/sd/trace.txt","w"))==NULL)
    {
        printf("Error opening file\n");
        //delete[] log;
        //log=0;
        return;
    }

    printf("Saving trace to file\n");
    //miosix::Timer timer;
    //timer.start();
    long long stick = miosix::getTime();
    for(int i=0;i<debugCount;i++)
    {
        fprintf(f,"%d %d\n",log[i].id,log[i].time);
    }
    long long etick = miosix::getTime();
    printf("Time required %d seconds\n",static_cast<int>((etick - stick)/1000000000));
    fclose(f);

    debugCount=0;
    //delete[] log;
    //log=0;
    #endif //ENABLE_TRACE
}

void Trace::IRQaddToLog(int id, int time)
{
    #ifdef ENABLE_TRACE
    if(doLog==false || debugCount>=logSize) return;
    DebugValue d;
    d.id=id;
    d.time=time;
    log[debugCount]=d;
    debugCount++;
    #endif //ENABLE_TRACE
}

Trace::DebugValue *Trace::log=0;
int Trace::debugCount=0;
volatile bool Trace::doLog=false;