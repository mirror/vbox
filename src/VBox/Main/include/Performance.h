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
        void init(unsigned long length);
        unsigned long length();
        void put(unsigned long value);
        void copyTo(unsigned long *data);
    private:
        unsigned long *mData;
        unsigned long  mLength;
        unsigned long  mEnd;
        bool           mWrapped;
    };

    class SubMetric : public CircularBuffer
    {
    public:
        SubMetric(const char *name)
        : mName(name) {};
        void query(unsigned long *data);
        const char *getName() { return mName; };
    private:
        const char *mName;
    };


    /* Collector Hardware Abstraction Layer *********************************/
    class CollectorHAL
    {
    public:
        virtual int getHostCpuLoad(unsigned long *user, unsigned long *kernel, unsigned long *idle);
        virtual int getHostCpuMHz(unsigned long *mhz) = 0;
        virtual int getHostMemoryUsage(unsigned long *total, unsigned long *used, unsigned long *available) = 0;
        virtual int getProcessCpuLoad(RTPROCESS process, unsigned long *user, unsigned long *kernel);
        virtual int getProcessMemoryUsage(RTPROCESS process, unsigned long *used) = 0;

        virtual int getRawHostCpuLoad(unsigned long *user, unsigned long *kernel, unsigned long *idle);
        virtual int getRawProcessCpuLoad(RTPROCESS process, unsigned long *user, unsigned long *kernel);
    };

    /* Base Metrics *********************************************************/
    class BaseMetric
    {
    public:
        BaseMetric(CollectorHAL *hal, const char *name, ComPtr<IUnknown> object)
            : mHAL(hal), mLength(0), mName(name), mObject(object), mLastSampleTaken(0), mEnabled(false) {};

        virtual void init(unsigned long period, unsigned long length) = 0;
        virtual void collect() = 0;
        virtual const char *getUnit() = 0;
        virtual unsigned long getMinValue() = 0;
        virtual unsigned long getMaxValue() = 0;

        void collectorBeat(uint64_t nowAt);

        void enable() { mEnabled = true; };
        void disable() { mEnabled = false; };

        bool isEnabled() { return mEnabled; };
        unsigned long getPeriod() { return mPeriod; };
        unsigned long getLength() { return mLength; };
        const char *getName() { return mName; };
        ComPtr<IUnknown> getObject() { return mObject; };
        bool associatedWith(ComPtr<IUnknown> object) { return mObject == object; };

    protected:
        CollectorHAL    *mHAL;
        unsigned long    mPeriod;
        unsigned long    mLength;
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
        void init(unsigned long period, unsigned long length);

        void collect();
        const char *getUnit() { return "%"; };
        unsigned long getMinValue() { return 0; };
        unsigned long getMaxValue() { return 100000000; };

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
        unsigned long mUserPrev;
        unsigned long mKernelPrev;
        unsigned long mIdlePrev;
    };

    class HostCpuMhz : public BaseMetric
    {
    public:
        HostCpuMhz(CollectorHAL *hal, ComPtr<IUnknown> object, SubMetric *mhz)
        : BaseMetric(hal, "CPU/MHz", object), mMHz(mhz) {};

        void init(unsigned long period, unsigned long length);
        void collect();
        const char *getUnit() { return "MHz"; };
        unsigned long getMinValue() { return 0; };
        unsigned long getMaxValue() { return UINT32_MAX; };
    private:
        SubMetric *mMHz;
    };

    class HostRamUsage : public BaseMetric
    {
    public:
        HostRamUsage(CollectorHAL *hal, ComPtr<IUnknown> object, SubMetric *total, SubMetric *used, SubMetric *available)
        : BaseMetric(hal, "RAM/Usage", object), mTotal(total), mUsed(used), mAvailable(available) {};

        void init(unsigned long period, unsigned long length);
        void collect();
        const char *getUnit() { return "kB"; };
        unsigned long getMinValue() { return 0; };
        unsigned long getMaxValue() { return UINT32_MAX; };
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

        void init(unsigned long period, unsigned long length);
        void collect();
        const char *getUnit() { return "%"; };
        unsigned long getMinValue() { return 0; };
        unsigned long getMaxValue() { return 100000000; };
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
        unsigned long mHostTotalPrev;
        unsigned long mProcessUserPrev;
        unsigned long mProcessKernelPrev;
    };

    class MachineRamUsage : public BaseMetric
    {
    public:
        MachineRamUsage(CollectorHAL *hal, ComPtr<IUnknown> object, RTPROCESS process, SubMetric *used)
        : BaseMetric(hal, "RAM/Usage", object), mProcess(process), mUsed(used) {};

        void init(unsigned long period, unsigned long length);
        void collect();
        const char *getUnit() { return "kB"; };
        unsigned long getMinValue() { return 0; };
        unsigned long getMaxValue() { return UINT32_MAX; };
    private:
        RTPROCESS  mProcess;
        SubMetric *mUsed;
    };

    /* Aggregate Functions **************************************************/
    class Aggregate
    {
    public:
        virtual unsigned long compute(unsigned long *data, unsigned long length) = 0;
        virtual const char *getName() = 0;
    };

    class AggregateAvg : public Aggregate
    {
    public:
        virtual unsigned long compute(unsigned long *data, unsigned long length);
        virtual const char *getName();
    };

    class AggregateMin : public Aggregate
    {
    public:
        virtual unsigned long compute(unsigned long *data, unsigned long length);
        virtual const char *getName();
    };

    class AggregateMax : public Aggregate
    {
    public:
        virtual unsigned long compute(unsigned long *data, unsigned long length);
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
        unsigned long getMinValue() { return mBaseMetric->getMinValue(); };
        unsigned long getMaxValue() { return mBaseMetric->getMaxValue(); };
        unsigned long getPeriod() { return mBaseMetric->getPeriod(); };
        unsigned long getLength() { return mAggregate ? 1 : mBaseMetric->getLength(); };
        void query(unsigned long **data, unsigned long *count);

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
        virtual BaseMetric   *createHostCpuLoad(ComPtr<IUnknown> object, SubMetric *user, SubMetric *kernel, SubMetric *idle);
        virtual BaseMetric   *createMachineCpuLoad(ComPtr<IUnknown> object, RTPROCESS process, SubMetric *user, SubMetric *kernel);
    };

    class MetricFactoryLinux : public MetricFactory
    {
    public:
        MetricFactoryLinux();
        virtual BaseMetric   *createHostCpuLoad(ComPtr<IUnknown> object, SubMetric *user, SubMetric *kernel, SubMetric *idle);
        virtual BaseMetric   *createMachineCpuLoad(ComPtr<IUnknown> object, RTPROCESS process, SubMetric *user, SubMetric *kernel);
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

