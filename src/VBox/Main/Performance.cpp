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

#include <VBox/com/array.h>
#include <VBox/com/ptr.h>
#include <VBox/com/string.h>
#include <VBox/err.h>
#include <iprt/string.h>
#include <iprt/mem.h>

#include "Performance.h"

using namespace pm;

// Default factory

BaseMetric *MetricFactory::createHostCpuLoad(ComPtr<IUnknown> object, SubMetric *user, SubMetric *kernel, SubMetric *idle)
{
    Assert(mHAL);
    return new HostCpuLoad(mHAL, object, user, kernel, idle);
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
    return new MachineCpuLoad(mHAL, object, process, user, kernel);
}
BaseMetric *MetricFactory::createMachineRamUsage(ComPtr<IUnknown> object, RTPROCESS process, SubMetric *used)
{
    Assert(mHAL);
    return new MachineRamUsage(mHAL, object, process, used);
}

// Stubs for non-pure virtual methods

int CollectorHAL::getHostCpuLoad(unsigned long *user, unsigned long *kernel, unsigned long *idle)
{
    return E_NOTIMPL;
}

int CollectorHAL::getProcessCpuLoad(RTPROCESS process, unsigned long *user, unsigned long *kernel)
{
    return E_NOTIMPL;
}

int CollectorHAL::getRawHostCpuLoad(unsigned long *user, unsigned long *kernel, unsigned long *idle)
{
    return E_NOTIMPL;
}

int CollectorHAL::getRawProcessCpuLoad(RTPROCESS process, unsigned long *user, unsigned long *kernel)
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
            collect();
        }
    }
}

void HostCpuLoad::init(unsigned long period, unsigned long length)
{
    mPeriod = period;
    mLength = length;
    mUser->init(mLength);
    mKernel->init(mLength);
    mIdle->init(mLength);
}

void HostCpuLoad::collect()
{
    unsigned long user, kernel, idle;
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
    unsigned long user, kernel, idle;
    unsigned long userDiff, kernelDiff, idleDiff, totalDiff;

    int rc = mHAL->getRawHostCpuLoad(&user, &kernel, &idle);
    if (RT_SUCCESS(rc))
    {
        userDiff   = user   - mUserPrev;
        kernelDiff = kernel - mKernelPrev;
        idleDiff   = idle   - mIdlePrev;
        totalDiff  = userDiff + kernelDiff + idleDiff;
    
        mUser->put(PM_CPU_LOAD_MULTIPLIER * userDiff / totalDiff);
        mKernel->put(PM_CPU_LOAD_MULTIPLIER * kernelDiff / totalDiff);
        mIdle->put(PM_CPU_LOAD_MULTIPLIER * idleDiff / totalDiff);
    
        mUserPrev   = user;
        mKernelPrev = kernel;
        mIdlePrev   = idle;
    }
}

void HostCpuMhz::init(unsigned long period, unsigned long length)
{
    mPeriod = period;
    mLength = length;
    mMHz->init(mLength);
}

void HostCpuMhz::collect()
{
    unsigned long mhz;
    int rc = mHAL->getHostCpuMHz(&mhz);
    if (RT_SUCCESS(rc))
        mMHz->put(mhz);
}

void HostRamUsage::init(unsigned long period, unsigned long length)
{
    mPeriod = period;
    mLength = length;
    mTotal->init(mLength);
    mUsed->init(mLength);
    mAvailable->init(mLength);
}

void HostRamUsage::collect()
{
    unsigned long total, used, available;
    int rc = mHAL->getHostMemoryUsage(&total, &used, &available);
    if (RT_SUCCESS(rc))
    {
        mTotal->put(total);
        mUsed->put(used);
        mAvailable->put(available);
    }
}



void MachineCpuLoad::init(unsigned long period, unsigned long length)
{
    mPeriod = period;
    mLength = length;
    mUser->init(mLength);
    mKernel->init(mLength);
}

void MachineCpuLoad::collect()
{
    unsigned long user, kernel;
    int rc = mHAL->getProcessCpuLoad(mProcess, &user, &kernel);
    if (RT_SUCCESS(rc))
    {
        mUser->put(user);
        mKernel->put(kernel);
    }
}

void MachineCpuLoadRaw::collect()
{
    unsigned long hostUser, hostKernel, hostIdle, hostTotal;
    unsigned long processUser, processKernel;

    int rc = mHAL->getRawHostCpuLoad(&hostUser, &hostKernel, &hostIdle);
    if (RT_SUCCESS(rc))
    {
        hostTotal = hostUser + hostKernel + hostIdle;
    
        rc = mHAL->getRawProcessCpuLoad(mProcess, &processUser, &processKernel);
        AssertRC(rc);
        if (RT_SUCCESS(rc))
        {
            mUser->put(PM_CPU_LOAD_MULTIPLIER * (processUser - mProcessUserPrev) / (hostTotal - mHostTotalPrev));
            mUser->put(PM_CPU_LOAD_MULTIPLIER * (processKernel - mProcessKernelPrev ) / (hostTotal - mHostTotalPrev));
        
            mHostTotalPrev     = hostTotal;
            mProcessUserPrev   = processUser;
            mProcessKernelPrev = processKernel;
        }
    }
}

void MachineRamUsage::init(unsigned long period, unsigned long length)
{
    mPeriod = period;
    mLength = length;
    mUsed->init(mLength);
}

void MachineRamUsage::collect()
{
    unsigned long used;
    int rc = mHAL->getProcessMemoryUsage(mProcess, &used);
    if (RT_SUCCESS(rc))
        mUsed->put(used);
}

void CircularBuffer::init(unsigned long length)
{
    if (mData)
        RTMemFree(mData);
    mLength = length;
    mData = (unsigned long *)RTMemAllocZ(length * sizeof(unsigned long));
    mWrapped = false;
    mEnd = 0;
}

unsigned long CircularBuffer::length()
{
    return mWrapped ? mLength : mEnd;
}

void CircularBuffer::put(unsigned long value)
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

void CircularBuffer::copyTo(unsigned long *data)
{
    if (mWrapped)
    {
        memcpy(data, mData + mEnd, (mLength - mEnd) * sizeof(unsigned long));
        // Copy the wrapped part
        if (mEnd)
            memcpy(data + mEnd, mData, mEnd * sizeof(unsigned long));
    }
    else
        memcpy(data, mData, mEnd * sizeof(unsigned long));
}

void SubMetric::query(unsigned long *data)
{
    copyTo(data);
}
    
void Metric::query(unsigned long **data, unsigned long *count)
{
    unsigned long length;
    unsigned long *tmpData;

    length = mSubMetric->length();
    if (length)
    {
        tmpData = (unsigned long*)RTMemAlloc(sizeof(*tmpData)*length);
        mSubMetric->query(tmpData);
        if (mAggregate)
        {
            *count = 1;
            *data  = (unsigned long*)RTMemAlloc(sizeof(**data));
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

unsigned long AggregateAvg::compute(unsigned long *data, unsigned long length)
{
    uint64_t tmp = 0;
    for (unsigned long i = 0; i < length; ++i)
        tmp += data[i];
    return (unsigned long)(tmp / length);
}

const char * AggregateAvg::getName()
{
    return "avg";
}

unsigned long AggregateMin::compute(unsigned long *data, unsigned long length)
{
    unsigned long tmp = *data;
    for (unsigned long i = 0; i < length; ++i)
        if (data[i] < tmp)
            tmp = data[i];
    return tmp;
}

const char * AggregateMin::getName()
{
    return "min";
}

unsigned long AggregateMax::compute(unsigned long *data, unsigned long length)
{
    unsigned long tmp = *data;
    for (unsigned long i = 0; i < length; ++i)
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
    com::SafeIfaceArray <IUnknown> objectArray(ComSafeArrayInArg(objects));
    com::SafeArray <INPTR BSTR> nameArray(ComSafeArrayInArg(metricNames));
    if (objectArray.isNull())
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

    printf("Filter::match(%p, %s)\n", static_cast<const IUnknown*> (object), name.c_str());
    for (it = mElements.begin(); it != mElements.end(); it++)
    {
        printf("...matching against(%p, %s)\n", static_cast<const IUnknown*> ((*it).first), (*it).second.c_str());
        if ((*it).first.isNull() || (*it).first == object)
        {
            // Objects match, compare names
            if ((*it).second == "*" || (*it).second == name)
            {
                printf("...found!\n");
                return true;
            }
        }
    }
    printf("...no matches!\n");
    return false;
}
