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

#include <Wbemidl.h>
#include <iprt/err.h>

#include "Logging.h"
#include "Performance.h"

namespace pm {

class CollectorWin : public CollectorHAL
{
public:
    CollectorWin();
    ~CollectorWin();

    virtual int getHostCpuLoad(ULONG *user, ULONG *kernel, ULONG *idle);
    virtual int getHostCpuMHz(ULONG *mhz);
    virtual int getHostMemoryUsage(ULONG *total, ULONG *used, ULONG *available);
    virtual int getProcessCpuLoad(RTPROCESS process, ULONG *user, ULONG *kernel);
    virtual int getProcessMemoryUsage(RTPROCESS process, ULONG *used);

    virtual int getRawHostCpuLoad(uint64_t *user, uint64_t *kernel, uint64_t *idle);
    virtual int getRawProcessCpuLoad(RTPROCESS process, uint64_t *user, uint64_t *kernel, uint64_t *total);
private:
    long        getPropertyHandle(IWbemObjectAccess *objAccess, LPCWSTR name);
    int         getObjects(IWbemHiPerfEnum *mEnum, IWbemObjectAccess ***objArray, DWORD *numReturned);

    IWbemRefresher     *mRefresher;
    IWbemServices      *mNameSpace;

    IWbemHiPerfEnum    *mEnumProcessor;
    long               mEnumProcessorID;
    long               mHostCpuLoadNameHandle;
    long               mHostCpuLoadUserHandle;
    long               mHostCpuLoadKernelHandle;
    long               mHostCpuLoadIdleHandle;

    IWbemHiPerfEnum    *mEnumProcess;
    long               mEnumProcessID;
    long               mProcessPIDHandle;
    long               mProcessCpuLoadUserHandle;
    long               mProcessCpuLoadKernelHandle;
    long               mProcessCpuLoadTimestampHandle;
    long               mProcessMemoryUsedHandle;
};

MetricFactoryWin::MetricFactoryWin()
{
    mHAL = new CollectorWin();
    Assert(mHAL);
}

CollectorWin::CollectorWin() : mRefresher(0), mNameSpace(0), mEnumProcessor(0), mEnumProcess(0)
{
    HRESULT                 hr = S_OK;
    IWbemConfigureRefresher *pConfig = NULL;
    IWbemLocator            *pWbemLocator = NULL;
    BSTR                    bstrNameSpace = NULL;

    if (SUCCEEDED (hr = CoCreateInstance(
        CLSID_WbemLocator, 
        NULL,
        CLSCTX_INPROC_SERVER,
        IID_IWbemLocator,
        (void**) &pWbemLocator)))
    {
        // Connect to the desired namespace.
        bstrNameSpace = SysAllocString(L"\\\\.\\root\\cimv2");
        if (bstrNameSpace)
        {
            hr = pWbemLocator->ConnectServer(
                bstrNameSpace,
                NULL, // User name
                NULL, // Password
                NULL, // Locale
                0L,   // Security flags
                NULL, // Authority
                NULL, // Wbem context
                &mNameSpace);
        }
        pWbemLocator->Release();
        SysFreeString(bstrNameSpace);
    }

    if (FAILED (hr)) {
        Log (("Failed to get namespace. HR = %x\n", hr));
        return;
    }

    if (SUCCEEDED (hr = CoCreateInstance(
        CLSID_WbemRefresher,
        NULL,
        CLSCTX_INPROC_SERVER,
        IID_IWbemRefresher, 
        (void**) &mRefresher)))
    {
        if (SUCCEEDED (hr = mRefresher->QueryInterface(
            IID_IWbemConfigureRefresher,
            (void **)&pConfig)))
        {
            // Add an enumerator to the refresher.
            if (SUCCEEDED (hr = pConfig->AddEnum(
                mNameSpace, 
                L"Win32_PerfRawData_PerfOS_Processor", 
                0, 
                NULL, 
                &mEnumProcessor, 
                &mEnumProcessorID)))
            {
                hr = pConfig->AddEnum(
                    mNameSpace,
                    L"Win32_PerfRawData_PerfProc_Process", 
                    0, 
                    NULL, 
                    &mEnumProcess, 
                    &mEnumProcessID);
            }
            pConfig->Release();
        }
    }


    if (FAILED (hr)) {
        Log (("Failed to add enumerators. HR = %x\n", hr));
        return;
    }

    // Retrieve property handles

    if (FAILED (hr = mRefresher->Refresh(0L)))
    {
        Log (("Refresher failed. HR = %x\n", hr));
        return;
    }

    IWbemObjectAccess       **apEnumAccess = NULL;
    DWORD                   dwNumReturned = 0;

    if (RT_FAILURE(getObjects(mEnumProcessor, &apEnumAccess, &dwNumReturned)))
        return;

    mHostCpuLoadNameHandle   = getPropertyHandle(apEnumAccess[0], L"Name");
    mHostCpuLoadUserHandle   = getPropertyHandle(apEnumAccess[0], L"PercentUserTime");
    mHostCpuLoadKernelHandle = getPropertyHandle(apEnumAccess[0], L"PercentPrivilegedTime");
    mHostCpuLoadIdleHandle   = getPropertyHandle(apEnumAccess[0], L"PercentProcessorTime");

    delete [] apEnumAccess;

    if (RT_FAILURE(getObjects(mEnumProcess, &apEnumAccess, &dwNumReturned)))
        return;

    mProcessPIDHandle              = getPropertyHandle(apEnumAccess[0], L"IDProcess");
    mProcessCpuLoadUserHandle      = getPropertyHandle(apEnumAccess[0], L"PercentUserTime");
    mProcessCpuLoadKernelHandle    = getPropertyHandle(apEnumAccess[0], L"PercentPrivilegedTime");
    mProcessCpuLoadTimestampHandle = getPropertyHandle(apEnumAccess[0], L"Timestamp_Sys100NS");
    mProcessMemoryUsedHandle       = getPropertyHandle(apEnumAccess[0], L"WorkingSet");

    delete [] apEnumAccess;
}

CollectorWin::~CollectorWin()
{
    if (NULL != mNameSpace)
    {
        mNameSpace->Release();
    }
    if (NULL != mEnumProcessor)
    {
        mEnumProcessor->Release();
    }
    if (NULL != mEnumProcess)
    {
        mEnumProcess->Release();
    }
    if (NULL != mRefresher)
    {
        mRefresher->Release();
    }
}

long CollectorWin::getPropertyHandle(IWbemObjectAccess *objAccess, LPCWSTR name)
{
    HRESULT hr;
    CIMTYPE tmp;
    long    handle;

    if (FAILED (hr = objAccess->GetPropertyHandle(
        name,
        &tmp,
        &handle)))
    {
        Log (("Failed to get property handle for '%ls'. HR = %x\n", name, hr));
        return 0;   /// @todo use throw
    }

    return handle;
}

int CollectorWin::getObjects(IWbemHiPerfEnum *mEnum, IWbemObjectAccess ***objArray, DWORD *numReturned)
{
    HRESULT hr;
    DWORD   dwNumObjects = 0;

    *objArray    = NULL;
    *numReturned = 0;
    hr = mEnum->GetObjects(0L, dwNumObjects, *objArray, numReturned);

    // If the buffer was not big enough,
    // allocate a bigger buffer and retry.
    if (hr == WBEM_E_BUFFER_TOO_SMALL 
        && *numReturned > dwNumObjects)
    {
        *objArray = new IWbemObjectAccess*[*numReturned];
        if (NULL == *objArray)
        {
            Log (("Could not allocate enumerator access objects\n"));
            return VERR_NO_MEMORY;
        }

        SecureZeroMemory(*objArray,
            *numReturned*sizeof(IWbemObjectAccess*));
        dwNumObjects = *numReturned;

        if (FAILED (hr = mEnum->GetObjects(0L, 
            dwNumObjects, *objArray, numReturned)))
        {
            delete [] objArray;
            Log (("Failed to get objects from enumerator. HR = %x\n", hr));
            return VERR_INTERNAL_ERROR;
        }
    }
    else if (FAILED (hr))
    {
        Log (("Failed to get objects from enumerator. HR = %x\n", hr));
        return VERR_INTERNAL_ERROR;
    }

    return VINF_SUCCESS;
}

int CollectorWin::getHostCpuLoad(ULONG *user, ULONG *kernel, ULONG *idle)
{
    return VERR_NOT_IMPLEMENTED;
}

int CollectorWin::getRawHostCpuLoad(uint64_t *user, uint64_t *kernel, uint64_t *idle)
{
    HRESULT hr;
    IWbemObjectAccess       **apEnumAccess = NULL;
    DWORD                   dwNumReturned = 0;

    LogFlowThisFuncEnter();

    if (FAILED (hr = mRefresher->Refresh(0L)))
    {
        Log (("Refresher failed. HR = %x\n", hr));
        return VERR_INTERNAL_ERROR;
    }

    int rc = getObjects(mEnumProcessor, &apEnumAccess, &dwNumReturned);
    if (RT_FAILURE(rc))
        return rc;

    for (unsigned i = 0; i < dwNumReturned; i++)
    {
        long  bytesRead = 0;
        WCHAR tmpBuf[200];

        if (FAILED (hr = apEnumAccess[i]->ReadPropertyValue(
            mHostCpuLoadNameHandle,
            sizeof(tmpBuf),
            &bytesRead,
            (byte*)tmpBuf)))
        {
            Log (("Failed to read 'Name' property. HR = %x\n", hr));
            return VERR_INTERNAL_ERROR;
        }
        if (wcscmp(tmpBuf, L"_Total") == 0)
        {
            if (FAILED (hr = apEnumAccess[i]->ReadQWORD(
                mHostCpuLoadUserHandle,
                user)))
            {
            Log (("Failed to read 'PercentUserTime' property. HR = %x\n", hr));
                return VERR_INTERNAL_ERROR;
            }
            if (FAILED (hr = apEnumAccess[i]->ReadQWORD(
                mHostCpuLoadKernelHandle,
                kernel)))
            {
            Log (("Failed to read 'PercentPrivilegedTime' property. HR = %x\n", hr));
                return VERR_INTERNAL_ERROR;
            }
            if (FAILED (hr = apEnumAccess[i]->ReadQWORD(
                mHostCpuLoadIdleHandle,
                idle)))
            {
            Log (("Failed to read 'PercentProcessorTime' property. HR = %x\n", hr));
                return VERR_INTERNAL_ERROR;
            }
            rc = VINF_SUCCESS;
        }
        apEnumAccess[i]->Release();
        apEnumAccess[i] = NULL;
    }
    delete [] apEnumAccess;

    LogFlowThisFunc(("user=%lu kernel=%lu idle=%lu\n", *user, *kernel, *idle));
    LogFlowThisFuncLeave();

    return rc;
}

int CollectorWin::getHostCpuMHz(ULONG *mhz)
{
    return VERR_NOT_IMPLEMENTED;
}

int CollectorWin::getHostMemoryUsage(ULONG *total, ULONG *used, ULONG *available)
{
    MEMORYSTATUSEX mstat;
    
    mstat.dwLength = sizeof(mstat);
    if (GlobalMemoryStatusEx(&mstat))
    {
        *total = (ULONG)( mstat.ullTotalPhys / 1000 );
        *available = (ULONG)( mstat.ullAvailPhys / 1000 );
        *used = *total - *available;
    }
    else
        return RTErrConvertFromWin32(GetLastError());

    return VINF_SUCCESS;
}

int CollectorWin::getProcessCpuLoad(RTPROCESS process, ULONG *user, ULONG *kernel)
{
    return VERR_NOT_IMPLEMENTED;
}

int CollectorWin::getRawProcessCpuLoad(RTPROCESS process, uint64_t *user, uint64_t *kernel, uint64_t *total)
{
    HRESULT hr;
    IWbemObjectAccess       **apEnumAccess = NULL;
    DWORD                   dwNumReturned = 0;

    LogFlowThisFuncEnter();

    if (FAILED (hr = mRefresher->Refresh(0L)))
    {
        Log (("Refresher failed. HR = %x\n", hr));
        return VERR_INTERNAL_ERROR;
    }

    int rc = getObjects(mEnumProcess, &apEnumAccess, &dwNumReturned);
    if (RT_FAILURE(rc))
        return rc;

    rc = VERR_NOT_FOUND;

    for (unsigned i = 0; i < dwNumReturned; i++)
    {
        DWORD dwIDProcess;

        if (FAILED (hr = apEnumAccess[i]->ReadDWORD(
            mProcessPIDHandle,
            &dwIDProcess)))
        {
            Log (("Failed to read 'IDProcess' property. HR = %x\n", hr));
            return VERR_INTERNAL_ERROR;
        }
        LogFlowThisFunc (("Matching machine process %x against %x...\n", process, dwIDProcess));
        if (dwIDProcess == process)
        {
            LogFlowThisFunc (("Match found.\n"));
            if (FAILED (hr = apEnumAccess[i]->ReadQWORD(
                mProcessCpuLoadUserHandle,
                user)))
            {
            Log (("Failed to read 'PercentUserTime' property. HR = %x\n", hr));
                return VERR_INTERNAL_ERROR;
            }
            if (FAILED (hr = apEnumAccess[i]->ReadQWORD(
                mProcessCpuLoadKernelHandle,
                kernel)))
            {
            Log (("Failed to read 'PercentPrivilegedTime' property. HR = %x\n", hr));
                return VERR_INTERNAL_ERROR;
            }
            if (FAILED (hr = apEnumAccess[i]->ReadQWORD(
                mProcessCpuLoadTimestampHandle,
                total)))
            {
            Log (("Failed to read 'Timestamp_Sys100NS' property. HR = %x\n", hr));
                return VERR_INTERNAL_ERROR;
            }
            rc = VINF_SUCCESS;
        }
        apEnumAccess[i]->Release();
        apEnumAccess[i] = NULL;
    }
    delete [] apEnumAccess;

    LogFlowThisFunc(("user=%lu kernel=%lu total=%lu\n", *user, *kernel, *total));
    LogFlowThisFuncLeave();

    return rc;
}

int CollectorWin::getProcessMemoryUsage(RTPROCESS process, ULONG *used)
{
    HRESULT hr;
    IWbemObjectAccess       **apEnumAccess = NULL;
    DWORD                   dwNumReturned = 0;

    LogFlowThisFuncEnter();

    if (FAILED (hr = mRefresher->Refresh(0L)))
    {
        Log (("Refresher failed. HR = %x\n", hr));
        return VERR_INTERNAL_ERROR;
    }

    int rc = getObjects(mEnumProcess, &apEnumAccess, &dwNumReturned);
    if (RT_FAILURE(rc))
        return rc;

    rc = VERR_NOT_FOUND;

    for (unsigned i = 0; i < dwNumReturned; i++)
    {
        DWORD dwIDProcess;

        if (FAILED (hr = apEnumAccess[i]->ReadDWORD(
            mProcessPIDHandle,
            &dwIDProcess)))
        {
            Log (("Failed to read 'IDProcess' property. HR = %x\n", hr));
            return VERR_INTERNAL_ERROR;
        }
        if (dwIDProcess == process)
        {
            uint64_t u64used = 0;

            if (FAILED (hr = apEnumAccess[i]->ReadQWORD(
                mProcessMemoryUsedHandle,
                &u64used)))
            {
            Log (("Failed to read 'WorkingSet' property. HR = %x\n", hr));
                return VERR_INTERNAL_ERROR;
            }
            *used = (ULONG)(u64used / 1024);
            rc = VINF_SUCCESS;
        }
        apEnumAccess[i]->Release();
        apEnumAccess[i] = NULL;
    }
    delete [] apEnumAccess;

    LogFlowThisFunc(("used=%lu\n", *used));
    LogFlowThisFuncLeave();

    return rc;
}

}
