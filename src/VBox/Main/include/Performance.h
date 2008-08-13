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


#include <iprt/types.h>
#include <VBox/com/defs.h>
#include <VBox/com/ptr.h>
#include <list>
#include <string>

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
        void put(ULONG value);
        void copyTo(ULONG *data);
    private:
        ULONG *mData;
        ULONG  mLength;
        ULONG  mEnd;
        bool           mWrapped;
    };

    class SubMetric : public CircularBuffer
    {
    public:
        SubMetric(const char *name)
        : mName(name) {};
        void query(ULONG *data);
        const char *getName() { return mName; };
    private:
        const char *mName;
    };


    /* Collector Hardware Abstraction Layer *********************************/
    class CollectorHAL
    {
    public:
        virtual int getHostCpuLoad(ULONG *user, ULONG *kernel, ULONG *idle);
        virtual int getHostCpuMHz(ULONG *mhz) = 0;
        virtual int getHostMemoryUsage(ULONG *total, ULONG *used, ULONG *available) = 0;
        virtual int getProcessCpuLoad(RTPROCESS process, ULONG *user, ULONG *kernel);
        virtual int getProcessMemoryUsage(RTPROCESS process, ULONG *used) = 0;

        virtual int getRawHostCpuLoad(uint64_t *user, uint64_t *kernel, uint64_t *idle);
        virtual int getRawProcessCpuLoad(RTPROCESS process, uint64_t *user, uint64_t *kernel, uint64_t *total);
    };

    /* Base Metrics *********************************************************/
    class BaseMetric
    {
    public:
        BaseMetric(CollectorHAL *hal, const char *name, ComPtr<IUnknown> object)
            : mHAL(hal), mPeriod(0), mLength(0), mName(name), mObject(object), mLastSampleTaken(0), mEnabled(false) {};

        virtual void init(ULONG period, ULONG length) = 0;
        virtual void collect() = 0;
        virtual const char *getUnit() = 0;
        virtual ULONG getMinValue() = 0;
        virtual ULONG getMaxValue() = 0;

        void collectorBeat(uint64_t nowAt);

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
        void collect();
        const char *getUnit() { return "MHz"; };
        ULONG getMinValue() { return 0; };
        ULONG getMaxValue() { return INT32_MAX; };
    private:
        SubMetric *mMHz;
    };

    class HostRamUsage : public BaseMetric
    {
    public:
        HostRamUsage(CollectorHAL *hal, ComPtr<IUnknown> object, SubMetric *total, SubMetric *used, SubMetric *available)
        : BaseMetric(hal, "RAM/Usage", object), mTotal(total), mUsed(used), mAvailable(available) {};

        void init(ULONG period, ULONG length);
        void collect();
        const char *getUnit() { return "kB"; };
        ULONG getMinValue() { return 0; };
        ULONG getMaxValue() { return INT32_MAX; };
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
        void collect();
        const char *getUnit() { return "kB"; };
        ULONG getMinValue() { return 0; };
        ULONG getMaxValue() { return INT32_MAX; };
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
        const char *getUnit() { return mBaseMetric->getUnit(); };
        ULONG getMinValue() { return mBaseMetric->getMinValue(); };
        ULONG getMaxValue() { return mBaseMetric->getMaxValue(); };
        ULONG getPeriod() { return mBaseMetric->getPeriod(); };
        ULONG getLength() { return mAggregate ? 1 : mBaseMetric->getLength(); };
        void query(ULONG **data, ULONG *count);

    private:
        std::string mName;
        BaseMetric *mBaseMetric;
        SubMetric  *mSubMetric;
        Aggregate  *mAggregate;
    };

    /* Metric Factories *****************************************************/
    class MetricFactory
    {
    public:
        MetricFactory() : mHAL(0) {};
        ~MetricFactory() { delete mHAL; };

        virtual BaseMetric   *createHostCpuLoad(ComPtr<IUnknown> object, SubMetric *user, SubMetric *kernel, SubMetric *idle);
        virtual BaseMetric   *createHostCpuMHz(ComPtr<IUnknown> object, SubMetric *mhz);
        virtual BaseMetric   *createHostRamUsage(ComPtr<IUnknown> object, SubMetric *total, SubMetric *used, SubMetric *available);
        virtual BaseMetric   *createMachineCpuLoad(ComPtr<IUnknown> object, RTPROCESS process, SubMetric *user, SubMetric *kernel);
        virtual BaseMetric   *createMachineRamUsage(ComPtr<IUnknown> object, RTPROCESS process, SubMetric *used);
    protected:
        CollectorHAL *mHAL;
    };

    class MetricFactorySolaris : public MetricFactory
    {
    public:
        MetricFactorySolaris();
        // Nothing else to do here (yet)
    };

    class MetricFactoryLinux : public MetricFactory
    {
    public:
        MetricFactoryLinux();
        // Nothing else to do here (yet)
    };

    class MetricFactoryWin : public MetricFactory
    {
    public:
        MetricFactoryWin();
        // Nothing else to do here (yet)
    };

    class MetricFactoryOS2 : public MetricFactory
    {
    public:
        MetricFactoryOS2();
        // Nothing else to do here (yet)
    };

    class MetricFactoryDarwin : public MetricFactory
    {
    public:
        MetricFactoryDarwin();
        // Nothing else to do here (yet)
    };

    class Filter
    {
    public:
        Filter(ComSafeArrayIn(INPTR BSTR, metricNames),
               ComSafeArrayIn(IUnknown * , objects));
        bool match(const ComPtr<IUnknown> object, const std::string &name) const;
    private:
        typedef std::pair<const ComPtr<IUnknown>, const std::string> FilterElement;
        typedef std::list<FilterElement> ElementList;

        ElementList mElements;

        void processMetricList(const std::string &name, const ComPtr<IUnknown> object);
    };
}

