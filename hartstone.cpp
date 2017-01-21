
#include <cstdio>
#include <algorithm>
#include <cassert>
#include "miosix.h"
#include "profiler.h"
#include "hartstone.h"
#include "board_settings.h"
#include "kernel/scheduler/control/control_scheduler.h"

using namespace std;
using namespace miosix;

///Periods for the baseline threads
static const long long baselinePeriods[5]=
{
    //Since timing system is in ns, TICK_FREQ is 1GHz
    static_cast<long long>((rescaled*1000000000ll)/2.0f),  //2Hz
    static_cast<long long>((rescaled*1000000000ll)/4.0f),  //4Hz
    static_cast<long long>((rescaled*1000000000ll)/8.0f),  //8Hz
    static_cast<long long>((rescaled*1000000000ll)/16.0f), //~16Hz 62ms
    static_cast<long long>((rescaled*1000000000ll)/32.0f)  //~32Hz 31ms
};

///Loads for the baseline threads
static const int baselineLoads[5]=
{
    32*rescaled,16*rescaled,8*rescaled,4*rescaled,2*rescaled
};
#if defined(_BOARD_STM3210E_EVAL)
//Leds for visual feedback of test progress
typedef Gpio<GPIOF_BASE,6> led0;
typedef Gpio<GPIOF_BASE,7> led1;
typedef Gpio<GPIOF_BASE,8> led2;
typedef Gpio<GPIOF_BASE,9> led3;
#elif defined(_BOARD_STM3220G_EVAL)
typedef Gpio<GPIOG_BASE,6> led0;
typedef Gpio<GPIOG_BASE,8> led1;
typedef Gpio<GPIOI_BASE,9> led2;
typedef Gpio<GPIOC_BASE,7> led3;
#elif defined (_BOARD_STM32F4DISCOVERY)
typedef Gpio<GPIOD_BASE,12> led0;
typedef Gpio<GPIOD_BASE,13> led1;
typedef Gpio<GPIOD_BASE,14> led2;
typedef Gpio<GPIOD_BASE,15> led3;
#else
#error 
#endif

/**
 * Mark the beginning and the end of a benchmark in the profiler
 */
class BenchmarkMarker
{
public:
    BenchmarkMarker()
    {
        InterruptDisableLock dLock;
        Trace::IRQaddToLog(30,0); //id=30 is a special tag indicating start
    }

    ~BenchmarkMarker()
    {
        InterruptDisableLock dLock;
        Trace::IRQaddToLog(29,0); //id=29 is a special tag indicating start
    }
};

//
// class Hartstone
//

void Hartstone::performBenchmark()
{
    static const char * const header[4]=
    {
        "=== First benchmark\n",
        "=== Second benchmark\n",
        "=== Third benchmark\n",
        "=== Fourth benchmark\n"
    };
    
    spawnBaseline();
    for(int i=0;i<4;i++)
    {
        if(!quiet) printf(header[i]);
        if(resetBaseline()==false) continue;
        failFlag=false;
        #ifdef COUNT_MISSES
        missCount=0;
        #endif //COUNT_MISSES
        benchmarks[i]();
    }
    joinThreads();
    if(!quiet) printf("=== End\n");
    else printf("\n");
}

void Hartstone::setQuiet(bool v)
{
    quiet=v;
}

#ifdef SCHED_TYPE_CONTROL_BASED
#ifdef SCHED_CONTROL_FIXED_POINT
#error "FIXME: add support for fixed point also here"
#endif //SCHED_CONTROL_FIXED_POINT
void Hartstone::rescaleAlfa()
{
    PauseKernelLock dLock;
    //NOTE: With the full workload div deadline algorithm PKsetAlfa() will
    //touch alfaPrime, not alfa, and alfaPrime is not subject to the constraint
    //that the sum of all alfaPrime should be one, so no rescailng is done here
    ControlScheduler::PKsetAlfa(Thread::getCurrentThread(),0.05f);
    for(unsigned int i=0;i<threads.size();i++)
    {
        float k=(0.00125f*threads[i].load/(0.001f*threads[i].period));
        ControlScheduler::PKsetAlfa(threads[i].thread,k);
    }
}
#endif //SCHED_TYPE_CONTROL_BASED

void Hartstone::commonStats()
{
    #ifdef COUNT_MISSES
    printf("Miss count %d\n",missCount);
    #endif //COUNT_MISSES
    int numFailed=0;
    float cpuLoad=0;
    for(unsigned int i=0;i<threads.size();i++)
    {
        if(threads[i].missedDeadline) numFailed++;
        cpuLoad+=0.00125f*threads[i].load/(0.001f*threads[i].period);
    }
    printf("%d thread(s) missed a deadline (",numFailed);
    for(unsigned int i=0;i<threads.size();i++)
    {
        if(threads[i].missedDeadline)
            printf(" %d",i+1);
    }
    printf(" )\nNumber of context switches %d\n",Profiler::count());
    #ifndef COUNT_MISSES
    printf("CPU load that caused the failure was %f\n",cpuLoad);
    #endif //COUNT_MISSES
    printf("\n");
}

void Hartstone::setExternelBenchmark(int idx, void (*benchmark)())
{
    if(idx<1 || idx>4) return;
    if(benchmark==0)
    {
        switch(idx)
        {
            case 1:
                benchmarks[0]=Hartstone::benchmark1;
                break;
            case 2:
                benchmarks[1]=Hartstone::benchmark2;
                break;
            case 3:
                benchmarks[2]=Hartstone::benchmark3;
                break;
            case 4:
                benchmarks[3]=Hartstone::benchmark4;
                break;
        }
    } else benchmarks[idx-1]=benchmark;
}

void Hartstone::addThread()
{
    threads.push_back(ThreadData());
    threads[threads.size()-1].thread=Thread::create(entry,STACK_DEFAULT_FOR_PTHREAD,1,
            reinterpret_cast<void*>(threads.size()-1),Thread::JOINABLE);
}

void Hartstone::spawnBaseline()
{
    led0::mode(Mode::OUTPUT);
    led1::mode(Mode::OUTPUT);
    led2::mode(Mode::OUTPUT);
    led3::mode(Mode::OUTPUT);
    for(int i=0;i<5;i++)
    {
        threads.push_back(ThreadData());
        threads[i].period=baselinePeriods[i];
        threads[i].load=baselineLoads[i];
        threads[i].thread=Thread::create(entry,STACK_DEFAULT_FOR_PTHREAD,1,
                reinterpret_cast<void*>(i),Thread::JOINABLE);
    }
    #ifdef SCHED_TYPE_CONTROL_BASED
    ControlScheduler::disableAutomaticAlfaChange();
    rescaleAlfa();
    #endif //SCHED_TYPE_CONTROL_BASED
    Trace::start();
}

bool Hartstone::resetBaseline()
{
    if(threads.size()!=5)
    {
        printf("Consistency error: expecting five threads\n");
        return false;
    }
    for(int i=0;i<5;i++)
    {
        threads[i].period=baselinePeriods[i];
        threads[i].load=baselineLoads[i];
    }
    #ifdef SCHED_TYPE_CONTROL_BASED
    rescaleAlfa();
    #endif //SCHED_TYPE_CONTROL_BASED
    Thread::sleep(1000*rescaled);//Ensure all threads have the newer values
    for(int i=0;i<5;i++)
    {
        threads[i].missedDeadline=false;
    }
    Thread::sleep(1000*rescaled);//Sanity check, no deadline misses should occur
    for(int i=0;i<5;i++)
    {
        if(threads[i].missedDeadline)
        {
            printf("Consistency error: baseline misses deadlines\n");
            return false;
        }
    }
    return true;
}

void Hartstone::joinThreads()
{
    #ifdef SCHED_TYPE_CONTROL_BASED
    ControlScheduler::enableAutomaticAlfaChange();
    #endif //SCHED_TYPE_CONTROL_BASED
    for(vector<ThreadData>::iterator it=threads.begin();it!=threads.end();++it)
    {
        it->thread->terminate();
        it->thread->join();
    }
    threads.clear();
    led0::low();
    led1::low();
    led2::low();
    led3::low();
    Trace::stop();
    Trace::print();
}

void Hartstone::benchmark1()
{
    float frequency=32.0f; //Fifth thread starts with a frequency of 32Hz
    int count=1;
    {
        Profiler::start();
        BenchmarkMarker bmMark;
        for(;;)
        {
            frequency+=8.0f;
            threads.at(4).period=static_cast<long long>( // TICK_FREQ = 1GHz
                    (rescaled*1000000000ll)/frequency);
            #ifdef SCHED_TYPE_CONTROL_BASED
            rescaleAlfa();
            #endif //SCHED_TYPE_CONTROL_BASED
            Thread::sleep(1000*rescaled);
            if(failFlag) break;
            count++;
        }
    }
    if(!quiet)
    {
        printf("First deadline missed after %d iterations\n",count);
        printf("Thread 5 had frequency=%dHz period=%dms\n",
                static_cast<int>(frequency),static_cast<int>(threads.at(4).period / 1000000));
        commonStats();
    } else printf("%d ",count);
}

void Hartstone::benchmark2()
{
    float frequency[5]={2.0f,4.0f,8.0f,16.0f,32.0f};
    float ratio=1.0f;
    int count=1;
    {
        Profiler::start();
        BenchmarkMarker bmMark;
        for(;;)
        {
            ratio+=0.1f;
            for(int i=0;i<5;i++)
            {
                threads[i].period=static_cast<long long> // TICK_FREQ = 1 GHz
                        ((rescaled*1000000000ll)/(frequency[i]*ratio));
            }
            #ifdef SCHED_TYPE_CONTROL_BASED
            rescaleAlfa();
            #endif //SCHED_TYPE_CONTROL_BASED
            Thread::sleep(1000*rescaled);
            if(failFlag) break;
            count++;
        }
    }
    if(!quiet)
    {
        printf("First deadline missed after %d iterations\n",count);
        printf("Scale ratio = %3.1f\n",ratio);
        for(int i=0;i<5;i++)
        {
            printf("Thread %d had frequency=%fHz period=%dms\n",
                    i+1,frequency[i]*ratio,
                    static_cast<int>(threads.at(i).period / 1000000));
        }
        commonStats();
    } else printf("%d ",count);
}

void Hartstone::benchmark3()
{
    int count=1;
    {
        Profiler::start();
        BenchmarkMarker bmMark;
        for(;;)
        {
            for(int i=0;i<5;i++)
            {
                threads[i].load+=rescaled;
            }
            #ifdef SCHED_TYPE_CONTROL_BASED
            rescaleAlfa();
            #endif //SCHED_TYPE_CONTROL_BASED
            Thread::sleep(1000*rescaled);
            if(failFlag) break;
            count++;
        }
    }
    if(!quiet)
    {
        printf("First deadline missed after %d iterations\n",count);
        for(int i=0;i<5;i++)
        {
            printf("Thread %d had load=%dkwips\n",i+1,threads.at(i).load);
        }
        commonStats();
    } else printf("%d ",count);
}

void Hartstone::benchmark4()
{
    int count=1;
    {
        Profiler::start();
        BenchmarkMarker bmMark;
        for(;;)
        {
            threads.push_back(ThreadData());
            threads[threads.size()-1].thread=Thread::create(entry,STACK_DEFAULT_FOR_PTHREAD,1,
                    reinterpret_cast<void*>(threads.size()-1),Thread::JOINABLE);
            #ifdef SCHED_TYPE_CONTROL_BASED
            rescaleAlfa();
            #endif //SCHED_TYPE_CONTROL_BASED
            Thread::sleep(1000*rescaled);
            if(failFlag) break;
            count++;
        }
    }
    if(!quiet)
    {
        printf("First deadline missed after %d iterations\n",count);
        printf("Total number of threads active=%d\n",threads.size());
        commonStats();
    } else printf("%d ",count);
}

void Hartstone::entry(void *argv)
{
    const int id=reinterpret_cast<int>(argv);
    long long tick=getTime(); // tick is in ns
    for(;;)
    {
        long long prevTick=tick;
        tick+=threads[id].period;
        #ifdef SCHED_TYPE_EDF
        Thread::setPriority(Priority(tick)); //Change deadline
        #endif //SCHED_TYPE_EDF
        Thread::nanoSleepUntil(prevTick); //Make sure the task is run periodically
        if(Thread::testTerminate()) break;
        blinkLeds(id);
        kiloWhets(threads[id].load);
        if(getTime()>tick)
        {
            if(stopOnMiss) Profiler::stop();
            threads[id].missedDeadline=true;
            failFlag=true;
            #ifdef COUNT_MISSES
            {
                FastInterruptDisableLock dLock;
                long long curTick=getTime();
                while(curTick>tick)
                {
                    tick+=threads[id].period;
                    missCount++;
                }
                //This because when the loop restarts there's a tick+=period
                tick-=threads[id].period;
            }
            #endif //COUNT_MISSES
        }
    }
}

void Hartstone::kiloWhets(unsigned int n)
{
    // Here the goal is not to test the performance of the architecture/compiler
    // but the scheduler. So instead of the real kilo-whets we just sleep for
    // an approriate amount of time.
    // For consistency with the simulations which are done with 1 kilo-whet
    // equals to 25 time units and 1 second is 20000 time units, 1 kilo-whet
    // is simulated with a busy-wait delay of 1.25ms
    for(unsigned int i=0;i<n;i++)
    {
        delayMs(1);
        delayUs(250);
    }
}

void Hartstone::blinkLeds(int id)
{
    switch(id)
    {
        case 0:
            if(led0::value()) led0::low(); else led0::high();
            break;
        case 1:
            if(led1::value()) led1::low(); else led1::high();
            break;
        case 2:
            if(led2::value()) led2::low(); else led2::high();
            break;
        case 3:
            if(led3::value()) led3::low(); else led3::high();
            break;
        default:
            break;
    }
}

vector<Hartstone::ThreadData> Hartstone::threads;
bool Hartstone::failFlag;
bool Hartstone::quiet=false;
bool Hartstone::stopOnMiss=true;
#ifdef COUNT_MISSES
volatile int Hartstone::missCount;
#endif //COUNT_MISSES
void (*Hartstone::benchmarks[4])()=
{
    Hartstone::benchmark1,
    Hartstone::benchmark2,
    Hartstone::benchmark3,
    Hartstone::benchmark4
};
