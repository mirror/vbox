/* $Id$ */

/** @file
 *
 * VBox Windows-specific Performance Classes implementation.
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

#include <pdh.h>
#include <pdhmsg.h>
#include <iprt/err.h>

#include "Performance.h"

//#pragma comment(lib, "pdh.lib")

namespace pm {

class CollectorWin : public CollectorHAL
{
public:
    CollectorWin();
    ~CollectorWin();

    virtual int getHostCpuLoad(unsigned long *user, unsigned long *kernel, unsigned long *idle);
    virtual int getHostCpuMHz(unsigned long *mhz);
    virtual int getHostMemoryUsage(unsigned long *total, unsigned long *used, unsigned long *available);
    virtual int getProcessCpuLoad(RTPROCESS process, unsigned long *user, unsigned long *kernel);
    virtual int getProcessMemoryUsage(RTPROCESS process, unsigned long *used);

private:
    int         convertPdhStatusToRTErr(PDH_STATUS pdhStatus);
    HCOUNTER    addCounter(HQUERY hQuery, LPCTSTR name);
    int         getCounterValue(HCOUNTER hCounter, DWORD flags, unsigned long *value);

    HQUERY      mHostCpuLoadQuery;
    HCOUNTER    mHostCpuLoadUserCounter;
    HCOUNTER    mHostCpuLoadKernelCounter;
    HCOUNTER    mHostCpuLoadIdleCounter;
};

MetricFactoryWin::MetricFactoryWin()
{
    mHAL = new CollectorWin();
    Assert(mHAL);
}

CollectorWin::CollectorWin() : mHostCpuLoadQuery(0)
{
    PDH_STATUS pdhStatus;

    pdhStatus = PdhOpenQuery(NULL, NULL, &mHostCpuLoadQuery);
    if (pdhStatus != ERROR_SUCCESS)
        return;

    mHostCpuLoadUserCounter   = addCounter (mHostCpuLoadQuery,
                                            L"\\Processor(_Total)\\% User Time");
    mHostCpuLoadKernelCounter = addCounter (mHostCpuLoadQuery,
                                            L"\\Processor(_Total)\\% Privileged Time");
    mHostCpuLoadIdleCounter   = addCounter (mHostCpuLoadQuery,
                                            L"\\Processor(_Total)\\% Idle Time");
}

CollectorWin::~CollectorWin()
{
    if (mHostCpuLoadQuery)
        PdhCloseQuery (mHostCpuLoadQuery);
}

int CollectorWin::convertPdhStatusToRTErr(PDH_STATUS pdhStatus)
{
    switch (pdhStatus)
    {
        case PDH_INVALID_ARGUMENT:
            return VERR_INVALID_PARAMETER;
        case PDH_INVALID_HANDLE:
            return VERR_INVALID_HANDLE;
    }

    return RTErrConvertFromWin32(pdhStatus);
}

HCOUNTER CollectorWin::addCounter(HQUERY hQuery, LPCTSTR name)
{
    PDH_STATUS pdhStatus;
    HCOUNTER   hCounter;

    pdhStatus = PdhAddCounter (hQuery, name, 0, &hCounter);
    if (pdhStatus != ERROR_SUCCESS)
        return 0;
    return hCounter;
}

int CollectorWin::getCounterValue(HCOUNTER hCounter, DWORD flags, unsigned long *value)
{
    PDH_STATUS           pdhStatus;
    PDH_FMT_COUNTERVALUE fmtValue;

    pdhStatus = PdhGetFormattedCounterValue (hCounter,
                                             PDH_FMT_LONG | flags,
                                             NULL,
                                             &fmtValue);
    if (pdhStatus != ERROR_SUCCESS)
        return convertPdhStatusToRTErr(pdhStatus);
    
    *value = fmtValue.longValue;
    
    return VINF_SUCCESS;
}

int CollectorWin::getHostCpuLoad(unsigned long *user, unsigned long *kernel, unsigned long *idle)
{
    int         rc;
    PDH_STATUS  pdhStatus;

    pdhStatus = PdhCollectQueryData (mHostCpuLoadQuery);
    if (pdhStatus != ERROR_SUCCESS)
        return convertPdhStatusToRTErr(pdhStatus);

    rc = getCounterValue (mHostCpuLoadUserCounter, PDH_FMT_1000, user);
    AssertRCReturn(rc, rc);

    rc = getCounterValue (mHostCpuLoadKernelCounter, PDH_FMT_1000, kernel);
    AssertRCReturn(rc, rc);

    rc = getCounterValue (mHostCpuLoadIdleCounter, PDH_FMT_1000, idle);
    AssertRCReturn(rc, rc);

    return VINF_SUCCESS;
}

int CollectorWin::getHostCpuMHz(unsigned long *mhz)
{
    return E_NOTIMPL;
}

int CollectorWin::getHostMemoryUsage(unsigned long *total, unsigned long *used, unsigned long *available)
{
    MEMORYSTATUSEX mstat;
    
    mstat.dwLength = sizeof(mstat);
    if (GlobalMemoryStatusEx(&mstat))
    {
        *total = (unsigned long)( mstat.ullTotalPhys / 1000 );
        *available = (unsigned long)( mstat.ullAvailPhys / 1000 );
        *used = *total - *available;
    }
    else
        return RTErrConvertFromWin32(GetLastError());

    return VINF_SUCCESS;
}

int CollectorWin::getProcessCpuLoad(RTPROCESS process, unsigned long *user, unsigned long *kernel)
{
    return E_NOTIMPL;
}

int CollectorWin::getProcessMemoryUsage(RTPROCESS process, unsigned long *used)
{
    return E_NOTIMPL;
}

}