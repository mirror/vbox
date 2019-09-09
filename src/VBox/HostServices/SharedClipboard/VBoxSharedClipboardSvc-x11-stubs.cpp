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
    LogFlowFunc(("called, returning VINF_SUCCESS\n"));
    return VINF_SUCCESS;
}

/** Terminate the host side of the shared clipboard - called by the hgcm layer. */
void VBoxClipboardSvcImplDestroy(void)
{
    LogFlowFunc(("called, returning\n"));
}

/**
  * Enable the shared clipboard - called by the hgcm clipboard subsystem.
  *
  * @returns RT status code
  * @param   pClient            Structure containing context information about the guest system
  * @param   fHeadless          Whether headless.
  */
int VBoxClipboardSvcImplConnect(PSHCLCLIENT pClient, bool fHeadless)
{
    RT_NOREF(pClient, fHeadless);
    LogFlowFunc(("called, returning VINF_SUCCESS\n"));
    return VINF_SUCCESS;
}

/**
 * Synchronise the contents of the host clipboard with the guest, called by the HGCM layer
 * after a save and restore of the guest.
 */
int SharedClipboardSvcImplSync(PSHCLCLIENT pClient)
{
    RT_NOREF(pClient);
    LogFlowFunc(("called, returning VINF_SUCCESS\n"));
    return VINF_SUCCESS;
}

/**
 * Shut down the shared clipboard subsystem and "disconnect" the guest.
 *
 * @param   pClient         Structure containing context information about the guest system
 */
int SharedClipboardSvcImplDisconnect(PSHCLCLIENT pClient)
{
    RT_NOREF(pClient);
    return VINF_SUCCESS;
}

/**
 * The guest is taking possession of the shared clipboard.  Called by the HGCM clipboard
 * subsystem.
 *
 * @param pClient               Context data for the guest system.
 * @param pCmdCtx               Command context to use.
 * @param pFormats              Clipboard formats the guest is offering.
 */
int SharedClipboardSvcImplFormatAnnounce(PSHCLCLIENT pClient, PSHCLCLIENTCMDCTX pCmdCtx,
                                       PSHCLFORMATDATA pFormats)
{
    RT_NOREF(pClient, pCmdCtx, pFormats);
    return VINF_SUCCESS;
}

/**
 * Called by the HGCM clipboard subsystem when the guest wants to read the host clipboard.
 *
 * @param pClient       Context information about the guest VM
 * @param pCmdCtx       Command context to use.
 * @param pData         Data block to put read data into.
 * @param pcbActual     Where to store the actual amount of data available.
 */
int SharedClipboardSvcImplReadData(PSHCLCLIENT pClient, PSHCLCLIENTCMDCTX pCmdCtx,
                                 PSHCLDATABLOCK pData, uint32_t *pcbActual)
{
    RT_NOREF(pClient, pCmdCtx, pData);

    /* No data available. */
    *pcbActual = 0;

    return VINF_SUCCESS;
}

int SharedClipboardSvcImplWriteData(PSHCLCLIENT pClient, PSHCLCLIENTCMDCTX pCmdCtx,
                                  PSHCLDATABLOCK pData)
{
    RT_NOREF(pClient, pCmdCtx, pData);
    return VERR_NOT_IMPLEMENTED;
}

