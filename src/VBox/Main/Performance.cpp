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

// Linux factory

MetricFactoryLinux::MetricFactoryLinux()
{
    mHAL = new CollectorLinux();
    Assert(mHAL);
}

BaseMetric *MetricFactoryLinux::createHostCpuLoad(ComPtr<IUnknown> object, SubMetric *user, SubMetric *kernel, SubMetric *idle)
{
    Assert(mHAL);
    return new HostCpuLoadRaw(mHAL, object, user, kernel, idle);
}

BaseMetric *MetricFactoryLinux::createMachineCpuLoad(ComPtr<IUnknown> object, RTPROCESS process, SubMetric *user, SubMetric *kernel)
{
    Assert(mHAL);
    return new MachineCpuLoadRaw(mHAL, object, process, user, kernel);
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

// Collector HAL for Linux
#include <stdio.h>

int CollectorLinux::getRawHostCpuLoad(unsigned long *user, unsigned long *kernel, unsigned long *idle)
{
#ifdef RT_OS_LINUX
    int rc = VINF_SUCCESS;
    unsigned long nice;
    FILE *f = fopen("/proc/stat", "r");

    if (f)
    {
        if (fscanf(f, "cpu %lu %lu %lu %lu", user, &nice, kernel, idle) == 4)
            *user += nice;
        else
            rc = VERR_FILE_IO_ERROR;
        fclose(f);
    }
    else
        rc = VERR_ACCESS_DENIED;

    return rc;
#else
    return E_NOTIMPL;
#endif
}

int CollectorLinux::getRawProcessCpuLoad(RTPROCESS process, unsigned long *user, unsigned long *kernel)
{
#ifdef RT_OS_LINUX
    int rc = VINF_SUCCESS;
    char *pszName;
    pid_t pid2;
    char c;
    int iTmp;
    unsigned uTmp;
    unsigned long ulTmp;
    char buf[80]; /* @todo: this should be tied to max allowed proc name. */

    RTStrAPrintf(&pszName, "/proc/%d/stat", process);
    //printf("Opening %s...\n", pszName);
    FILE *f = fopen(pszName, "r");
    RTMemFree(pszName);

    if (f)
    {
        if (fscanf(f, "%d %s %c %d %d %d %d %d %u %lu %lu %lu %lu %lu %lu",
                   &pid2, buf, &c, &iTmp, &iTmp, &iTmp, &iTmp, &iTmp, &uTmp,
                   &ulTmp, &ulTmp, &ulTmp, &ulTmp, user, kernel) == 15)
        {
            Assert((pid_t)process == pid2);
        }
        else
            rc = VERR_FILE_IO_ERROR;
        fclose(f);
    }
    else
        rc = VERR_ACCESS_DENIED;

    return rc;
#else
    return E_NOTIMPL;
#endif
}

int CollectorLinux::getHostCpuMHz(unsigned long *mhz)
{
    return E_NOTIMPL;
}

int CollectorLinux::getHostMemoryUsage(unsigned long *total, unsigned long *used, unsigned long *available)
{
#ifdef RT_OS_LINUX
    int rc = VINF_SUCCESS;
    unsigned long buffers, cached;
    FILE *f = fopen("/proc/meminfo", "r");

    if (f)
    {
        int processed = fscanf(f, "MemTotal: %lu kB", total);
        processed    += fscanf(f, "MemFree: %lu kB", available);
        processed    += fscanf(f, "Buffers: %lu kB", &buffers);
        processed    += fscanf(f, "Cached: %lu kB", &cached);
        if (processed == 4)
            *available += buffers + cached;
        else
            rc = VERR_FILE_IO_ERROR;
        fclose(f);
    }
    else
        rc = VERR_ACCESS_DENIED;

    return rc;
#else
    return E_NOTIMPL;
#endif
}
int CollectorLinux::getProcessMemoryUsage(RTPROCESS process, unsigned long *used)
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
    mHAL->getHostCpuLoad(&user, &kernel, &idle);
    mUser->put(user);
    mKernel->put(kernel);
    mIdle->put(idle);
}

void HostCpuLoadRaw::collect()
{
    unsigned long user, kernel, idle;
    unsigned long userDiff, kernelDiff, idleDiff, totalDiff;

    int rc = mHAL->getRawHostCpuLoad(&user, &kernel, &idle);
    AssertRC(rc);
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
    AssertRC(rc);
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
    AssertRC(rc);
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
    AssertRC(rc);
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
    AssertRC(rc);
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
    AssertRC(rc);
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
    return tmp / length;
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
    for (size_t i = 0; i < objectArray.size(); ++i)
        processMetricList(std::string(com::Utf8Str(nameArray[i])), objectArray[i]);
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
    return true;
}
