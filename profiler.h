
#ifndef PROFILER_H
#define	PROFILER_H

/**
 * Class to monitor the number of context switches
 */
class Profiler
{
public:

    /**
     * Called by the kernel @ every context switch
     */
    static void IRQoneContextSwitchHappened();

    /**
     * Start counting context switches.
     * Also reset contex switch counter.
     */
    static void start()
    {
        numContextSwitches=0;
        profile=true;
    }

    /**
     * Stop counting context switches.
     */
    static void stop()
    {
        profile=false;
    }

    /**
     * \return the number of context switches counted.
     */
    static unsigned int count()
    {
        return numContextSwitches;
    }

private:
    Profiler();
    
    static volatile bool profile; ///< true if profiling enabled
    static volatile unsigned int numContextSwitches; ///< Ctxsw counter
};

// if not defined, all trace member functions become no-ops
//#define ENABLE_TRACE

/**
 * Provides a way to gather detailed statistics about bursts assigned to
 * each thread to optimize the scheduler.
 */
class Trace
{
public:

    /**
     * Start profiling
     */
    static void start();

    /**
     * Stop profiling
     */
    static void stop() { doLog=false; }

    /**
     * Print profiled data.
     * Also deallocates the buffer.
     */
    static void print();

    /**
     * Add a new entry to the log
     * \param id thread id (0 to 31)
     * \param time burst time assigned to a thread by the scheduler
     */
    static void IRQaddToLog(int id, int time);

private:
    Trace();
    
    struct DebugValue
    {
        unsigned int time:27;
        unsigned int id:5;//Thread ID, from 0 to 31
    };

    ///Considering each DebugValue is 4 bytes, this amounts to 512KBytes
    static const int logSize=192*1024;
    
    static DebugValue *log;
    static int debugCount;
    static volatile bool doLog;
};

#endif //PROFILER_H
