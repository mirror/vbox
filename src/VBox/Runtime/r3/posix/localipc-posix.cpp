/* $Id$ */
/** @file
 * IPRT - Local IPC Server & Client, Posix.
 */

/*
 * Copyright (C) 2006-2013 Oracle Corporation
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
#include "internal/iprt.h"
#include <iprt/localipc.h>

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/ctype.h>
#include <iprt/critsect.h>
#include <iprt/mem.h>
#include <iprt/poll.h>
#include <iprt/socket.h>
#include <iprt/string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <unistd.h>

#include "internal/magics.h"
#include "internal/socket.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Local IPC service instance, POSIX.
 */
typedef struct RTLOCALIPCSERVERINT
{
    /** The magic (RTLOCALIPCSERVER_MAGIC). */
    uint32_t            u32Magic;
    /** The creation flags. */
    uint32_t            fFlags;
    /** Critical section protecting the structure. */
    RTCRITSECT          CritSect;
    /** The number of references to the instance. */
    uint32_t volatile   cRefs;
    /** Indicates that there is a pending cancel request. */
    bool volatile       fCancelled;
    /** The server socket. */
    RTSOCKET            hSocket;
    /** Thread currently listening for clients. */
    RTTHREAD            hListenThread;
    /** The name we bound the server to (native charset encoding). */
    struct sockaddr_un  Name;
} RTLOCALIPCSERVERINT;
/** Pointer to a local IPC server instance (POSIX). */
typedef RTLOCALIPCSERVERINT *PRTLOCALIPCSERVERINT;


/**
 * Local IPC session instance, POSIX.
 */
typedef struct RTLOCALIPCSESSIONINT
{
    /** The magic (RTLOCALIPCSESSION_MAGIC). */
    uint32_t            u32Magic;
    /** Critical section protecting the structure. */
    RTCRITSECT          CritSect;
    /** The number of references to the instance. */
    uint32_t volatile   cRefs;
    /** Indicates that there is a pending cancel request. */
    bool volatile       fCancelled;
    /** Set if this is the server side, clear if the client. */
    bool                fServerSide;
    /** The client socket. */
    RTSOCKET            hSocket;
    /** Thread currently doing read related activites. */
    RTTHREAD            hWriteThread;
    /** Thread currently doing write related activies. */
    RTTHREAD            hReadThread;
} RTLOCALIPCSESSIONINT;
/** Pointer to a local IPC session instance (Windows). */
typedef RTLOCALIPCSESSIONINT *PRTLOCALIPCSESSIONINT;


/** Local IPC name prefix. */
#define RTLOCALIPC_POSIX_NAME_PREFIX    "/tmp/.iprt-localipc-"


/**
 * Validates the user specified name.
 *
 * @returns IPRT status code.
 * @param   pszName             The name to validate.
 * @param   pcchName            Where to return the length.
 */
static int rtLocalIpcPosixValidateName(const char *pszName, size_t *pcchName)
{
    AssertPtrReturn(pszName, VERR_INVALID_POINTER);

    uint32_t cchName = 0;
    for (;;)
    {
        char ch = pszName[cchName];
        if (!ch)
            break;
        AssertReturn(!RT_C_IS_CNTRL(ch), VERR_INVALID_NAME);
        AssertReturn((unsigned)ch < 0x80, VERR_INVALID_NAME);
        AssertReturn(ch != '\\', VERR_INVALID_NAME);
        AssertReturn(ch != '/', VERR_INVALID_NAME);
        cchName++;
    }

    *pcchName = cchName;
    AssertReturn(sizeof(RTLOCALIPC_POSIX_NAME_PREFIX) + cchName <= RT_SIZEOFMEMB(struct sockaddr_un, sun_path),
                 VERR_FILENAME_TOO_LONG);
    AssertReturn(cchName, VERR_INVALID_NAME);

    return VINF_SUCCESS;
}


/**
 * Constructs a local (unix) domain socket name.
 *
 * @returns IPRT status code.
 * @param   pAddr               The address structure to construct the name in.
 * @param   pcbAddr             Where to return the address size.
 * @param   pszName             The user specified name (valid).
 * @param   cchName             The user specified name length.
 */
static int rtLocalIpcPosixConstructName(struct sockaddr_un *pAddr, uint8_t *pcbAddr, const char *pszName, size_t cchName)
{
    AssertMsgReturn(cchName + sizeof(RTLOCALIPC_POSIX_NAME_PREFIX) <= sizeof(pAddr->sun_path),
                    ("cchName=%zu sizeof(sun_path)=%zu\n", cchName, sizeof(pAddr->sun_path)),
                    VERR_FILENAME_TOO_LONG);

/** @todo Bother converting to local codeset/encoding??  */

    RT_ZERO(*pAddr);
#ifdef RT_OS_OS2 /* Size must be exactly right on OS/2. */
    *pcbAddr = sizeof(*pAddr);
#else
    *pcbAddr = RT_OFFSETOF(struct sockaddr_un, sun_path) + (uint8_t)cchName + sizeof(RTLOCALIPC_POSIX_NAME_PREFIX);
#endif
#ifdef HAVE_SUN_LEN_MEMBER
    pAddr->sun_len     = *pcbAddr;
#endif
    pAddr->sun_family  = AF_LOCAL;
    memcpy(pAddr->sun_path, RTLOCALIPC_POSIX_NAME_PREFIX, sizeof(RTLOCALIPC_POSIX_NAME_PREFIX) - 1);
    memcpy(&pAddr->sun_path[sizeof(RTLOCALIPC_POSIX_NAME_PREFIX) - 1], pszName, cchName + 1);

    return VINF_SUCCESS;
}



RTDECL(int) RTLocalIpcServerCreate(PRTLOCALIPCSERVER phServer, const char *pszName, uint32_t fFlags)
{
    /*
     * Parameter validation.
     */
    AssertPtrReturn(phServer, VERR_INVALID_POINTER);
    *phServer = NIL_RTLOCALIPCSERVER;

    AssertReturn(!(fFlags & ~RTLOCALIPC_FLAGS_VALID_MASK), VERR_INVALID_FLAGS);

    size_t cchName;
    int rc = rtLocalIpcPosixValidateName(pszName, &cchName);
    if (RT_SUCCESS(rc))
    {
        /*
         * Allocate memory for the instance and initialize it.
         */
        PRTLOCALIPCSERVERINT pThis = (PRTLOCALIPCSERVERINT)RTMemAllocZ(sizeof(*pThis));
        if (pThis)
        {
            pThis->u32Magic      = RTLOCALIPCSERVER_MAGIC;
            pThis->fFlags        = fFlags;
            pThis->cRefs         = 1;
            pThis->fCancelled    = false;
            pThis->hListenThread = NIL_RTTHREAD;
            rc = RTCritSectInit(&pThis->CritSect);
            if (RT_SUCCESS(rc))
            {
                /*
                 * Create the local (unix) socket and bind to it.
                 */
                rc = rtSocketCreate(&pThis->hSocket, AF_LOCAL, SOCK_STREAM, 0 /*iProtocol*/);
                if (RT_SUCCESS(rc))
                {
                    RTSocketSetInheritance(pThis->hSocket, false /*fInheritable*/);

                    uint8_t cbAddr;
                    rc = rtLocalIpcPosixConstructName(&pThis->Name, &cbAddr, pszName, cchName);
                    if (RT_SUCCESS(rc))
                    {
                        rc = rtSocketBindRawAddr(pThis->hSocket, &pThis->Name, cbAddr);
                        if (rc == VERR_NET_ADDRESS_IN_USE)
                        {
                            unlink(pThis->Name.sun_path);
                            rc = rtSocketBindRawAddr(pThis->hSocket, &pThis->Name, cbAddr);
                        }
                        if (RT_SUCCESS(rc))
                        {
                            *phServer = pThis;
                            return VINF_SUCCESS;
                        }
                    }
                    RTSocketRelease(pThis->hSocket);
                }
                RTCritSectDelete(&pThis->CritSect);
            }
            RTMemFree(pThis);
        }
        else
            rc = VERR_NO_MEMORY;
    }
    return rc;
}


/**
 * Retains a reference to the server instance.
 *
 * @returns
 * @param   pThis               The server instance.
 */
DECLINLINE(void) rtLocalIpcServerRetain(PRTLOCALIPCSERVERINT pThis)
{
    uint32_t cRefs = ASMAtomicIncU32(&pThis->cRefs);
    Assert(cRefs < UINT32_MAX / 2 && cRefs);
}


/**
 * Server instance destructor.
 * @param   pThis               The server instance.
 */
static void rtLocalIpcServerDtor(PRTLOCALIPCSERVERINT pThis)
{
    pThis->u32Magic = ~RTLOCALIPCSERVER_MAGIC;
    RTSocketRelease(pThis->hSocket);
    RTCritSectDelete(&pThis->CritSect);
    unlink(pThis->Name.sun_path);
    RTMemFree(pThis);
}


/**
 * Releases a reference to the server instance.
 *
 * @param   pThis               The server instance.
 */
DECLINLINE(void) rtLocalIpcServerRelease(PRTLOCALIPCSERVERINT pThis)
{
    uint32_t cRefs = ASMAtomicDecU32(&pThis->cRefs);
    Assert(cRefs < UINT32_MAX / 2);
    if (!cRefs)
        rtLocalIpcServerDtor(pThis);
}


/**
 * The core of RTLocalIpcServerCancel, used by both the destroy and cancel APIs.
 *
 * @returns IPRT status code
 * @param   pThis               The server instance.
 */
static int rtLocalIpcServerCancel(PRTLOCALIPCSERVERINT pThis)
{
    RTCritSectEnter(&pThis->CritSect);
    pThis->fCancelled = true;
    if (pThis->hListenThread != NIL_RTTHREAD)
        RTThreadPoke(pThis->hListenThread);
    RTCritSectLeave(&pThis->CritSect);
    return VINF_SUCCESS;
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
    AssertReturn(pThis->u32Magic == RTLOCALIPCSERVER_MAGIC, VERR_INVALID_HANDLE);

    /*
     * Invalidate the server, releasing the caller's reference to the instance
     * data and making sure any other thread in the listen API will wake up.
     */
    AssertReturn(ASMAtomicCmpXchgU32(&pThis->u32Magic, ~RTLOCALIPCSERVER_MAGIC, RTLOCALIPCSERVER_MAGIC), VERR_WRONG_ORDER);

    rtLocalIpcServerCancel(pThis);
    rtLocalIpcServerRelease(pThis);

    return VINF_SUCCESS;
}


RTDECL(int) RTLocalIpcServerCancel(RTLOCALIPCSERVER hServer)
{
    /*
     * Validate input.
     */
    PRTLOCALIPCSERVERINT pThis = (PRTLOCALIPCSERVERINT)hServer;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTLOCALIPCSERVER_MAGIC, VERR_INVALID_HANDLE);

    /*
     * Do the job.
     */
    rtLocalIpcServerRetain(pThis);
    rtLocalIpcServerCancel(pThis);
    rtLocalIpcServerRelease(pThis);
    return VINF_SUCCESS;
}


RTDECL(int) RTLocalIpcServerListen(RTLOCALIPCSERVER hServer, PRTLOCALIPCSESSION phClientSession)
{
    /*
     * Validate input.
     */
    PRTLOCALIPCSERVERINT pThis = (PRTLOCALIPCSERVERINT)hServer;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTLOCALIPCSERVER_MAGIC, VERR_INVALID_HANDLE);

    /*
     * Begin listening.
     */
    rtLocalIpcServerRetain(pThis);
    int rc = RTCritSectEnter(&pThis->CritSect);
    if (RT_SUCCESS(rc))
    {
        if (pThis->hListenThread == NIL_RTTHREAD)
        {
            pThis->hListenThread = RTThreadSelf();

            /*
             * The listening retry loop.
             */
            for (;;)
            {
                if (pThis->fCancelled)
                {
                    rc = VERR_CANCELLED;
                    break;
                }

                rc = RTCritSectLeave(&pThis->CritSect);
                AssertRCBreak(rc);

                rc = rtSocketListen(pThis->hSocket, pThis->fFlags & RTLOCALIPC_FLAGS_MULTI_SESSION ? 10 : 0);
                if (RT_SUCCESS(rc))
                {
                    struct sockaddr_un  Addr;
                    size_t              cbAddr = sizeof(Addr);
                    RTSOCKET            hClient;
                    rc = rtSocketAccept(pThis->hSocket, &hClient, (struct sockaddr *)&Addr, &cbAddr);

                    int rc2 = RTCritSectEnter(&pThis->CritSect);
                    AssertRCBreakStmt(rc2, rc = RT_SUCCESS(rc) ? rc2 : rc);

                    if (RT_SUCCESS(rc))
                    {
                        /*
                         * Create a client session.
                         */
                        PRTLOCALIPCSESSIONINT pSession = (PRTLOCALIPCSESSIONINT)RTMemAllocZ(sizeof(*pSession));
                        if (pSession)
                        {
                            pSession->u32Magic      = RTLOCALIPCSESSION_MAGIC;
                            pSession->cRefs         = 1;
                            pSession->fCancelled    = false;
                            pSession->fServerSide   = true;
                            pSession->hSocket       = hClient;
                            pSession->hReadThread   = NIL_RTTHREAD;
                            pSession->hWriteThread  = NIL_RTTHREAD;
                            rc = RTCritSectInit(&pSession->CritSect);
                            if (RT_SUCCESS(rc))
                                *phClientSession = pSession;
                            else
                                RTMemFree(pSession);
                        }
                        else
                            rc = VERR_NO_MEMORY;
                    }
                    else if (   rc != VERR_INTERRUPTED
                             && rc != VERR_TRY_AGAIN)
                        break;
                }
                else
                {
                    int rc2 = RTCritSectEnter(&pThis->CritSect);
                    AssertRCBreakStmt(rc2, rc = RT_SUCCESS(rc) ? rc2 : rc);
                    if (   rc != VERR_INTERRUPTED
                        && rc != VERR_TRY_AGAIN)
                        break;
                }
            }

            pThis->hListenThread = NIL_RTTHREAD;
        }
        else
        {
            AssertFailed();
            rc = VERR_RESOURCE_BUSY;
        }
        int rc2 = RTCritSectLeave(&pThis->CritSect);
        AssertStmt(RT_SUCCESS(rc2), rc = RT_SUCCESS(rc) ? rc2 : rc);
    }
    rtLocalIpcServerRelease(pThis);

    return rc;
}


RTDECL(int) RTLocalIpcSessionConnect(PRTLOCALIPCSESSION phSession, const char *pszName, uint32_t fFlags)
{
    /*
     * Parameter validation.
     */
    AssertPtrReturn(phSession, VERR_INVALID_POINTER);
    *phSession = NIL_RTLOCALIPCSESSION;

    AssertReturn(!fFlags, VERR_INVALID_FLAGS);

    size_t cchName;
    int rc = rtLocalIpcPosixValidateName(pszName, &cchName);
    if (RT_SUCCESS(rc))
    {
        /*
         * Allocate memory for the instance and initialize it.
         */
        PRTLOCALIPCSESSIONINT pThis = (PRTLOCALIPCSESSIONINT)RTMemAllocZ(sizeof(*pThis));
        if (pThis)
        {
            pThis->u32Magic         = RTLOCALIPCSESSION_MAGIC;
            pThis->cRefs            = 1;
            pThis->fCancelled       = false;
            pThis->fServerSide      = false;
            pThis->hSocket          = NIL_RTSOCKET;
            pThis->hReadThread      = NIL_RTTHREAD;
            pThis->hWriteThread     = NIL_RTTHREAD;
            rc = RTCritSectInit(&pThis->CritSect);
            if (RT_SUCCESS(rc))
            {
                /*
                 * Create the local (unix) socket and try connect to the server.
                 */
                rc = rtSocketCreate(&pThis->hSocket, AF_LOCAL, SOCK_STREAM, 0 /*iProtocol*/);
                if (RT_SUCCESS(rc))
                {
                    RTSocketSetInheritance(pThis->hSocket, false /*fInheritable*/);

                    struct sockaddr_un  Addr;
                    uint8_t             cbAddr;
                    rc = rtLocalIpcPosixConstructName(&Addr, &cbAddr, pszName, cchName);
                    if (RT_SUCCESS(rc))
                    {
                        rc = rtSocketConnectRaw(pThis->hSocket, &Addr, cbAddr);
                        if (RT_SUCCESS(rc))
                        {
                            *phSession = pThis;
                            return VINF_SUCCESS;
                        }
                    }
                    RTCritSectDelete(&pThis->CritSect);
                }
            }
            RTMemFree(pThis);
        }
        else
            rc = VERR_NO_MEMORY;
    }
    return rc;
}


/**
 * Retains a reference to the session instance.
 *
 * @param   pThis               The server instance.
 */
DECLINLINE(void) rtLocalIpcSessionRetain(PRTLOCALIPCSESSIONINT pThis)
{
    uint32_t cRefs = ASMAtomicIncU32(&pThis->cRefs);
    Assert(cRefs < UINT32_MAX / 2 && cRefs);
}


/**
 * Session instance destructor.
 * @param   pThis               The server instance.
 */
static void rtLocalIpcSessionDtor(PRTLOCALIPCSESSIONINT pThis)
{
    pThis->u32Magic = ~RTLOCALIPCSESSION_MAGIC;
    if (RTSocketRelease(pThis->hSocket) == 0)
        pThis->hSocket = NIL_RTSOCKET;
    RTCritSectDelete(&pThis->CritSect);
    RTMemFree(pThis);
}


/**
 * Releases a reference to the session instance.
 *
 * @param   pThis               The session instance.
 */
DECLINLINE(void) rtLocalIpcSessionRelease(PRTLOCALIPCSESSIONINT pThis)
{
    uint32_t cRefs = ASMAtomicDecU32(&pThis->cRefs);
    Assert(cRefs < UINT32_MAX / 2);
    if (!cRefs)
        rtLocalIpcSessionDtor(pThis);
}


/**
 * The core of RTLocalIpcSessionCancel, used by both the destroy and cancel APIs.
 *
 * @returns IPRT status code
 * @param   pThis               The session instance.
 */
static int rtLocalIpcSessionCancel(PRTLOCALIPCSESSIONINT pThis)
{
    RTCritSectEnter(&pThis->CritSect);
    pThis->fCancelled = true;
    if (pThis->hReadThread != NIL_RTTHREAD)
        RTThreadPoke(pThis->hReadThread);
    if (pThis->hWriteThread != NIL_RTTHREAD)
        RTThreadPoke(pThis->hWriteThread);
    RTCritSectLeave(&pThis->CritSect);
    return VINF_SUCCESS;
}


RTDECL(int) RTLocalIpcSessionClose(RTLOCALIPCSESSION hSession)
{
    /*
     * Validate input.
     */
    if (hSession == NIL_RTLOCALIPCSESSION)
        return VINF_SUCCESS;
    PRTLOCALIPCSESSIONINT pThis = hSession;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTLOCALIPCSESSION_MAGIC, VERR_INVALID_HANDLE);

    /*
     * Invalidate the session, releasing the caller's reference to the instance
     * data and making sure any other thread in the listen API will wake up.
     */
    AssertReturn(ASMAtomicCmpXchgU32(&pThis->u32Magic, ~RTLOCALIPCSESSION_MAGIC, RTLOCALIPCSESSION_MAGIC), VERR_WRONG_ORDER);

    rtLocalIpcSessionCancel(pThis);
    rtLocalIpcSessionRelease(pThis);

    return VINF_SUCCESS;
}


RTDECL(int) RTLocalIpcSessionCancel(RTLOCALIPCSESSION hSession)
{
    /*
     * Validate input.
     */
    PRTLOCALIPCSESSIONINT pThis = hSession;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTLOCALIPCSESSION_MAGIC, VERR_INVALID_HANDLE);

    /*
     * Do the job.
     */
    rtLocalIpcSessionRetain(pThis);
    rtLocalIpcSessionCancel(pThis);
    rtLocalIpcSessionRelease(pThis);
    return VINF_SUCCESS;
}


RTDECL(int) RTLocalIpcSessionRead(RTLOCALIPCSESSION hSession, void *pvBuffer, size_t cbBuffer, size_t *pcbRead)
{
    /*
     * Validate input.
     */
    PRTLOCALIPCSESSIONINT pThis = hSession;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTLOCALIPCSESSION_MAGIC, VERR_INVALID_HANDLE);

    /*
     * Do the job.
     */
    rtLocalIpcSessionRetain(pThis);

    int rc = RTCritSectEnter(&pThis->CritSect);
    if (RT_SUCCESS(rc))
    {
        if (pThis->hReadThread == NIL_RTTHREAD)
        {
            pThis->hReadThread = RTThreadSelf();

            for (;;)
            {
                if (!pThis->fCancelled)
                {
                    rc = RTCritSectLeave(&pThis->CritSect);
                    AssertRCBreak(rc);

                    rc = RTSocketRead(pThis->hSocket, pvBuffer, cbBuffer, pcbRead);

                    int rc2 = RTCritSectEnter(&pThis->CritSect);
                    AssertRCBreakStmt(rc2, rc = RT_SUCCESS(rc) ? rc2 : rc);

                    if (   rc == VERR_INTERRUPTED
                        || rc == VERR_TRY_AGAIN)
                        continue;
                }
                else
                    rc = VERR_CANCELLED;
                break;
            }

            pThis->hReadThread = NIL_RTTHREAD;
        }
        int rc2 = RTCritSectLeave(&pThis->CritSect);
        AssertStmt(RT_SUCCESS(rc2), rc = RT_SUCCESS(rc) ? rc2 : rc);
    }

    rtLocalIpcSessionRelease(pThis);
    return rc;
}


RTDECL(int) RTLocalIpcSessionWrite(RTLOCALIPCSESSION hSession, const void *pvBuffer, size_t cbBuffer)
{
    /*
     * Validate input.
     */
    PRTLOCALIPCSESSIONINT pThis = hSession;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTLOCALIPCSESSION_MAGIC, VERR_INVALID_HANDLE);

    /*
     * Do the job.
     */
    rtLocalIpcSessionRetain(pThis);

    int rc = RTCritSectEnter(&pThis->CritSect);
    if (RT_SUCCESS(rc))
    {
        if (pThis->hWriteThread == NIL_RTTHREAD)
        {
            pThis->hWriteThread = RTThreadSelf();

            for (;;)
            {
                if (!pThis->fCancelled)
                {
                    rc = RTCritSectLeave(&pThis->CritSect);
                    AssertRCBreak(rc);

                    rc = RTSocketWrite(pThis->hSocket, pvBuffer, cbBuffer);

                    int rc2 = RTCritSectEnter(&pThis->CritSect);
                    AssertRCBreakStmt(rc2, rc = RT_SUCCESS(rc) ? rc2 : rc);

                    if (   rc == VERR_INTERRUPTED
                        || rc == VERR_TRY_AGAIN)
                        continue;
                }
                else
                    rc = VERR_CANCELLED;
                break;
            }

            pThis->hWriteThread = NIL_RTTHREAD;
        }
        int rc2 = RTCritSectLeave(&pThis->CritSect);
        AssertStmt(RT_SUCCESS(rc2), rc = RT_SUCCESS(rc) ? rc2 : rc);
    }

    rtLocalIpcSessionRelease(pThis);
    return rc;
}


RTDECL(int) RTLocalIpcSessionFlush(RTLOCALIPCSESSION hSession)
{
    /*
     * Validate input.
     */
    PRTLOCALIPCSESSIONINT pThis = hSession;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTLOCALIPCSESSION_MAGIC, VERR_INVALID_HANDLE);

    /*
     * This is a no-op because apparently write doesn't return until the
     * result is read.  At least that's what the reply to a 2003-04-08 LKML
     * posting title "fsync() on unix domain sockets?" indicates.
     *
     * For conformity, make sure there isn't any active writes concurrent to this call.
     */
    rtLocalIpcSessionRetain(pThis);

    int rc = RTCritSectEnter(&pThis->CritSect);
    if (RT_SUCCESS(rc))
    {
        if (pThis->hWriteThread == NIL_RTTHREAD)
            rc = RTCritSectLeave(&pThis->CritSect);
        else
        {
            rc = RTCritSectLeave(&pThis->CritSect);
            if (RT_SUCCESS(rc))
                rc = VERR_RESOURCE_BUSY;
        }
    }

    rtLocalIpcSessionRelease(pThis);
    return rc;
}


RTDECL(int) RTLocalIpcSessionWaitForData(RTLOCALIPCSESSION hSession, uint32_t cMillies)
{
    /*
     * Validate input.
     */
    PRTLOCALIPCSESSIONINT pThis = hSession;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTLOCALIPCSESSION_MAGIC, VERR_INVALID_HANDLE);

    /*
     * Do the job.
     */
    rtLocalIpcSessionRetain(pThis);

    int rc = RTCritSectEnter(&pThis->CritSect);
    if (RT_SUCCESS(rc))
    {
        if (pThis->hReadThread == NIL_RTTHREAD)
        {
            pThis->hReadThread = RTThreadSelf();

            for (;;)
            {
                if (!pThis->fCancelled)
                {
                    rc = RTCritSectLeave(&pThis->CritSect);
                    AssertRCBreak(rc);

                    uint32_t fEvents = 0;
                    rc = RTSocketSelectOneEx(pThis->hSocket, RTPOLL_EVT_READ | RTPOLL_EVT_ERROR, &fEvents, cMillies);

                    int rc2 = RTCritSectEnter(&pThis->CritSect);
                    AssertRCBreakStmt(rc2, rc = RT_SUCCESS(rc) ? rc2 : rc);

                    if (RT_SUCCESS(rc))
                    {
                        if (pThis->fCancelled)
                            rc = VERR_CANCELLED;
                        else if (fEvents & RTPOLL_EVT_ERROR)
                            rc = VERR_BROKEN_PIPE;
                    }
                    else if (   rc == VERR_INTERRUPTED
                             || rc == VERR_TRY_AGAIN)
                        continue;
                }
                else
                    rc = VERR_CANCELLED;
                break;
            }

            pThis->hReadThread = NIL_RTTHREAD;
        }
        int rc2 = RTCritSectLeave(&pThis->CritSect);
        AssertStmt(RT_SUCCESS(rc2), rc = RT_SUCCESS(rc) ? rc2 : rc);
    }

    rtLocalIpcSessionRelease(pThis);
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


RTDECL(int) RTLocalIpcSessionQueryGroupId(RTLOCALIPCSESSION hSession, PRTGID pGid)
{
    return VERR_NOT_SUPPORTED;
}



