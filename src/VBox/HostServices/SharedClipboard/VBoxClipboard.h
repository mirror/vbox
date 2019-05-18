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

#ifndef VBOX_INCLUDED_SRC_SharedClipboard_VBoxClipboard_h
#define VBOX_INCLUDED_SRC_SharedClipboard_VBoxClipboard_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/hgcmsvc.h>
#include <VBox/log.h>

#include <VBox/GuestHost/SharedClipboard.h>

typedef struct _VBOXCLIPBOARDSVCCTX
{
    struct _VBOXCLIPBOARDSVCCTX *pNext;
    struct _VBOXCLIPBOARDSVCCTX *pPrev;

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

} VBOXCLIPBOARDSVCCTX, *PVBOXCLIPBOARDSVCCTX;

/*
 * The service functions. Locking is between the service thread and the platform dependent windows thread.
 */
void vboxSvcClipboardReportMsg(PVBOXCLIPBOARDSVCCTX pSvcCtx, uint32_t u32Msg, uint32_t u32Formats);
void vboxSvcClipboardCompleteReadData(PVBOXCLIPBOARDSVCCTX pSvcCtx, int rc, uint32_t cbActual);

/*
 * Platform-dependent implementations.
 */
int VBoxClipboardSvcImplInit(void);
void VBoxClipboardSvcImplDestroy(void);

int VBoxClipboardSvcImplConnect(PVBOXCLIPBOARDSVCCTX pSvcCtx, bool fHeadless);
void VBoxClipboardSvcImplDisconnect(PVBOXCLIPBOARDSVCCTX pSvcCtx);
void VBoxClipboardSvcImplFormatAnnounce(PVBOXCLIPBOARDSVCCTX pSvcCtx, uint32_t u32Formats);
int VBoxClipboardSvcImplReadData(PVBOXCLIPBOARDSVCCTX pSvcCtx, uint32_t u32Format, void *pv, uint32_t cb, uint32_t *pcbActual);
void VBoxClipboardSvcImplWriteData(PVBOXCLIPBOARDSVCCTX pSvcCtx, void *pv, uint32_t cb, uint32_t u32Format);
int VBoxClipboardSvcImplSync(PVBOXCLIPBOARDSVCCTX pSvcCtx);

/* Host unit testing interface */
#ifdef UNIT_TEST
uint32_t TestClipSvcGetMode(void);
#endif

#endif /* !VBOX_INCLUDED_SRC_SharedClipboard_VBoxClipboard_h */

