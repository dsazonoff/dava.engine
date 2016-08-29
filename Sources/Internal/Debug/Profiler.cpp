#include "Profiler.h"
#include "Debug/DVAssert.h"
#include "Logger/Logger.h"
#include "Platform/SystemTimer.h"
#include "FileSystem/File.h"
#include "Concurrency/Thread.h"
#include "Base/RingArray.h"

//==============================================================================

static inline uint64 CurTimeUs()
{
    return DAVA::SystemTimer::Instance()->GetAbsoluteUs();
}

static bool NameEquals(const char* name1, const char* name2)
{
    if (name1 == name2)
        return true;

    return strcmp(name1, name2) == 0;
}

namespace DAVA
{
namespace Profiler
{
struct TimeCounter
{
    uint64 startTime = 0;
    uint64 endTime = 0;
    const char* name = nullptr;
    Thread::Id tid = 0;
};

using CounterArray = RingArray<TimeCounter, 1024>;

static CounterArray counters;
static bool profilerStarted = false;

Vector<CounterArray> snapshots;

void Start()
{
    profilerStarted = true;
}

void Stop()
{
    profilerStarted = false;
}

ScopedCounter::ScopedCounter(const char* counterName)
{
    if (profilerStarted)
    {
        TimeCounter& c = counters.next();

        endTime = &c.endTime;
        c.startTime = CurTimeUs();
        c.endTime = 0;
        c.name = counterName;
        c.tid = Thread::GetCurrentId();
    }
}

ScopedCounter::~ScopedCounter()
{
    if (profilerStarted && endTime)
    {
        *endTime = CurTimeUs();
    }
}

int32 MakeSnapshot()
{
    snapshots.push_back(counters);
    return int32(snapshots.size() - 1);
}

void DeleteSnapshots()
{
    snapshots.clear();
}

uint64 GetLastCounterTime(const char* counterName)
{
    uint64 timeDelta = 0;
    CounterArray::reverse_iterator it = counters.rbegin(), itEnd = counters.rend();
    for (; it != itEnd; it++)
    {
        const TimeCounter& c = *it;
        if (c.endTime != 0 && NameEquals(counterName, c.name))
        {
            timeDelta = c.endTime - c.startTime;
            break;
        }
    }

    return timeDelta;
}

void DumpInternal(CounterArray::iterator begin, CounterArray& array, File* file)
{
    Thread::Id threadID = begin->tid;
    uint64 endTime = begin->endTime;
    Vector<uint64> parents;

    char buf[1024];
    CounterArray::iterator end = array.end();
    for (CounterArray::iterator it = begin; it != end; ++it)
    {
        const TimeCounter& c = *it;

        if (c.tid != threadID)
            continue;

        if (c.startTime > endTime || c.endTime == 0)
            break;

        if (c.tid == threadID)
        {
            while (parents.size() && (c.startTime >= parents.back()) && c.startTime != c.endTime)
                parents.pop_back();

            Snprintf(buf, countof(buf), "%*s%s [%llu us]", int(parents.size() * 2), "", c.name, c.endTime - c.startTime);
            if (file)
            {
                file->WriteLine(buf);
            }
            else
            {
                Logger::Info(buf);
            }

            parents.push_back(c.endTime);
        }
    }
}

void DumpLast(const char* counterName, uint32 counterCount, File* file, int32 snapshot)
{
    DVASSERT((snapshot != NO_SNAPSHOT_ID || !profilerStarted) && "Stop profiler before dumping");

    CounterArray* array = &counters;
    if (snapshot != NO_SNAPSHOT_ID)
    {
        DVASSERT(snapshot < int32(snapshots.size()));
        array = &snapshots[snapshot];
    }

    CounterArray::reverse_iterator it = array->rbegin(), itEnd = array->rend();
    for (; it != itEnd; it++)
    {
        if (it->endTime != 0 && NameEquals(counterName, it->name))
        {
            DumpInternal(CounterArray::iterator(it), *array, file);
            counterCount--;
        }

        if (counterCount == 0)
            break;
    }
}

void DumpAverage(const char* counterName, uint32 counterCount, File* file, int32 snapshot)
{
    DVASSERT((snapshot != NO_SNAPSHOT_ID || !profilerStarted) && "Stop profiler before dumping");

    //TODO
}

void DumpJSON(File* file, int32 snapshot)
{
    DVASSERT((snapshot != NO_SNAPSHOT_ID || !profilerStarted) && "Stop profiler before dumping");

    CounterArray* array = &counters;
    if (snapshot != NO_SNAPSHOT_ID)
    {
        DVASSERT(snapshot < int32(snapshots.size()));
        array = &snapshots[snapshot];
    }

    file->WriteLine("{ \"traceEvents\": [ ");

    char buf[1024];
    CounterArray::iterator it = array->begin(), itEnd = array->end();
    CounterArray::iterator last(array->rbegin());
    for (; it != itEnd; it++)
    {
        Snprintf(buf, 1024, "{ \"pid\":%u, \"tid\":%llu, \"ts\":%llu, \"ph\":\"%s\", \"cat\":\"%s\", \"name\":\"%s\" }%s",
                 0, uint64(it->tid), it->startTime, "B", "", it->name, ", ");

        file->WriteLine(buf);

        Snprintf(buf, 1024, "{ \"pid\":%u, \"tid\":%llu, \"ts\":%llu, \"ph\":\"%s\", \"cat\":\"%s\", \"name\":\"%s\" }%s",
                 0, uint64(it->tid), it->endTime ? it->endTime : it->startTime, "E", "", it->name, (it == last) ? "" : ", ");

        file->WriteLine(buf);
    }

    file->WriteLine("] }");
}

} //ns Profiler
} //ns DAVA
