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
#include <VBox/GuestHost/SharedClipboard-transfers.h>

using namespace HGCM;

#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
struct SHCLCLIENTSTATE;
#endif /* VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS */

/**
 * Structure for keeping a Shared Clipboard HGCM message context.
 */
typedef struct _SHCLMSGCTX
{
    /** Context ID. */
    uint64_t uContextID;
} SHCLMSGCTX, *PSHCLMSGCTX;

/**
 * Structure for keeping a single HGCM message.
 */
typedef struct _SHCLCLIENTMSG
{
    /** Stored message type. */
    uint32_t         uMsg;
    /** Number of stored HGCM parameters. */
    uint32_t         cParms;
    /** Stored HGCM parameters. */
    PVBOXHGCMSVCPARM paParms;
    /** Message context. */
    SHCLMSGCTX       Ctx;
} SHCLCLIENTMSG, *PSHCLCLIENTMSG;

typedef struct SHCLCLIENTTRANSFERSTATE
{
    /** Directory of the transfer to start. */
    SHCLTRANSFERDIR enmTransferDir;
} SHCLCLIENTTRANSFERSTATE, *PSHCLCLIENTTRANSFERSTATE;

/**
 * Structure for keeping generic client state data within the Shared Clipboard host service.
 * This structure needs to be serializable by SSM (must be a POD type).
 */
typedef struct SHCLCLIENTSTATE
{
    struct SHCLCLIENTSTATE *pNext;
    struct SHCLCLIENTSTATE *pPrev;

    SHCLCONTEXT            *pCtx;

    /** The client's HGCM ID. Not related to the session ID below! */
    uint32_t                uClientID;
    /** The client's session ID. */
    SHCLSESSIONID           uSessionID;
    /** Optional protocol version the client uses. Set to 0 by default. */
    uint32_t                uProtocolVer;
    /** Guest feature flags, VBOX_SHCL_GF_0_XXX. */
    uint64_t                fGuestFeatures0;
    /** Guest feature flags, VBOX_SHCL_GF_1_XXX. */
    uint64_t                fGuestFeatures1;
    /** Maximum chunk size to use for data transfers. Set to _64K by default. */
    uint32_t                cbChunkSize;
    SHCLSOURCE              enmSource;
    /** The client's transfers state. */
    SHCLCLIENTTRANSFERSTATE Transfers;
} SHCLCLIENTSTATE, *PSHCLCLIENTSTATE;

typedef struct _SHCLCLIENTCMDCTX
{
    uint64_t uContextID;
} SHCLCLIENTCMDCTX, *PSHCLCLIENTCMDCTX;

typedef struct _SHCLCLIENT
{
    /** General client state data. */
    SHCLCLIENTSTATE          State;
    /** The client's message queue (FIFO). */
    RTCList<SHCLCLIENTMSG *> queueMsg;
    /** The client's own event source.
     *  Needed for events which are not bound to a specific transfer. */
    SHCLEVENTSOURCE          Events;
#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
    /** Transfer contextdata. */
    SHCLTRANSFERCTX          TransferCtx;
#endif
    /** Structure for keeping the client's pending (deferred return) state.
     *  A client is in a deferred state when it asks for the next HGCM message,
     *  but the service can't provide it yet. That way a client will block (on the guest side, does not return)
     *  until the service can complete the call. */
    struct
    {
        /** The client's HGCM call handle. Needed for completing a deferred call. */
        VBOXHGCMCALLHANDLE   hHandle;
        /** Message type (function number) to use when completing the deferred call.
         *  A non-0 value means the client is in pending mode. */
        uint32_t             uType;
        /** Parameter count to use when completing the deferred call. */
        uint32_t             cParms;
        /** Parameters to use when completing the deferred call. */
        PVBOXHGCMSVCPARM     paParms;
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
int shclSvcDataReadRequest(PSHCLCLIENT pClient, PSHCLDATAREQ pDataReq, PSHCLEVENTID puEvent);
int shclSvcDataReadSignal(PSHCLCLIENT pClient, PSHCLCLIENTCMDCTX pCmdCtx, PSHCLDATABLOCK pData);
int shclSvcFormatsReport(PSHCLCLIENT pClient, PSHCLFORMATDATA pFormats);

uint32_t shclSvcGetMode(void);
int shclSvcSetSource(PSHCLCLIENT pClient, SHCLSOURCE enmSource);

void shclSvcMsgQueueReset(PSHCLCLIENT pClient);
PSHCLCLIENTMSG shclSvcMsgAlloc(uint32_t uMsg, uint32_t cParms);
void shclSvcMsgFree(PSHCLCLIENTMSG pMsg);
void shclSvcMsgSetPeekReturn(PSHCLCLIENTMSG pMsg, PVBOXHGCMSVCPARM paDstParms, uint32_t cDstParms);
int shclSvcMsgAdd(PSHCLCLIENT pClient, PSHCLCLIENTMSG pMsg, bool fAppend);
int shclSvcMsgPeek(PSHCLCLIENT pClient, VBOXHGCMCALLHANDLE hCall, uint32_t cParms, VBOXHGCMSVCPARM paParms[], bool fWait);
int shclSvcMsgGet(PSHCLCLIENT pClient, VBOXHGCMCALLHANDLE hCall, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);

int shclSvcClientInit(PSHCLCLIENT pClient, uint32_t uClientID);
void shclSvcClientReset(PSHCLCLIENT pClient);

int shclSvcClientStateInit(PSHCLCLIENTSTATE pClientState, uint32_t uClientID);
int shclSvcClientStateDestroy(PSHCLCLIENTSTATE pClientState);
void shclSvcClientStateReset(PSHCLCLIENTSTATE pClientState);

int shclSvcClientWakeup(PSHCLCLIENT pClient);

# ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
int shclSvcTransferModeSet(uint32_t fMode);
int shclSvcTransferStart(PSHCLCLIENT pClient, SHCLTRANSFERDIR enmDir, SHCLSOURCE enmSource, PSHCLTRANSFER *ppTransfer);
int shclSvcTransferStop(PSHCLCLIENT pClient, PSHCLTRANSFER pTransfer);
bool shclSvcTransferMsgIsAllowed(uint32_t uMode, uint32_t uMsg);
void shclSvcClientTransfersReset(PSHCLCLIENT pClient);
#endif /* VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS */

/** @name Platform-dependent implementations for the Shared Clipboard host service.
 * @{
 */
/**
 * Called on initialization.
 */
int ShClSvcImplInit(void);
/**
 * Called on destruction.
 */
void ShClSvcImplDestroy(void);
/**
 * Called when a new HGCM client connects.
 *
 * @returns VBox status code.
 * @param   pClient             Shared Clipboard client context.
 * @param   fHeadless           Whether this is a headless connection or not.
 */
int ShClSvcImplConnect(PSHCLCLIENT pClient, bool fHeadless);
/**
 * Called when a HGCM client disconnects.
 *
 * @returns VBox status code.
 * @param   pClient             Shared Clipboard client context.
 */
int ShClSvcImplDisconnect(PSHCLCLIENT pClient);
/**
 * Called when the guest reported available clipboard formats to the host OS.
 *
 * @returns VBox status code.
 * @param   pClient             Shared Clipboard client context.
 * @param   pCmdCtx             Shared Clipboard command context.
 * @param   pFormats            Announced formats from the guest.
 */
int ShClSvcImplFormatAnnounce(PSHCLCLIENT pClient, PSHCLCLIENTCMDCTX pCmdCtx, PSHCLFORMATDATA pFormats);
/** @todo Document: Can return VINF_HGCM_ASYNC_EXECUTE to defer returning read data.*/
/**
 * Called when the guest wants to read host clipboard data.
 *
 * @returns VBox status code.
 * @param   pClient             Shared Clipboard client context.
 * @param   pCmdCtx             Shared Clipboard command context.
 * @param   pData               Where to return the read clipboard data.
 * @param   pcbActual           Where to return the amount of bytes read.
 */
int ShClSvcImplReadData(PSHCLCLIENT pClient, PSHCLCLIENTCMDCTX pCmdCtx, PSHCLDATABLOCK pData, uint32_t *pcbActual);
/**
 * Called when the guest writes clipboard data to the host.
 *
 * @returns VBox status code.
 * @param   pClient             Shared Clipboard client context.
 * @param   pCmdCtx             Shared Clipboard command context.
 * @param   pData               Clipboard data from the guest.
 */
int ShClSvcImplWriteData(PSHCLCLIENT pClient, PSHCLCLIENTCMDCTX pCmdCtx, PSHCLDATABLOCK pData);
/**
 * Called when synchronization of the clipboard contents of the host clipboard with the guest is needed.
 *
 * @returns VBox status code.
 * @param   pClient             Shared Clipboard client context.
 */
int ShClSvcImplSync(PSHCLCLIENT pClient);
/** @} */

#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
/** @name Host implementations for Shared Clipboard transfers.
 * @{
 */
/**
 * Called when a transfer gets created.
 *
 * @returns VBox status code.
 * @param   pClient             Shared Clipboard client context.
 * @param   pTransfer           Shared Clipboard transfer created.
 */
int ShClSvcImplTransferCreate(PSHCLCLIENT pClient, PSHCLTRANSFER pTransfer);
/**
 * Called when a transfer gets destroyed.
 *
 * @returns VBox status code.
 * @param   pClient             Shared Clipboard client context.
 * @param   pTransfer           Shared Clipboard transfer to destroy.
 */
int ShClSvcImplTransferDestroy(PSHCLCLIENT pClient, PSHCLTRANSFER pTransfer);
/**
 * Called when getting (determining) the transfer roots on the host side.
 *
 * @returns VBox status code.
 * @param   pClient             Shared Clipboard client context.
 * @param   pTransfer           Shared Clipboard transfer to get roots for.
 */
int ShClSvcImplTransferGetRoots(PSHCLCLIENT pClient, PSHCLTRANSFER pTransfer);
/** @} */
#endif

#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
/** @name Internal Shared Clipboard transfer host service functions.
 * @{
 */
int shclSvcTransferAreaDetach(PSHCLCLIENTSTATE pClientState, PSHCLTRANSFER pTransfer);
int shclSvcTransferHandler(PSHCLCLIENT pClient, VBOXHGCMCALLHANDLE callHandle, uint32_t u32Function,
                           uint32_t cParms, VBOXHGCMSVCPARM paParms[], uint64_t tsArrival);
int shclSvcTransferHostHandler(uint32_t u32Function, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
/** @} */

/** @name Shared Clipboard transfer interface implementations for the host service.
 * @{
 */
int shclSvcTransferIfaceOpen(PSHCLPROVIDERCTX pCtx);
int shclSvcTransferIfaceClose(PSHCLPROVIDERCTX pCtx);

int shclSvcTransferIfaceGetRoots(PSHCLPROVIDERCTX pCtx, PSHCLROOTLIST *ppRootList);

int shclSvcTransferIfaceListOpen(PSHCLPROVIDERCTX pCtx, PSHCLLISTOPENPARMS pOpenParms, PSHCLLISTHANDLE phList);
int shclSvcTransferIfaceListClose(PSHCLPROVIDERCTX pCtx, SHCLLISTHANDLE hList);
int shclSvcTransferIfaceListHdrRead(PSHCLPROVIDERCTX pCtx, SHCLLISTHANDLE hList, PSHCLLISTHDR pListHdr);
int shclSvcTransferIfaceListHdrWrite(PSHCLPROVIDERCTX pCtx, SHCLLISTHANDLE hList, PSHCLLISTHDR pListHdr);
int shclSvcTransferIfaceListEntryRead(PSHCLPROVIDERCTX pCtx, SHCLLISTHANDLE hList, PSHCLLISTENTRY pListEntry);
int shclSvcTransferIfaceListEntryWrite(PSHCLPROVIDERCTX pCtx, SHCLLISTHANDLE hList, PSHCLLISTENTRY pListEntry);

int shclSvcTransferIfaceObjOpen(PSHCLPROVIDERCTX pCtx, PSHCLOBJOPENCREATEPARMS pCreateParms,
                                PSHCLOBJHANDLE phObj);
int shclSvcTransferIfaceObjClose(PSHCLPROVIDERCTX pCtx, SHCLOBJHANDLE hObj);
int shclSvcTransferIfaceObjRead(PSHCLPROVIDERCTX pCtx, SHCLOBJHANDLE hObj,
                                void *pvData, uint32_t cbData, uint32_t fFlags, uint32_t *pcbRead);
int shclSvcTransferIfaceObjWrite(PSHCLPROVIDERCTX pCtx, SHCLOBJHANDLE hObj,
                                 void *pvData, uint32_t cbData, uint32_t fFlags, uint32_t *pcbWritten);
/** @} */

/** @name Shared Clipboard transfer callbacks for the host service.
 * @{
 */
DECLCALLBACK(void) VBoxSvcClipboardTransferPrepareCallback(PSHCLTRANSFERCALLBACKDATA pData);
DECLCALLBACK(void) VBoxSvcClipboardDataHeaderCompleteCallback(PSHCLTRANSFERCALLBACKDATA pData);
DECLCALLBACK(void) VBoxSvcClipboardDataCompleteCallback(PSHCLTRANSFERCALLBACKDATA pData);
DECLCALLBACK(void) VBoxSvcClipboardTransferCompleteCallback(PSHCLTRANSFERCALLBACKDATA pData, int rc);
DECLCALLBACK(void) VBoxSvcClipboardTransferCanceledCallback(PSHCLTRANSFERCALLBACKDATA pData);
DECLCALLBACK(void) VBoxSvcClipboardTransferErrorCallback(PSHCLTRANSFERCALLBACKDATA pData, int rc);
/** @} */
#endif /* VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS */

/* Host unit testing interface */
#ifdef UNIT_TEST
uint32_t TestClipSvcGetMode(void);
#endif

#endif /* !VBOX_INCLUDED_SRC_SharedClipboard_VBoxSharedClipboardSvc_internal_h */

