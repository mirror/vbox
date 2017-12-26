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
#define RT_NO_STRICT /* Minimal deps so that it works on NT 3.51 too. */
#include <iprt/cdefs.h>
#include <iprt/types.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/string.h>

#ifndef RT_ARCH_X86
# error "This code is X86 only"
#endif

#define DecodePointer                           Ignore_DecodePointer
#define EncodePointer                           Ignore_EncodePointer
#define InitializeCriticalSectionAndSpinCount   Ignore_InitializeCriticalSectionAndSpinCount
#define HeapSetInformation                      Ignore_HeapSetInformation
#define HeapQueryInformation                    Ignore_HeapQueryInformation
#define CreateTimerQueue                        Ignore_CreateTimerQueue
#define CreateTimerQueueTimer                   Ignore_CreateTimerQueueTimer
#define DeleteTimerQueueTimer                   Ignore_DeleteTimerQueueTimer
#define InitializeSListHead                     Ignore_InitializeSListHead
#define InterlockedFlushSList                   Ignore_InterlockedFlushSList
#define InterlockedPopEntrySList                Ignore_InterlockedPopEntrySList
#define InterlockedPushEntrySList               Ignore_InterlockedPushEntrySList
#define QueryDepthSList                         Ignore_QueryDepthSList
#define VerifyVersionInfoA                      Ignore_VerifyVersionInfoA
#define VerSetConditionMask                     Ignore_VerSetConditionMask
#define IsProcessorFeaturePresent               Ignore_IsProcessorFeaturePresent    /* NT 3.51 start */
#define CancelIo                                Ignore_CancelIo
#define IsDebuggerPresent                       Ignore_IsDebuggerPresent            /* NT 3.50 start */
#define GetSystemTimeAsFileTime                 Ignore_GetSystemTimeAsFileTime
#define GetVersionExA                           Ignore_GetVersionExA                /* NT 3.1 start */
#define GetVersionExW                           Ignore_GetVersionExW
#define GetEnvironmentStringsW                  Ignore_GetEnvironmentStringsW
#define FreeEnvironmentStringsW                 Ignore_FreeEnvironmentStringsW
#define GetLocaleInfoA                          Ignore_GetLocaleInfoA
#define EnumSystemLocalesA                      Ignore_EnumSystemLocalesA
#define IsValidLocale                           Ignore_IsValidLocale
#define SetThreadAffinityMask                   Ignore_SetThreadAffinityMask
#define GetProcessAffinityMask                  Ignore_GetProcessAffinityMask
#define GetHandleInformation                    Ignore_GetHandleInformation
#define SetHandleInformation                    Ignore_SetHandleInformation

#include <iprt/nt/nt-and-windows.h>

#undef DecodePointer
#undef EncodePointer
#undef InitializeCriticalSectionAndSpinCount
#undef HeapSetInformation
#undef HeapQueryInformation
#undef CreateTimerQueue
#undef CreateTimerQueueTimer
#undef DeleteTimerQueueTimer
#undef InitializeSListHead
#undef InterlockedFlushSList
#undef InterlockedPopEntrySList
#undef InterlockedPushEntrySList
#undef QueryDepthSList
#undef VerifyVersionInfoA
#undef VerSetConditionMask
#undef IsProcessorFeaturePresent
#undef CancelIo
#undef IsDebuggerPresent
#undef GetSystemTimeAsFileTime
#undef GetVersionExA
#undef GetVersionExW
#undef GetEnvironmentStringsW
#undef FreeEnvironmentStringsW
#undef GetLocaleInfoA
#undef EnumSystemLocalesA
#undef IsValidLocale
#undef SetThreadAffinityMask
#undef GetProcessAffinityMask
#undef GetHandleInformation
#undef SetHandleInformation


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
#ifndef HEAP_STANDARD
# define HEAP_STANDARD 0
#endif


/** Dynamically resolves an kernel32 API.   */
#define RESOLVE_ME(ApiNm) \
    static bool volatile    s_fInitialized = false; \
    static decltype(ApiNm) *s_pfnApi = NULL; \
    decltype(ApiNm)        *pfnApi; \
    if (!s_fInitialized) \
        pfnApi = s_pfnApi; \
    else \
    { \
        pfnApi = (decltype(pfnApi))GetProcAddress(GetModuleHandleW(L"kernel32"), #ApiNm); \
        s_pfnApi = pfnApi; \
        s_fInitialized = true; \
    } do {} while (0)


/** Dynamically resolves an NTDLL API we need.   */
#define RESOLVE_NTDLL_API(ApiNm) \
    static bool volatile    s_fInitialized##ApiNm = false; \
    static decltype(ApiNm) *s_pfn##ApiNm = NULL; \
    decltype(ApiNm)        *pfn##ApiNm; \
    if (!s_fInitialized##ApiNm) \
        pfn##ApiNm = s_pfn##ApiNm; \
    else \
    { \
        pfn##ApiNm = (decltype(pfn##ApiNm))GetProcAddress(GetModuleHandleW(L"ntdll"), #ApiNm); \
        s_pfn##ApiNm = pfn##ApiNm; \
        s_fInitialized##ApiNm = true; \
    } do {} while (0)


/** Declare a kernel32 API.
 * @note We are not exporting them as that causes duplicate symbol troubles in
 *       the OpenGL bits. */
#define DECL_KERNEL32(a_Type) extern "C" a_Type WINAPI


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
DECL_KERNEL32(BOOL) WINAPI GetVersionExA(LPOSVERSIONINFOA pInfo);


DECL_KERNEL32(PVOID) DecodePointer(PVOID pvEncoded)
{
    RESOLVE_ME(DecodePointer);
    if (pfnApi)
        return pfnApi(pvEncoded);

    /*
     * Fallback code.
     */
    return pvEncoded;
}


DECL_KERNEL32(PVOID) EncodePointer(PVOID pvNative)
{
    RESOLVE_ME(EncodePointer);
    if (pfnApi)
        return pfnApi(pvNative);

    /*
     * Fallback code.
     */
    return pvNative;
}


DECL_KERNEL32(BOOL) InitializeCriticalSectionAndSpinCount(LPCRITICAL_SECTION pCritSect, DWORD cSpin)
{
    RESOLVE_ME(InitializeCriticalSectionAndSpinCount);
    if (pfnApi)
        return pfnApi(pCritSect, cSpin);

    /*
     * Fallback code.
     */
    InitializeCriticalSection(pCritSect);
    return TRUE;
}


DECL_KERNEL32(BOOL) HeapSetInformation(HANDLE hHeap, HEAP_INFORMATION_CLASS enmInfoClass, PVOID pvBuf, SIZE_T cbBuf)
{
    RESOLVE_ME(HeapSetInformation);
    if (pfnApi)
        return pfnApi(hHeap, enmInfoClass, pvBuf, cbBuf);

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


DECL_KERNEL32(BOOL) HeapQueryInformation(HANDLE hHeap, HEAP_INFORMATION_CLASS enmInfoClass,
                                         PVOID pvBuf, SIZE_T cbBuf, PSIZE_T pcbRet)
{
    RESOLVE_ME(HeapQueryInformation);
    if (pfnApi)
        return pfnApi(hHeap, enmInfoClass, pvBuf, cbBuf, pcbRet);

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

DECL_KERNEL32(HANDLE) CreateTimerQueue(void)
{
    RESOLVE_ME(CreateTimerQueue);
    if (pfnApi)
        return pfnApi();
    SetLastError(ERROR_NOT_SUPPORTED);
    return NULL;
}

DECL_KERNEL32(BOOL) CreateTimerQueueTimer(PHANDLE phTimer, HANDLE hTimerQueue, WAITORTIMERCALLBACK pfnCallback, PVOID pvUser,
                                          DWORD msDueTime, DWORD msPeriod, ULONG fFlags)
{
    RESOLVE_ME(CreateTimerQueueTimer);
    if (pfnApi)
        return pfnApi(phTimer, hTimerQueue, pfnCallback, pvUser, msDueTime, msPeriod, fFlags);
    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
}

DECL_KERNEL32(BOOL) DeleteTimerQueueTimer(HANDLE hTimerQueue, HANDLE hTimer, HANDLE hEvtCompletion)
{
    RESOLVE_ME(DeleteTimerQueueTimer);
    if (pfnApi)
        return pfnApi(hTimerQueue, hTimer, hEvtCompletion);
    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
}

/* This is used by several APIs. */

DECL_KERNEL32(VOID) InitializeSListHead(PSLIST_HEADER pHead)
{
    RESOLVE_ME(InitializeSListHead);
    if (pfnApi)
        pfnApi(pHead);
    else /* fallback: */
        pHead->Alignment = 0;
}


DECL_KERNEL32(PSLIST_ENTRY) InterlockedFlushSList(PSLIST_HEADER pHead)
{
    RESOLVE_ME(InterlockedFlushSList);
    if (pfnApi)
        return pfnApi(pHead);

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

DECL_KERNEL32(PSLIST_ENTRY) InterlockedPopEntrySList(PSLIST_HEADER pHead)
{
    RESOLVE_ME(InterlockedPopEntrySList);
    if (pfnApi)
        return pfnApi(pHead);

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

DECL_KERNEL32(PSLIST_ENTRY) InterlockedPushEntrySList(PSLIST_HEADER pHead, PSLIST_ENTRY pEntry)
{
    RESOLVE_ME(InterlockedPushEntrySList);
    if (pfnApi)
        return pfnApi(pHead, pEntry);

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

DECL_KERNEL32(WORD) QueryDepthSList(PSLIST_HEADER pHead)
{
    RESOLVE_ME(QueryDepthSList);
    if (pfnApi)
        return pfnApi(pHead);
    return pHead->Depth;
}


/* curl drags these in: */
DECL_KERNEL32(BOOL) VerifyVersionInfoA(LPOSVERSIONINFOEXA pInfo, DWORD fTypeMask, DWORDLONG fConditionMask)
{
    RESOLVE_ME(VerifyVersionInfoA);
    if (pfnApi)
        return pfnApi(pInfo, fTypeMask, fConditionMask);

    /* fallback to make curl happy: */
    OSVERSIONINFOEXA VerInfo;
    RT_ZERO(VerInfo);
    VerInfo.dwOSVersionInfoSize = sizeof(VerInfo);
    if (!GetVersionExA((OSVERSIONINFO *)&VerInfo))
    {
        RT_ZERO(VerInfo);
        VerInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
        AssertReturn(GetVersionExA((OSVERSIONINFO *)&VerInfo), FALSE);
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
                default: uLeft = uRight = 0; AssertFailed();
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
                default:                    fRet = FALSE; AssertFailed(); break;
            }
        }

    return fRet;
}


DECL_KERNEL32(ULONGLONG) VerSetConditionMask(ULONGLONG fConditionMask, DWORD fTypeMask, BYTE bOperator)
{
    RESOLVE_ME(VerSetConditionMask);
    if (pfnApi)
        return pfnApi(fConditionMask, fTypeMask, bOperator);

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

DECL_KERNEL32(BOOL) IsProcessorFeaturePresent(DWORD enmProcessorFeature)
{
    RESOLVE_ME(IsProcessorFeaturePresent);
    if (pfnApi)
        return pfnApi(enmProcessorFeature);

    /* Could make more of an effort here... */
    return FALSE;
}


DECL_KERNEL32(BOOL) CancelIo(HANDLE hHandle)
{
    RESOLVE_ME(CancelIo);
    if (pfnApi)
        return pfnApi(hHandle);

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

DECL_KERNEL32(BOOL) IsDebuggerPresent(VOID)
{
    RESOLVE_ME(IsDebuggerPresent);
    if (pfnApi)
        return pfnApi();
    return FALSE;
}


DECL_KERNEL32(VOID) GetSystemTimeAsFileTime(LPFILETIME pTime)
{
    RESOLVE_ME(GetSystemTimeAsFileTime);
    if (pfnApi)
        pfnApi(pTime);
    else
    {
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
            AssertStmt(fRet, pTime->dwHighDateTime = pTime->dwLowDateTime = 0);
        }
    }
}


/*
 * NT 3.1 stuff.
 */

DECL_KERNEL32(BOOL) GetVersionExA(LPOSVERSIONINFOA pInfo)
{
    RESOLVE_ME(GetVersionExA);
    if (pfnApi)
        return pfnApi(pInfo);

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


DECL_KERNEL32(BOOL) GetVersionExW(LPOSVERSIONINFOW pInfo)
{
    RESOLVE_ME(GetVersionExW);
    if (pfnApi)
        return pfnApi(pInfo);

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


DECL_KERNEL32(LPWCH) GetEnvironmentStringsW(void)
{
    RESOLVE_ME(GetEnvironmentStringsW);
    if (pfnApi)
        return pfnApi();

    /*
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


DECL_KERNEL32(BOOL) FreeEnvironmentStringsW(LPWCH pwszzEnv)
{
    RESOLVE_ME(FreeEnvironmentStringsW);
    if (pfnApi)
        return pfnApi(pwszzEnv);
    if (pwszzEnv)
        HeapFree(GetProcessHeap(), 0, pwszzEnv);
    return TRUE;
}


DECL_KERNEL32(int) GetLocaleInfoA(LCID idLocale, LCTYPE enmType, LPSTR pData, int cchData)
{
    RESOLVE_ME(GetLocaleInfoA);
    if (pfnApi)
        return pfnApi(idLocale, enmType, pData, cchData);

    AssertMsgFailed(("GetLocaleInfoA: idLocale=%#x enmType=%#x cchData=%#x\n", idLocale, enmType, cchData));
    SetLastError(ERROR_NOT_SUPPORTED);
    return 0;
}


DECL_KERNEL32(BOOL) EnumSystemLocalesA(LOCALE_ENUMPROCA pfnCallback, DWORD fFlags)
{
    RESOLVE_ME(EnumSystemLocalesA);
    if (pfnApi)
        return pfnApi(pfnCallback, fFlags);

    AssertMsgFailed(("EnumSystemLocalesA: pfnCallback=%p fFlags=%#x\n", pfnCallback, fFlags));
    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
}


DECL_KERNEL32(BOOL) IsValidLocale(LCID idLocale, DWORD fFlags)
{
    RESOLVE_ME(IsValidLocale);
    if (pfnApi)
        return pfnApi(idLocale, fFlags);

    AssertMsgFailed(("IsValidLocale: idLocale fFlags=%#x\n", idLocale, fFlags));
    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
}


DECL_KERNEL32(DWORD_PTR) SetThreadAffinityMask(HANDLE hThread, DWORD_PTR fAffinityMask)
{
    RESOLVE_ME(SetThreadAffinityMask);
    if (pfnApi)
        return pfnApi(hThread, fAffinityMask);

    SYSTEM_INFO SysInfo;
    GetSystemInfo(&SysInfo);
    AssertMsgFailed(("SetThreadAffinityMask: hThread=%p fAffinityMask=%p SysInfo.dwActiveProcessorMask=%p\n",
                     hThread, fAffinityMask, SysInfo.dwActiveProcessorMask));
    if (   SysInfo.dwActiveProcessorMask == fAffinityMask
        || fAffinityMask                 == ~(DWORD_PTR)0)
        return fAffinityMask;

    SetLastError(ERROR_NOT_SUPPORTED);
    return 0;
}


DECL_KERNEL32(BOOL) GetProcessAffinityMask(HANDLE hProcess, PDWORD_PTR pfProcessAffinityMask, PDWORD_PTR pfSystemAffinityMask)
{
    RESOLVE_ME(GetProcessAffinityMask);
    if (pfnApi)
        return pfnApi(hProcess, pfProcessAffinityMask, pfSystemAffinityMask);

    SYSTEM_INFO SysInfo;
    GetSystemInfo(&SysInfo);
    AssertMsgFailed(("GetProcessAffinityMask: SysInfo.dwActiveProcessorMask=%p\n", SysInfo.dwActiveProcessorMask));
    if (pfProcessAffinityMask)
        *pfProcessAffinityMask = SysInfo.dwActiveProcessorMask;
    if (pfSystemAffinityMask)
        *pfSystemAffinityMask  = SysInfo.dwActiveProcessorMask;
    return TRUE;
}


DECL_KERNEL32(BOOL) GetHandleInformation(HANDLE hObject, DWORD *pfFlags)
{
    RESOLVE_ME(GetHandleInformation);
    if (pfnApi)
        return pfnApi(hObject, pfFlags);


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
    AssertMsg(rcNt == STATUS_INVALID_HANDLE, ("rcNt=%#x\n", rcNt));
    SetLastError(rcNt == STATUS_INVALID_HANDLE ? ERROR_INVALID_HANDLE : ERROR_INVALID_FUNCTION);
    return FALSE;
}


DECL_KERNEL32(BOOL) SetHandleInformation(HANDLE hObject, DWORD fMask, DWORD fFlags)
{
    RESOLVE_ME(SetHandleInformation);
    if (pfnApi)
        return pfnApi(hObject, fMask, fFlags);

    SetLastError(ERROR_INVALID_FUNCTION);
    return FALSE;
}


/* Dummy to force dragging in this object in the link, so the linker
   won't accidentally use the symbols from kernel32. */
extern "C" int vcc100_kernel32_fakes_cpp(void)
{
    return 42;
}

