
// Soft realtime 2 benchmark, target is to minimize deadline misses with CPU
// load becoming unfeasible fot 30<t<45, total simulation time is 120s

#include <cstdio>
#include <vector>
#include "rescaled.h"
#include "miosix.h"
#include "miosix/kernel/scheduler/scheduler.h"
#include "hartstone.h"

#ifndef COUNT_MISSES
#warning "miss count required"
#endif

using namespace std;
using namespace miosix;

/**
 * Modified workload variator that acts as the first hartstone benchmak,
 * but keeps the workload at around 48% for t<30 && t>45, while for
 * 30<t<45 thread 5's period is modified so the workload is:
 *
 * thread        |   1   |   2   |   3   |   4   |   5   |
 * --------------+-------+-------+-------+-------+-------+
 * period   (ms) | 500   | 250   | 125   |  62   |   3   |
 * workload (ms) |  40   |  20   |  10   |   5   |   2.5 |
 * --------------+-------+-------+-------+-------+-------+-------+
 * CPU load (%)  |   8   |   8   |   8   |   8.1 |  83.3 | 115.4 |
 *
 */
void benchmark1m()
{
    #ifdef SCHED_TYPE_CONTROL_BASED
    {
        PauseKernelLock dLock;
        static const int thetas[]={1,1,1,1,125};
        for(int i=0;i<5;i++)
        {
            Thread *t=Hartstone::getThreads()[i].thread;
            ControlScheduler::PKsetTheta(t,thetas[i]);
        }
    }
    #endif //SCHED_TYPE_CONTROL_BASED
    //Using frequency = 64Hz so that workload is ~0.48
    float frequency=64.0f;
    Hartstone::getThreads().at(4).period= // TICK_FREQ = 1 GHz
            static_cast<long long>((rescaled*1000000000ll)/frequency);
    #ifdef SCHED_TYPE_CONTROL_BASED
    Hartstone::rescaleAlfa();
    #endif //SCHED_TYPE_CONTROL_BASED
    Profiler::start();
    Thread::sleep(30000*rescaled);

    frequency=333.0f;
    Hartstone::getThreads().at(4).period= // TICK_FREQ = 1GHz
            static_cast<long long>((rescaled*1000000000ll)/frequency);
    #ifdef SCHED_TYPE_CONTROL_BASED
    Hartstone::rescaleAlfa();
    #endif //SCHED_TYPE_CONTROL_BASED
    Thread::sleep(15000*rescaled);

    frequency=64;
    Hartstone::getThreads().at(4).period= // TICK_FREQ = 1GHz
            static_cast<long long>((rescaled*1000000000ll)/frequency);
    #ifdef SCHED_TYPE_CONTROL_BASED
    Hartstone::rescaleAlfa();
    #endif //SCHED_TYPE_CONTROL_BASED
    Thread::sleep(75000*rescaled);
    Profiler::stop();
    Hartstone::commonStats();
}

/**
 * Modified workload variator that acts as the second hartstone benchmak,
 * but keeps the workload at around 48% for t<30 && t>45, while for
 * 30<t<45 thread's periods are modified so the workload is:
 *
 * thread        |   1   |   2   |   3   |   4   |   5   |
 * --------------+-------+-------+-------+-------+-------+
 * period   (ms) | 160   |  80   |  40   |  20   |  10   |
 * workload (ms) |  40   |  20   |  10   |   5   |   2.5 |
 * --------------+-------+-------+-------+-------+-------+-------+
 * CPU load (%)  |  25   |  25   |  25   |  25   |  25   | 125   |
 *
 */
void benchmark2m()
{
    #ifdef SCHED_TYPE_CONTROL_BASED
    {
        PauseKernelLock dLock;
        static const int thetas[]={ 1, 2,10,10,10};
        for(int i=0;i<5;i++)
        {
            Thread *t=Hartstone::getThreads()[i].thread;
            ControlScheduler::PKsetTheta(t,thetas[i]);
        }
    }
    #endif //SCHED_TYPE_CONTROL_BASED
    //Using ratio = 1.2 so that workload is ~0.48
    float ratio=1.2f;
    float frequency[5]={2.0f,4.0f,8.0f,16.0f,32.0f};
    for(int i=0;i<5;i++)
    {
        Hartstone::getThreads().at(i).period= // TICK_FREQ = 1GHz
            static_cast<long long>(((rescaled*1000000000ll)/(frequency[i]*ratio))+0.5f);
    }
    #ifdef SCHED_TYPE_CONTROL_BASED
    Hartstone::rescaleAlfa();
    #endif //SCHED_TYPE_CONTROL_BASED
    Profiler::start();
    Thread::sleep(30000*rescaled);

    ratio=3.125f;
    for(int i=0;i<5;i++)
    {
        Hartstone::getThreads().at(i).period= // TICK_FREQ = 1GHz
            static_cast<long long>(((rescaled*1000000000ll)/(frequency[i]*ratio))+0.5f);
    }
    #ifdef SCHED_TYPE_CONTROL_BASED
    Hartstone::rescaleAlfa();
    #endif //SCHED_TYPE_CONTROL_BASED
    Thread::sleep(15000*rescaled);

    ratio=1.2f;
    for(int i=0;i<5;i++)
    {
        Hartstone::getThreads().at(i).period=
            static_cast<long long>(((rescaled*1000000000ll)/(frequency[i]*ratio))+0.5f);
    }
    #ifdef SCHED_TYPE_CONTROL_BASED
    Hartstone::rescaleAlfa();
    #endif //SCHED_TYPE_CONTROL_BASED
    Thread::sleep(75000*rescaled);
    Profiler::stop();
    Hartstone::commonStats();
}

/**
 * Modified workload variator that acts as the third hartstone benchmak,
 * but keeps the workload at around 48% for t<30 && t>45, while for
 * 30<t<45 thread's workloads are modified so the workload is:
 *
 * thread        |   1   |   2   |   3   |   4   |   5   |
 * --------------+-------+-------+-------+-------+-------+
 * period   (ms) | 500   | 250   | 125   |  62   |  31   |
 * workload (ms) |  52.5 |  32.5 |  22.5 |  17.5 |  15   |
 * --------------+-------+-------+-------+-------+-------+-------+
 * CPU load (%)  |  10.5 |  13   |  18   |  28.2 |  48.4 | 118.1 |
 *
 */
void benchmark3m()
{
    #ifdef SCHED_TYPE_CONTROL_BASED
    {
        PauseKernelLock dLock;
        static const int thetas[]={10,10,10,10, 1};
        for(int i=0;i<5;i++)
        {
            Thread *t=Hartstone::getThreads()[i].thread;
            ControlScheduler::PKsetTheta(t,thetas[i]);
        }
    }
    #endif //SCHED_TYPE_CONTROL_BASED
    //This correspond to first iteration, which results in workload ~0.48
    for(int i=0;i<5;i++)
    {
        Hartstone::getThreads().at(i).load+=rescaled;
    }
    #ifdef SCHED_TYPE_CONTROL_BASED
    Hartstone::rescaleAlfa();
    #endif //SCHED_TYPE_CONTROL_BASED
    Profiler::start();
    Thread::sleep(30000*rescaled);

    for(int i=0;i<5;i++)
    {
        Hartstone::getThreads().at(i).load+=10*rescaled;
    }
    #ifdef SCHED_TYPE_CONTROL_BASED
    Hartstone::rescaleAlfa();
    #endif //SCHED_TYPE_CONTROL_BASED
    Thread::sleep(15000*rescaled);

    for(int i=0;i<5;i++)
    {
        Hartstone::getThreads().at(i).load-=10*rescaled;
    }
    #ifdef SCHED_TYPE_CONTROL_BASED
    Hartstone::rescaleAlfa();
    #endif //SCHED_TYPE_CONTROL_BASED
    Thread::sleep(75000*rescaled);
    Profiler::stop();
    Hartstone::commonStats();
}

/**
 * Modified workload variator that acts as the fourth hartstone benchmak,
 * but keeps the workload at around 48% for t<30 && t>45, while for
 * 30<t<45 spawns 9 more threads:
 *
 * thread        |   1   |   2   |   3   |   4   |   5   | 6..15 |
 * --------------+-------+-------+-------+-------+-------+-------+
 * period   (ms) | 500   | 250   | 125   |  62   |  31   | 125   |
 * workload (ms) |  40   |  20   |  10   |   5   |   2.5 |  10   |
 * --------------+-------+-------+-------+-------+-------+-------+-------+
 * CPU load (%)  |   8   |   8   |   8   |   8.1 |   8.1 |  80   | 120.2 |
 *
 */
void benchmark4m()
{
    vector<Hartstone::ThreadData>& threads=Hartstone::getThreads();
    //This correspond to first iteration, which results in workload ~0.48
    Hartstone::addThread();
    #ifdef SCHED_TYPE_CONTROL_BASED
    {
         PauseKernelLock dLock;
         static const int thetas[]={  1,  1,100,100,100,100};
         for(int i=0;i<6;i++)
             ControlScheduler::PKsetTheta(threads.at(i).thread,thetas[i]);
    }
    Hartstone::rescaleAlfa();
    #endif //SCHED_TYPE_CONTROL_BASED
    Profiler::start();
    Thread::sleep(30000*rescaled);

    for(int i=0;i<9;i++) Hartstone::addThread();
    #ifdef SCHED_TYPE_CONTROL_BASED
    {
        PauseKernelLock dLock;
        ControlScheduler::PKsetTheta(threads.at(6).thread,100);
    }
    Hartstone::rescaleAlfa();
    #endif //SCHED_TYPE_CONTROL_BASED
    Thread::sleep(15000*rescaled);

    //Ask all threads to terminate
    for(int i=0;i<9;i++) threads.at(6+i).thread->terminate();
 
    Thread::sleep(75000*rescaled);
    Profiler::stop();
    Hartstone::commonStats();
}

int main()
{
#ifdef SCHED_TYPE_CONTROL_BASED
    ControlScheduler::fixmeManualBurst(0.004f);
#endif
    getchar();
    Hartstone::setQuiet(false);
    Hartstone::setStopProfilerOnMiss(false);
    Hartstone::setExternelBenchmark(1,benchmark1m);
    Hartstone::setExternelBenchmark(2,benchmark2m);
    Hartstone::setExternelBenchmark(3,benchmark3m);
    Hartstone::setExternelBenchmark(4,benchmark4m);
    Hartstone::performBenchmark();
}
