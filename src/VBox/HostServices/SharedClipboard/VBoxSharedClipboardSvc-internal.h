/* $Id$ */
/** @file
 * Shared Clipboard Service - Internal header.
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VBOX_INCLUDED_SRC_SharedClipboard_VBoxSharedClipboardSvc_internal_h
#define VBOX_INCLUDED_SRC_SharedClipboard_VBoxSharedClipboardSvc_internal_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <algorithm>
#include <list>
#include <map>

#include <iprt/list.h>
#include <iprt/cpp/list.h> /* For RTCList. */

#include <VBox/hgcmsvc.h>
#include <VBox/log.h>

#include <VBox/HostServices/Service.h>
#include <VBox/GuestHost/SharedClipboard.h>
#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
# include <iprt/semaphore.h>
# include <VBox/GuestHost/SharedClipboard-uri.h>
#endif

using namespace HGCM;

#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
/**
 * Structure for handling a single URI object context.
 */
typedef struct _VBOXCLIPBOARDCLIENTURIOBJCTX
{
    /** Pointer to current object being processed. */
    SharedClipboardURIObject      *pObj;
} VBOXCLIPBOARDCLIENTURIOBJCTX, *PVBOXCLIPBOARDCLIENTURIOBJCTX;

struct VBOXCLIPBOARDCLIENTSTATE;
#endif /* VBOX_WITH_SHARED_CLIPBOARD_URI_LIST */

typedef struct _VBOXCLIPBOARDCLIENTMSG
{
    /** Stored message type. */
    uint32_t         m_uMsg;
    /** Number of stored HGCM parameters. */
    uint32_t         m_cParms;
    /** Stored HGCM parameters. */
    PVBOXHGCMSVCPARM m_paParms;
} VBOXCLIPBOARDCLIENTMSG, *PVBOXCLIPBOARDCLIENTMSG;

#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
typedef struct VBOXCLIPBOARDCLIENTURISTATE
{
    /** Whether to start a new transfer. */
    bool                          fTransferStart;
    /** Directory of the transfer to start. */
    SHAREDCLIPBOARDURITRANSFERDIR enmTransferDir;
} VBOXCLIPBOARDCLIENTURISTATE, *PVBOXCLIPBOARDCLIENTURISTATE;
#endif /* VBOX_WITH_SHARED_CLIPBOARD_URI_LIST */

/**
 * Structure for keeping generic client state data within the Shared Clipboard host service.
 * This structure needs to be serializable by SSM.
 */
typedef struct VBOXCLIPBOARDCLIENTSTATE
{
    struct VBOXCLIPBOARDCLIENTSTATE *pNext;
    struct VBOXCLIPBOARDCLIENTSTATE *pPrev;

    VBOXCLIPBOARDCONTEXT *pCtx;

    uint32_t u32ClientID;

    SHAREDCLIPBOARDSOURCE enmSource;

    /** The guest is waiting for a message. */
    bool fAsync;
    /** The guest is waiting for data from the host */
    bool fReadPending;
    /** Whether the host host has sent a quit message. */
    bool fHostMsgQuit;
    /** Whether the host host has requested reading clipboard data from the guest. */
    bool fHostMsgReadData;
    /** Whether the host host has reported its available formats. */
    bool fHostMsgFormats;

#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
    /** The client's URI state. */
    VBOXCLIPBOARDCLIENTURISTATE URI;
#endif

    struct {
        VBOXHGCMCALLHANDLE callHandle;
        uint32_t           cParms;
        VBOXHGCMSVCPARM   *paParms;
    } async;

    struct {
        VBOXHGCMCALLHANDLE callHandle;
        uint32_t           cParms;
        VBOXHGCMSVCPARM   *paParms;
    } asyncRead;

    struct {
         void *pv;
         uint32_t cb;
         uint32_t u32Format;
    } data;

    uint32_t u32AvailableFormats;
    uint32_t u32RequestedFormat;

    /** The client's message queue (FIFO). */
    RTCList<VBOXCLIPBOARDCLIENTMSG *> queueMsg;
} VBOXCLIPBOARDCLIENTSTATE, *PVBOXCLIPBOARDCLIENTSTATE;

/**
 * Structure for keeping a HGCM client state within the Shared Clipboard host service.
 */
typedef struct _VBOXCLIPBOARDCLIENTDATA
{
    /** General client state data. */
    VBOXCLIPBOARDCLIENTSTATE       State;
#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
    /** URI context data. */
    SHAREDCLIPBOARDURICTX          URI;
#endif
} VBOXCLIPBOARDCLIENTDATA, *PVBOXCLIPBOARDCLIENTDATA;

typedef struct _VBOXCLIPBOARDCLIENT
{
    /** The client's HGCM client ID. */
    uint32_t                 uClientID;
    /** Pointer to the client'data, owned by HGCM. */
    PVBOXCLIPBOARDCLIENTDATA pData;
    /** Optional protocol version the client uses. Set to 0 by default. */
    uint32_t                 uProtocolVer;
    /** Flag indicating whether this client currently is deferred mode,
     *  meaning that it did not return to the caller yet. */
    bool                     fDeferred;
    /** Structure for keeping the client's deferred state.
     *  A client is in a deferred state when it asks for the next HGCM message,
     *  but the service can't provide it yet. That way a client will block (on the guest side, does not return)
     *  until the service can complete the call. */
    struct
    {
        /** The client's HGCM call handle. Needed for completing a deferred call. */
        VBOXHGCMCALLHANDLE hHandle;
        /** Message type (function number) to use when completing the deferred call. */
        uint32_t           uType;
        /** Parameter count to use when completing the deferred call. */
        uint32_t           cParms;
        /** Parameters to use when completing the deferred call. */
        PVBOXHGCMSVCPARM   paParms;
    } Deferred;
} VBOXCLIPBOARDCLIENT, *PVBOXCLIPBOARDCLIENT;

/** Map holding pointers to drag and drop clients. Key is the (unique) HGCM client ID. */
typedef std::map<uint32_t, VBOXCLIPBOARDCLIENT *> ClipboardClientMap;

/** Simple queue (list) which holds deferred (waiting) clients. */
typedef std::list<uint32_t> ClipboardClientQueue;

/*
 * The service functions. Locking is between the service thread and the platform-dependent (window) thread.
 */
int vboxSvcClipboardCompleteReadData(PVBOXCLIPBOARDCLIENTDATA pClientData, int rc, uint32_t cbActual);
uint32_t vboxSvcClipboardGetMode(void);
int vboxSvcClipboardReportMsg(PVBOXCLIPBOARDCLIENTDATA pClientData, uint32_t uMsg, uint32_t uFormats);
int vboxSvcClipboardSetSource(PVBOXCLIPBOARDCLIENTDATA pClientData, SHAREDCLIPBOARDSOURCE enmSource);

int vboxSvcClipboardClientDefer(PVBOXCLIPBOARDCLIENT pClient,
                                VBOXHGCMCALLHANDLE hHandle, uint32_t u32Function, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
int vboxSvcClipboardClientComplete(PVBOXCLIPBOARDCLIENT pClient, VBOXHGCMCALLHANDLE hHandle, int rc);
int vboxSvcClipboardClientDeferredComplete(PVBOXCLIPBOARDCLIENT pClient, int rc);
int vboxSvcClipboardClientDeferredSetMsgInfo(PVBOXCLIPBOARDCLIENT pClient, uint32_t uMsg, uint32_t cParms);

void vboxSvcClipboardMsgQueueReset(PVBOXCLIPBOARDCLIENTSTATE pState);
PVBOXCLIPBOARDCLIENTMSG vboxSvcClipboardMsgAlloc(uint32_t uMsg, uint32_t cParms);
void vboxSvcClipboardMsgFree(PVBOXCLIPBOARDCLIENTMSG pMsg);
int vboxSvcClipboardMsgAdd(PVBOXCLIPBOARDCLIENTSTATE pState, PVBOXCLIPBOARDCLIENTMSG pMsg, bool fAppend);
int vboxSvcClipboardMsgGetNextInfo(PVBOXCLIPBOARDCLIENTSTATE pState, uint32_t *puType, uint32_t *pcParms);
int vboxSvcClipboardMsgGetNext(PVBOXCLIPBOARDCLIENTSTATE pState,
                               uint32_t uMsg, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);

# ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
bool vboxSvcClipboardURIMsgIsAllowed(uint32_t uMode, uint32_t uMsg);
int  vboxSvcClipboardURIReportMsg(PVBOXCLIPBOARDCLIENTDATA pClientData, uint32_t u32Msg, uint32_t u32Formats);
bool vboxSvcClipboardURIReturnMsg(PVBOXCLIPBOARDCLIENTDATA pClientData, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
# endif /* VBOX_WITH_SHARED_CLIPBOARD_URI_LIST */

/*
 * Platform-dependent implementations.
 */
int VBoxClipboardSvcImplInit(void);
void VBoxClipboardSvcImplDestroy(void);

int VBoxClipboardSvcImplConnect(PVBOXCLIPBOARDCLIENTDATA pClientData, bool fHeadless);
int VBoxClipboardSvcImplDisconnect(PVBOXCLIPBOARDCLIENTDATA pClientData);
int VBoxClipboardSvcImplFormatAnnounce(PVBOXCLIPBOARDCLIENTDATA pClientData, uint32_t u32Formats);
int VBoxClipboardSvcImplReadData(PVBOXCLIPBOARDCLIENTDATA pClientData, uint32_t u32Format, void *pv, uint32_t cb, uint32_t *pcbActual);
int VBoxClipboardSvcImplWriteData(PVBOXCLIPBOARDCLIENTDATA pClientData, void *pv, uint32_t cb, uint32_t u32Format);
/**
 * Synchronise the contents of the host clipboard with the guest, called by the HGCM layer
 * after a save and restore of the guest.
 */
int VBoxClipboardSvcImplSync(PVBOXCLIPBOARDCLIENTDATA pClientData);

#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
int vboxSvcClipboardURITransferOpen(PSHAREDCLIPBOARDPROVIDERCTX pCtx);
int vboxSvcClipboardURITransferClose(PSHAREDCLIPBOARDPROVIDERCTX pCtx);

int vboxSvcClipboardURIListOpen(PSHAREDCLIPBOARDPROVIDERCTX pCtx,
                                PVBOXCLIPBOARDLISTHDR pListHdr, PVBOXCLIPBOARDLISTHANDLE phList);
int vboxSvcClipboardURIListClose(PSHAREDCLIPBOARDPROVIDERCTX pCtx, VBOXCLIPBOARDLISTHANDLE hList);
int vboxSvcClipboardURIListHdrRead(PSHAREDCLIPBOARDPROVIDERCTX pCtx, VBOXCLIPBOARDLISTHANDLE hList,
                                   PVBOXCLIPBOARDLISTHDR pListHdr);
int vboxSvcClipboardURIListHdrWrite(PSHAREDCLIPBOARDPROVIDERCTX pCtx, VBOXCLIPBOARDLISTHANDLE hList,
                                    PVBOXCLIPBOARDLISTHDR pListHdr);
int vboxSvcClipboardURIListEntryRead(PSHAREDCLIPBOARDPROVIDERCTX pCtx, VBOXCLIPBOARDLISTHANDLE hList,
                                     PVBOXCLIPBOARDLISTENTRY pListEntry);
int vboxSvcClipboardURIListEntryWrite(PSHAREDCLIPBOARDPROVIDERCTX pCtx, VBOXCLIPBOARDLISTHANDLE hList,
                                      PVBOXCLIPBOARDLISTENTRY pListEntry);

int vboxSvcClipboardURIObjOpen(PSHAREDCLIPBOARDPROVIDERCTX pCtx, const char *pszPath,
                               PVBOXCLIPBOARDCREATEPARMS pCreateParms, PSHAREDCLIPBOARDOBJHANDLE phObj);
int vboxSvcClipboardURIObjClose(PSHAREDCLIPBOARDPROVIDERCTX pCtx, SHAREDCLIPBOARDOBJHANDLE hObj);
int vboxSvcClipboardURIObjRead(PSHAREDCLIPBOARDPROVIDERCTX pCtx, SHAREDCLIPBOARDOBJHANDLE hObj,
                               void *pvData, uint32_t cbData, uint32_t fFlags, uint32_t *pcbRead);
int vboxSvcClipboardURIObjWrite(PSHAREDCLIPBOARDPROVIDERCTX pCtx, SHAREDCLIPBOARDOBJHANDLE hObj,
                                void *pvData, uint32_t cbData, uint32_t fFlags, uint32_t *pcbWritten);

DECLCALLBACK(void) VBoxSvcClipboardURITransferPrepareCallback(PSHAREDCLIPBOARDURITRANSFERCALLBACKDATA pData);
DECLCALLBACK(void) VBoxSvcClipboardURIDataHeaderCompleteCallback(PSHAREDCLIPBOARDURITRANSFERCALLBACKDATA pData);
DECLCALLBACK(void) VBoxSvcClipboardURIDataCompleteCallback(PSHAREDCLIPBOARDURITRANSFERCALLBACKDATA pData);
DECLCALLBACK(void) VBoxSvcClipboardURITransferCompleteCallback(PSHAREDCLIPBOARDURITRANSFERCALLBACKDATA pData, int rc);
DECLCALLBACK(void) VBoxSvcClipboardURITransferCanceledCallback(PSHAREDCLIPBOARDURITRANSFERCALLBACKDATA pData);
DECLCALLBACK(void) VBoxSvcClipboardURITransferErrorCallback(PSHAREDCLIPBOARDURITRANSFERCALLBACKDATA pData, int rc);

int VBoxClipboardSvcImplURITransferCreate(PVBOXCLIPBOARDCLIENTDATA pClientData, PSHAREDCLIPBOARDURITRANSFER pTransfer);
int VBoxClipboardSvcImplURITransferDestroy(PVBOXCLIPBOARDCLIENTDATA pClientData, PSHAREDCLIPBOARDURITRANSFER pTransfer);
#endif

/* Host unit testing interface */
#ifdef UNIT_TEST
uint32_t TestClipSvcGetMode(void);
#endif

#endif /* !VBOX_INCLUDED_SRC_SharedClipboard_VBoxSharedClipboardSvc_internal_h */

