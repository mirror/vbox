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

#include <iprt/list.h>

#include <VBox/hgcmsvc.h>
#include <VBox/log.h>

#include <VBox/GuestHost/SharedClipboard.h>
#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
# include <iprt/semaphore.h>
# include <VBox/GuestHost/SharedClipboard-uri.h>
#endif

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

/**
 * Structure for keeping generic client state data within the Shared Clipboard host service.
 * This structure needs to be serializable by SSM.
 */
struct VBOXCLIPBOARDCLIENTSTATE
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
};
typedef struct VBOXCLIPBOARDCLIENTSTATE *PVBOXCLIPBOARDCLIENTSTATE;

/**
 * Structure for keeping a HGCM client state within the Shared Clipboard host service.
 */
typedef struct _VBOXCLIPBOARDCLIENTDATA
{
    /** General client state data. */
    VBOXCLIPBOARDCLIENTSTATE       State;
#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
    SHAREDCLIPBOARDURICTX          URI;
#endif
} VBOXCLIPBOARDCLIENTDATA, *PVBOXCLIPBOARDCLIENTDATA;

/*
 * The service functions. Locking is between the service thread and the platform-dependent (window) thread.
 */
int vboxSvcClipboardCompleteReadData(PVBOXCLIPBOARDCLIENTDATA pClientData, int rc, uint32_t cbActual);
uint32_t vboxSvcClipboardGetMode(void);
int vboxSvcClipboardReportMsg(PVBOXCLIPBOARDCLIENTDATA pClientData, uint32_t uMsg, uint32_t uFormats);
int vboxSvcClipboardSetSource(PVBOXCLIPBOARDCLIENTDATA pClientData, SHAREDCLIPBOARDSOURCE enmSource);

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
int VBoxSvcClipboardProviderImplURIReadDataHdr(PSHAREDCLIPBOARDPROVIDERCTX pCtx, PVBOXCLIPBOARDDATAHDR *ppDataHdr);
int VBoxSvcClipboardProviderImplURIWriteDataHdr(PSHAREDCLIPBOARDPROVIDERCTX pCtx, const PVBOXCLIPBOARDDATAHDR pDataHdr);
int VBoxSvcClipboardProviderImplURIReadDataChunk(PSHAREDCLIPBOARDPROVIDERCTX pCtx, const PVBOXCLIPBOARDDATAHDR pDataHdr, void *pvChunk, uint32_t cbChunk, uint32_t fFlags, uint32_t *pcbRead);
int VBoxSvcClipboardProviderImplURIWriteDataChunk(PSHAREDCLIPBOARDPROVIDERCTX pCtx, const PVBOXCLIPBOARDDATAHDR pDataHdr, const void *pvChunk, uint32_t cbChunk, uint32_t fFlags, uint32_t *pcbWritten);
int VBoxSvcClipboardProviderImplURIReadDir(PSHAREDCLIPBOARDPROVIDERCTX pCtx, PVBOXCLIPBOARDDIRDATA *ppDirData);
int VBoxSvcClipboardProviderImplURIWriteDir(PSHAREDCLIPBOARDPROVIDERCTX pCtx, const PVBOXCLIPBOARDDIRDATA pDirData);
int VBoxSvcClipboardProviderImplURIReadFileHdr(PSHAREDCLIPBOARDPROVIDERCTX pCtx, PVBOXCLIPBOARDFILEHDR *ppFileHdr);
int VBoxSvcClipboardProviderImplURIWriteFileHdr(PSHAREDCLIPBOARDPROVIDERCTX pCtx, const PVBOXCLIPBOARDFILEHDR pFileHdr);
int VBoxSvcClipboardProviderImplURIReadFileData(PSHAREDCLIPBOARDPROVIDERCTX pCtx, void *pvData, uint32_t cbData, uint32_t fFlags, uint32_t *pcbRead);
int VBoxSvcClipboardProviderImplURIWriteFileData(PSHAREDCLIPBOARDPROVIDERCTX pCtx, void *pvData, uint32_t cbData, uint32_t fFlags, uint32_t *pcbWritten);

DECLCALLBACK(void) VBoxSvcClipboardURITransferPrepareCallback(PSHAREDCLIPBOARDURITRANSFERCALLBACKDATA pData);
DECLCALLBACK(void) VBoxSvcClipboardURIDataHeaderCompleteCallback(PSHAREDCLIPBOARDURITRANSFERCALLBACKDATA pData);
DECLCALLBACK(void) VBoxSvcClipboardURIDataCompleteCallback(PSHAREDCLIPBOARDURITRANSFERCALLBACKDATA pData);
DECLCALLBACK(void) VBoxSvcClipboardURITransferCompleteCallback(PSHAREDCLIPBOARDURITRANSFERCALLBACKDATA pData, int rc);
DECLCALLBACK(void) VBoxSvcClipboardURITransferCanceledCallback(PSHAREDCLIPBOARDURITRANSFERCALLBACKDATA pData);
DECLCALLBACK(void) VBoxSvcClipboardURITransferErrorCallback(PSHAREDCLIPBOARDURITRANSFERCALLBACKDATA pData, int rc);

int VBoxClipboardSvcImplURITransferCreate(PVBOXCLIPBOARDCLIENTDATA pClientData, PSHAREDCLIPBOARDURITRANSFER pTransfer);
int VBoxClipboardSvcImplURITransferDestroy(PVBOXCLIPBOARDCLIENTDATA pClientData, PSHAREDCLIPBOARDURITRANSFER pTransfer);

void VBoxClipboardSvcImplURIOnDataHeaderComplete(PSHAREDCLIPBOARDURITRANSFERCALLBACKDATA pData);
#endif

/* Host unit testing interface */
#ifdef UNIT_TEST
uint32_t TestClipSvcGetMode(void);
#endif

#endif /* !VBOX_INCLUDED_SRC_SharedClipboard_VBoxSharedClipboardSvc_internal_h */

