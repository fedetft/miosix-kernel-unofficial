
#ifndef HARTSTONE_H
#define	HARTSTONE_H

#include "miosix.h"
#include "rescaled.h"
#include <vector>

//Count number of dedline misses, only useful for soft realtime benchmarks
#define COUNT_MISSES

class Hartstone
{
public:

    /**
     * Holds data for a thread during the benchmark
     */
    class ThreadData
    {
    public:
        /**
         * Create a new ThreadData instance.
         * Default frequency is 8Hz and default workload is 8kwips
         */
        ThreadData():
                period(static_cast<long long>((rescaled*1000000000ll)/8.0f)),
                load(8*rescaled), missedDeadline(false), thread(0) {}

        volatile long long period; ///< Thread period
        volatile int load; ///< Thread load
        volatile bool missedDeadline; ///< True if thread missed a deadline
        miosix::Thread *thread; ///< Spawned thread
    };

    /**
     * Perform the hartstone benchmark.
     */
    static void performBenchmark();

    /**
     * Set quiet mode (defaults to false)
     * \param q true to enable quiet mode, false to disable it
     */
    static void setQuiet(bool q);

    /**
     * \return true if quiet mode set
     */
    static bool getQuiet() { return quiet; }

    #ifdef SCHED_TYPE_CONTROL_BASED
    static void rescaleAlfa();
    #endif //SCHED_TYPE_CONTROL_BASED

    /**
     * Print stats common to all benchmarks
     */
    static void commonStats();

    /**
     * Set an external benchmark
     * \param idx which benchmark is it supposed to replace (1..4)
     * \param benchmark function pointer to a benchmark function, or null to
     * restore the original benchmark
     */
    static void setExternelBenchmark(int idx, void (*benchmark)());

    /**
     * \return a reference to the thread list. Note that through this reference
     * it is possible to modify the thread list, this is necessary for the
     * implementation of external tests
     */
    static std::vector<ThreadData>& getThreads() { return threads; }

    /**
     * \return the fail flag, set to true of a thread has missed a deadline,
     * used for external tests
     */
    static bool getFailFlag() { return failFlag; }

    /**
     * Add a thread with default workload, to be used ONLY for the
     * implementation of external tests
     */
    static void addThread();

    /**
     * If stop is true, the worker threads call Profiler::stop() as soon as
     * they miss the first deadline, otherwise profiling goes on.
     * Default is true.
     * \param stop true if the desired behaviour is to stop profiling on miss
     * NOTE: only useful if running externel tests, with the default hartstone
     * benchmark don't disable stop on miss!
     */
    static void setStopProfilerOnMiss(bool stop) { stopOnMiss=stop; }

private:

    /**
     * Spawn the baseline set of threads
     */
    static void spawnBaseline();

    /**
     * Restore periods of the baseline set of threads
     */
    static bool resetBaseline();

    /**
     * Join the threads after the benchmark
     */
    static void joinThreads();

    /**
     * First benchmark. Starting from the baseline, frequency of fifth thread
     * is increased by 8Hz till a deadline is missed.
     */
    static void benchmark1();

    /**
     * Second benchmark. Frequencies are scaled by 1.1, 1.2 ...
     */
    static void benchmark2();

    /**
     * Third benchmark. Workload of all threads is increased by 1kilo-whet at
     * a time
     */
    static void benchmark3();

    /**
     * Fourth benchmark. Threads are added
     */
    static void benchmark4();

    /**
     * Entry point of all spawned threads
     */
    static void entry(void *argv);

    /**
     * Simulate a workload of n kilo-whets
     * \param n number of kilo-whets
     */
    static void kiloWhets(unsigned int n);

    /**
     * Provide visual feedback of the benchmark
     * \param id thread id
     */
    static void blinkLeds(int id);

    static std::vector<ThreadData> threads;
    static bool failFlag;
    static bool quiet;
    static bool stopOnMiss;
    #ifdef COUNT_MISSES
    static volatile int missCount;
    #endif //COUNT_MISSES
    static void (*benchmarks[4])();
};

#endif //HARTSTONE_H
