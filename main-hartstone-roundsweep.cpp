
// Miosix code for the hartstone round sweep test.
// The fixmeManualBurst() member function was added to allow
// manual nominal burst duration tweaking

#include <cstdio>
#include "hartstone.h"

#include "rescaled.h"
#include "miosix.h"
#include "miosix/kernel/scheduler/scheduler.h"

#ifdef COUNT_MISSES
#warning "miss count not necessary"
#endif

using namespace std;
using namespace miosix;

int main()
{
    getchar();
    //Burst from 3ms to 200us
    for(float burst=0.003f;burst>0.00019f;burst-=0.0002f)
    {
        printf("--> %f\n",burst);
        ControlScheduler::fixmeManualBurst(burst);
        delayMs(500);
        Hartstone::performBenchmark();
    }
//#ifdef SCHED_TYPE_CONTROL_BASED
//    printf("--> %f\n",0.0001f);
//    ControlScheduler::fixmeManualBurst(0.00015f);
//#endif
//    delayMs(500);
//    Hartstone::performBenchmark();
}
