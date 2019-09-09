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

#include <iprt/cpp/list.h> /* For RTCList. */
#include <iprt/list.h>
#include <iprt/semaphore.h>

#include <VBox/hgcmsvc.h>
#include <VBox/log.h>

#include <VBox/HostServices/Service.h>
#include <VBox/GuestHost/SharedClipboard.h>
#include <VBox/GuestHost/SharedClipboard-uri.h>

using namespace HGCM;

#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
struct SHCLCLIENTSTATE;
#endif /* VBOX_WITH_SHARED_CLIPBOARD_URI_LIST */

/**
 * Structure for keeping a Shared Clipboard HGCM message context.
 */
typedef struct _VBOXSHCLMSGCTX
{
    /** Context ID. */
    uint32_t uContextID;
} VBOXSHCLMSGCTX, *PVBOXSHCLMSGCTX;

/**
 * Structure for keeping a single HGCM message.
 */
typedef struct _SHCLCLIENTMSG
{
    /** Stored message type. */
    uint32_t         m_uMsg;
    /** Number of stored HGCM parameters. */
    uint32_t         m_cParms;
    /** Stored HGCM parameters. */
    PVBOXHGCMSVCPARM m_paParms;
    /** Message context. */
    VBOXSHCLMSGCTX   m_Ctx;
} SHCLCLIENTMSG, *PSHCLCLIENTMSG;

typedef struct SHCLCLIENTURISTATE
{
    /** Directory of the transfer to start. */
    SHCLURITRANSFERDIR enmTransferDir;
} SHCLCLIENTURISTATE, *PSHCLCLIENTURISTATE;

/**
 * Structure for keeping generic client state data within the Shared Clipboard host service.
 * This structure needs to be serializable by SSM (must be a POD type).
 */
typedef struct SHCLCLIENTSTATE
{
    struct SHCLCLIENTSTATE *pNext;
    struct SHCLCLIENTSTATE *pPrev;

    SHCLCONTEXT            *pCtx;

    /** The client's HGCM ID. */
    uint32_t                         u32ClientID;
    /** Optional protocol version the client uses. Set to 0 by default. */
    uint32_t                         uProtocolVer;
    /** Maximum chunk size to use for data transfers. Set to _64K by default. */
    uint32_t                         cbChunkSize;
    SHCLSOURCE            enmSource;
    /** The client's URI state. */
    SHCLCLIENTURISTATE      URI;
} SHCLCLIENTSTATE, *PSHCLCLIENTSTATE;

typedef struct _SHCLCLIENTCMDCTX
{
    uint32_t                          uContextID;
} SHCLCLIENTCMDCTX, *PSHCLCLIENTCMDCTX;

typedef struct _SHCLCLIENT
{
    /** The client's HGCM client ID. */
    uint32_t                          uClientID;
    /** General client state data. */
    SHCLCLIENTSTATE          State;
    /** The client's message queue (FIFO). */
    RTCList<SHCLCLIENTMSG *> queueMsg;
    /** The client's own event source. */
    SHCLEVENTSOURCE        Events;
#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
    /** URI context data. */
    SHCLURICTX             URI;
#endif
    /** Structure for keeping the client's pending (deferred return) state.
     *  A client is in a deferred state when it asks for the next HGCM message,
     *  but the service can't provide it yet. That way a client will block (on the guest side, does not return)
     *  until the service can complete the call. */
    struct
    {
        /** The client's HGCM call handle. Needed for completing a deferred call. */
        VBOXHGCMCALLHANDLE hHandle;
        /** Message type (function number) to use when completing the deferred call.
         *  A non-0 value means the client is in pending mode. */
        uint32_t           uType;
        /** Parameter count to use when completing the deferred call. */
        uint32_t           cParms;
        /** Parameters to use when completing the deferred call. */
        PVBOXHGCMSVCPARM   paParms;
    } Pending;
} SHCLCLIENT, *PSHCLCLIENT;

/**
 * Structure for keeping a single event source map entry.
 * Currently empty.
 */
typedef struct _SHCLEVENTSOURCEMAPENTRY
{
} SHCLEVENTSOURCEMAPENTRY;

/** Map holding information about connected HGCM clients. Key is the (unique) HGCM client ID.
 *  The value is a weak pointer to PSHCLCLIENT, which is owned by HGCM. */
typedef std::map<uint32_t, PSHCLCLIENT> ClipboardClientMap;

/** Map holding information about event sources. Key is the (unique) event source ID. */
typedef std::map<SHCLEVENTSOURCEID, SHCLEVENTSOURCEMAPENTRY> ClipboardEventSourceMap;

/** Simple queue (list) which holds deferred (waiting) clients. */
typedef std::list<uint32_t> ClipboardClientQueue;

/**
 * Structure for keeping the Shared Clipboard service extension state.
 *
 * A service extension is optional, and can be installed by a host component
 * to communicate with the Shared Clipboard host service.
 */
typedef struct _SHCLEXTSTATE
{
    /** Pointer to the actual service extension handle. */
    PFNHGCMSVCEXT  pfnExtension;
    /** Opaque pointer to extension-provided data. Don't touch. */
    void          *pvExtension;
    /** The HGCM client ID currently assigned to this service extension.
     *  At the moment only one HGCM client can be assigned per extension. */
    uint32_t       uClientID;
    /** Whether the host service is reading clipboard data currently. */
    bool           fReadingData;
    /** Whether the service extension has sent the clipboard formats while
     *  the the host service is reading clipboard data from it. */
    bool           fDelayedAnnouncement;
    /** The actual clipboard formats announced while the host service
     *  is reading clipboard data from the extension. */
    uint32_t       uDelayedFormats;
} SHCLEXTSTATE, *PSHCLEXTSTATE;

/*
 * The service functions. Locking is between the service thread and the platform-dependent (window) thread.
 */
int vboxSvcClipboardDataReadRequest(PSHCLCLIENT pClient, PSHCLDATAREQ pDataReq, PSHCLEVENTID puEvent);
int vboxSvcClipboardDataReadSignal(PSHCLCLIENT pClient, PSHCLCLIENTCMDCTX pCmdCtx, PSHCLDATABLOCK pData);
int vboxSvcClipboardFormatsReport(PSHCLCLIENT pClient, PSHCLFORMATDATA pFormats);

uint32_t vboxSvcClipboardGetMode(void);
int vboxSvcClipboardSetSource(PSHCLCLIENT pClient, SHCLSOURCE enmSource);

void vboxSvcClipboardMsgQueueReset(PSHCLCLIENT pClient);
PSHCLCLIENTMSG vboxSvcClipboardMsgAlloc(uint32_t uMsg, uint32_t cParms);
void vboxSvcClipboardMsgFree(PSHCLCLIENTMSG pMsg);
void vboxSvcClipboardMsgSetPeekReturn(PSHCLCLIENTMSG pMsg, PVBOXHGCMSVCPARM paDstParms, uint32_t cDstParms);
int vboxSvcClipboardMsgAdd(PSHCLCLIENT pClient, PSHCLCLIENTMSG pMsg, bool fAppend);
int vboxSvcClipboardMsgPeek(PSHCLCLIENT pClient, VBOXHGCMCALLHANDLE hCall, uint32_t cParms, VBOXHGCMSVCPARM paParms[], bool fWait);
int vboxSvcClipboardMsgGet(PSHCLCLIENT pClient, VBOXHGCMCALLHANDLE hCall, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);

int vboxSvcClipboardClientWakeup(PSHCLCLIENT pClient);

# ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
int vboxSvcClipboardURITransferStart(PSHCLCLIENT pClient,
                                     SHCLURITRANSFERDIR enmDir, SHCLSOURCE enmSource,
                                     PSHCLURITRANSFER *ppTransfer);
bool vboxSvcClipboardURIMsgIsAllowed(uint32_t uMode, uint32_t uMsg);
# endif /* VBOX_WITH_SHARED_CLIPBOARD_URI_LIST */

/*
 * Platform-dependent implementations.
 */
int VBoxClipboardSvcImplInit(void);
void VBoxClipboardSvcImplDestroy(void);

int VBoxClipboardSvcImplConnect(PSHCLCLIENT pClient, bool fHeadless);
int VBoxClipboardSvcImplDisconnect(PSHCLCLIENT pClient);
int VBoxClipboardSvcImplFormatAnnounce(PSHCLCLIENT pClient, PSHCLCLIENTCMDCTX pCmdCtx, PSHCLFORMATDATA pFormats);
/** @todo Document: Can return VINF_HGCM_ASYNC_EXECUTE to defer returning read data.*/
int VBoxClipboardSvcImplReadData(PSHCLCLIENT pClient, PSHCLCLIENTCMDCTX pCmdCtx, PSHCLDATABLOCK pData, uint32_t *pcbActual);
int VBoxClipboardSvcImplWriteData(PSHCLCLIENT pClient, PSHCLCLIENTCMDCTX pCmdCtx, PSHCLDATABLOCK pData);
/**
 * Synchronise the contents of the host clipboard with the guest, called by the HGCM layer
 * after a save and restore of the guest.
 */
int VBoxClipboardSvcImplSync(PSHCLCLIENT pClient);

#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
int vboxSvcClipboardURITransferOpen(PSHCLPROVIDERCTX pCtx);
DECLCALLBACK(int) vboxSvcClipboardURITransferClose(PSHCLPROVIDERCTX pCtx);

int vboxSvcClipboardURIGetRoots(PSHCLPROVIDERCTX pCtx, PSHCLROOTLIST *ppRootList);

int vboxSvcClipboardURIListOpen(PSHCLPROVIDERCTX pCtx,
                                PSHCLLISTOPENPARMS pOpenParms, PSHCLLISTHANDLE phList);
int vboxSvcClipboardURIListClose(PSHCLPROVIDERCTX pCtx, SHCLLISTHANDLE hList);
int vboxSvcClipboardURIListHdrRead(PSHCLPROVIDERCTX pCtx, SHCLLISTHANDLE hList,
                                   PSHCLLISTHDR pListHdr);
int vboxSvcClipboardURIListHdrWrite(PSHCLPROVIDERCTX pCtx, SHCLLISTHANDLE hList,
                                    PSHCLLISTHDR pListHdr);
int vboxSvcClipboardURIListEntryRead(PSHCLPROVIDERCTX pCtx, SHCLLISTHANDLE hList,
                                     PSHCLLISTENTRY pListEntry);
int vboxSvcClipboardURIListEntryWrite(PSHCLPROVIDERCTX pCtx, SHCLLISTHANDLE hList,
                                      PSHCLLISTENTRY pListEntry);

int vboxSvcClipboardURIObjOpen(PSHCLPROVIDERCTX pCtx, PSHCLOBJOPENCREATEPARMS pCreateParms,
                               PSHCLOBJHANDLE phObj);
int vboxSvcClipboardURIObjClose(PSHCLPROVIDERCTX pCtx, SHCLOBJHANDLE hObj);
int vboxSvcClipboardURIObjRead(PSHCLPROVIDERCTX pCtx, SHCLOBJHANDLE hObj,
                               void *pvData, uint32_t cbData, uint32_t fFlags, uint32_t *pcbRead);
int vboxSvcClipboardURIObjWrite(PSHCLPROVIDERCTX pCtx, SHCLOBJHANDLE hObj,
                                void *pvData, uint32_t cbData, uint32_t fFlags, uint32_t *pcbWritten);

DECLCALLBACK(void) VBoxSvcClipboardURITransferPrepareCallback(PSHCLURITRANSFERCALLBACKDATA pData);
DECLCALLBACK(void) VBoxSvcClipboardURIDataHeaderCompleteCallback(PSHCLURITRANSFERCALLBACKDATA pData);
DECLCALLBACK(void) VBoxSvcClipboardURIDataCompleteCallback(PSHCLURITRANSFERCALLBACKDATA pData);
DECLCALLBACK(void) VBoxSvcClipboardURITransferCompleteCallback(PSHCLURITRANSFERCALLBACKDATA pData, int rc);
DECLCALLBACK(void) VBoxSvcClipboardURITransferCanceledCallback(PSHCLURITRANSFERCALLBACKDATA pData);
DECLCALLBACK(void) VBoxSvcClipboardURITransferErrorCallback(PSHCLURITRANSFERCALLBACKDATA pData, int rc);

int VBoxClipboardSvcImplURITransferCreate(PSHCLCLIENT pClient, PSHCLURITRANSFER pTransfer);
int VBoxClipboardSvcImplURITransferDestroy(PSHCLCLIENT pClient, PSHCLURITRANSFER pTransfer);
#endif /*VBOX_WITH_SHARED_CLIPBOARD_URI_LIST */

/* Host unit testing interface */
#ifdef UNIT_TEST
uint32_t TestClipSvcGetMode(void);
#endif

#endif /* !VBOX_INCLUDED_SRC_SharedClipboard_VBoxSharedClipboardSvc_internal_h */

