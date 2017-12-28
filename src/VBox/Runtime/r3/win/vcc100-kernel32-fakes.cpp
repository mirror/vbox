/* $Id$ */
/** @file
 * IPRT - Tricks to make the Visual C++ 2010 CRT work on NT4, W2K and XP.
 */

/*
 * Copyright (C) 2012-2017 Oracle Corporation
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
#include <iprt/cdefs.h>
#include <iprt/types.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#ifdef DEBUG
# include <stdio.h> /* _snprintf */
#endif

#ifndef RT_ARCH_X86
# error "This code is X86 only"
#endif

#include <iprt/nt/nt-and-windows.h>

#include "vcc100-fakes.h"


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
#ifndef HEAP_STANDARD
# define HEAP_STANDARD 0
#endif


/** Declare a kernel32 API.
 * @note We are not exporting them as that causes duplicate symbol troubles in
 *       the OpenGL bits. */
#define DECL_KERNEL32(a_Type) extern "C" a_Type WINAPI



/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
static volatile bool g_fInitialized = false;
#define MAKE_IMPORT_ENTRY(a_uMajorVer, a_uMinorVer, a_Name, a_cb) DECLARE_FUNCTION_POINTER(a_Name, a_cb)
#include "vcc100-kernel32-fakes.h"


/**
 * Resolves all the APIs ones and for all, updating the fake IAT entries.
 */
static void InitFakes(void)
{
    CURRENT_VERSION_VARIABLE();

    HMODULE hmod = GetModuleHandleW(L"kernel32");
    MY_ASSERT(hmod != NULL, "kernel32");

#undef MAKE_IMPORT_ENTRY
#define MAKE_IMPORT_ENTRY(a_uMajorVer, a_uMinorVer, a_Name, a_cb) RESOLVE_IMPORT(a_uMajorVer, a_uMinorVer, a_Name, a_cb)
#include "vcc100-kernel32-fakes.h"

    g_fInitialized = true;
}


DECL_KERNEL32(PVOID) Fake_DecodePointer(PVOID pvEncoded)
{
    INIT_FAKES(DecodePointer,(pvEncoded));

    /*
     * Fallback code.
     */
    return pvEncoded;
}


DECL_KERNEL32(PVOID) Fake_EncodePointer(PVOID pvNative)
{
    INIT_FAKES(EncodePointer, (pvNative));

    /*
     * Fallback code.
     */
    return pvNative;
}


DECL_KERNEL32(BOOL) Fake_InitializeCriticalSectionAndSpinCount(LPCRITICAL_SECTION pCritSect, DWORD cSpin)
{
    INIT_FAKES(InitializeCriticalSectionAndSpinCount, (pCritSect, cSpin));

    /*
     * Fallback code.
     */
    InitializeCriticalSection(pCritSect);
    return TRUE;
}


DECL_KERNEL32(BOOL) Fake_HeapSetInformation(HANDLE hHeap, HEAP_INFORMATION_CLASS enmInfoClass, PVOID pvBuf, SIZE_T cbBuf)
{
    INIT_FAKES(HeapSetInformation, (hHeap, enmInfoClass, pvBuf, cbBuf));

    /*
     * Fallback code.
     */
    if (enmInfoClass == HeapCompatibilityInformation)
    {
        if (   cbBuf != sizeof(ULONG)
            || !pvBuf
            || *(PULONG)pvBuf == HEAP_STANDARD
           )
        {
            SetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;
        }
        return TRUE;
    }

    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
}


DECL_KERNEL32(BOOL) Fake_HeapQueryInformation(HANDLE hHeap, HEAP_INFORMATION_CLASS enmInfoClass,
                                              PVOID pvBuf, SIZE_T cbBuf, PSIZE_T pcbRet)
{
    INIT_FAKES(HeapQueryInformation, (hHeap, enmInfoClass, pvBuf, cbBuf, pcbRet));

    /*
     * Fallback code.
     */
    if (enmInfoClass == HeapCompatibilityInformation)
    {
        *pcbRet = sizeof(ULONG);
        if (cbBuf < sizeof(ULONG) || !pvBuf)
        {
            SetLastError(ERROR_INSUFFICIENT_BUFFER);
            return FALSE;
        }
        *(PULONG)pvBuf = HEAP_STANDARD;
        return TRUE;
    }

    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
}


/* These are used by INTEL\mt_obj\Timer.obj: */

DECL_KERNEL32(HANDLE) Fake_CreateTimerQueue(void)
{
    INIT_FAKES(CreateTimerQueue, ());

    SetLastError(ERROR_NOT_SUPPORTED);
    return NULL;
}

DECL_KERNEL32(BOOL) Fake_CreateTimerQueueTimer(PHANDLE phTimer, HANDLE hTimerQueue, WAITORTIMERCALLBACK pfnCallback, PVOID pvUser,
                                               DWORD msDueTime, DWORD msPeriod, ULONG fFlags)
{
    INIT_FAKES(CreateTimerQueueTimer, (phTimer, hTimerQueue, pfnCallback, pvUser, msDueTime, msPeriod, fFlags));

    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
}

DECL_KERNEL32(BOOL) Fake_DeleteTimerQueueTimer(HANDLE hTimerQueue, HANDLE hTimer, HANDLE hEvtCompletion)
{
    INIT_FAKES(DeleteTimerQueueTimer, (hTimerQueue, hTimer, hEvtCompletion));

    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
}

/* This is used by several APIs. */

DECL_KERNEL32(VOID) Fake_InitializeSListHead(PSLIST_HEADER pHead)
{
    INIT_FAKES_VOID(InitializeSListHead, (pHead));

    /* fallback: */
    pHead->Alignment = 0;
}


DECL_KERNEL32(PSLIST_ENTRY) Fake_InterlockedFlushSList(PSLIST_HEADER pHead)
{
    INIT_FAKES(InterlockedFlushSList, (pHead));

    /* fallback: */
    PSLIST_ENTRY pRet = NULL;
    if (pHead->Next.Next)
    {
        for (;;)
        {
            SLIST_HEADER OldHead = *pHead;
            SLIST_HEADER NewHead;
            NewHead.Alignment = 0;
            NewHead.Sequence  = OldHead.Sequence + 1;
            if (ASMAtomicCmpXchgU64(&pHead->Alignment, NewHead.Alignment, OldHead.Alignment))
            {
                pRet = OldHead.Next.Next;
                break;
            }
        }
    }
    return pRet;
}

DECL_KERNEL32(PSLIST_ENTRY) Fake_InterlockedPopEntrySList(PSLIST_HEADER pHead)
{
    INIT_FAKES(InterlockedPopEntrySList, (pHead));

    /* fallback: */
    PSLIST_ENTRY pRet = NULL;
    for (;;)
    {
        SLIST_HEADER OldHead = *pHead;
        pRet = OldHead.Next.Next;
        if (pRet)
        {
            SLIST_HEADER NewHead;
            __try
            {
                NewHead.Next.Next = pRet->Next;
            }
            __except(EXCEPTION_EXECUTE_HANDLER)
            {
                continue;
            }
            NewHead.Depth     = OldHead.Depth - 1;
            NewHead.Sequence  = OldHead.Sequence + 1;
            if (ASMAtomicCmpXchgU64(&pHead->Alignment, NewHead.Alignment, OldHead.Alignment))
                break;
        }
        else
            break;
    }
    return pRet;
}

DECL_KERNEL32(PSLIST_ENTRY) Fake_InterlockedPushEntrySList(PSLIST_HEADER pHead, PSLIST_ENTRY pEntry)
{
    INIT_FAKES(InterlockedPushEntrySList, (pHead, pEntry));

    /* fallback: */
    PSLIST_ENTRY pRet = NULL;
    for (;;)
    {
        SLIST_HEADER OldHead = *pHead;
        pRet = OldHead.Next.Next;
        pEntry->Next = pRet;
        SLIST_HEADER NewHead;
        NewHead.Next.Next = pEntry;
        NewHead.Depth     = OldHead.Depth + 1;
        NewHead.Sequence  = OldHead.Sequence + 1;
        if (ASMAtomicCmpXchgU64(&pHead->Alignment, NewHead.Alignment, OldHead.Alignment))
            break;
    }
    return pRet;
}

DECL_KERNEL32(WORD) Fake_QueryDepthSList(PSLIST_HEADER pHead)
{
    INIT_FAKES(QueryDepthSList, (pHead));

    /* fallback: */
    return pHead->Depth;
}


/* curl drags these in: */
DECL_KERNEL32(BOOL) Fake_VerifyVersionInfoA(LPOSVERSIONINFOEXA pInfo, DWORD fTypeMask, DWORDLONG fConditionMask)
{
    INIT_FAKES(VerifyVersionInfoA, (pInfo, fTypeMask, fConditionMask));

    /* fallback to make curl happy: */
    OSVERSIONINFOEXA VerInfo;
    RT_ZERO(VerInfo);
    VerInfo.dwOSVersionInfoSize = sizeof(VerInfo);
    if (!GetVersionExA((OSVERSIONINFO *)&VerInfo))
    {
        RT_ZERO(VerInfo);
        VerInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
        BOOL fRet = GetVersionExA((OSVERSIONINFO *)&VerInfo);
        if (fRet)
        { /* likely */ }
        else
        {
            MY_ASSERT(false, "VerifyVersionInfoA: #1");
            return FALSE;
        }
    }

    BOOL fRet = TRUE;
    for (unsigned i = 0; i < 8 && fRet; i++)
        if (fTypeMask & RT_BIT_32(i))
        {
            uint32_t uLeft, uRight;
            switch (RT_BIT_32(i))
            {
#define MY_CASE(a_Num, a_Member) case a_Num: uLeft = VerInfo.a_Member; uRight = pInfo->a_Member; break
                MY_CASE(VER_MINORVERSION,       dwMinorVersion);
                MY_CASE(VER_MAJORVERSION,       dwMajorVersion);
                MY_CASE(VER_BUILDNUMBER,        dwBuildNumber);
                MY_CASE(VER_PLATFORMID,         dwPlatformId);
                MY_CASE(VER_SERVICEPACKMINOR,   wServicePackMinor);
                MY_CASE(VER_SERVICEPACKMAJOR,   wServicePackMinor);
                MY_CASE(VER_SUITENAME,          wSuiteMask);
                MY_CASE(VER_PRODUCT_TYPE,       wProductType);
#undef  MY_CASE
                default: uLeft = uRight = 0; MY_ASSERT(false, "VerifyVersionInfoA: #2");
            }
            switch ((uint8_t)(fConditionMask >> (i*8)))
            {
                case VER_EQUAL:             fRet = uLeft == uRight; break;
                case VER_GREATER:           fRet = uLeft >  uRight; break;
                case VER_GREATER_EQUAL:     fRet = uLeft >= uRight; break;
                case VER_LESS:              fRet = uLeft <  uRight; break;
                case VER_LESS_EQUAL:        fRet = uLeft <= uRight; break;
                case VER_AND:               fRet = (uLeft & uRight) == uRight; break;
                case VER_OR:                fRet = (uLeft & uRight) != 0; break;
                default:                    fRet = FALSE; MY_ASSERT(false, "VerifyVersionInfoA: #3"); break;
            }
        }

    return fRet;
}


DECL_KERNEL32(ULONGLONG) Fake_VerSetConditionMask(ULONGLONG fConditionMask, DWORD fTypeMask, BYTE bOperator)
{
    INIT_FAKES(VerSetConditionMask, (fConditionMask, fTypeMask, bOperator));

    /* fallback: */
    for (unsigned i = 0; i < 8; i++)
        if (fTypeMask & RT_BIT_32(i))
        {
            uint64_t fMask  = UINT64_C(0xff) << (i*8);
            fConditionMask &= ~fMask;
            fConditionMask |= (uint64_t)bOperator << (i*8);

        }
    return fConditionMask;
}


/*
 * NT 3.51 stuff.
 */

DECL_KERNEL32(BOOL) Fake_IsProcessorFeaturePresent(DWORD enmProcessorFeature)
{
    INIT_FAKES(IsProcessorFeaturePresent, (enmProcessorFeature));

    /* Could make more of an effort here... */
    return FALSE;
}


DECL_KERNEL32(BOOL) Fake_CancelIo(HANDLE hHandle)
{
    INIT_FAKES(CancelIo, (hHandle));

    /* NT 3.51 have the NTDLL API this corresponds to. */
    RESOLVE_NTDLL_API(NtCancelIoFile);
    if (pfnNtCancelIoFile)
    {
        IO_STATUS_BLOCK Ios = RTNT_IO_STATUS_BLOCK_INITIALIZER;
        NTSTATUS rcNt = pfnNtCancelIoFile(hHandle, &Ios);
        if (RT_SUCCESS(rcNt))
            return TRUE;
        if (rcNt == STATUS_INVALID_HANDLE)
            SetLastError(ERROR_INVALID_HANDLE);
        else
            SetLastError(ERROR_INVALID_FUNCTION);
    }
    else
        SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
}


/*
 * NT 3.50 stuff.
 */

DECL_KERNEL32(BOOL) Fake_IsDebuggerPresent(VOID)
{
    INIT_FAKES(IsDebuggerPresent, ());

    /* Fallback: */
    return FALSE;
}


DECL_KERNEL32(VOID) Fake_GetSystemTimeAsFileTime(LPFILETIME pTime)
{
    INIT_FAKES_VOID(GetSystemTimeAsFileTime, (pTime));

    DWORD dwVersion = GetVersion();
    if (   (dwVersion & 0xff) > 3
        || (   (dwVersion & 0xff) == 3
            && ((dwVersion >> 8) & 0xff) >= 50) )
    {
        PKUSER_SHARED_DATA pUsd = (PKUSER_SHARED_DATA)MM_SHARED_USER_DATA_VA;

        /* use interrupt time */
        LARGE_INTEGER Time;
        do
        {
            Time.HighPart = pUsd->SystemTime.High1Time;
            Time.LowPart  = pUsd->SystemTime.LowPart;
        } while (pUsd->SystemTime.High2Time != Time.HighPart);

        pTime->dwHighDateTime = Time.HighPart;
        pTime->dwLowDateTime  = Time.LowPart;
    }
    else
    {
        /* NT 3.1 didn't have a KUSER_SHARED_DATA nor a GetSystemTimeAsFileTime export. */
        SYSTEMTIME SystemTime;
        GetSystemTime(&SystemTime);
        BOOL fRet = SystemTimeToFileTime(&SystemTime, pTime);
        if (fRet)
        { /* likely */ }
        else
        {
            MY_ASSERT(false, "GetSystemTimeAsFileTime: #2");
            pTime->dwHighDateTime = 0;
            pTime->dwLowDateTime  = 0;
        }
    }
}


/*
 * NT 3.1 stuff.
 */

DECL_KERNEL32(BOOL) Fake_GetVersionExA(LPOSVERSIONINFOA pInfo)
{
    INIT_FAKES(GetVersionExA, (pInfo));

    /*
     * Fallback.
     */
    DWORD dwVersion = GetVersion();

    /* Common fields: */
    pInfo->dwMajorVersion = dwVersion & 0xff;
    pInfo->dwMinorVersion = (dwVersion >> 8) & 0xff;
    if (!(dwVersion & RT_BIT_32(31)))
        pInfo->dwBuildNumber = dwVersion >> 16;
    else
        pInfo->dwBuildNumber = 511;
    pInfo->dwPlatformId = VER_PLATFORM_WIN32_NT;
/** @todo get CSD from registry. */
    pInfo->szCSDVersion[0] = '\0';

    /* OSVERSIONINFOEX fields: */
    if (pInfo->dwOSVersionInfoSize > sizeof((*pInfo)))
    {
        LPOSVERSIONINFOEXA pInfoEx = (LPOSVERSIONINFOEXA)pInfo;
        if (pInfoEx->dwOSVersionInfoSize > RT_UOFFSETOF(OSVERSIONINFOEXA, wServicePackMinor))
        {
            pInfoEx->wServicePackMajor = 0;
            pInfoEx->wServicePackMinor = 0;
        }
        if (pInfoEx->dwOSVersionInfoSize > RT_UOFFSETOF(OSVERSIONINFOEXA, wSuiteMask))
            pInfoEx->wSuiteMask = 0;
        if (pInfoEx->dwOSVersionInfoSize > RT_UOFFSETOF(OSVERSIONINFOEXA, wProductType))
            pInfoEx->wProductType = VER_NT_WORKSTATION;
        if (pInfoEx->wReserved > RT_UOFFSETOF(OSVERSIONINFOEXA, wProductType))
            pInfoEx->wReserved = 0;
    }

    return TRUE;
}


DECL_KERNEL32(BOOL) Fake_GetVersionExW(LPOSVERSIONINFOW pInfo)
{
    INIT_FAKES(GetVersionExW, (pInfo));

    /*
     * Fallback.
     */
    DWORD dwVersion = GetVersion();

    /* Common fields: */
    pInfo->dwMajorVersion = dwVersion & 0xff;
    pInfo->dwMinorVersion = (dwVersion >> 8) & 0xff;
    if (!(dwVersion & RT_BIT_32(31)))
        pInfo->dwBuildNumber = dwVersion >> 16;
    else
        pInfo->dwBuildNumber = 511;
    pInfo->dwPlatformId = VER_PLATFORM_WIN32_NT;
/** @todo get CSD from registry. */
    pInfo->szCSDVersion[0] = '\0';

    /* OSVERSIONINFOEX fields: */
    if (pInfo->dwOSVersionInfoSize > sizeof((*pInfo)))
    {
        LPOSVERSIONINFOEXW pInfoEx = (LPOSVERSIONINFOEXW)pInfo;
        if (pInfoEx->dwOSVersionInfoSize > RT_UOFFSETOF(OSVERSIONINFOEXW, wServicePackMinor))
        {
            pInfoEx->wServicePackMajor = 0;
            pInfoEx->wServicePackMinor = 0;
        }
        if (pInfoEx->dwOSVersionInfoSize > RT_UOFFSETOF(OSVERSIONINFOEXW, wSuiteMask))
            pInfoEx->wSuiteMask = 0;
        if (pInfoEx->dwOSVersionInfoSize > RT_UOFFSETOF(OSVERSIONINFOEXW, wProductType))
            pInfoEx->wProductType = VER_NT_WORKSTATION;
        if (pInfoEx->wReserved > RT_UOFFSETOF(OSVERSIONINFOEXW, wProductType))
            pInfoEx->wReserved = 0;
    }

    return TRUE;
}


DECL_KERNEL32(LPWCH) Fake_GetEnvironmentStringsW(void)
{
    INIT_FAKES(GetEnvironmentStringsW, ());

    /*
     * Fallback:
     *
     * Environment is ANSI in NT 3.1. We should only be here for NT 3.1.
     * For now, don't try do a perfect job converting it, just do it.
     */
    char    *pszzEnv = (char *)RTNtCurrentPeb()->ProcessParameters->Environment;
    size_t   offEnv  = 0;
    while (pszzEnv[offEnv] != '\0')
    {
        size_t cchLen = strlen(&pszzEnv[offEnv]);
        offEnv += cchLen + 1;
    }
    size_t const cchEnv = offEnv + 1;

    PRTUTF16 pwszzEnv = (PRTUTF16)HeapAlloc(GetProcessHeap(), 0, cchEnv * sizeof(RTUTF16));
    if (!pwszzEnv)
        return NULL;
    for (offEnv = 0; offEnv < cchEnv; offEnv++)
    {
        unsigned char ch = pwszzEnv[offEnv];
        if (!(ch & 0x80))
            pwszzEnv[offEnv] = ch;
        else
            pwszzEnv[offEnv] = '_';
    }
    return pwszzEnv;
}


DECL_KERNEL32(BOOL) Fake_FreeEnvironmentStringsW(LPWCH pwszzEnv)
{
    INIT_FAKES(FreeEnvironmentStringsW, (pwszzEnv));

    /* Fallback: */
    if (pwszzEnv)
        HeapFree(GetProcessHeap(), 0, pwszzEnv);
    return TRUE;
}


DECL_KERNEL32(int) Fake_GetLocaleInfoA(LCID idLocale, LCTYPE enmType, LPSTR pData, int cchData)
{
    INIT_FAKES(GetLocaleInfoA, (idLocale, enmType, pData, cchData));

    /* Fallback: */
    MY_ASSERT(false, "GetLocaleInfoA: idLocale=%#x enmType=%#x cchData=%#x", idLocale, enmType, cchData);
    SetLastError(ERROR_NOT_SUPPORTED);
    return 0;
}


DECL_KERNEL32(BOOL) Fake_EnumSystemLocalesA(LOCALE_ENUMPROCA pfnCallback, DWORD fFlags)
{
    INIT_FAKES(EnumSystemLocalesA, (pfnCallback, fFlags));

    /* Fallback: */
    MY_ASSERT(false, "EnumSystemLocalesA: pfnCallback=%p fFlags=%#x", pfnCallback, fFlags);
    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
}


DECL_KERNEL32(BOOL) Fake_IsValidLocale(LCID idLocale, DWORD fFlags)
{
    INIT_FAKES(IsValidLocale, (idLocale, fFlags));

    /* Fallback: */
    MY_ASSERT(false, "IsValidLocale: idLocale fFlags=%#x", idLocale, fFlags);
    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
}


DECL_KERNEL32(DWORD_PTR) Fake_SetThreadAffinityMask(HANDLE hThread, DWORD_PTR fAffinityMask)
{
    INIT_FAKES(SetThreadAffinityMask, (hThread, fAffinityMask));

    /* Fallback: */
    SYSTEM_INFO SysInfo;
    GetSystemInfo(&SysInfo);
    MY_ASSERT(false, "SetThreadAffinityMask: hThread=%p fAffinityMask=%p SysInfo.dwActiveProcessorMask=%p",
              hThread, fAffinityMask, SysInfo.dwActiveProcessorMask);
    if (   SysInfo.dwActiveProcessorMask == fAffinityMask
        || fAffinityMask                 == ~(DWORD_PTR)0)
        return fAffinityMask;

    SetLastError(ERROR_NOT_SUPPORTED);
    return 0;
}


DECL_KERNEL32(BOOL) Fake_GetProcessAffinityMask(HANDLE hProcess, PDWORD_PTR pfProcessAffinityMask, PDWORD_PTR pfSystemAffinityMask)
{
    INIT_FAKES(GetProcessAffinityMask, (hProcess, pfProcessAffinityMask, pfSystemAffinityMask));

    /* Fallback: */
    SYSTEM_INFO SysInfo;
    GetSystemInfo(&SysInfo);
    MY_ASSERT(false, "GetProcessAffinityMask: SysInfo.dwActiveProcessorMask=%p", SysInfo.dwActiveProcessorMask);
    if (pfProcessAffinityMask)
        *pfProcessAffinityMask = SysInfo.dwActiveProcessorMask;
    if (pfSystemAffinityMask)
        *pfSystemAffinityMask  = SysInfo.dwActiveProcessorMask;
    return TRUE;
}


DECL_KERNEL32(BOOL) Fake_GetHandleInformation(HANDLE hObject, DWORD *pfFlags)
{
    INIT_FAKES(GetHandleInformation, (hObject, pfFlags));

    /* Fallback: */
    OBJECT_HANDLE_FLAG_INFORMATION  Info  = { 0, 0 };
    DWORD                           cbRet = sizeof(Info);
    NTSTATUS rcNt = NtQueryObject(hObject, ObjectHandleFlagInformation, &Info, sizeof(Info), &cbRet);
    if (NT_SUCCESS(rcNt))
    {
        *pfFlags = (Info.Inherit          ? HANDLE_FLAG_INHERIT : 0)
                 | (Info.ProtectFromClose ? HANDLE_FLAG_PROTECT_FROM_CLOSE : 0);
        return TRUE;
    }
    *pfFlags = 0;
    MY_ASSERT(rcNt == STATUS_INVALID_HANDLE, "rcNt=%#x", rcNt);
    SetLastError(rcNt == STATUS_INVALID_HANDLE ? ERROR_INVALID_HANDLE : ERROR_INVALID_FUNCTION); /* see also process-win.cpp */
    return FALSE;
}


DECL_KERNEL32(BOOL) Fake_SetHandleInformation(HANDLE hObject, DWORD fMask, DWORD fFlags)
{
    INIT_FAKES(SetHandleInformation, (hObject, fMask, fFlags));

    /* Fallback: */
    SetLastError(ERROR_INVALID_FUNCTION);
    return FALSE;
}


/* Dummy to force dragging in this object in the link, so the linker
   won't accidentally use the symbols from kernel32. */
extern "C" int vcc100_kernel32_fakes_cpp(void)
{
    return 42;
}

