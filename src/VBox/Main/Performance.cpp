/* $Id$ */

/** @file
 *
 * VBox Performance Classes implementation.
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

/*
 * @todo list:
 *
 * 1) Solaris backend
 * 2) Linux backend
 * 3) Detection of erroneous metric names
 * 4) Min/max ranges for metrics
 * 5) Darwin backend
 * 6) [OS/2 backend]
 */

#include <VBox/com/array.h>
#include <VBox/com/ptr.h>
#include <VBox/com/string.h>
#include <VBox/err.h>
#include <iprt/string.h>
#include <iprt/mem.h>

#include "Logging.h"
#include "Performance.h"

using namespace pm;

// Default factory

BaseMetric *MetricFactory::createHostCpuLoad(ComPtr<IUnknown> object, SubMetric *user, SubMetric *kernel, SubMetric *idle)
{
    Assert(mHAL);
    return new HostCpuLoadRaw(mHAL, object, user, kernel, idle);
}
BaseMetric *MetricFactory::createHostCpuMHz(ComPtr<IUnknown> object, SubMetric *mhz)
{
    Assert(mHAL);
    return new HostCpuMhz(mHAL, object, mhz);
}
BaseMetric *MetricFactory::createHostRamUsage(ComPtr<IUnknown> object, SubMetric *total, SubMetric *used, SubMetric *available)
{
    Assert(mHAL);
    return new HostRamUsage(mHAL, object, total, used, available);
}
BaseMetric *MetricFactory::createMachineCpuLoad(ComPtr<IUnknown> object, RTPROCESS process, SubMetric *user, SubMetric *kernel)
{
    Assert(mHAL);
    return new MachineCpuLoadRaw(mHAL, object, process, user, kernel);
}
BaseMetric *MetricFactory::createMachineRamUsage(ComPtr<IUnknown> object, RTPROCESS process, SubMetric *used)
{
    Assert(mHAL);
    return new MachineRamUsage(mHAL, object, process, used);
}

// Stubs for non-pure virtual methods

int CollectorHAL::getHostCpuLoad(ULONG *user, ULONG *kernel, ULONG *idle)
{
    return E_NOTIMPL;
}

int CollectorHAL::getProcessCpuLoad(RTPROCESS process, ULONG *user, ULONG *kernel)
{
    return E_NOTIMPL;
}

int CollectorHAL::getRawHostCpuLoad(uint64_t *user, uint64_t *kernel, uint64_t *idle)
{
    return E_NOTIMPL;
}

int CollectorHAL::getRawProcessCpuLoad(RTPROCESS process, uint64_t *user, uint64_t *kernel, uint64_t *total)
{
    return E_NOTIMPL;
}

void BaseMetric::collectorBeat(uint64_t nowAt)
{
    if (isEnabled())
    {
        if (nowAt - mLastSampleTaken >= mPeriod * 1000)
        {
            mLastSampleTaken = nowAt;
            LogFlowThisFunc (("Collecting data for obj(%p)...\n", (void *)mObject));
            collect();
        }
    }
}

/*bool BaseMetric::associatedWith(ComPtr<IUnknown> object)
{
    LogFlowThisFunc (("mObject(%p) == object(%p) is %s.\n", mObject, object, mObject == object ? "true" : "false"));
    return mObject == object;
}*/

void HostCpuLoad::init(ULONG period, ULONG length)
{
    mPeriod = period;
    mLength = length;
    mUser->init(mLength);
    mKernel->init(mLength);
    mIdle->init(mLength);
}

void HostCpuLoad::collect()
{
    ULONG user, kernel, idle;
    int rc = mHAL->getHostCpuLoad(&user, &kernel, &idle);
    if (RT_SUCCESS(rc))
    {
        mUser->put(user);
        mKernel->put(kernel);
        mIdle->put(idle);
    }
}

void HostCpuLoadRaw::collect()
{
    uint64_t user, kernel, idle;
    uint64_t userDiff, kernelDiff, idleDiff, totalDiff;

    int rc = mHAL->getRawHostCpuLoad(&user, &kernel, &idle);
    if (RT_SUCCESS(rc))
    {
        userDiff   = user   - mUserPrev;
        kernelDiff = kernel - mKernelPrev;
        idleDiff   = idle   - mIdlePrev;
        totalDiff  = userDiff + kernelDiff + idleDiff;

        if (totalDiff == 0)
        {
            /* This is only possible if none of counters has changed! */
            LogFlowThisFunc (("Impossible! User, kernel and idle raw "
                "counters has not changed since last sample.\n" ));
            mUser->put(0);
            mKernel->put(0);
            mIdle->put(0);
        }
        else
        {
            mUser->put((ULONG)(PM_CPU_LOAD_MULTIPLIER * userDiff / totalDiff));
            mKernel->put((ULONG)(PM_CPU_LOAD_MULTIPLIER * kernelDiff / totalDiff));
            mIdle->put((ULONG)(PM_CPU_LOAD_MULTIPLIER * idleDiff / totalDiff));
        }
    
        mUserPrev   = user;
        mKernelPrev = kernel;
        mIdlePrev   = idle;
    }
}

void HostCpuMhz::init(ULONG period, ULONG length)
{
    mPeriod = period;
    mLength = length;
    mMHz->init(mLength);
}

void HostCpuMhz::collect()
{
    ULONG mhz;
    int rc = mHAL->getHostCpuMHz(&mhz);
    if (RT_SUCCESS(rc))
        mMHz->put(mhz);
}

void HostRamUsage::init(ULONG period, ULONG length)
{
    mPeriod = period;
    mLength = length;
    mTotal->init(mLength);
    mUsed->init(mLength);
    mAvailable->init(mLength);
}

void HostRamUsage::collect()
{
    ULONG total, used, available;
    int rc = mHAL->getHostMemoryUsage(&total, &used, &available);
    if (RT_SUCCESS(rc))
    {
        mTotal->put(total);
        mUsed->put(used);
        mAvailable->put(available);
    }
}



void MachineCpuLoad::init(ULONG period, ULONG length)
{
    mPeriod = period;
    mLength = length;
    mUser->init(mLength);
    mKernel->init(mLength);
}

void MachineCpuLoad::collect()
{
    ULONG user, kernel;
    int rc = mHAL->getProcessCpuLoad(mProcess, &user, &kernel);
    if (RT_SUCCESS(rc))
    {
        mUser->put(user);
        mKernel->put(kernel);
    }
}

void MachineCpuLoadRaw::collect()
{
    uint64_t processUser, processKernel, hostTotal;

    int rc = mHAL->getRawProcessCpuLoad(mProcess, &processUser, &processKernel, &hostTotal);
    if (RT_SUCCESS(rc))
    {
        if (hostTotal == mHostTotalPrev)
        {
            /* Nearly impossible, but... */
            mUser->put(0);
            mKernel->put(0);
        }
        else
        {
            mUser->put((ULONG)(PM_CPU_LOAD_MULTIPLIER * (processUser - mProcessUserPrev) / (hostTotal - mHostTotalPrev)));
            mKernel->put((ULONG)(PM_CPU_LOAD_MULTIPLIER * (processKernel - mProcessKernelPrev ) / (hostTotal - mHostTotalPrev)));
        }
    
        mHostTotalPrev     = hostTotal;
        mProcessUserPrev   = processUser;
        mProcessKernelPrev = processKernel;
    }
}

void MachineRamUsage::init(ULONG period, ULONG length)
{
    mPeriod = period;
    mLength = length;
    mUsed->init(mLength);
}

void MachineRamUsage::collect()
{
    ULONG used;
    int rc = mHAL->getProcessMemoryUsage(mProcess, &used);
    if (RT_SUCCESS(rc))
        mUsed->put(used);
}

void CircularBuffer::init(ULONG length)
{
    if (mData)
        RTMemFree(mData);
    mLength = length;
    mData = (ULONG *)RTMemAllocZ(length * sizeof(ULONG));
    mWrapped = false;
    mEnd = 0;
}

ULONG CircularBuffer::length()
{
    return mWrapped ? mLength : mEnd;
}

void CircularBuffer::put(ULONG value)
{
    if (mData)
    {
        mData[mEnd++] = value;
        if (mEnd >= mLength)
        {
            mEnd = 0;
            mWrapped = true;
        }
    }
}

void CircularBuffer::copyTo(ULONG *data)
{
    if (mWrapped)
    {
        memcpy(data, mData + mEnd, (mLength - mEnd) * sizeof(ULONG));
        // Copy the wrapped part
        if (mEnd)
            memcpy(data + (mLength - mEnd), mData, mEnd * sizeof(ULONG));
    }
    else
        memcpy(data, mData, mEnd * sizeof(ULONG));
}

void SubMetric::query(ULONG *data)
{
    copyTo(data);
}
    
void Metric::query(ULONG **data, ULONG *count)
{
    ULONG length;
    ULONG *tmpData;

    length = mSubMetric->length();
    if (length)
    {
        tmpData = (ULONG*)RTMemAlloc(sizeof(*tmpData)*length);
        mSubMetric->query(tmpData);
        if (mAggregate)
        {
            *count = 1;
            *data  = (ULONG*)RTMemAlloc(sizeof(**data));
            **data = mAggregate->compute(tmpData, length);
            RTMemFree(tmpData);
        }
        else
        {
            *count = length;
            *data  = tmpData;
        }
    }
    else
    {
        *count = 0;
        *data  = 0;
    }
}

ULONG AggregateAvg::compute(ULONG *data, ULONG length)
{
    uint64_t tmp = 0;
    for (ULONG i = 0; i < length; ++i)
        tmp += data[i];
    return (ULONG)(tmp / length);
}

const char * AggregateAvg::getName()
{
    return "avg";
}

ULONG AggregateMin::compute(ULONG *data, ULONG length)
{
    ULONG tmp = *data;
    for (ULONG i = 0; i < length; ++i)
        if (data[i] < tmp)
            tmp = data[i];
    return tmp;
}

const char * AggregateMin::getName()
{
    return "min";
}

ULONG AggregateMax::compute(ULONG *data, ULONG length)
{
    ULONG tmp = *data;
    for (ULONG i = 0; i < length; ++i)
        if (data[i] > tmp)
            tmp = data[i];
    return tmp;
}

const char * AggregateMax::getName()
{
    return "max";
}

Filter::Filter(ComSafeArrayIn(INPTR BSTR, metricNames),
               ComSafeArrayIn(IUnknown *, objects))
{
    com::SafeArray <INPTR BSTR> nameArray(ComSafeArrayInArg(metricNames));

    if (ComSafeArrayInIsNull(objects))
    {
        if (nameArray.size())
        {
            for (size_t i = 0; i < nameArray.size(); ++i)
                processMetricList(std::string(com::Utf8Str(nameArray[i])), ComPtr<IUnknown>());
        }
        else
            processMetricList(std::string("*"), ComPtr<IUnknown>());
    }
    else
    {
        com::SafeIfaceArray <IUnknown> objectArray(ComSafeArrayInArg(objects));

        for (size_t i = 0; i < objectArray.size(); ++i)
            switch (nameArray.size())
            {
                case 0:
                    processMetricList(std::string("*"), objectArray[i]);
                    break;
                case 1:
                    processMetricList(std::string(com::Utf8Str(nameArray[0])), objectArray[i]);
                    break;
                default:
                    processMetricList(std::string(com::Utf8Str(nameArray[i])), objectArray[i]);
                    break;
            }
    }
}

void Filter::processMetricList(const std::string &name, const ComPtr<IUnknown> object)
{
    std::string::size_type startPos = 0;

    for (std::string::size_type pos = name.find(",");
         pos != std::string::npos;
         pos = name.find(",", startPos))
    {
        mElements.push_back(std::make_pair(object, name.substr(startPos, pos - startPos)));
        startPos = pos + 1;
    }
    mElements.push_back(std::make_pair(object, name.substr(startPos)));
}

bool Filter::match(const ComPtr<IUnknown> object, const std::string &name) const
{
    ElementList::const_iterator it;

    LogFlowThisFunc(("Filter::match(%p, %s)\n", static_cast<const IUnknown*> (object), name.c_str()));
    for (it = mElements.begin(); it != mElements.end(); it++)
    {
        LogFlowThisFunc(("...matching against(%p, %s)\n", static_cast<const IUnknown*> ((*it).first), (*it).second.c_str()));
        if ((*it).first.isNull() || (*it).first == object)
        {
            // Objects match, compare names
            if ((*it).second == "*" || (*it).second == name)
            {
                LogFlowThisFunc(("...found!\n"));
                return true;
            }
        }
    }
    LogFlowThisFunc(("...no matches!\n"));
    return false;
}
