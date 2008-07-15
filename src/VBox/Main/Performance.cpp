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

#include <VBox/com/ptr.h>
#include <VBox/err.h>
#include <iprt/string.h>
#include <iprt/mem.h>

#include "Performance.h"

using namespace pm;

// Default factory

BaseMetric *MetricFactory::createHostCpuLoad(IUnknown *object, SubMetric *user, SubMetric *kernel, SubMetric *idle)
{
    Assert(mHAL);
    return new HostCpuLoad(mHAL, object, user, kernel, idle);
}
BaseMetric *MetricFactory::createHostCpuMHz(IUnknown *object, SubMetric *mhz)
{
    Assert(mHAL);
    return new HostCpuMhz(mHAL, object, mhz);
}
BaseMetric *MetricFactory::createHostRamUsage(IUnknown *object, SubMetric *total, SubMetric *used, SubMetric *available)
{
    Assert(mHAL);
    return new HostRamUsage(mHAL, object, total, used, available);
}
BaseMetric *MetricFactory::createMachineCpuLoad(IUnknown *object, RTPROCESS process, SubMetric *user, SubMetric *kernel)
{
    Assert(mHAL);
    return new MachineCpuLoad(mHAL, object, process, user, kernel);
}
BaseMetric *MetricFactory::createMachineRamUsage(IUnknown *object, RTPROCESS process, SubMetric *used)
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

BaseMetric *MetricFactoryLinux::createHostCpuLoad(IUnknown *object, SubMetric *user, SubMetric *kernel, SubMetric *idle)
{
    Assert(mHAL);
    return new HostCpuLoadRaw(mHAL, object, user, kernel, idle);
}

BaseMetric *MetricFactoryLinux::createMachineCpuLoad(IUnknown *object, RTPROCESS process, SubMetric *user, SubMetric *kernel)
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
    return E_NOTIMPL;
}
int CollectorLinux::getProcessMemoryUsage(RTPROCESS process, unsigned long *used)
{
    return E_NOTIMPL;
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

    mHAL->getRawHostCpuLoad(&user, &kernel, &idle);

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

void HostCpuMhz::collect()
{
    unsigned long mhz;
    mHAL->getHostCpuMHz(&mhz);
    mMHz->put(mhz);
}

void HostRamUsage::collect()
{
    unsigned long total, used, available;
    mHAL->getHostMemoryUsage(&total, &used, &available);
    mTotal->put(total);
    mUsed->put(used);
    mAvailable->put(available);
}


void MachineCpuLoad::collect()
{
    unsigned long user, kernel;
    mHAL->getProcessCpuLoad(mProcess, &user, &kernel);
    mUser->put(user);
    mKernel->put(kernel);
}

void MachineCpuLoadRaw::collect()
{
    unsigned long hostUser, hostKernel, hostIdle, hostTotal;
    unsigned long processUser, processKernel;

    mHAL->getRawHostCpuLoad(&hostUser, &hostKernel, &hostIdle);
    hostTotal = hostUser + hostKernel + hostIdle;

    mHAL->getRawProcessCpuLoad(mProcess, &processUser, &processKernel);
    mUser->put(PM_CPU_LOAD_MULTIPLIER * (processUser - mProcessUserPrev) / (hostTotal - mHostTotalPrev));
    mUser->put(PM_CPU_LOAD_MULTIPLIER * (processKernel - mProcessKernelPrev ) / (hostTotal - mHostTotalPrev));

    mHostTotalPrev     = hostTotal;
    mProcessUserPrev   = processUser;
    mProcessKernelPrev = processKernel;
}

void MachineRamUsage::collect()
{
    unsigned long used;
    mHAL->getProcessMemoryUsage(mProcess, &used);
    mUsed->put(used);
}

void CircularBuffer::init(unsigned long length)
{
    if (mData)
        RTMemFree(mData);
    mLength = length;
    mData = (unsigned long *)RTMemAllocZ(length * sizeof(unsigned long));
    mPosition = 0;
}

unsigned long CircularBuffer::length()
{
    return mLength;
}

void CircularBuffer::put(unsigned long value)
{
    mData[mPosition++] = value;
    if (mPosition >= mLength)
        mPosition = 0;
}

void CircularBuffer::copyTo(unsigned long *data, unsigned long length)
{
    memcpy(data, mData + mPosition, (length - mPosition) * sizeof(unsigned long));
    // Copy the wrapped part
    if (mPosition)
        memcpy(data + mPosition, mData, mPosition * sizeof(unsigned long));
}

void SubMetric::query(unsigned long *data, unsigned long count)
{
    copyTo(data, count);
}
    
void Metric::query(unsigned long **data, unsigned long *count)
{
    unsigned long length;
    unsigned long *tmpData;

    length = mSubMetric->length();
    if (length)
    {
        tmpData = (unsigned long*)RTMemAlloc(sizeof(*tmpData)*length);
        mSubMetric->query(tmpData, length);
        if (mAggregate)
        {
            *count = 1;
            *data  = (unsigned long*)RTMemAlloc(sizeof(**data));
            **data = mAggregate->compute(tmpData, length);
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
    return 0;
}

const char * AggregateAvg::getName()
{
    return "avg";
}

unsigned long AggregateMin::compute(unsigned long *data, unsigned long length)
{
    return 0;
}

const char * AggregateMin::getName()
{
    return "min";
}

unsigned long AggregateMax::compute(unsigned long *data, unsigned long length)
{
    return 0;
}

const char * AggregateMax::getName()
{
    return "max";
}

