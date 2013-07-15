/* $Id$ */
/** @file
 * IPRT - Local IPC, Windows Implementation Using Named Pipes.
 */

/*
 * Copyright (C) 2008-2013 Oracle Corporation
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

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
/*
 * We have to force NT 5.0 here because of
 * ConvertStringSecurityDescriptorToSecurityDescriptor. Note that because of
 * FILE_FLAG_FIRST_PIPE_INSTANCE this code actually requires W2K SP2+.
 */
#ifndef _WIN32_WINNT
# define _WIN32_WINNT 0x0500 /* for ConvertStringSecurityDescriptorToSecurityDescriptor */
#elif _WIN32_WINNT < 0x0500
# undef _WIN32_WINNT
# define _WIN32_WINNT 0x0500
#endif
#include <Windows.h>
#include <sddl.h>

#include <iprt/alloc.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/critsect.h>
#include <iprt/err.h>
#include <iprt/ldr.h>
#include <iprt/localipc.h>
#include <iprt/param.h>
#include <iprt/string.h>
#include <iprt/thread.h>
#include <iprt/time.h>

#include "internal/magics.h"


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** Pipe prefix string. */
#define RTLOCALIPC_WIN_PREFIX   "\\\\.\\pipe\\IPRT-"

/** DACL for block all network access and local users other than the creator/owner.
 *
 * ACE format: (ace_type;ace_flags;rights;object_guid;inherit_object_guid;account_sid)
 *
 * Note! FILE_GENERIC_WRITE (SDDL_FILE_WRITE) is evil here because it includes
 *       the FILE_CREATE_PIPE_INSTANCE(=FILE_APPEND_DATA) flag. Thus the hardcoded
 *       value 0x0012019b in the 2nd ACE. It expands to:
 *          0x00000001 - FILE_READ_DATA
 *          0x00000008 - FILE_READ_EA
 *          0x00000080 - FILE_READ_ATTRIBUTES
 *          0x00020000 - READ_CONTROL
 *          0x00100000 - SYNCHRONIZE
 *          0x00000002 - FILE_WRITE_DATA
 *          0x00000010 - FILE_WRITE_EA
 *          0x00000100 - FILE_WRITE_ATTRIBUTES
 *          0x0012019b
 *       or FILE_GENERIC_READ | (FILE_GENERIC_WRITE & ~FILE_CREATE_PIPE_INSTANCE)
 *
 * @todo Double check this!
 * @todo Drop the EA rights too? Since they doesn't mean anything to PIPS according to the docs.
 * @todo EVERYONE -> AUTHENTICATED USERS or something more appropriate?
 * @todo Have trouble allowing the owner FILE_CREATE_PIPE_INSTANCE access, so for now I'm hacking
 *       it just to get progress - the service runs as local system.
 *       The CREATOR OWNER and PERSONAL SELF works (the former is only involved in inheriting
 *       it seems, which is why it won't work. The latter I've no idea about. Perhaps the solution
 *       is to go the annoying route of OpenProcessToken, QueryTokenInformation,
 *          ConvertSidToStringSid and then use the result... Suggestions are very welcome
 */
#define RTLOCALIPC_WIN_SDDL \
    SDDL_DACL SDDL_DELIMINATOR \
        SDDL_ACE_BEGIN SDDL_ACCESS_DENIED ";;" SDDL_GENERIC_ALL ";;;" SDDL_NETWORK SDDL_ACE_END \
        SDDL_ACE_BEGIN SDDL_ACCESS_ALLOWED ";;" "0x0012019b"    ";;;" SDDL_EVERYONE SDDL_ACE_END \
        SDDL_ACE_BEGIN SDDL_ACCESS_ALLOWED ";;" SDDL_FILE_ALL ";;;" SDDL_LOCAL_SYSTEM SDDL_ACE_END

//        SDDL_ACE_BEGIN SDDL_ACCESS_ALLOWED ";;" SDDL_GENERIC_ALL ";;;" SDDL_PERSONAL_SELF SDDL_ACE_END \
//        SDDL_ACE_BEGIN SDDL_ACCESS_ALLOWED ";CIOI;" SDDL_GENERIC_ALL ";;;" SDDL_CREATOR_OWNER SDDL_ACE_END
//        SDDL_ACE_BEGIN SDDL_ACCESS_ALLOWED ";;" "0x0012019b"    ";;;" SDDL_EVERYONE SDDL_ACE_END
//        SDDL_ACE_BEGIN SDDL_ACCESS_ALLOWED ";;" SDDL_FILE_ALL ";;;" SDDL_LOCAL_SYSTEM SDDL_ACE_END


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Local IPC service instance, Windows.
 */
typedef struct RTLOCALIPCSERVERINT
{
    /** The magic (RTLOCALIPCSERVER_MAGIC). */
    uint32_t u32Magic;
    /** The creation flags. */
    uint32_t fFlags;
    /** Critical section protecting the structure. */
    RTCRITSECT CritSect;
    /** The number of references to the instance.
     * @remarks The reference counting isn't race proof. */
    uint32_t volatile cRefs;
    /** Indicates that there is a pending cancel request. */
    bool volatile fCancelled;
    /** The name pipe handle. */
    HANDLE hNmPipe;
    /** The handle to the event object we're using for overlapped I/O. */
    HANDLE hEvent;
    /** The overlapped I/O structure. */
    OVERLAPPED OverlappedIO;
    /** The pipe name. */
    char szName[1];
} RTLOCALIPCSERVERINT;
/** Pointer to a local IPC server instance (Windows). */
typedef RTLOCALIPCSERVERINT *PRTLOCALIPCSERVERINT;


/**
 * Local IPC session instance, Windows.
 */
typedef struct RTLOCALIPCSESSIONINT
{
    /** The magic (RTLOCALIPCSESSION_MAGIC). */
    uint32_t            u32Magic;
    /** Critical section protecting the structure. */
    RTCRITSECT          CritSect;
    /** The number of references to the instance.
     * @remarks The reference counting isn't race proof. */
    uint32_t volatile   cRefs;
    /** Set if there is already pending I/O. */
    bool                fIOPending;
    /** Set if the zero byte read that the poll code using is pending. */
    bool                fZeroByteRead;
    /** Indicates that there is a pending cancel request. */
    bool volatile       fCancelled;
    /** The name pipe handle. */
    HANDLE              hNmPipe;
    /** The handle to the event object we're using for overlapped I/O. */
    HANDLE              hEvent;
    /** The overlapped I/O structure. */
    OVERLAPPED          OverlappedIO;
    /** Bounce buffer for writes. */
    uint8_t            *pbBounceBuf;
    /** Amount of used buffer space. */
    size_t              cbBounceBufUsed;
    /** Amount of allocated buffer space. */
    size_t              cbBounceBufAlloc;
    /** Buffer for the zero byte read.
     *  Used in RTLocalIpcSessionWaitForData(). */
    uint8_t             abBuf[8];
} RTLOCALIPCSESSIONINT;
/** Pointer to a local IPC session instance (Windows). */
typedef RTLOCALIPCSESSIONINT *PRTLOCALIPCSESSIONINT;

typedef BOOL WINAPI FNCONVERTSTRINGSECURITYDESCRIPTORTOSECURITYDESCRIPTOR(LPCTSTR, DWORD, PSECURITY_DESCRIPTOR, PULONG);
typedef FNCONVERTSTRINGSECURITYDESCRIPTORTOSECURITYDESCRIPTOR
    *PFNCONVERTSTRINGSECURITYDESCRIPTORTOSECURITYDESCRIPTOR; /* No, nobody fell on the keyboard, really! */


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static int rtLocalIpcWinCreateSession(PRTLOCALIPCSESSION phClientSession, HANDLE hNmPipeSession);


/**
 * Builds and allocates the security descriptor required for securing the local pipe.
 *
 * @return  IPRT status code.
 * @param   ppDesc              Where to store the allocated security descriptor on success.
 *                              Must be free'd using LocalFree().
 */
static int rtLocalIpcServerWinAllocSecurityDescriptior(PSECURITY_DESCRIPTOR *ppDesc)
{
    /** @todo Stuff this into RTInitOnce? Later. */
    PFNCONVERTSTRINGSECURITYDESCRIPTORTOSECURITYDESCRIPTOR
        pfnConvertStringSecurityDescriptorToSecurityDescriptor = NULL;

    RTLDRMOD hAdvApi32 = NIL_RTLDRMOD;
    int rc = RTLdrLoadSystem("Advapi32.dll", true /*fNoUnload*/, &hAdvApi32);
    if (RT_SUCCESS(rc))
        rc = RTLdrGetSymbol(hAdvApi32, "ConvertStringSecurityDescriptorToSecurityDescriptorW",
                            (void**)&pfnConvertStringSecurityDescriptorToSecurityDescriptor);

    PSECURITY_DESCRIPTOR pSecDesc = NULL;
    if (RT_SUCCESS(rc))
    {
        AssertPtr(pfnConvertStringSecurityDescriptorToSecurityDescriptor);

        /*
         * We'll create a security descriptor from a SDDL that denies
         * access to network clients (this is local IPC after all), it
         * makes some further restrictions to prevent non-authenticated
         * users from screwing around.
         */
        PRTUTF16 pwszSDDL;
        rc = RTStrToUtf16(RTLOCALIPC_WIN_SDDL, &pwszSDDL);
        if (RT_SUCCESS(rc))
        {
            if (!pfnConvertStringSecurityDescriptorToSecurityDescriptor((LPCTSTR)pwszSDDL,
                                                                        SDDL_REVISION_1,
                                                                        &pSecDesc,
                                                                        NULL))
            {
                rc = RTErrConvertFromWin32(GetLastError());
            }

            RTUtf16Free(pwszSDDL);
        }
    }
    else
    {
        /* Windows OSes < W2K SP2 not supported for now, bail out. */
        /** @todo Implement me! */
        rc = VERR_NOT_SUPPORTED;
    }

    if (hAdvApi32 != NIL_RTLDRMOD)
         RTLdrClose(hAdvApi32);

    if (RT_SUCCESS(rc))
    {
        AssertPtr(pSecDesc);
        *ppDesc = pSecDesc;
    }

    return rc;
}

/**
 * Creates a named pipe instance.
 *
 * This is used by both RTLocalIpcServerCreate and RTLocalIpcServerListen.
 *
 * @return  IPRT status code.
 * @param   phNmPipe            Where to store the named pipe handle on success. This
 *                              will be set to INVALID_HANDLE_VALUE on failure.
 * @param   pszFullPipeName     The full named pipe name.
 * @param   fFirst              Set on the first call (from RTLocalIpcServerCreate), otherwise clear.
 *                              Governs the FILE_FLAG_FIRST_PIPE_INSTANCE flag.
 */
static int rtLocalIpcServerWinCreatePipeInstance(PHANDLE phNmPipe, const char *pszFullPipeName, bool fFirst)
{
    *phNmPipe = INVALID_HANDLE_VALUE;

    PSECURITY_DESCRIPTOR pSecDesc;
    int rc = rtLocalIpcServerWinAllocSecurityDescriptior(&pSecDesc);
    if (RT_SUCCESS(rc))
    {
        SECURITY_ATTRIBUTES SecAttrs;
        SecAttrs.nLength = sizeof(SECURITY_ATTRIBUTES);
        SecAttrs.lpSecurityDescriptor = pSecDesc;
        SecAttrs.bInheritHandle = FALSE;

        DWORD fOpenMode = PIPE_ACCESS_DUPLEX
                        | PIPE_WAIT
                        | FILE_FLAG_OVERLAPPED;

        bool fSupportsFirstInstance = false;

        OSVERSIONINFOEX OSInfoEx;
        RT_ZERO(OSInfoEx);
        OSInfoEx.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
        if (   GetVersionEx((LPOSVERSIONINFO) &OSInfoEx)
            && OSInfoEx.dwPlatformId == VER_PLATFORM_WIN32_NT)
        {
            if (   /* Vista+. */
                   OSInfoEx.dwMajorVersion >= 6
                   /* Windows XP+. */
                || (   OSInfoEx.dwMajorVersion == 5
                    && OSInfoEx.dwMinorVersion >  0)
                   /* Windows 2000. */
                || (   OSInfoEx.dwMajorVersion == 5
                    && OSInfoEx.dwMinorVersion == 0
                    && OSInfoEx.wServicePackMajor >= 2))
            {
                /* Requires at least W2K (5.0) SP2+. This is non-fatal. */
                fSupportsFirstInstance = true;
            }
        }

        if (fFirst && fSupportsFirstInstance)
            fOpenMode |= FILE_FLAG_FIRST_PIPE_INSTANCE;

        HANDLE hNmPipe = CreateNamedPipe(pszFullPipeName,               /* lpName */
                                         fOpenMode,                     /* dwOpenMode */
                                         PIPE_TYPE_BYTE,                /* dwPipeMode */
                                         PIPE_UNLIMITED_INSTANCES,      /* nMaxInstances */
                                         PAGE_SIZE,                     /* nOutBufferSize (advisory) */
                                         PAGE_SIZE,                     /* nInBufferSize (ditto) */
                                         30*1000,                       /* nDefaultTimeOut = 30 sec */
                                         NULL /** @todo !!! Fix this !!! &SecAttrs */);                    /* lpSecurityAttributes */
        LocalFree(pSecDesc);
        if (hNmPipe != INVALID_HANDLE_VALUE)
        {
            *phNmPipe = hNmPipe;
        }
        else
            rc = RTErrConvertFromWin32(GetLastError());
    }

    return rc;
}


RTDECL(int) RTLocalIpcServerCreate(PRTLOCALIPCSERVER phServer, const char *pszName, uint32_t fFlags)
{
    /*
     * Basic parameter validation.
     */
    AssertPtrReturn(phServer, VERR_INVALID_POINTER);
    AssertPtrReturn(pszName, VERR_INVALID_POINTER);
    AssertReturn(*pszName, VERR_INVALID_PARAMETER);
    AssertReturn(!(fFlags & ~(RTLOCALIPC_FLAGS_VALID_MASK)), VERR_INVALID_PARAMETER);
    AssertReturn((fFlags & RTLOCALIPC_FLAGS_MULTI_SESSION), VERR_INVALID_PARAMETER); /** @todo Implement !RTLOCALIPC_FLAGS_MULTI_SESSION */

    /*
     * Allocate and initialize the instance data.
     */
    size_t cchName = strlen(pszName);
    size_t cch = RT_OFFSETOF(RTLOCALIPCSERVERINT, szName[cchName + sizeof(RTLOCALIPC_WIN_PREFIX)]);
    PRTLOCALIPCSERVERINT pThis = (PRTLOCALIPCSERVERINT)RTMemAlloc(cch);
    if (!pThis)
        return VERR_NO_MEMORY;
    pThis->u32Magic = RTLOCALIPCSERVER_MAGIC;
    pThis->cRefs = 1; /* the one we return */
    pThis->fCancelled = false;
    memcpy(pThis->szName, RTLOCALIPC_WIN_PREFIX, sizeof(RTLOCALIPC_WIN_PREFIX) - 1);
    memcpy(&pThis->szName[sizeof(RTLOCALIPC_WIN_PREFIX) - 1], pszName, cchName + 1);
    int rc = RTCritSectInit(&pThis->CritSect);
    if (RT_SUCCESS(rc))
    {
        pThis->hEvent = CreateEvent(NULL /*lpEventAttributes*/, TRUE /*bManualReset*/,
                                    FALSE /*bInitialState*/, NULL /*lpName*/);
        if (pThis->hEvent != NULL)
        {
            RT_ZERO(pThis->OverlappedIO);
            pThis->OverlappedIO.Internal = STATUS_PENDING;
            pThis->OverlappedIO.hEvent = pThis->hEvent;

            rc = rtLocalIpcServerWinCreatePipeInstance(&pThis->hNmPipe,
                                                       pThis->szName, true /* fFirst */);
            if (RT_SUCCESS(rc))
            {
                *phServer = pThis;
                return VINF_SUCCESS;
            }

            BOOL fRc = CloseHandle(pThis->hEvent);
            AssertMsg(fRc, ("%d\n", GetLastError())); NOREF(fRc);
        }
        else
            rc = RTErrConvertFromWin32(GetLastError());

        int rc2 = RTCritSectDelete(&pThis->CritSect);
        AssertRC(rc2);
    }
    RTMemFree(pThis);
    return rc;
}


/**
 * Call when the reference count reaches 0.
 * Caller owns the critsect.
 * @param   pThis       The instance to destroy.
 */
static void rtLocalIpcServerWinDestroy(PRTLOCALIPCSERVERINT pThis)
{
    BOOL fRc = CloseHandle(pThis->hNmPipe);
    AssertMsg(fRc, ("%d\n", GetLastError())); NOREF(fRc);
    pThis->hNmPipe = INVALID_HANDLE_VALUE;

    fRc = CloseHandle(pThis->hEvent);
    AssertMsg(fRc, ("%d\n", GetLastError())); NOREF(fRc);
    pThis->hEvent = NULL;

    RTCritSectLeave(&pThis->CritSect);
    RTCritSectDelete(&pThis->CritSect);

    RTMemFree(pThis);
}


RTDECL(int) RTLocalIpcServerDestroy(RTLOCALIPCSERVER hServer)
{
    /*
     * Validate input.
     */
    if (hServer == NIL_RTLOCALIPCSERVER)
        return VINF_SUCCESS;
    PRTLOCALIPCSERVERINT pThis = (PRTLOCALIPCSERVERINT)hServer;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTLOCALIPCSERVER_MAGIC, VERR_INVALID_MAGIC);

    /*
     * Cancel any thread currently busy using the server,
     * leaving the cleanup to it.
     */
    RTCritSectEnter(&pThis->CritSect);
    ASMAtomicUoWriteU32(&pThis->u32Magic, ~RTLOCALIPCSERVER_MAGIC);
    ASMAtomicUoWriteBool(&pThis->fCancelled, true);
    Assert(pThis->cRefs);
    pThis->cRefs--;

    if (pThis->cRefs)
    {
        BOOL fRc = SetEvent(pThis->hEvent);
        AssertMsg(fRc, ("%d\n", GetLastError())); NOREF(fRc);

        RTCritSectLeave(&pThis->CritSect);
    }
    else
        rtLocalIpcServerWinDestroy(pThis);

    return VINF_SUCCESS;
}


RTDECL(int) RTLocalIpcServerListen(RTLOCALIPCSERVER hServer, PRTLOCALIPCSESSION phClientSession)
{
    /*
     * Validate input.
     */
    PRTLOCALIPCSERVERINT pThis = (PRTLOCALIPCSERVERINT)hServer;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTLOCALIPCSERVER_MAGIC, VERR_INVALID_MAGIC);

    /*
     * Enter the critsect before inspecting the object further.
     */
    int rc;
    RTCritSectEnter(&pThis->CritSect);
    if (pThis->fCancelled)
    {
        pThis->fCancelled = false;
        rc = VERR_CANCELLED;
        RTCritSectLeave(&pThis->CritSect);
    }
    else
    {
        pThis->cRefs++;
        ResetEvent(pThis->hEvent);
        RTCritSectLeave(&pThis->CritSect);

        /*
         * Try connect a client. We need to use overlapped I/O here because
         * of the cancellation done by RTLocalIpcServerCancel and RTLocalIpcServerDestroy.
         */
        SetLastError(NO_ERROR);
        BOOL fRc = ConnectNamedPipe(pThis->hNmPipe, &pThis->OverlappedIO);
        DWORD dwErr = fRc ? NO_ERROR : GetLastError();
        if (    !fRc
            &&  dwErr == ERROR_IO_PENDING)
        {
            WaitForSingleObject(pThis->hEvent, INFINITE);
            DWORD dwIgnored;
            fRc = GetOverlappedResult(pThis->hNmPipe, &pThis->OverlappedIO, &dwIgnored, FALSE /* bWait*/);
            dwErr = fRc ? NO_ERROR : GetLastError();
        }

        RTCritSectEnter(&pThis->CritSect);
        if (   !pThis->fCancelled /* Event signalled but not cancelled? */
            && pThis->u32Magic == RTLOCALIPCSERVER_MAGIC)
        {
            /*
             * Still alive, some error or an actual client.
             *
             * If it's the latter we'll have to create a new pipe instance that
             * replaces the current one for the server. The current pipe instance
             * will be assigned to the client session.
             */
            if (   fRc
                || dwErr == ERROR_PIPE_CONNECTED)
            {
                HANDLE hNmPipe;
                rc = rtLocalIpcServerWinCreatePipeInstance(&hNmPipe, pThis->szName, false /* fFirst */);
                if (RT_SUCCESS(rc))
                {
                    HANDLE hNmPipeSession = pThis->hNmPipe; /* consumed */
                    pThis->hNmPipe = hNmPipe;
                    rc = rtLocalIpcWinCreateSession(phClientSession, hNmPipeSession);
                }
                else
                {
                    /*
                     * We failed to create a new instance for the server, disconnect
                     * the client and fail. Don't try service the client here.
                     */
                    fRc = DisconnectNamedPipe(pThis->hNmPipe);
                    AssertMsg(fRc, ("%d\n", GetLastError()));
                }
            }
            else
                rc = RTErrConvertFromWin32(dwErr);
        }
        else
        {
            /*
             * Cancelled or destroyed.
             *
             * Cancel the overlapped io if it didn't complete (must be done
             * in the this thread) or disconnect the client.
             */
            if (    fRc
                ||  dwErr == ERROR_PIPE_CONNECTED)
                fRc = DisconnectNamedPipe(pThis->hNmPipe);
            else if (dwErr == ERROR_IO_PENDING)
                fRc = CancelIo(pThis->hNmPipe);
            else
                fRc = TRUE;
            AssertMsg(fRc, ("%d\n", GetLastError()));
            rc = VERR_CANCELLED;
        }

        pThis->cRefs--;
        if (pThis->cRefs)
            RTCritSectLeave(&pThis->CritSect);
        else
            rtLocalIpcServerWinDestroy(pThis);
    }

    return rc;
}


RTDECL(int) RTLocalIpcServerCancel(RTLOCALIPCSERVER hServer)
{
    /*
     * Validate input.
     */
    PRTLOCALIPCSERVERINT pThis = (PRTLOCALIPCSERVERINT)hServer;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTLOCALIPCSERVER_MAGIC, VERR_INVALID_MAGIC);

    /*
     * Enter the critical section, then set the cancellation flag
     * and signal the event (to wake up anyone in/at WaitForSingleObject).
     */
    int rc = RTCritSectEnter(&pThis->CritSect);
    if (RT_SUCCESS(rc))
    {
        ASMAtomicUoWriteBool(&pThis->fCancelled, true);
        BOOL fRc = SetEvent(pThis->hEvent);
        AssertMsg(fRc, ("%d\n", GetLastError())); NOREF(fRc);

        rc = RTCritSectLeave(&pThis->CritSect);
    }

    return rc;
}


/**
 * Create a session instance.
 *
 * @returns IPRT status code.
 *
 * @param   phClientSession         Where to store the session handle on success.
 * @param   hNmPipeSession          The named pipe handle. This will be consumed by this session, meaning on failure
 *                                  to create the session it will be closed.
 */
static int rtLocalIpcWinCreateSession(PRTLOCALIPCSESSION phClientSession, HANDLE hNmPipeSession)
{
    AssertPtrReturn(phClientSession, VERR_INVALID_POINTER);
    AssertReturn(hNmPipeSession != INVALID_HANDLE_VALUE, VERR_INVALID_HANDLE);

    int rc;

    /*
     * Allocate and initialize the session instance data.
     */
    PRTLOCALIPCSESSIONINT pThis = (PRTLOCALIPCSESSIONINT)RTMemAlloc(sizeof(*pThis));
    if (pThis)
    {
        pThis->u32Magic = RTLOCALIPCSESSION_MAGIC;
        pThis->cRefs = 1; /* our ref */
        pThis->fCancelled = false;
        pThis->fIOPending = false;
        pThis->fZeroByteRead = false;
        pThis->hNmPipe = hNmPipeSession;

        rc = RTCritSectInit(&pThis->CritSect);
        if (RT_SUCCESS(rc))
        {
            pThis->hEvent = CreateEvent(NULL /*lpEventAttributes*/, TRUE /*bManualReset*/,
                                        FALSE /*bInitialState*/, NULL /*lpName*/);
            if (pThis->hEvent != NULL)
            {
                RT_ZERO(pThis->OverlappedIO);
                pThis->OverlappedIO.Internal = STATUS_PENDING;
                pThis->OverlappedIO.hEvent = pThis->hEvent;

                *phClientSession = pThis;
                return VINF_SUCCESS;
            }

            /* bail out */
            rc = RTErrConvertFromWin32(GetLastError());
            RTCritSectDelete(&pThis->CritSect);
        }
        RTMemFree(pThis);
    }
    else
        rc = VERR_NO_MEMORY;

    BOOL fRc = CloseHandle(hNmPipeSession);
    AssertMsg(fRc, ("%d\n", GetLastError())); NOREF(fRc);
    return rc;
}

RTDECL(int) RTLocalIpcSessionConnect(PRTLOCALIPCSESSION phSession, const char *pszName, uint32_t fFlags)
{
    AssertPtrReturn(phSession, VERR_INVALID_POINTER);
    AssertPtrReturn(pszName, VERR_INVALID_POINTER);
    AssertReturn(*pszName, VERR_INVALID_PARAMETER);
    AssertReturn(!fFlags, VERR_INVALID_PARAMETER); /* Flags currently unused, must be 0. */

    PRTLOCALIPCSESSIONINT pThis = (PRTLOCALIPCSESSIONINT)RTMemAlloc(sizeof(*pThis));
    if (!pThis)
        return VERR_NO_MEMORY;

    pThis->u32Magic = RTLOCALIPCSESSION_MAGIC;
    pThis->cRefs = 1; /* The one we return. */
    pThis->fIOPending = false;
    pThis->fZeroByteRead = false;
    pThis->fCancelled = false;
    pThis->pbBounceBuf = NULL;
    pThis->cbBounceBufAlloc = 0;
    pThis->cbBounceBufUsed = 0;

    int rc = RTCritSectInit(&pThis->CritSect);
    if (RT_SUCCESS(rc))
    {
        pThis->hEvent = CreateEvent(NULL /*lpEventAttributes*/, TRUE /*bManualReset*/,
                                    FALSE /*bInitialState*/, NULL /*lpName*/);
        if (pThis->hEvent != NULL)
        {
            RT_ZERO(pThis->OverlappedIO);
            pThis->OverlappedIO.Internal = STATUS_PENDING;
            pThis->OverlappedIO.hEvent = pThis->hEvent;

            char *pszPipe;
            if (RTStrAPrintf(&pszPipe, "%s%s", RTLOCALIPC_WIN_PREFIX, pszName))
            {
                HANDLE hPipe = CreateFile(pszPipe,                  /* pipe name */
                                          GENERIC_READ |            /* read and write access */
                                          GENERIC_WRITE,
                                          0,                        /* no sharing */
                                          NULL,                     /* lpSecurityAttributes */
                                          OPEN_EXISTING,            /* opens existing pipe */
                                          FILE_FLAG_OVERLAPPED,     /* default attributes */
                                          NULL);                    /* no template file */
                RTStrFree(pszPipe);

                if (hPipe != INVALID_HANDLE_VALUE)
                {
                    pThis->hNmPipe = hPipe;
                    *phSession = pThis;
                    return VINF_SUCCESS;
                }
                else
                    rc = RTErrConvertFromWin32(GetLastError());
            }
            else
                rc = VERR_NO_MEMORY;

            BOOL fRc = CloseHandle(pThis->hEvent);
            AssertMsg(fRc, ("%d\n", GetLastError())); NOREF(fRc);
        }
        else
            rc = RTErrConvertFromWin32(GetLastError());

        int rc2 = RTCritSectDelete(&pThis->CritSect);
        AssertRC(rc2);
    }

    RTMemFree(pThis);
    return rc;
}


/**
 * Call when the reference count reaches 0.
 * Caller owns the critsect.
 * @param   pThis       The instance to destroy.
 */
static void rtLocalIpcSessionWinDestroy(PRTLOCALIPCSESSIONINT pThis)
{
    BOOL fRc = CloseHandle(pThis->hNmPipe);
    AssertMsg(fRc, ("%d\n", GetLastError())); NOREF(fRc);
    pThis->hNmPipe = INVALID_HANDLE_VALUE;

    fRc = CloseHandle(pThis->hEvent);
    AssertMsg(fRc, ("%d\n", GetLastError())); NOREF(fRc);
    pThis->hEvent = NULL;

    RTCritSectLeave(&pThis->CritSect);
    RTCritSectDelete(&pThis->CritSect);

    RTMemFree(pThis);
}


RTDECL(int) RTLocalIpcSessionClose(RTLOCALIPCSESSION hSession)
{
    /*
     * Validate input.
     */
    if (hSession == NIL_RTLOCALIPCSESSION)
        return VINF_SUCCESS;
    PRTLOCALIPCSESSIONINT pThis = (PRTLOCALIPCSESSIONINT)hSession;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTLOCALIPCSESSION_MAGIC, VERR_INVALID_MAGIC);

    /*
     * Cancel any thread currently busy using the session,
     * leaving the cleanup to it.
     */
    RTCritSectEnter(&pThis->CritSect);
    ASMAtomicUoWriteU32(&pThis->u32Magic, ~RTLOCALIPCSESSION_MAGIC);
    ASMAtomicUoWriteBool(&pThis->fCancelled, true);
    pThis->cRefs--;

    if (pThis->cRefs > 0)
    {
        BOOL fRc = SetEvent(pThis->hEvent);
        AssertMsg(fRc, ("%d\n", GetLastError())); NOREF(fRc);

        RTCritSectLeave(&pThis->CritSect);
    }
    else
        rtLocalIpcSessionWinDestroy(pThis);

    return VINF_SUCCESS;
}


RTDECL(int) RTLocalIpcSessionRead(RTLOCALIPCSESSION hSession, void *pvBuffer, size_t cbBuffer, size_t *pcbRead)
{
    PRTLOCALIPCSESSIONINT pThis = (PRTLOCALIPCSESSIONINT)hSession;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTLOCALIPCSESSION_MAGIC, VERR_INVALID_HANDLE);
    AssertPtrReturn(pvBuffer, VERR_INVALID_POINTER);
    /* pcbRead is optional. */

    int rc = RTCritSectEnter(&pThis->CritSect);
    if (RT_SUCCESS(rc))
    {
        /* No concurrent readers, sorry. */
        if (pThis->cRefs == 1)
        {
            pThis->cRefs++;

            /*
             * If pcbRead is non-NULL this indicates the maximum number of bytes to read.
             * If pcbRead is NULL the this is the exact number of bytes to read.
             */
            size_t cbToRead = pcbRead ? *pcbRead : cbBuffer;
            size_t cbTotalRead = 0;
            while (cbToRead > 0)
            {
                /*
                 * Kick of a an overlapped read.  It should return immediately if
                 * there is bytes in the buffer.  If not, we'll cancel it and see
                 * what we get back.
                 */
                rc = ResetEvent(pThis->OverlappedIO.hEvent); Assert(rc == TRUE);
                DWORD cbRead = 0;
                pThis->fIOPending = true;
                RTCritSectLeave(&pThis->CritSect);

                if (ReadFile(pThis->hNmPipe, pvBuffer,
                             cbToRead <= ~(DWORD)0 ? (DWORD)cbToRead : ~(DWORD)0,
                             &cbRead, &pThis->OverlappedIO))
                    rc = VINF_SUCCESS;
                else if (GetLastError() == ERROR_IO_PENDING)
                {
                    WaitForSingleObject(pThis->OverlappedIO.hEvent, INFINITE);
                    if (GetOverlappedResult(pThis->hNmPipe, &pThis->OverlappedIO,
                                            &cbRead, TRUE /*fWait*/))
                        rc = VINF_SUCCESS;
                    else
                    {
                        DWORD dwErr = GetLastError();
                        AssertMsgFailed(("err=%ld\n",  dwErr));
                            rc = RTErrConvertFromWin32(dwErr);
                    }
                }
                 else
                    {
                        DWORD dwErr = GetLastError();
                        AssertMsgFailed(("err2=%ld\n",  dwErr));
                            rc = RTErrConvertFromWin32(dwErr);
                    }

                RTCritSectEnter(&pThis->CritSect);
                pThis->fIOPending = false;
                if (RT_FAILURE(rc))
                    break;

                /* Advance. */
                cbToRead    -= cbRead;
                cbTotalRead += cbRead;
                pvBuffer     = (uint8_t *)pvBuffer + cbRead;
            }

            if (pcbRead)
            {
                *pcbRead = cbTotalRead;
                if (   RT_FAILURE(rc)
                    && cbTotalRead
                    && rc != VERR_INVALID_POINTER)
                    rc = VINF_SUCCESS;
            }

            pThis->cRefs--;
        }
        else
            rc = VERR_WRONG_ORDER;
        RTCritSectLeave(&pThis->CritSect);
    }

    return rc;
}


/**
 * Common worker for handling I/O completion.
 *
 * This is used by RTLocalIpcSessionClose and RTLocalIpcSessionWrite.
 *
 * @returns IPRT status code.
 * @param   pThis               The pipe instance handle.
 */
static int rtLocalIpcSessionWriteCheckCompletion(PRTLOCALIPCSESSIONINT pThis)
{
    int rc;
    DWORD dwRc = WaitForSingleObject(pThis->OverlappedIO.hEvent, 0);
    if (dwRc == WAIT_OBJECT_0)
    {
        DWORD cbWritten = 0;
        if (GetOverlappedResult(pThis->hNmPipe, &pThis->OverlappedIO, &cbWritten, TRUE))
        {
            for (;;)
            {
                if (cbWritten >= pThis->cbBounceBufUsed)
                {
                    pThis->fIOPending = false;
                    rc = VINF_SUCCESS;
                    break;
                }

                /* resubmit the remainder of the buffer - can this actually happen? */
                memmove(&pThis->pbBounceBuf[0], &pThis->pbBounceBuf[cbWritten], pThis->cbBounceBufUsed - cbWritten);
                rc = ResetEvent(pThis->OverlappedIO.hEvent); Assert(rc == TRUE);
                if (!WriteFile(pThis->hNmPipe, pThis->pbBounceBuf, (DWORD)pThis->cbBounceBufUsed,
                               &cbWritten, &pThis->OverlappedIO))
                {
                    DWORD dwErr = GetLastError();
                    if (dwErr == ERROR_IO_PENDING)
                        rc = VINF_TRY_AGAIN;
                    else
                    {
                        pThis->fIOPending = false;
                        if (dwErr == ERROR_NO_DATA)
                            rc = VERR_BROKEN_PIPE;
                        else
                            rc = RTErrConvertFromWin32(dwErr);
                    }
                    break;
                }
                Assert(cbWritten > 0);
            }
        }
        else
        {
            pThis->fIOPending = false;
            rc = RTErrConvertFromWin32(GetLastError());
        }
    }
    else if (dwRc == WAIT_TIMEOUT)
        rc = VINF_TRY_AGAIN;
    else
    {
        pThis->fIOPending = false;
        if (dwRc == WAIT_ABANDONED)
            rc = VERR_INVALID_HANDLE;
        else
            rc = RTErrConvertFromWin32(GetLastError());
    }
    return rc;
}


RTDECL(int) RTLocalIpcSessionWrite(RTLOCALIPCSESSION hSession, const void *pvBuffer, size_t cbBuffer)
{
    PRTLOCALIPCSESSIONINT pThis = (PRTLOCALIPCSESSIONINT)hSession;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTLOCALIPCSESSION_MAGIC, VERR_INVALID_HANDLE);
    AssertPtrReturn(pvBuffer, VERR_INVALID_POINTER);
    AssertReturn(cbBuffer, VERR_INVALID_PARAMETER);

    int rc = RTCritSectEnter(&pThis->CritSect);
    if (RT_SUCCESS(rc))
    {
        /* No concurrent writers, sorry. */
        if (pThis->cRefs == 1)
        {
            pThis->cRefs++;

            /*
             * If I/O is pending, wait for it to complete.
             */
            if (pThis->fIOPending)
            {
                rc = rtLocalIpcSessionWriteCheckCompletion(pThis);
                while (rc == VINF_TRY_AGAIN)
                {
                    Assert(pThis->fIOPending);
                    HANDLE hEvent = pThis->OverlappedIO.hEvent;
                    RTCritSectLeave(&pThis->CritSect);
                    WaitForSingleObject(pThis->OverlappedIO.hEvent, INFINITE);
                    RTCritSectEnter(&pThis->CritSect);
                }
            }
            if (RT_SUCCESS(rc))
            {
                Assert(!pThis->fIOPending);

                /*
                 * Try write everything.
                 * No bounce buffering, cUsers protects us.
                 */
                size_t cbTotalWritten = 0;
                while (cbBuffer > 0)
                {
                    BOOL fRc = ResetEvent(pThis->OverlappedIO.hEvent); Assert(fRc == TRUE);
                    pThis->fIOPending = true;
                    RTCritSectLeave(&pThis->CritSect);

                    DWORD cbWritten = 0;
                    fRc = WriteFile(pThis->hNmPipe, pvBuffer,
                                    cbBuffer <= ~(DWORD)0 ? (DWORD)cbBuffer : ~(DWORD)0,
                                    &cbWritten, &pThis->OverlappedIO);
                    if (fRc)
                    {
                        rc = VINF_SUCCESS;
                    }
                    else
                    {
                        DWORD dwErr = GetLastError();
                        if (dwErr == ERROR_IO_PENDING)
                        {
                            DWORD dwRc = WaitForSingleObject(pThis->OverlappedIO.hEvent, INFINITE);
                            if (dwRc == WAIT_OBJECT_0)
                            {
                                if (GetOverlappedResult(pThis->hNmPipe, &pThis->OverlappedIO, &cbWritten, TRUE /*fWait*/))
                                    rc = VINF_SUCCESS;
                                else
                                    rc = RTErrConvertFromWin32(GetLastError());
                            }
                            else if (dwRc == WAIT_TIMEOUT)
                                rc = VERR_TIMEOUT;
                            else if (dwRc == WAIT_ABANDONED)
                                rc = VERR_INVALID_HANDLE;
                            else
                                rc = RTErrConvertFromWin32(GetLastError());
                        }
                        else if (dwErr == ERROR_NO_DATA)
                            rc = VERR_BROKEN_PIPE;
                        else
                            rc = RTErrConvertFromWin32(dwErr);
                    }

                    RTCritSectEnter(&pThis->CritSect);
                    pThis->fIOPending = false;
                    if (RT_FAILURE(rc))
                        break;

                    /* Advance. */
                    pvBuffer        = (char const *)pvBuffer + cbWritten;
                    cbTotalWritten += cbWritten;
                    cbBuffer       -= cbWritten;
                }
            }

            pThis->cRefs--;
        }
        else
            rc = VERR_WRONG_ORDER;
        RTCritSectLeave(&pThis->CritSect);
    }

    return rc;
}


RTDECL(int) RTLocalIpcSessionFlush(RTLOCALIPCSESSION hSession)
{
    /* No flushing on Windows needed since RTLocalIpcSessionWrite will block until
     * all data was written (or an error occurred). */
    /** @todo Implement this as soon as we want an explicit asynchronous version of
     *        RTLocalIpcSessionWrite on Windows. */
    return VINF_SUCCESS;
}


RTDECL(int) RTLocalIpcSessionWaitForData(RTLOCALIPCSESSION hSession, uint32_t cMillies)
{
    PRTLOCALIPCSESSIONINT pThis = (PRTLOCALIPCSESSIONINT)hSession;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTLOCALIPCSESSION_MAGIC, VERR_INVALID_HANDLE);

    uint64_t const StartMsTS = RTTimeMilliTS();

    int rc = RTCritSectEnter(&pThis->CritSect);
    if (RT_FAILURE(rc))
        return rc;
    for (unsigned iLoop = 0;; iLoop++)
    {
        HANDLE hWait = INVALID_HANDLE_VALUE;

        if (pThis->fIOPending)
            hWait = pThis->OverlappedIO.hEvent;
        else
        {
            /* Peek at the pipe buffer and see how many bytes it contains. */
            DWORD cbAvailable;
            BOOL fRc = PeekNamedPipe(pThis->hNmPipe, NULL, 0, NULL, &cbAvailable, NULL);
            if (   fRc
                && cbAvailable)
            {
                rc = VINF_SUCCESS;
                break;
            }
            else if (!fRc)
            {
                rc = RTErrConvertFromWin32(GetLastError());
                break;
            }

            /* Start a zero byte read operation that we can wait on. */
            if (cMillies == 0)
            {
                rc = VERR_TIMEOUT;
                break;
            }
            AssertBreakStmt(pThis->cRefs == 1, rc = VERR_WRONG_ORDER);
            fRc = ResetEvent(pThis->OverlappedIO.hEvent); Assert(fRc == TRUE);
            DWORD cbRead = 0;
            if (ReadFile(pThis->hNmPipe, pThis->abBuf, 0, &cbRead, &pThis->OverlappedIO))
            {
                rc = VINF_SUCCESS;
                if (iLoop > 10)
                    RTThreadYield();
            }
            else if (GetLastError() == ERROR_IO_PENDING)
            {
                pThis->cRefs++;
                pThis->fIOPending = true;
                pThis->fZeroByteRead = true;
                hWait = pThis->OverlappedIO.hEvent;
            }
            else
                rc = RTErrConvertFromWin32(GetLastError());
        }

        if (RT_FAILURE(rc))
            break;

        /*
         * Check for timeout.
         */
        DWORD cMsMaxWait = INFINITE;
        if (   cMillies != RT_INDEFINITE_WAIT
            && (   hWait != INVALID_HANDLE_VALUE
                || iLoop > 10)
           )
        {
            uint64_t cElapsed = RTTimeMilliTS() - StartMsTS;
            if (cElapsed >= cMillies)
            {
                rc = VERR_TIMEOUT;
                break;
            }
            cMsMaxWait = cMillies - (uint32_t)cElapsed;
        }

        /*
         * Wait.
         */
        if (hWait != INVALID_HANDLE_VALUE)
        {
            RTCritSectLeave(&pThis->CritSect);

            DWORD dwRc = WaitForSingleObject(hWait, cMsMaxWait);
            if (dwRc == WAIT_OBJECT_0)
                rc = VINF_SUCCESS;
            else if (dwRc == WAIT_TIMEOUT)
                rc = VERR_TIMEOUT;
            else if (dwRc == WAIT_ABANDONED)
                rc = VERR_INVALID_HANDLE;
            else
                rc = RTErrConvertFromWin32(GetLastError());

            if (   RT_FAILURE(rc)
                && pThis->u32Magic != RTLOCALIPCSESSION_MAGIC)
                return rc;

            int rc2 = RTCritSectEnter(&pThis->CritSect);
            AssertRC(rc2);
            if (pThis->fZeroByteRead)
            {
                Assert(pThis->cRefs);
                pThis->cRefs--;
                pThis->fIOPending = false;

                if (rc != VINF_SUCCESS)
                {
                    BOOL fRc = CancelIo(pThis->hNmPipe);
                    Assert(fRc == TRUE);
                }

                DWORD cbRead = 0;
                BOOL fRc = GetOverlappedResult(pThis->hNmPipe, &pThis->OverlappedIO, &cbRead, TRUE /*fWait*/);
                if (   !fRc
                    && RT_SUCCESS(rc))
                {
                    DWORD dwRc = GetLastError();
                    if (dwRc == ERROR_OPERATION_ABORTED)
                        rc = VERR_CANCELLED;
                    else
                        rc = RTErrConvertFromWin32(dwRc);
                }
            }

            if (RT_FAILURE(rc))
                break;
        }
    }

    int rc2 = RTCritSectLeave(&pThis->CritSect);
    if (RT_SUCCESS(rc))
        rc = rc2;

    return rc;
}


RTDECL(int) RTLocalIpcSessionCancel(RTLOCALIPCSESSION hSession)
{
    PRTLOCALIPCSESSIONINT pThis = (PRTLOCALIPCSESSIONINT)hSession;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTLOCALIPCSESSION_MAGIC, VERR_INVALID_HANDLE);

    /*
     * Enter the critical section, then set the cancellation flag
     * and signal the event (to wake up anyone in/at WaitForSingleObject).
     */
    int rc = RTCritSectEnter(&pThis->CritSect);
    if (RT_SUCCESS(rc))
    {
        ASMAtomicUoWriteBool(&pThis->fCancelled, true);
        BOOL fRc = SetEvent(pThis->hEvent);
        AssertMsg(fRc, ("%d\n", GetLastError())); NOREF(fRc);

        RTCritSectLeave(&pThis->CritSect);
    }

    return rc;
}


RTDECL(int) RTLocalIpcSessionQueryProcess(RTLOCALIPCSESSION hSession, PRTPROCESS pProcess)
{
    return VERR_NOT_SUPPORTED;
}


RTDECL(int) RTLocalIpcSessionQueryUserId(RTLOCALIPCSESSION hSession, PRTUID pUid)
{
    return VERR_NOT_SUPPORTED;
}


RTDECL(int) RTLocalIpcSessionQueryGroupId(RTLOCALIPCSESSION hSession, PRTUID pUid)
{
    return VERR_NOT_SUPPORTED;
}

