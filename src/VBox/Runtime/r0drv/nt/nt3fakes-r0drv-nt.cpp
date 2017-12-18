/* $Id$ */
/** @file
 * IPRT - NT 3.x fakes for NT 4.0 KPIs.
 */

/*
 * Copyright (C) 2006-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define PsGetVersion                PsGetVersion_Nt4Plus
#define ZwQuerySystemInformation    ZwQuerySystemInformation_Nt4Plus
#define KeInitializeTimerEx         KeInitializeTimerEx_Nt4Plus
#define KeSetTimerEx                KeSetTimerEx_Nt4Plus
#define IoAttachDeviceToDeviceStack IoAttachDeviceToDeviceStack_Nt4Plus
#define PsGetCurrentProcessId       PsGetCurrentProcessId_Nt4Plus
#define ZwYieldExecution            ZwYieldExecution_Nt4Plus

#include "the-nt-kernel.h"
#include <iprt/mem.h>

#include <iprt/assert.h>
#include <iprt/err.h>
#include "internal-r0drv-nt.h"

#undef PsGetVersion
#undef ZwQuerySystemInformation
#undef KeInitializeTimerEx
#undef KeSetTimerEx
#undef IoAttachDeviceToDeviceStack
#undef PsGetCurrentProcessId
#undef ZwYieldExecution


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
static uint32_t g_uNt3Version = 351;



extern "C" DECLEXPORT(BOOLEAN) __stdcall
PsGetVersion(ULONG *puMajor, ULONG *puMinor, ULONG *puBuildNo, UNICODE_STRING *pCsdStr)
{
    if (puMajor)
        *puMajor = 3;
    if (puMinor)
        *puMinor = 51;
    if (puBuildNo)
        *puMinor = 1057;
    if (pCsdStr)
    {
        pCsdStr->Buffer[0] = '\0';
        pCsdStr->Length = 0;
    }
    return FALSE; /* not checked. */
}


extern "C" DECLEXPORT(NTSTATUS) __stdcall
ZwQuerySystemInformation(SYSTEM_INFORMATION_CLASS enmClass, PVOID pvBuf, ULONG cbBuf, PULONG pcbActual)
{
    switch (enmClass)
    {
        case SystemModuleInformation:
        {
            PRTL_PROCESS_MODULES pInfo    = (PRTL_PROCESS_MODULES)pvBuf;
            ULONG                cbNeeded = RT_UOFFSETOF(RTL_PROCESS_MODULES, Modules[2]);
            if (pcbActual)
                *pcbActual = cbNeeded;
            if (cbBuf < cbNeeded)
                return STATUS_INFO_LENGTH_MISMATCH;

            pInfo->NumberOfModules = 2;

            /* ntoskrnl.exe */
            pInfo->Modules[0].Section           = NULL;
            pInfo->Modules[0].MappedBase        = (PVOID)UINT32_C(0x80100000);
            pInfo->Modules[0].ImageBase         = (PVOID)UINT32_C(0x80100000);
            pInfo->Modules[0].ImageSize         = UINT32_C(0x80400000) - UINT32_C(0x80100000);
            pInfo->Modules[0].Flags             = 0;
            pInfo->Modules[0].LoadOrderIndex    = 0;
            pInfo->Modules[0].InitOrderIndex    = 0;
            pInfo->Modules[0].LoadCount         = 1024;
            pInfo->Modules[0].OffsetToFileName  = sizeof("\\SystemRoot\\System32\\") - 1;
            memcpy(pInfo->Modules[0].FullPathName, RT_STR_TUPLE("\\SystemRoot\\System32\\ntoskrnl.exe"));

            /* hal.dll */
            pInfo->Modules[1].Section           = NULL;
            pInfo->Modules[1].MappedBase        = (PVOID)UINT32_C(0x80400000);
            pInfo->Modules[1].ImageBase         = (PVOID)UINT32_C(0x80400000);
            pInfo->Modules[1].ImageSize         = _512K;
            pInfo->Modules[1].Flags             = 0;
            pInfo->Modules[1].LoadOrderIndex    = 1;
            pInfo->Modules[1].InitOrderIndex    = 0;
            pInfo->Modules[1].LoadCount         = 1024;
            pInfo->Modules[1].OffsetToFileName  = sizeof("\\SystemRoot\\System32\\") - 1;
            memcpy(pInfo->Modules[1].FullPathName, RT_STR_TUPLE("\\SystemRoot\\System32\\hal.dll"));

            return STATUS_SUCCESS;
        }

        default:
            return STATUS_INVALID_INFO_CLASS;
    }
}


extern "C" DECLEXPORT(VOID)
KeInitializeTimerEx(PKTIMER pTimer, TIMER_TYPE enmType)
{
    KeInitializeTimer(pTimer);
    NOREF(enmType);
    /** @todo Default is NotificationTimer, for SyncrhonizationTimer we need to
     *        do more work.  timer-r0drv-nt.cpp is using the latter. :/  */
}


extern "C" DECLEXPORT(BOOLEAN) __stdcall
KeSetTimerEx(PKTIMER pTimer, LARGE_INTEGER DueTime, LONG cMsPeriod, PKDPC pDpc)
{
    AssertReturn(cMsPeriod == 0, FALSE);
    return KeSetTimer(pTimer, DueTime, pDpc);
}


extern "C" DECLEXPORT(PDEVICE_OBJECT)
IoAttachDeviceToDeviceStack(PDEVICE_OBJECT pSourceDevice, PDEVICE_OBJECT pTargetDevice)
{
    NOREF(pSourceDevice); NOREF(pTargetDevice);
    return NULL;
}


extern "C" DECLEXPORT(HANDLE)
PsGetCurrentProcessId(void)
{
    uint8_t const *pbProcess = (uint8_t const *)IoGetCurrentProcess();
    if (g_uNt3Version >= 350)
        return *(HANDLE const *)&pbProcess[0x94];
    return *(HANDLE const *)&pbProcess[0xb0];
}


extern "C" DECLEXPORT(NTSTATUS)
ZwYieldExecution(VOID)
{
    LARGE_INTEGER Interval;
    Interval.QuadPart = 0;
    KeDelayExecutionThread(KernelMode, FALSE, &Interval);
    return STATUS_SUCCESS;
}

