/* $Id$ */
/** @file
 * IPRT - Local IPC, Windows Implementation Using Named Pipes.
 */

/*
 * Copyright (C) 2010 Oracle Corporation
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

#include <iprt/localipc.h>
#include <iprt/thread.h>
#include <iprt/critsect.h>
#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/param.h>
#include <iprt/err.h>
#include <iprt/string.h>
#include <iprt/asm.h>

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
    uint32_t u32Magic;
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
} RTLOCALIPCSESSIONINT;
/** Pointer to a local IPC session instance (Windows). */
typedef RTLOCALIPCSESSIONINT *PRTLOCALIPCSESSIONINT;


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static int rtLocalIpcWinCreateSession(PRTLOCALIPCSESSION phClientSession, HANDLE hNmPipeSession);


/**
 * Creates a named pipe instance.
 *
 * This is used by both RTLocalIpcServerCreate and RTLocalIpcServerListen.
 *
 * @returns Windows error code, that is NO_ERROR and *phNmPipe on success and some ERROR_* on failure.
 *
 * @param   phNmPipe            Where to store the named pipe handle on success. This
 *                              will be set to INVALID_HANDLE_VALUE on failure.
 * @param   pszFullPipeName     The full named pipe name.
 * @param   fFirst              Set on the first call (from RTLocalIpcServerCreate), otherwise clear.
 *                              Governs the FILE_FLAG_FIRST_PIPE_INSTANCE flag.
 */
static DWORD rtLocalIpcServerWinCreatePipeInstance(PHANDLE phNmPipe, const char *pszFullPipeName, bool fFirst)
{
    *phNmPipe = INVALID_HANDLE_VALUE;

    /*
     * We'll create a security descriptor from a SDDL that denies
     * access to network clients (this is local IPC after all), it
     * makes some further restrictions to prevent non-authenticated
     * users from screwing around.
     */
    DWORD err;
    PSECURITY_DESCRIPTOR pSecDesc = NULL;
#if 0 /** @todo dynamically resolve this as it is the only thing that prevents
       * loading IPRT on NT4. */
    if (ConvertStringSecurityDescriptorToSecurityDescriptor(RTLOCALIPC_WIN_SDDL,
                                                            SDDL_REVISION_1,
                                                            &pSecDesc,
                                                            NULL))
#else
    AssertFatalFailed();
    SetLastError(-1);
    if (0)
#endif
    {
        SECURITY_ATTRIBUTES SecAttrs;
        SecAttrs.nLength = sizeof(SecAttrs);
        SecAttrs.lpSecurityDescriptor = pSecDesc;
        SecAttrs.bInheritHandle = FALSE;

        DWORD fOpenMode = PIPE_ACCESS_DUPLEX
                        | PIPE_WAIT
                        | FILE_FLAG_OVERLAPPED;
        if (fFirst)
            fOpenMode |= FILE_FLAG_FIRST_PIPE_INSTANCE; /* Note! Requires W2K SP2+. */

        HANDLE hNmPipe = CreateNamedPipe(pszFullPipeName,               /* lpName */
                                         fOpenMode,                     /* dwOpenMode */
                                         PIPE_TYPE_BYTE,                /* dwPipeMode */
                                         PIPE_UNLIMITED_INSTANCES,      /* nMaxInstances */
                                         PAGE_SIZE,                     /* nOutBufferSize (advisory) */
                                         PAGE_SIZE,                     /* nInBufferSize (ditto) */
                                         30*1000,                       /* nDefaultTimeOut = 30 sec */
                                         &SecAttrs);                    /* lpSecurityAttributes */
        err = GetLastError();
        LocalFree(pSecDesc);
        if (hNmPipe != INVALID_HANDLE_VALUE)
        {
            *phNmPipe = hNmPipe;
            return NO_ERROR;
        }
    }
    else
        err = GetLastError();

    AssertReturn(err != NO_ERROR, ERROR_GEN_FAILURE);
    return err;
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

    AssertReturn(fFlags & RTLOCALIPC_FLAGS_MULTI_SESSION, VERR_NOT_IMPLEMENTED); /** @todo implement !RTLOCALIPC_FLAGS_MULTI_SESSION */

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
        DWORD err = NO_ERROR;
        pThis->hEvent = CreateEvent(NULL /*lpEventAttributes*/, TRUE /*bManualReset*/, FALSE /*bInitialState*/, NULL /*lpName*/);
        if (pThis->hEvent != NULL)
        {
            memset(&pThis->OverlappedIO, 0, sizeof(pThis->OverlappedIO));
            pThis->OverlappedIO.Internal = STATUS_PENDING;
            pThis->OverlappedIO.hEvent = pThis->hEvent;

            err = rtLocalIpcServerWinCreatePipeInstance(&pThis->hNmPipe, pThis->szName, true /* fFirst */);
            if (err == NO_ERROR)
            {
                *phServer = pThis;
                return VINF_SUCCESS;
            }
        }
        else
            err = GetLastError();
        rc = RTErrConvertFromWin32(err);
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
    pThis->cRefs--;

    if (pThis->cRefs > 0)
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
         * of the cancellation a by RTLocalIpcServerCancel and RTLocalIpcServerDestroy.
         */
        SetLastError(NO_ERROR);
        BOOL fRc = ConnectNamedPipe(pThis->hNmPipe, &pThis->OverlappedIO);
        DWORD err = fRc ? NO_ERROR : GetLastError();
        if (    !fRc
            &&  err == ERROR_IO_PENDING)
        {
            WaitForSingleObject(pThis->hEvent, INFINITE);
            DWORD dwIgnored;
            fRc = GetOverlappedResult(pThis->hNmPipe, &pThis->OverlappedIO, &dwIgnored, FALSE /* bWait*/);
            err = fRc ? NO_ERROR : GetLastError();
        }

        RTCritSectEnter(&pThis->CritSect);
        if (    !pThis->fCancelled
            &&  pThis->u32Magic == RTLOCALIPCSERVER_MAGIC)
        {
            /*
             * Still alive, some error or an actual client.
             *
             * If it's the latter we'll have to create a new pipe instance that
             * replaces the current one for the server. The current pipe instance
             * will be assigned to the client session.
             */
            if (    fRc
                ||  err == ERROR_PIPE_CONNECTED)
            {
                HANDLE hNmPipe;
                DWORD err = rtLocalIpcServerWinCreatePipeInstance(&hNmPipe, pThis->szName, false /* fFirst */);
                if (err == NO_ERROR)
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
                    rc = RTErrConvertFromWin32(err);
                    fRc = DisconnectNamedPipe(pThis->hNmPipe);
                    AssertMsg(fRc, ("%d\n", GetLastError()));
                }
            }
            else
                rc = RTErrConvertFromWin32(err);
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
                ||  err == ERROR_PIPE_CONNECTED)
                fRc = DisconnectNamedPipe(pThis->hNmPipe);
            else if (err == ERROR_IO_PENDING)
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
    RTCritSectEnter(&pThis->CritSect);

    ASMAtomicUoWriteBool(&pThis->fCancelled, true);
    BOOL fRc = SetEvent(pThis->hEvent);
    AssertMsg(fRc, ("%d\n", GetLastError())); NOREF(fRc);

    RTCritSectLeave(&pThis->CritSect);

    return VINF_SUCCESS;
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
        pThis->hNmPipe = hNmPipeSession;

        rc = RTCritSectInit(&pThis->CritSect);
        if (RT_SUCCESS(rc))
        {
            pThis->hEvent = CreateEvent(NULL /*lpEventAttributes*/, TRUE /*bManualReset*/, FALSE /*bInitialState*/, NULL /*lpName*/);
            if (pThis->hEvent != NULL)
            {
                memset(&pThis->OverlappedIO, 0, sizeof(pThis->OverlappedIO));
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
    return VINF_SUCCESS;
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
    PRTLOCALIPCSESSIONINT pThis = (RTLOCALIPCSESSION)hSession;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic != RTLOCALIPCSESSION_MAGIC, VERR_INVALID_MAGIC);

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
    return VINF_SUCCESS;
}


RTDECL(int) RTLocalIpcSessionWrite(RTLOCALIPCSESSION hSession, const void *pvBuffer, size_t cbBuffer)
{
    return VINF_SUCCESS;
}


RTDECL(int) RTLocalIpcSessionFlush(RTLOCALIPCSESSION hSession)
{
    return VINF_SUCCESS;
}


RTDECL(int) RTLocalIpcSessionWaitForData(RTLOCALIPCSESSION hSession, uint32_t cMillies)
{
    RTThreadSleep(1000);
    return VINF_SUCCESS;
}


RTDECL(int) RTLocalIpcSessionCancel(RTLOCALIPCSESSION hSession)
{
    return VINF_SUCCESS;
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

