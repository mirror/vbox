/* $Id$ */

/** @file
 *
 * VBox Performance Classes declaration.
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */


#include <VBox/com/defs.h>
#include <VBox/com/ptr.h>

#include <iprt/types.h>
#include <iprt/err.h>

#include <algorithm>
#include <list>
#include <string>
#include <vector>

namespace pm
{
    /* CPU load is measured in 1/1000 of per cent. */
    const uint64_t PM_CPU_LOAD_MULTIPLIER = UINT64_C(100000);

    /* Sub Metrics **********************************************************/
    class CircularBuffer
    {
    public:
        CircularBuffer() : mData(0), mLength(0), mEnd(0), mWrapped(false) {};
        void init(ULONG length);
        ULONG length();
        ULONG getSequenceNumber() { return mSequenceNumber; }
        void put(ULONG value);
        void copyTo(ULONG *data);
    private:
        ULONG *mData;
        ULONG  mLength;
        ULONG  mEnd;
        ULONG  mSequenceNumber;
        bool   mWrapped;
    };

    class SubMetric : public CircularBuffer
    {
    public:
        SubMetric(const char *name, const char *description)
        : mName(name), mDescription(description) {};
        void query(ULONG *data);
        const char *getName() { return mName; };
        const char *getDescription() { return mDescription; };
    private:
        const char *mName;
        const char *mDescription;
    };


    /* Collector Hardware Abstraction Layer *********************************/
    enum {
        COLLECT_NONE      = 0x0,
        COLLECT_CPU_LOAD  = 0x1,
        COLLECT_RAM_USAGE = 0x2
    };
    typedef int HintFlags;
    typedef std::pair<RTPROCESS, HintFlags> ProcessFlagsPair;

    inline bool processEqual(ProcessFlagsPair pair, RTPROCESS process)
    {
        return pair.first == process;
    }

    class CollectorHints
    {
    public:
        typedef std::list<ProcessFlagsPair> ProcessList;

        CollectorHints() : mHostFlags(COLLECT_NONE) {}
        void collectHostCpuLoad()
            { mHostFlags |= COLLECT_CPU_LOAD; }
        void collectHostRamUsage()
            { mHostFlags |= COLLECT_RAM_USAGE; }
        void collectProcessCpuLoad(RTPROCESS process)
            { findProcess(process).second |= COLLECT_CPU_LOAD; }
        void collectProcessRamUsage(RTPROCESS process)
            { findProcess(process).second |= COLLECT_RAM_USAGE; }
        bool isHostCpuLoadCollected() const
            { return (mHostFlags & COLLECT_CPU_LOAD) != 0; }
        bool isHostRamUsageCollected() const
            { return (mHostFlags & COLLECT_RAM_USAGE) != 0; }
        bool isProcessCpuLoadCollected(RTPROCESS process)
            { return (findProcess(process).second & COLLECT_CPU_LOAD) != 0; }
        bool isProcessRamUsageCollected(RTPROCESS process)
            { return (findProcess(process).second & COLLECT_RAM_USAGE) != 0; }
        void getProcesses(std::vector<RTPROCESS>& processes) const
        {
            processes.clear();
            processes.reserve(mProcesses.size());
            for (ProcessList::const_iterator it = mProcesses.begin(); it != mProcesses.end(); it++)
                processes.push_back(it->first);
        }
        const ProcessList& getProcessFlags() const
        {
            return mProcesses;
        }
    private:
        HintFlags   mHostFlags;
        ProcessList mProcesses;

        ProcessFlagsPair& findProcess(RTPROCESS process)
        {
            ProcessList::iterator it;

            it = std::find_if(mProcesses.begin(),
                              mProcesses.end(),
                              std::bind2nd(std::ptr_fun(processEqual), process));

            if (it != mProcesses.end())
                return *it;

            /* Not found -- add new */
            mProcesses.push_back(ProcessFlagsPair(process, COLLECT_NONE));
            return mProcesses.back();
        }
    };

    class CollectorHAL
    {
    public:
        virtual ~CollectorHAL() { };
        virtual int preCollect(const CollectorHints& /* hints */) { return VINF_SUCCESS; }
        virtual int getHostCpuLoad(ULONG *user, ULONG *kernel, ULONG *idle);
        virtual int getHostCpuMHz(ULONG *mhz);
        virtual int getHostMemoryUsage(ULONG *total, ULONG *used, ULONG *available) = 0;
        virtual int getProcessCpuLoad(RTPROCESS process, ULONG *user, ULONG *kernel);
        virtual int getProcessMemoryUsage(RTPROCESS process, ULONG *used) = 0;

        virtual int getRawHostCpuLoad(uint64_t *user, uint64_t *kernel, uint64_t *idle);
        virtual int getRawProcessCpuLoad(RTPROCESS process, uint64_t *user, uint64_t *kernel, uint64_t *total);
    };

    extern CollectorHAL *createHAL();

    /* Base Metrics *********************************************************/
    class BaseMetric
    {
    public:
        BaseMetric(CollectorHAL *hal, const char *name, ComPtr<IUnknown> object)
            : mHAL(hal), mPeriod(0), mLength(0), mName(name), mObject(object), mLastSampleTaken(0), mEnabled(false) {};

        virtual void init(ULONG period, ULONG length) = 0;
        virtual void preCollect(CollectorHints& hints) = 0;
        virtual void collect() = 0;
        virtual const char *getUnit() = 0;
        virtual ULONG getMinValue() = 0;
        virtual ULONG getMaxValue() = 0;
        virtual ULONG getScale() = 0;

        bool collectorBeat(uint64_t nowAt);

        void enable() { mEnabled = true; };
        void disable() { mEnabled = false; };

        bool isEnabled() { return mEnabled; };
        ULONG getPeriod() { return mPeriod; };
        ULONG getLength() { return mLength; };
        const char *getName() { return mName; };
        ComPtr<IUnknown> getObject() { return mObject; };
        bool associatedWith(ComPtr<IUnknown> object) { return mObject == object; };

    protected:
        CollectorHAL    *mHAL;
        ULONG    mPeriod;
        ULONG    mLength;
        const char      *mName;
        ComPtr<IUnknown> mObject;
        uint64_t         mLastSampleTaken;
        bool             mEnabled;
    };

    class HostCpuLoad : public BaseMetric
    {
    public:
        HostCpuLoad(CollectorHAL *hal, ComPtr<IUnknown> object, SubMetric *user, SubMetric *kernel, SubMetric *idle)
        : BaseMetric(hal, "CPU/Load", object), mUser(user), mKernel(kernel), mIdle(idle) {};
        void init(ULONG period, ULONG length);

        void collect();
        const char *getUnit() { return "%"; };
        ULONG getMinValue() { return 0; };
        ULONG getMaxValue() { return PM_CPU_LOAD_MULTIPLIER; };
        ULONG getScale() { return PM_CPU_LOAD_MULTIPLIER / 100; }

    protected:
        SubMetric *mUser;
        SubMetric *mKernel;
        SubMetric *mIdle;
    };

    class HostCpuLoadRaw : public HostCpuLoad
    {
    public:
        HostCpuLoadRaw(CollectorHAL *hal, ComPtr<IUnknown> object, SubMetric *user, SubMetric *kernel, SubMetric *idle)
        : HostCpuLoad(hal, object, user, kernel, idle), mUserPrev(0), mKernelPrev(0), mIdlePrev(0) {};

        void preCollect(CollectorHints& hints);
        void collect();
    private:
        uint64_t mUserPrev;
        uint64_t mKernelPrev;
        uint64_t mIdlePrev;
    };

    class HostCpuMhz : public BaseMetric
    {
    public:
        HostCpuMhz(CollectorHAL *hal, ComPtr<IUnknown> object, SubMetric *mhz)
        : BaseMetric(hal, "CPU/MHz", object), mMHz(mhz) {};

        void init(ULONG period, ULONG length);
        void preCollect(CollectorHints& /* hints */) {}
        void collect();
        const char *getUnit() { return "MHz"; };
        ULONG getMinValue() { return 0; };
        ULONG getMaxValue() { return INT32_MAX; };
        ULONG getScale() { return 1; }
    private:
        SubMetric *mMHz;
    };

    class HostRamUsage : public BaseMetric
    {
    public:
        HostRamUsage(CollectorHAL *hal, ComPtr<IUnknown> object, SubMetric *total, SubMetric *used, SubMetric *available)
        : BaseMetric(hal, "RAM/Usage", object), mTotal(total), mUsed(used), mAvailable(available) {};

        void init(ULONG period, ULONG length);
        void preCollect(CollectorHints& hints);
        void collect();
        const char *getUnit() { return "kB"; };
        ULONG getMinValue() { return 0; };
        ULONG getMaxValue() { return INT32_MAX; };
        ULONG getScale() { return 1; }
    private:
        SubMetric *mTotal;
        SubMetric *mUsed;
        SubMetric *mAvailable;
    };

    class MachineCpuLoad : public BaseMetric
    {
    public:
        MachineCpuLoad(CollectorHAL *hal, ComPtr<IUnknown> object, RTPROCESS process, SubMetric *user, SubMetric *kernel)
        : BaseMetric(hal, "CPU/Load", object), mProcess(process), mUser(user), mKernel(kernel) {};

        void init(ULONG period, ULONG length);
        void collect();
        const char *getUnit() { return "%"; };
        ULONG getMinValue() { return 0; };
        ULONG getMaxValue() { return PM_CPU_LOAD_MULTIPLIER; };
        ULONG getScale() { return PM_CPU_LOAD_MULTIPLIER / 100; }
    protected:
        RTPROCESS  mProcess;
        SubMetric *mUser;
        SubMetric *mKernel;
    };

    class MachineCpuLoadRaw : public MachineCpuLoad
    {
    public:
        MachineCpuLoadRaw(CollectorHAL *hal, ComPtr<IUnknown> object, RTPROCESS process, SubMetric *user, SubMetric *kernel)
        : MachineCpuLoad(hal, object, process, user, kernel), mHostTotalPrev(0), mProcessUserPrev(0), mProcessKernelPrev(0) {};

        void preCollect(CollectorHints& hints);
        void collect();
    private:
        uint64_t mHostTotalPrev;
        uint64_t mProcessUserPrev;
        uint64_t mProcessKernelPrev;
    };

    class MachineRamUsage : public BaseMetric
    {
    public:
        MachineRamUsage(CollectorHAL *hal, ComPtr<IUnknown> object, RTPROCESS process, SubMetric *used)
        : BaseMetric(hal, "RAM/Usage", object), mProcess(process), mUsed(used) {};

        void init(ULONG period, ULONG length);
        void preCollect(CollectorHints& hints);
        void collect();
        const char *getUnit() { return "kB"; };
        ULONG getMinValue() { return 0; };
        ULONG getMaxValue() { return INT32_MAX; };
        ULONG getScale() { return 1; }
    private:
        RTPROCESS  mProcess;
        SubMetric *mUsed;
    };

    /* Aggregate Functions **************************************************/
    class Aggregate
    {
    public:
        virtual ULONG compute(ULONG *data, ULONG length) = 0;
        virtual const char *getName() = 0;
    };

    class AggregateAvg : public Aggregate
    {
    public:
        virtual ULONG compute(ULONG *data, ULONG length);
        virtual const char *getName();
    };

    class AggregateMin : public Aggregate
    {
    public:
        virtual ULONG compute(ULONG *data, ULONG length);
        virtual const char *getName();
    };

    class AggregateMax : public Aggregate
    {
    public:
        virtual ULONG compute(ULONG *data, ULONG length);
        virtual const char *getName();
    };

    /* Metric Class *********************************************************/
    class Metric
    {
    public:
        Metric(BaseMetric *baseMetric, SubMetric *subMetric, Aggregate *aggregate) :
            mName(subMetric->getName()), mBaseMetric(baseMetric), mSubMetric(subMetric), mAggregate(aggregate)
        {
            if (mAggregate)
            {
                mName += ":";
                mName += mAggregate->getName();
            }
        }

        ~Metric()
        {
            delete mAggregate;
        }
        bool associatedWith(ComPtr<IUnknown> object) { return getObject() == object; };

        const char *getName() { return mName.c_str(); };
        ComPtr<IUnknown> getObject() { return mBaseMetric->getObject(); };
        const char *getDescription()
            { return mAggregate ? "" : mSubMetric->getDescription(); };
        const char *getUnit() { return mBaseMetric->getUnit(); };
        ULONG getMinValue() { return mBaseMetric->getMinValue(); };
        ULONG getMaxValue() { return mBaseMetric->getMaxValue(); };
        ULONG getPeriod() { return mBaseMetric->getPeriod(); };
        ULONG getLength()
            { return mAggregate ? 1 : mBaseMetric->getLength(); };
        ULONG getScale() { return mBaseMetric->getScale(); }
        void query(ULONG **data, ULONG *count, ULONG *sequenceNumber);

    private:
        std::string mName;
        BaseMetric *mBaseMetric;
        SubMetric  *mSubMetric;
        Aggregate  *mAggregate;
    };

    /* Filter Class *********************************************************/

    class Filter
    {
    public:
        Filter(ComSafeArrayIn(IN_BSTR, metricNames),
               ComSafeArrayIn(IUnknown * , objects));
        static bool patternMatch(const char *pszPat, const char *pszName,
                                 bool fSeenColon = false);
        bool match(const ComPtr<IUnknown> object, const std::string &name) const;
    private:
        void init(ComSafeArrayIn(IN_BSTR, metricNames),
                  ComSafeArrayIn(IUnknown * , objects));

        typedef std::pair<const ComPtr<IUnknown>, const std::string> FilterElement;
        typedef std::list<FilterElement> ElementList;

        ElementList mElements;

        void processMetricList(const std::string &name, const ComPtr<IUnknown> object);
    };
}

/* vi: set tabstop=4 shiftwidth=4 expandtab: */
