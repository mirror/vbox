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

#define _IMAGE_NT_HEADERS           RT_CONCAT(_IMAGE_NT_HEADERS,ARCH_BITS)
#include "the-nt-kernel.h"
#include <iprt/mem.h>

#include <iprt/assert.h>
#include <iprt/ctype.h>
#include <iprt/err.h>
#include <iprt/log.h>
#include <iprt/string.h>
#include <iprt/formats/mz.h>
#include <iprt/formats/pecoff.h>
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
static uint32_t         g_uNt3MajorVer  = 3;
static uint32_t         g_uNt3MinorVer  = 51;
static uint32_t         g_uNt3BuildNo   = 1057;
static bool             g_fNt3Checked   = false;
static bool             g_fNt3Smp       = false;
static bool volatile    g_fNt3VersionInitialized = false;

static uint8_t         *g_pbNt3OsKrnl   = (uint8_t *)UINT32_C(0x80100000);
static uint32_t         g_cbNt3OsKrnl   = 0x300000;
static uint8_t         *g_pbNt3Hal      = (uint8_t *)UINT32_C(0x80400000);
static uint32_t         g_cbNt3Hal      = _512K;
static bool volatile    g_fNt3ModuleInfoInitialized = false;


/**
 * Converts a string to a number, stopping at the first non-digit.
 *
 * @returns The value
 * @param   ppwcValue       Pointer to the string pointer variable.  Updated.
 * @param   pcwcValue       Pointer to the string length variable.  Updated.
 */
static uint32_t rtR0Nt3StringToNum(PCRTUTF16 *ppwcValue, size_t *pcwcValue)
{
    uint32_t  uValue   = 0;
    PCRTUTF16 pwcValue = *ppwcValue;
    size_t    cwcValue = *pcwcValue;

    while (cwcValue > 0)
    {
        RTUTF16 uc = *pwcValue;
        unsigned uDigit = (unsigned)uc - (unsigned)'0';
        if (uDigit < (unsigned)10)
        {
            uValue *= 10;
            uValue += uDigit;
        }
        else
            break;
        pwcValue++;
        cwcValue--;
    }

    *ppwcValue = pwcValue;
    *pcwcValue = cwcValue;
    return uValue;
}


/**
 * Implements RTL_QUERY_REGISTRY_ROUTINE for processing
 * 'HKLM/Software/Microsoft/Window NT/CurrentVersion/CurrentVersion'
 */
static NTSTATUS NTAPI rtR0Nt3VerEnumCallback_CurrentVersion(PWSTR pwszValueName, ULONG uValueType,
                                                            PVOID pvValue, ULONG cbValue, PVOID pvUser, PVOID pvEntryCtx)
{
    RT_NOREF(pwszValueName, pvEntryCtx);
    if (   uValueType == REG_SZ
        || uValueType == REG_EXPAND_SZ)
    {
        PCRTUTF16 pwcValue = (PCRTUTF16)pvValue;
        size_t    cwcValue  = cbValue / sizeof(*pwcValue);
        uint32_t uMajor = rtR0Nt3StringToNum(&pwcValue, &cwcValue);
        uint32_t uMinor = 0;
        if (cwcValue > 1)
        {
            pwcValue++;
            cwcValue--;
            uMinor = rtR0Nt3StringToNum(&pwcValue, &cwcValue);
        }

        if (uMajor >= 3)
        {
            g_uNt3MajorVer = uMajor;
            g_uNt3MinorVer = uMinor;
            RTLogBackdoorPrintf("rtR0Nt3VerEnumCallback_CurrentVersion found: uMajor=%u uMinor=%u\n", uMajor, uMinor);
            *(uint32_t *)pvUser |= RT_BIT_32(0);
            return STATUS_SUCCESS;
        }

        RTLogBackdoorPrintf("rtR0Nt3VerEnumCallback_CurrentVersion: '%.*ls'\n", cbValue / sizeof(RTUTF16), pvValue);
    }
    else
        RTLogBackdoorPrintf("rtR0Nt3VerEnumCallback_CurrentVersion: uValueType=%u %.*Rhxs\n", uValueType, cbValue, pvValue);
    return STATUS_SUCCESS;
}


/**
 * Implements RTL_QUERY_REGISTRY_ROUTINE for processing
 * 'HKLM/Software/Microsoft/Window NT/CurrentVersion/CurrentBuildNumber'
 */
static NTSTATUS NTAPI rtR0Nt3VerEnumCallback_CurrentBuildNumber(PWSTR pwszValueName, ULONG uValueType,
                                                                PVOID pvValue, ULONG cbValue, PVOID pvUser, PVOID pvEntryCtx)
{
    RT_NOREF(pwszValueName, pvEntryCtx);
    if (   uValueType == REG_SZ
        || uValueType == REG_EXPAND_SZ)
    {
        PCRTUTF16 pwcValue = (PCRTUTF16)pvValue;
        size_t    cwcValue  = cbValue / sizeof(*pwcValue);
        uint32_t uBuildNo = rtR0Nt3StringToNum(&pwcValue, &cwcValue);

        if (uBuildNo >= 100 && uBuildNo < _1M)
        {
            g_uNt3BuildNo = uBuildNo;
            RTLogBackdoorPrintf("rtR0Nt3VerEnumCallback_CurrentBuildNumber found: uBuildNo=%u\n", uBuildNo);
            *(uint32_t *)pvUser |= RT_BIT_32(1);
            return STATUS_SUCCESS;
        }

        RTLogBackdoorPrintf("rtR0Nt3VerEnumCallback_CurrentBuildNumber: '%.*ls'\n", cbValue / sizeof(RTUTF16), pvValue);
    }
    else
        RTLogBackdoorPrintf("rtR0Nt3VerEnumCallback_CurrentBuildNumber: uValueType=%u %.*Rhxs\n", uValueType, cbValue, pvValue);
    return STATUS_SUCCESS;
}


/**
 * Implements RTL_QUERY_REGISTRY_ROUTINE for processing
 * 'HKLM/Software/Microsoft/Window NT/CurrentVersion/CurrentType'
 */
static NTSTATUS NTAPI rtR0Nt3VerEnumCallback_CurrentType(PWSTR pwszValueName, ULONG uValueType,
                                                         PVOID pvValue, ULONG cbValue, PVOID pvUser, PVOID pvEntryCtx)
{
    RT_NOREF(pwszValueName, pvEntryCtx);
    if (   uValueType == REG_SZ
        || uValueType == REG_EXPAND_SZ)
    {
        PCRTUTF16 pwcValue = (PCRTUTF16)pvValue;
        size_t    cwcValue = cbValue / sizeof(*pwcValue);

        int fSmp = -1;
        if (cwcValue >= 12 && RTUtf16NICmpAscii(pwcValue, "Uniprocessor", 12) == 0)
        {
            cwcValue -= 12;
            pwcValue += 12;
            fSmp = 0;
        }
        else if (cwcValue >= 14 && RTUtf16NICmpAscii(pwcValue, "Multiprocessor", 14) == 0)
        {
            cwcValue -= 14;
            pwcValue += 14;
            fSmp = 1;
        }
        if (fSmp != -1)
        {
            while (cwcValue > 0 && RT_C_IS_SPACE(*pwcValue))
                cwcValue--, pwcValue++;

            int fChecked = -1;
            if (cwcValue >= 4 && RTUtf16NICmpAscii(pwcValue, "Free", 4) == 0)
                fChecked = 0;
            else if (cwcValue >= 7 && RTUtf16NICmpAscii(pwcValue, "Checked", 7) == 0)
                fChecked = 1;
            if (fChecked != -1)
            {
                g_fNt3Smp     = fSmp     != 0;
                g_fNt3Checked = fChecked != 0;
                RTLogBackdoorPrintf("rtR0Nt3VerEnumCallback_CurrentType found: fSmp=%d fChecked=%d\n", fSmp, fChecked);
                *(uint32_t *)pvUser |= RT_BIT_32(2);
                return STATUS_SUCCESS;
            }
        }

        RTLogBackdoorPrintf("rtR0Nt3VerEnumCallback_CurrentType: '%.*ls'\n", cbValue / sizeof(RTUTF16), pvValue);
    }
    else
        RTLogBackdoorPrintf("rtR0Nt3VerEnumCallback_CurrentType: uValueType=%u %.*Rhxs\n", uValueType, cbValue, pvValue);
    return STATUS_SUCCESS;
}


/**
 * Figure out the NT 3 version from the registry.
 */
static void rtR0Nt3InitVersion(void)
{
    RTL_QUERY_REGISTRY_TABLE aQuery[4];
    RT_ZERO(aQuery);
    aQuery[0].QueryRoutine = rtR0Nt3VerEnumCallback_CurrentVersion;
    aQuery[0].Flags        = 0;
    aQuery[0].Name         = L"CurrentVersion";
    aQuery[0].EntryContext = NULL;
    aQuery[0].DefaultType  = REG_NONE;

    aQuery[1].QueryRoutine = rtR0Nt3VerEnumCallback_CurrentBuildNumber;
    aQuery[1].Flags        = 0;
    aQuery[1].Name         = L"CurrentBuildNumber";
    aQuery[1].EntryContext = NULL;
    aQuery[1].DefaultType  = REG_NONE;

    aQuery[2].QueryRoutine = rtR0Nt3VerEnumCallback_CurrentType;
    aQuery[2].Flags        = 0;
    aQuery[2].Name         = L"CurrentType";
    aQuery[2].EntryContext = NULL;
    aQuery[2].DefaultType  = REG_NONE;

    uint32_t fFound = 0;
    //NTSTATUS rcNt = RtlQueryRegistryValues(RTL_REGISTRY_WINDOWS_NT, NULL, &aQuery[0], &fFound, NULL /*Environment*/);
    NTSTATUS rcNt = RtlQueryRegistryValues(RTL_REGISTRY_ABSOLUTE,
                                           L"\\Registry\\Machine\\Software\\Microsoft\\Windows NT\\CurrentVersion",
                                           &aQuery[0], &fFound, NULL /*Environment*/);
    if (!NT_SUCCESS(rcNt))
        RTLogBackdoorPrintf("rtR0Nt3InitVersion: RtlQueryRegistryValues failed: %#x\n", rcNt);
    else if (fFound != 7)
        RTLogBackdoorPrintf("rtR0Nt3InitVersion: Didn't get all values: fFound=%#x\n", fFound);

    g_fNt3VersionInitialized = true;
}


extern "C" DECLEXPORT(BOOLEAN) __stdcall
PsGetVersion(ULONG *puMajor, ULONG *puMinor, ULONG *puBuildNo, UNICODE_STRING *pCsdStr)
{
    if (!g_fNt3VersionInitialized)
        rtR0Nt3InitVersion();
    if (puMajor)
        *puMajor = g_uNt3MajorVer;
    if (puMinor)
        *puMinor = g_uNt3MinorVer;
    if (puBuildNo)
        *puBuildNo = g_uNt3BuildNo;
    if (pCsdStr)
    {
        pCsdStr->Buffer[0] = '\0';
        pCsdStr->Length = 0;
    }
    return g_fNt3Checked;
}


/**
 * Worker for rtR0Nt3InitModuleInfo.
 */
static bool rtR0Nt3InitModuleInfoOne(const char *pszImage, uint8_t const *pbCode, uint8_t **ppbModule, uint32_t *pcbModule)
{
    uintptr_t const uImageAlign = _64K;

    /* Align pbCode. */
    pbCode = (uint8_t const *)((uintptr_t)pbCode & ~(uintptr_t)(uImageAlign - 1));

    /* Scan backwards till we find a PE signature. */
    for (uint32_t cbChecked = 0; cbChecked < _64M; cbChecked += uImageAlign, pbCode -= uImageAlign)
    {
        uint32_t uZero     = 0;
        uint32_t offNewHdr = 0;
        __try
        {
            uZero     = *(uint32_t const *)pbCode;
            offNewHdr = *(uint32_t const *)&pbCode[RT_OFFSETOF(IMAGE_DOS_HEADER, e_lfanew)];
        }
        __except(EXCEPTION_EXECUTE_HANDLER)
        {
            RTLogBackdoorPrintf("rtR0Nt3InitModuleInfo: Exception at %p scanning for DOS header...\n", pbCode);
            continue;
        }
        if (   (uint16_t)uZero == IMAGE_DOS_SIGNATURE
            && offNewHdr < _2K
            && offNewHdr >= sizeof(IMAGE_DOS_HEADER))
        {
            RT_CONCAT(IMAGE_NT_HEADERS,ARCH_BITS) NtHdrs;
            __try
            {
                NtHdrs = *(decltype(NtHdrs) const *)&pbCode[offNewHdr];
            }
            __except(EXCEPTION_EXECUTE_HANDLER)
            {
                RTLogBackdoorPrintf("rtR0Nt3InitModuleInfo: Exception at %p reading NT headers...\n", pbCode);
                continue;
            }
            if (   NtHdrs.Signature == IMAGE_NT_SIGNATURE
                && NtHdrs.FileHeader.SizeOfOptionalHeader == sizeof(NtHdrs.OptionalHeader)
                && NtHdrs.FileHeader.NumberOfSections > 2
                && NtHdrs.FileHeader.NumberOfSections < _4K
                && NtHdrs.OptionalHeader.Magic == RT_CONCAT3(IMAGE_NT_OPTIONAL_HDR,ARCH_BITS,_MAGIC))
            {
                *ppbModule = (uint8_t *)pbCode;
                *pcbModule = NtHdrs.OptionalHeader.SizeOfImage;
                RTLogBackdoorPrintf("rtR0Nt3InitModuleInfo: Found %s at %#p LB %#x\n",
                                    pszImage, pbCode, NtHdrs.OptionalHeader.SizeOfImage);
                return true;
            }
        }
    }
    RTLogBackdoorPrintf("rtR0Nt3InitModuleInfo: Warning! Unable to locate %s...\n");
    return false;
}


/**
 * Initializes the module information (NTOSKRNL + HAL) using exported symbols.
 * This only works as long as noone is intercepting the symbols.
 */
static void rtR0Nt3InitModuleInfo(void)
{
    rtR0Nt3InitModuleInfoOne("ntoskrnl.exe", (uint8_t const *)(uintptr_t)IoGetCurrentProcess, &g_pbNt3OsKrnl, &g_cbNt3OsKrnl);
    rtR0Nt3InitModuleInfoOne("hal.dll",      (uint8_t const *)(uintptr_t)HalGetBusData,       &g_pbNt3Hal,    &g_cbNt3Hal);
    g_fNt3ModuleInfoInitialized = true;
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

            if (!g_fNt3ModuleInfoInitialized)
                rtR0Nt3InitModuleInfo();

            pInfo->NumberOfModules = 2;

            /* ntoskrnl.exe */
            pInfo->Modules[0].Section           = NULL;
            pInfo->Modules[0].MappedBase        = g_pbNt3OsKrnl;
            pInfo->Modules[0].ImageBase         = g_pbNt3OsKrnl;
            pInfo->Modules[0].ImageSize         = g_cbNt3OsKrnl;
            pInfo->Modules[0].Flags             = 0;
            pInfo->Modules[0].LoadOrderIndex    = 0;
            pInfo->Modules[0].InitOrderIndex    = 0;
            pInfo->Modules[0].LoadCount         = 1024;
            pInfo->Modules[0].OffsetToFileName  = sizeof("\\SystemRoot\\System32\\") - 1;
            memcpy(pInfo->Modules[0].FullPathName, RT_STR_TUPLE("\\SystemRoot\\System32\\ntoskrnl.exe"));

            /* hal.dll */
            pInfo->Modules[1].Section           = NULL;
            pInfo->Modules[1].MappedBase        = g_pbNt3Hal;
            pInfo->Modules[1].ImageBase         = g_pbNt3Hal;
            pInfo->Modules[1].ImageSize         = g_cbNt3Hal;
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
    if (!g_fNt3VersionInitialized)
        rtR0Nt3InitVersion();

    uint8_t const *pbProcess = (uint8_t const *)IoGetCurrentProcess();
    if (   g_uNt3MajorVer > 3
        || g_uNt3MinorVer >= 50)
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

