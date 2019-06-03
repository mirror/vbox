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

/**
 * Structure for maintaining a single URI transfer.
 * A transfer can contain one or multiple files / directories.
 */
typedef struct _VBOXCLIPBOARDCLIENTURITRANSFER
{
    /** Node for keeping this transfer in a RTList. */
    RTLISTNODE                     Node;
    /** Pointer to the client state (parent). */
    VBOXCLIPBOARDCLIENTSTATE      *pState;
    /** The transfer's own (local) area.
     *  The area itself has a clipboard ID assigned, which gets shared (maintained) across all
     *  VMs via VBoxSVC. */
    SharedClipboardArea            Area;
    /** The transfer's URI list, containing the fs object root entries. */
    SharedClipboardURIList         List;
    /** Current object being handled. */
    VBOXCLIPBOARDCLIENTURIOBJCTX   ObjCtx;
    /** The transfer header, needed for verification and accounting. */
    VBOXCLIPBOARDDATAHDR           Hdr;
    /** Intermediate meta data object. */
    SHAREDCLIPBOARDMETADATA        Meta;
} VBOXCLIPBOARDCLIENTURITRANSFER, *PVBOXCLIPBOARDCLIENTURITRANSFER;
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
        VBOXHGCMSVCPARM   *paParms;
    } async;

    struct {
        VBOXHGCMCALLHANDLE callHandle;
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
    /** List of concurrent transfers.
     *  At the moment we only support one transfer at a time (per client). */
    RTLISTANCHOR                   TransferList;
    /** Data related to URI transfer handling. */
    VBOXCLIPBOARDCLIENTURITRANSFER Transfer;
    /** Number of concurrent transfers.
     *  At the moment we only support one transfer at a time (per client). */
    uint32_t                       cTransfers;
#endif
} VBOXCLIPBOARDCLIENTDATA, *PVBOXCLIPBOARDCLIENTDATA;

/*
 * The service functions. Locking is between the service thread and the platform-dependent (window) thread.
 */
uint32_t vboxSvcClipboardGetMode(void);
void vboxSvcClipboardReportMsg(PVBOXCLIPBOARDCLIENTDATA pClientData, uint32_t u32Msg, uint32_t u32Formats);
void vboxSvcClipboardCompleteReadData(PVBOXCLIPBOARDCLIENTDATA pClientData, int rc, uint32_t cbActual);

int vboxSvcClipboardClientStateInit(PVBOXCLIPBOARDCLIENTSTATE pState, uint32_t uClientID);
void vboxSvcClipboardClientStateReset(PVBOXCLIPBOARDCLIENTSTATE pState);

#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
void vboxClipboardSvcURIDirDataDestroy(PVBOXCLIPBOARDDIRDATA pDirData);
void vboxClipboardSvcURIFileHdrDestroy(PVBOXCLIPBOARDFILEHDR pFileHdr);
void vboxClipboardSvcURIFileDataDestroy(PVBOXCLIPBOARDFILEDATA pFileData);
#endif

/*
 * Platform-dependent implementations.
 */
int VBoxClipboardSvcImplInit(void);
void VBoxClipboardSvcImplDestroy(void);

int VBoxClipboardSvcImplConnect(PVBOXCLIPBOARDCLIENTDATA pClientData, bool fHeadless);
void VBoxClipboardSvcImplDisconnect(PVBOXCLIPBOARDCLIENTDATA pClientData);
void VBoxClipboardSvcImplFormatAnnounce(PVBOXCLIPBOARDCLIENTDATA pClientData, uint32_t u32Formats);
int VBoxClipboardSvcImplReadData(PVBOXCLIPBOARDCLIENTDATA pClientData, uint32_t u32Format, void *pv, uint32_t cb, uint32_t *pcbActual);
void VBoxClipboardSvcImplWriteData(PVBOXCLIPBOARDCLIENTDATA pClientData, void *pv, uint32_t cb, uint32_t u32Format);
int VBoxClipboardSvcImplSync(PVBOXCLIPBOARDCLIENTDATA pClientData);

#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
int VBoxClipboardSvcImplURIReadDir(PVBOXCLIPBOARDCLIENTDATA pClientData, PVBOXCLIPBOARDDIRDATA pDirData);
int VBoxClipboardSvcImplURIWriteDir(PVBOXCLIPBOARDCLIENTDATA pClientData, PVBOXCLIPBOARDDIRDATA pDirData);
int VBoxClipboardSvcImplURIReadFileHdr(PVBOXCLIPBOARDCLIENTDATA pClientData, PVBOXCLIPBOARDFILEHDR pFileHdr);
int VBoxClipboardSvcImplURIWriteFileHdr(PVBOXCLIPBOARDCLIENTDATA pClientData, PVBOXCLIPBOARDFILEHDR pFileHdr);
int VBoxClipboardSvcImplURIReadFileData(PVBOXCLIPBOARDCLIENTDATA pClientData, PVBOXCLIPBOARDFILEDATA pFileData);
int VBoxClipboardSvcImplURIWriteFileData(PVBOXCLIPBOARDCLIENTDATA pClientData, PVBOXCLIPBOARDFILEDATA pFileData);
#endif

/* Host unit testing interface */
#ifdef UNIT_TEST
uint32_t TestClipSvcGetMode(void);
#endif

#endif /* !VBOX_INCLUDED_SRC_SharedClipboard_VBoxSharedClipboardSvc_internal_h */

