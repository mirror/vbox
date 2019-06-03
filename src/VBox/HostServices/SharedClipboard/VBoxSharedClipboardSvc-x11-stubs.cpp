/* $Id$*/
/** @file
 * Shared Clipboard Service - Linux host, a stub version with no functionality for use on headless hosts.
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_SHARED_CLIPBOARD
#include <VBox/HostServices/VBoxClipboardSvc.h>

#include <iprt/alloc.h>
#include <iprt/asm.h>        /* For atomic operations */
#include <iprt/assert.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/thread.h>
#include <iprt/process.h>
#include <iprt/semaphore.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include "VBoxSharedClipboardSvc-internal.h"


/** Initialise the host side of the shared clipboard - called by the hgcm layer. */
int VBoxClipboardSvcImplInit(void)
{
    LogFlowFunc(("called, returning VINF_SUCCESS.\n"));
    return VINF_SUCCESS;
}

/** Terminate the host side of the shared clipboard - called by the hgcm layer. */
void VBoxClipboardSvcImplDestroy(void)
{
    LogFlowFunc(("called, returning.\n"));
}

/**
  * Enable the shared clipboard - called by the hgcm clipboard subsystem.
  *
  * @param   pClientData    Structure containing context information about the guest system
  * @param   fHeadless      Whether headless.
  * @returns RT status code
  */
int VBoxClipboardSvcImplConnect(PVBOXCLIPBOARDCLIENTDATA pClientData, bool fHeadless)
{
    RT_NOREF(pClientData, fHeadless);
    LogFlowFunc(("called, returning VINF_SUCCESS.\n"));
    return VINF_SUCCESS;
}

/**
 * Synchronise the contents of the host clipboard with the guest, called by the HGCM layer
 * after a save and restore of the guest.
 */
int VBoxClipboardSvcImplSync(PVBOXCLIPBOARDCLIENTDATA /* pClientData */)
{
    LogFlowFunc(("called, returning VINF_SUCCESS.\n"));
    return VINF_SUCCESS;
}

/**
 * Shut down the shared clipboard subsystem and "disconnect" the guest.
 *
 * @param   pClientData    Structure containing context information about the guest system
 */
void VBoxClipboardSvcImplDisconnect(PVBOXCLIPBOARDCLIENTDATA pClientData)
{
    RT_NOREF(pClientData);
    LogFlowFunc(("called, returning.\n"));
}

/**
 * The guest is taking possession of the shared clipboard.  Called by the HGCM clipboard
 * subsystem.
 *
 * @param pClientData    Context data for the guest system
 * @param u32Formats Clipboard formats the guest is offering
 */
void VBoxClipboardSvcImplFormatAnnounce(PVBOXCLIPBOARDCLIENTDATA pClientData, uint32_t u32Formats)
{
    RT_NOREF(pClientData, u32Formats);
    LogFlowFunc(("called, returning.\n"));
}

/**
 * Called by the HGCM clipboard subsystem when the guest wants to read the host clipboard.
 *
 * @param pClientData   Context information about the guest VM
 * @param u32Format     The format that the guest would like to receive the data in
 * @param pv            Where to write the data to
 * @param cb            The size of the buffer to write the data to
 * @param pcbActual     Where to write the actual size of the written data
 */
int VBoxClipboardSvcImplReadData(PVBOXCLIPBOARDCLIENTDATA pClientData, uint32_t u32Format,
                                 void *pv, uint32_t cb, uint32_t *pcbActual)
{
    RT_NOREF(pClientData, u32Format, pv, cb);
    LogFlowFunc(("called, returning VINF_SUCCESS.\n"));
    /* No data available. */
    *pcbActual = 0;
    return VINF_SUCCESS;
}

/**
 * Called by the HGCM clipboard subsystem when we have requested data and that data arrives.
 *
 * @param pClientData   Context information about the guest VM
 * @param pv            Buffer to which the data was written
 * @param cb            The size of the data written
 * @param u32Format     The format of the data written
 */
void VBoxClipboardSvcImplWriteData(PVBOXCLIPBOARDCLIENTDATA pClientData, void *pv, uint32_t cb,
                                   uint32_t u32Format)
{
    RT_NOREF(pClientData, pv, cb, u32Format);
    LogFlowFunc(("called, returning.\n"));
}

#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
int VBoxClipboardSvcImplURIReadDir(PVBOXCLIPBOARDCLIENTDATA pClientData, PVBOXCLIPBOARDDIRDATA pDirData)
{
    RT_NOREF(pClientData, pDirData);
    return VERR_NOT_IMPLEMENTED;
}

int VBoxClipboardSvcImplURIWriteDir(PVBOXCLIPBOARDCLIENTDATA pClientData, PVBOXCLIPBOARDDIRDATA pDirData)
{
    RT_NOREF(pClientData, pDirData);
    return VERR_NOT_IMPLEMENTED;
}

int VBoxClipboardSvcImplURIReadFileHdr(PVBOXCLIPBOARDCLIENTDATA pClientData, PVBOXCLIPBOARDFILEHDR pFileHdr)
{
    RT_NOREF(pClientData, pFileHdr);
    return VERR_NOT_IMPLEMENTED;
}

int VBoxClipboardSvcImplURIWriteFileHdr(PVBOXCLIPBOARDCLIENTDATA pClientData, PVBOXCLIPBOARDFILEHDR pFileHdr)
{
    RT_NOREF(pClientData, pFileHdr);
    return VERR_NOT_IMPLEMENTED;
}

int VBoxClipboardSvcImplURIReadFileData(PVBOXCLIPBOARDCLIENTDATA pClientData, PVBOXCLIPBOARDFILEDATA pFileData)
{
    RT_NOREF(pClientData, pFileData);
    return VERR_NOT_IMPLEMENTED;
}

int VBoxClipboardSvcImplURIWriteFileData(PVBOXCLIPBOARDCLIENTDATA pClientData, PVBOXCLIPBOARDFILEDATA pFileData)
{
    RT_NOREF(pClientData, pFileData);
    return VERR_NOT_IMPLEMENTED;
}
#endif /* VBOX_WITH_SHARED_CLIPBOARD_URI_LIST */

