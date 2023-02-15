/** @file
 * TstHGCMMock.h - Mocking framework for testing HGCM-based host services.
 *
 * The goal is to run host service + Vbgl code as unmodified as possible as
 * part of testcases to gain test coverage which otherwise wouldn't possible
 * for heavily user-centric features like Shared Clipboard or drag'n drop (DnD).
 *
 * Currently, though, it's only the service that runs unmodified, the
 * testcases does custom Vbgl work.
 */

/*
 * Copyright (C) 2022-2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL), a copy of it is provided in the "COPYING.CDDL" file included
 * in the VirtualBox distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * SPDX-License-Identifier: GPL-3.0-only OR CDDL-1.0
 */

#ifndef VBOX_INCLUDED_GuestHost_HGCMMock_h
#define VBOX_INCLUDED_GuestHost_HGCMMock_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#ifdef IN_RING3

#include <iprt/list.h>
#include <VBox/VBoxGuestLib.h>
#include <VBox/hgcmsvc.h>


/** Simple call handle structure for the guest call completion callback. */
typedef struct VBOXHGCMCALLHANDLE_TYPEDEF
{
    /** Where to store the result code on call completion. */
    int32_t rc;
} VBOXHGCMCALLHANDLE_TYPEDEF;

/**
 * Enumeration for a HGCM mock function type.
 */
typedef enum TSTHGCMMOCKFNTYPE
{
    TSTHGCMMOCKFNTYPE_NONE = 0,
    TSTHGCMMOCKFNTYPE_CONNECT,
    TSTHGCMMOCKFNTYPE_DISCONNECT,
    TSTHGCMMOCKFNTYPE_CALL,
    TSTHGCMMOCKFNTYPE_HOST_CALL
} TSTHGCMMOCKFNTYPE;

/** Pointer to a mock HGCM service. */
typedef struct TSTHGCMMOCKSVC *PTSTHGCMMOCKSVC;

/**
 * Structure for mocking a server-side HGCM client.
 */
typedef struct TSTHGCMMOCKCLIENT
{
    /** Pointer to to mock service instance this client belongs to. */
    PTSTHGCMMOCKSVC            pSvc;
    /** Assigned HGCM client ID. */
    uint32_t                   idClient;
    /** Opaque pointer to service-specific client data.
     *  Can be NULL if not being used. */
    void                      *pvClient;
    /** Size (in bytes) of \a pvClient. */
    size_t                     cbClient;
    /** The client's current HGCM call handle. */
    VBOXHGCMCALLHANDLE_TYPEDEF hCall;
    /** Whether the current client call has an asynchronous
     *  call pending or not. */
    bool                       fAsyncExec;
    /** Event semaphore to signal call completion. */
    RTSEMEVENT                 hEvent;
} TSTHGCMMOCKCLIENT;
/** Pointer to a mock HGCM client. */
typedef TSTHGCMMOCKCLIENT *PTSTHGCMMOCKCLIENT;

/**
 * Structure for keeping HGCM mock function parameters.
 */
typedef struct TSTHGCMMOCKFN
{
    /** List node for storing this struct into a queue. */
    RTLISTNODE         Node;
    /** Function type. */
    TSTHGCMMOCKFNTYPE  enmType;
    /** Pointer to associated client. */
    PTSTHGCMMOCKCLIENT pClient;
    /** Union keeping function-specific parameters,
     *  depending on \a enmType. */
    union
    {
        struct
        {
            int32_t             iFunc;
            uint32_t            cParms;
            PVBOXHGCMSVCPARM    pParms;
            VBOXHGCMCALLHANDLE  hCall;
        } Call;
        struct
        {
            int32_t             iFunc;
            uint32_t            cParms;
            PVBOXHGCMSVCPARM    pParms;
        } HostCall;
    } u;
} TSTHGCMMOCKFN;
/** Pointer to a HGCM mock function parameters structure. */
typedef TSTHGCMMOCKFN *PTSTHGCMMOCKFN;

/**
 * Structure for keeping a HGCM mock service instance.
 */
typedef struct TSTHGCMMOCKSVC
{
    /** HGCM helper table to use. */
    VBOXHGCMSVCHELPERS fnHelpers;
    /** HGCM service function table to use. */
    VBOXHGCMSVCFNTABLE fnTable;
    /** Next HGCM client ID to assign.
     *  0 is considered as being invalid. */
    HGCMCLIENTID       uNextClientId;
    /** Array of connected HGCM mock clients.
     *  Currently limited to 4 clients maximum. */
    TSTHGCMMOCKCLIENT  aHgcmClient[4];
    /** Thread handle for the service's main loop. */
    RTTHREAD           hThread;
    /** Event semaphore for signalling a message
     *  queue change. */
    RTSEMEVENT         hEventQueue;
    /** Event semaphore for clients connecting to the server. */
    RTSEMEVENT         hEventConnect;
    /** Number of current host calls being served.
     *  Currently limited to one call at a time. */
    uint8_t            cHostCallers;
    /** Result code of last returned host call. */
    int                rcHostCall;
    /** Event semaphore for host calls. */
    RTSEMEVENT         hEventHostCall;
    /** List (queue) of function calls to process. */
    RTLISTANCHOR       lstCall;
    /** Shutdown indicator flag. */
    volatile bool      fShutdown;
} TSTHGCMMOCKSVC;


PTSTHGCMMOCKSVC    TstHgcmMockSvcInst(void);
PTSTHGCMMOCKCLIENT TstHgcmMockSvcWaitForConnectEx(PTSTHGCMMOCKSVC pSvc, RTMSINTERVAL msTimeout);
PTSTHGCMMOCKCLIENT TstHgcmMockSvcWaitForConnect(PTSTHGCMMOCKSVC pSvc);
int                TstHgcmMockSvcCreate(PTSTHGCMMOCKSVC pSvc);
int                TstHgcmMockSvcDestroy(PTSTHGCMMOCKSVC pSvc);
int                TstHgcmMockSvcStart(PTSTHGCMMOCKSVC pSvc);
int                TstHgcmMockSvcStop(PTSTHGCMMOCKSVC pSvc);

int                TstHgcmMockSvcHostCall(PTSTHGCMMOCKSVC pSvc, void *pvService, int32_t function, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);

RT_C_DECLS_BEGIN
DECLCALLBACK(DECLEXPORT(int)) VBoxHGCMSvcLoad(VBOXHGCMSVCFNTABLE *pTable);
RT_C_DECLS_END

# undef VBGLR3DECL
# define VBGLR3DECL(type) DECL_HIDDEN_NOTHROW(type) VBOXCALL
VBGLR3DECL(int)    VbglR3HGCMConnect(const char *pszServiceName, HGCMCLIENTID *pidClient);
VBGLR3DECL(int)    VbglR3HGCMDisconnect(HGCMCLIENTID idClient);
VBGLR3DECL(int)    VbglR3GetSessionId(uint64_t *pu64IdSession);
VBGLR3DECL(int)    VbglR3HGCMCall(PVBGLIOCHGCMCALL pInfo, size_t cbInfo);


#endif /* IN_RING3 */

#endif /* !VBOX_INCLUDED_GuestHost_HGCMMock_h */
