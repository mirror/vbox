/* $Id$ */
/** @file
 * Shared Clipboard Service - Host service entry points.
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


/** @page pg_hostclip       The Shared Clipboard Host Service
 *
 * The shared clipboard host service provides a proxy between the host's
 * clipboard and a similar proxy running on a guest.  The service is split
 * into a platform-independent core and platform-specific backends.  The
 * service defines two communication protocols - one to communicate with the
 * clipboard service running on the guest, and one to communicate with the
 * backend.  These will be described in a very skeletal fashion here.
 *
 * @section sec_hostclip_guest_proto  The guest communication protocol
 *
 * The guest clipboard service communicates with the host service via HGCM
 * (the host service runs as an HGCM service).  The guest clipboard must
 * connect to the host service before all else (Windows hosts currently only
 * support one simultaneous connection).  Once it has connected, it can send
 * HGCM messages to the host services, some of which will receive replies from
 * the host.  The host can only reply to a guest message, it cannot initiate
 * any communication.  The guest can in theory send any number of messages in
 * parallel (see the descriptions of the messages for the practice), and the
 * host will receive these in sequence, and may reply to them at once
 * (releasing the caller in the guest) or defer the reply until later.
 *
 * There are currently four messages defined.  The first is
 * VBOX_SHARED_CLIPBOARD_FN_GET_HOST_MSG, which waits for a message from the
 * host.  Host messages currently defined are
 * VBOX_SHARED_CLIPBOARD_HOST_MSG_QUIT (unused),
 * VBOX_SHARED_CLIPBOARD_HOST_MSG_READ_DATA (request that the guest send the
 * contents of its clipboard to the host) and
 * VBOX_SHARED_CLIPBOARD_HOST_MSG_REPORT_FORMATS (to notify the guest that new
 * clipboard data is available).  If a host message is sent while the guest is
 * not waiting, it will be queued until the guest requests it.  At most one
 * host message of each type will be kept in the queue.  The host code only
 * supports a single simultaneous VBOX_SHARED_CLIPBOARD_FN_GET_HOST_MSG call
 * from the guest.
 *
 * The second guest message is VBOX_SHARED_CLIPBOARD_FN_FORMATS, which tells
 * the host that the guest has new clipboard data available.  The third is
 * VBOX_SHARED_CLIPBOARD_FN_READ_DATA, which asks the host to send its
 * clipboard data and waits until it arrives.  The host supports at most one
 * simultaneous VBOX_SHARED_CLIPBOARD_FN_READ_DATA call from the guest - if a
 * second call is made before the first has returned, the first will be
 * aborted.
 *
 * The last guest message is VBOX_SHARED_CLIPBOARD_FN_WRITE_DATA, which is
 * used to send the contents of the guest clipboard to the host.  This call
 * should be used after the host has requested data from the guest.
 *
 * @section sec_hostclip_backend_proto  The communication protocol with the
 *                                      platform-specific backend
 *
 * This section may be written in the future :)
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_SHARED_CLIPBOARD
#include <VBox/HostServices/VBoxClipboardSvc.h>
#include <VBox/HostServices/VBoxClipboardExt.h>

#include <iprt/alloc.h>
#include <iprt/string.h>
#include <iprt/assert.h>
#include <iprt/critsect.h>
#include <VBox/err.h>
#include <VBox/vmm/ssm.h>

#include "VBoxSharedClipboardSvc-internal.h"
#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
# include "VBoxSharedClipboardSvc-uri.h"
#endif


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
static PVBOXHGCMSVCHELPERS g_pHelpers;

static RTCRITSECT g_CritSect;
static uint32_t g_uMode;

static PFNHGCMSVCEXT g_pfnExtension;
static void *g_pvExtension;

static PVBOXCLIPBOARDCLIENTDATA g_pClientData;

/* Serialization of data reading and format announcements from the RDP client. */
static bool g_fReadingData = false;
static bool g_fDelayedAnnouncement = false;
static uint32_t g_u32DelayedFormats = 0;

/** Is the clipboard running in headless mode? */
static bool g_fHeadless = false;


static void VBoxHGCMParmUInt32Set(VBOXHGCMSVCPARM *pParm, uint32_t u32)
{
    pParm->type = VBOX_HGCM_SVC_PARM_32BIT;
    pParm->u.uint32 = u32;
}

static int VBoxHGCMParmUInt32Get(VBOXHGCMSVCPARM *pParm, uint32_t *pu32)
{
    if (pParm->type == VBOX_HGCM_SVC_PARM_32BIT)
    {
        *pu32 = pParm->u.uint32;
        return VINF_SUCCESS;
    }

    return VERR_INVALID_PARAMETER;
}

#if 0
static void VBoxHGCMParmPtrSet (VBOXHGCMSVCPARM *pParm, void *pv, uint32_t cb)
{
    pParm->type             = VBOX_HGCM_SVC_PARM_PTR;
    pParm->u.pointer.size   = cb;
    pParm->u.pointer.addr   = pv;
}
#endif

static int VBoxHGCMParmPtrGet(VBOXHGCMSVCPARM *pParm, void **ppv, uint32_t *pcb)
{
    if (pParm->type == VBOX_HGCM_SVC_PARM_PTR)
    {
        *ppv = pParm->u.pointer.addr;
        *pcb = pParm->u.pointer.size;
        return VINF_SUCCESS;
    }

    return VERR_INVALID_PARAMETER;
}

uint32_t vboxSvcClipboardGetMode(void)
{
    return g_uMode;
}

#ifdef UNIT_TEST
/** Testing interface, getter for clipboard mode */
uint32_t TestClipSvcGetMode(void)
{
    return vboxSvcClipboardGetMode();
}
#endif

/** Getter for headless setting. Also needed by testcase. */
bool VBoxSvcClipboardGetHeadless(void)
{
    return g_fHeadless;
}

static void vboxSvcClipboardModeSet(uint32_t u32Mode)
{
    switch (u32Mode)
    {
        case VBOX_SHARED_CLIPBOARD_MODE_OFF:
        case VBOX_SHARED_CLIPBOARD_MODE_HOST_TO_GUEST:
        case VBOX_SHARED_CLIPBOARD_MODE_GUEST_TO_HOST:
        case VBOX_SHARED_CLIPBOARD_MODE_BIDIRECTIONAL:
            g_uMode = u32Mode;
            break;

        default:
            g_uMode = VBOX_SHARED_CLIPBOARD_MODE_OFF;
    }
}

bool VBoxSvcClipboardLock(void)
{
    return RT_SUCCESS(RTCritSectEnter(&g_CritSect));
}

void VBoxSvcClipboardUnlock(void)
{
    int rc2 = RTCritSectLeave(&g_CritSect);
    AssertRC(rc2);
}

/* Set the HGCM parameters according to pending messages.
 * Executed under the clipboard lock.
 */
static bool vboxSvcClipboardReturnMsg(PVBOXCLIPBOARDCLIENTDATA pClientData, VBOXHGCMSVCPARM paParms[])
{
    /* Message priority is taken into account. */
    if (pClientData->State.fHostMsgQuit)
    {
        LogFlowFunc(("vboxSvcClipboardReturnMsg: Quit\n"));
        VBoxHGCMParmUInt32Set(&paParms[0], VBOX_SHARED_CLIPBOARD_HOST_MSG_QUIT);
        VBoxHGCMParmUInt32Set(&paParms[1], 0);
        pClientData->State.fHostMsgQuit = false;
    }
    else if (pClientData->State.fHostMsgReadData)
    {
        uint32_t fFormat = 0;

        LogFlowFunc(("vboxSvcClipboardReturnMsg: ReadData %02X\n", pClientData->State.u32RequestedFormat));
        if (pClientData->State.u32RequestedFormat & VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT)
            fFormat = VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT;
        else if (pClientData->State.u32RequestedFormat & VBOX_SHARED_CLIPBOARD_FMT_BITMAP)
            fFormat = VBOX_SHARED_CLIPBOARD_FMT_BITMAP;
        else if (pClientData->State.u32RequestedFormat & VBOX_SHARED_CLIPBOARD_FMT_HTML)
            fFormat = VBOX_SHARED_CLIPBOARD_FMT_HTML;
#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
        else if (pClientData->State.u32RequestedFormat & VBOX_SHARED_CLIPBOARD_FMT_URI_LIST)
            fFormat = VBOX_SHARED_CLIPBOARD_FMT_URI_LIST;
#endif
        else
        {
            LogRel2(("Clipboard: Unsupported format from guest (0x%x), skipping\n", fFormat));
            pClientData->State.u32RequestedFormat = 0;
        }
        pClientData->State.u32RequestedFormat &= ~fFormat;
        VBoxHGCMParmUInt32Set(&paParms[0], VBOX_SHARED_CLIPBOARD_HOST_MSG_READ_DATA);
        VBoxHGCMParmUInt32Set(&paParms[1], fFormat);
        if (pClientData->State.u32RequestedFormat == 0)
            pClientData->State.fHostMsgReadData = false;
    }
    else if (pClientData->State.fHostMsgFormats)
    {
        LogFlowFunc(("vboxSvcClipboardReturnMsg: Formats %02X\n", pClientData->State.u32AvailableFormats));
        VBoxHGCMParmUInt32Set(&paParms[0], VBOX_SHARED_CLIPBOARD_HOST_MSG_REPORT_FORMATS);
        VBoxHGCMParmUInt32Set(&paParms[1], pClientData->State.u32AvailableFormats);
        pClientData->State.fHostMsgFormats = false;
    }
    else
    {
        /* No pending messages. */
        LogFlowFunc(("vboxSvcClipboardReturnMsg: no message\n"));
        return false;
    }

    /* Message information assigned. */
    return true;
}

void vboxSvcClipboardReportMsg(PVBOXCLIPBOARDCLIENTDATA pClientData, uint32_t u32Msg, uint32_t u32Formats)
{
    AssertPtrReturnVoid(pClientData);

    if (VBoxSvcClipboardLock())
    {
        switch (u32Msg)
        {
            case VBOX_SHARED_CLIPBOARD_HOST_MSG_QUIT:
            {
                LogFlowFunc(("Quit\n"));
                pClientData->State.fHostMsgQuit = true;
            } break;
            case VBOX_SHARED_CLIPBOARD_HOST_MSG_READ_DATA:
            {
                if (   vboxSvcClipboardGetMode () != VBOX_SHARED_CLIPBOARD_MODE_GUEST_TO_HOST
                    && vboxSvcClipboardGetMode () != VBOX_SHARED_CLIPBOARD_MODE_BIDIRECTIONAL)
                {
                    /* Skip the message. */
                    break;
                }

                LogFlowFunc(("ReadData %02X\n", u32Formats));
                pClientData->State.u32RequestedFormat = u32Formats;
                pClientData->State.fHostMsgReadData = true;
            } break;
            case VBOX_SHARED_CLIPBOARD_HOST_MSG_REPORT_FORMATS:
            {
                if (   vboxSvcClipboardGetMode () != VBOX_SHARED_CLIPBOARD_MODE_HOST_TO_GUEST
                    && vboxSvcClipboardGetMode () != VBOX_SHARED_CLIPBOARD_MODE_BIDIRECTIONAL)
                {
                    /* Skip the message. */
                    break;
                }

                LogFlowFunc(("Formats %02X\n", u32Formats));
                pClientData->State.u32AvailableFormats = u32Formats;
                pClientData->State.fHostMsgFormats = true;
            } break;
            default:
            {
                /* Invalid message. */
                LogFlowFunc(("invalid message %d\n", u32Msg));
            } break;
        }

        if (pClientData->State.fAsync)
        {
            /* The client waits for a response. */
            bool fMessageReturned = vboxSvcClipboardReturnMsg(pClientData, pClientData->State.async.paParms);

            /* Make a copy of the handle. */
            VBOXHGCMCALLHANDLE callHandle = pClientData->State.async.callHandle;

            if (fMessageReturned)
            {
                /* There is a response. */
                pClientData->State.fAsync = false;
            }

            VBoxSvcClipboardUnlock();

            if (fMessageReturned)
            {
                LogFlowFunc(("CallComplete\n"));
                g_pHelpers->pfnCallComplete(callHandle, VINF_SUCCESS);
            }
        }
        else
        {
            VBoxSvcClipboardUnlock();
        }
    }
}

static int svcInit(void)
{
    int rc = RTCritSectInit(&g_CritSect);

    if (RT_SUCCESS(rc))
    {
        vboxSvcClipboardModeSet(VBOX_SHARED_CLIPBOARD_MODE_OFF);

        rc = VBoxClipboardSvcImplInit();

        /* Clean up on failure, because 'svnUnload' will not be called
         * if the 'svcInit' returns an error.
         */
        if (RT_FAILURE(rc))
        {
            RTCritSectDelete(&g_CritSect);
        }
    }

    return rc;
}

static DECLCALLBACK(int) svcUnload(void *)
{
    VBoxClipboardSvcImplDestroy();
    RTCritSectDelete(&g_CritSect);
    return VINF_SUCCESS;
}

/**
 * Disconnect the host side of the shared clipboard and send a "host disconnected" message
 * to the guest side.
 */
static DECLCALLBACK(int) svcDisconnect(void *, uint32_t u32ClientID, void *pvClient)
{
    RT_NOREF(u32ClientID);

    PVBOXCLIPBOARDCLIENTDATA pClientData = (PVBOXCLIPBOARDCLIENTDATA)pvClient;

    LogFunc(("u32ClientID = %d\n", u32ClientID));

    vboxSvcClipboardReportMsg(pClientData, VBOX_SHARED_CLIPBOARD_HOST_MSG_QUIT, 0);

    vboxSvcClipboardCompleteReadData(pClientData, VERR_NO_DATA, 0);

#ifdef VBOX_VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
    vboxClipboardSvcURIDestroy(&pClientData->URI);
#endif

    VBoxClipboardSvcImplDisconnect(pClientData);

    g_pClientData = NULL;

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) svcConnect(void *, uint32_t u32ClientID, void *pvClient, uint32_t fRequestor, bool fRestoring)
{
    RT_NOREF(fRequestor, fRestoring);
    PVBOXCLIPBOARDCLIENTDATA pClientData = (PVBOXCLIPBOARDCLIENTDATA)pvClient;

    /* If there is already a client connected then we want to release it first. */
    if (g_pClientData != NULL)
    {
        uint32_t u32OldClientID = g_pClientData->State.u32ClientID;

        svcDisconnect(NULL, u32OldClientID, g_pClientData);

        /* And free the resources in the hgcm subsystem. */
        g_pHelpers->pfnDisconnectClient(g_pHelpers->pvInstance, u32OldClientID);
    }

    /* Register the client.
     * Note: Do *not* memset the struct, as it contains classes (for caching). */
    pClientData->State.u32ClientID = u32ClientID;

    int rc = VBoxClipboardSvcImplConnect(pClientData, VBoxSvcClipboardGetHeadless());
#ifdef VBOX_VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
    if (RT_SUCCESS(rc))
        rc = vboxClipboardSvcURICreate(&pClientData->URI);
#endif

    if (RT_SUCCESS(rc))
    {
        g_pClientData = pClientData;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static DECLCALLBACK(void) svcCall(void *,
                                  VBOXHGCMCALLHANDLE callHandle,
                                  uint32_t u32ClientID,
                                  void *pvClient,
                                  uint32_t u32Function,
                                  uint32_t cParms,
                                  VBOXHGCMSVCPARM paParms[],
                                  uint64_t tsArrival)
{
    RT_NOREF(u32ClientID, tsArrival);
    int rc = VINF_SUCCESS;

    LogFunc(("u32ClientID = %d, fn = %d, cParms = %d, pparms = %d\n",
             u32ClientID, u32Function, cParms, paParms));

    PVBOXCLIPBOARDCLIENTDATA pClientData = (PVBOXCLIPBOARDCLIENTDATA)pvClient;

    bool fAsynchronousProcessing = false;

#ifdef DEBUG
    uint32_t i;

    for (i = 0; i < cParms; i++)
    {
        /** @todo parameters other than 32 bit */
        LogFunc(("    pparms[%d]: type %d value %d\n", i, paParms[i].type, paParms[i].u.uint32));
    }
#endif

    switch (u32Function)
    {
        case VBOX_SHARED_CLIPBOARD_FN_GET_HOST_MSG:
        {
            /* The quest requests a host message. */
            LogFunc(("VBOX_SHARED_CLIPBOARD_FN_GET_HOST_MSG\n"));

            if (cParms != VBOX_SHARED_CLIPBOARD_CPARMS_GET_HOST_MSG)
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else if (   paParms[0].type != VBOX_HGCM_SVC_PARM_32BIT   /* msg */
                     || paParms[1].type != VBOX_HGCM_SVC_PARM_32BIT   /* formats */
                    )
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                /* Atomically verify the client's state. */
                if (VBoxSvcClipboardLock())
                {
                    bool fMessageReturned = vboxSvcClipboardReturnMsg (pClientData, paParms);

                    if (fMessageReturned)
                    {
                        /* Just return to the caller. */
                        pClientData->State.fAsync = false;
                    }
                    else
                    {
                        /* No event available at the time. Process asynchronously. */
                        fAsynchronousProcessing = true;

                        pClientData->State.fAsync           = true;
                        pClientData->State.async.callHandle = callHandle;
                        pClientData->State.async.paParms    = paParms;

                        LogFunc(("async.\n"));
                    }

                    VBoxSvcClipboardUnlock();
                }
                else
                {
                    rc = VERR_NOT_SUPPORTED;
                }
            }
        } break;

        case VBOX_SHARED_CLIPBOARD_FN_REPORT_FORMATS:
        {
            /* The guest reports that some formats are available. */
            LogFunc(("VBOX_SHARED_CLIPBOARD_FN_FORMATS\n"));

            if (cParms != VBOX_SHARED_CLIPBOARD_CPARMS_FORMATS)
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else if (   paParms[0].type != VBOX_HGCM_SVC_PARM_32BIT   /* formats */
                    )
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                uint32_t u32Formats;

                rc = VBoxHGCMParmUInt32Get(&paParms[0], &u32Formats);

                if (RT_SUCCESS (rc))
                {
                    if (   vboxSvcClipboardGetMode () != VBOX_SHARED_CLIPBOARD_MODE_GUEST_TO_HOST
                        && vboxSvcClipboardGetMode () != VBOX_SHARED_CLIPBOARD_MODE_BIDIRECTIONAL)
                    {
                        rc = VERR_NOT_SUPPORTED;
                        break;
                    }

                    if (g_pfnExtension)
                    {
                        VBOXCLIPBOARDEXTPARMS parms;

                        parms.u32Format = u32Formats;

                        g_pfnExtension (g_pvExtension, VBOX_CLIPBOARD_EXT_FN_FORMAT_ANNOUNCE, &parms, sizeof (parms));
                    }
                    else
                    {
                        VBoxClipboardSvcImplFormatAnnounce (pClientData, u32Formats);
                    }
                }
            }
        } break;

        case VBOX_SHARED_CLIPBOARD_FN_READ_DATA:
        {
            /* The guest wants to read data in the given format. */
            LogFunc(("VBOX_SHARED_CLIPBOARD_FN_READ_DATA\n"));

            if (cParms != VBOX_SHARED_CLIPBOARD_CPARMS_READ_DATA)
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else if (   paParms[0].type != VBOX_HGCM_SVC_PARM_32BIT   /* format */
                     || paParms[1].type != VBOX_HGCM_SVC_PARM_PTR     /* ptr */
                     || paParms[2].type != VBOX_HGCM_SVC_PARM_32BIT   /* size */
                    )
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                uint32_t u32Format;
                void     *pv;
                uint32_t cb;

                rc = VBoxHGCMParmUInt32Get(&paParms[0], &u32Format);

                if (RT_SUCCESS (rc))
                {
                    rc = VBoxHGCMParmPtrGet(&paParms[1], &pv, &cb);

                    if (RT_SUCCESS (rc))
                    {
                        if (   vboxSvcClipboardGetMode () != VBOX_SHARED_CLIPBOARD_MODE_HOST_TO_GUEST
                            && vboxSvcClipboardGetMode () != VBOX_SHARED_CLIPBOARD_MODE_BIDIRECTIONAL)
                        {
                            rc = VERR_NOT_SUPPORTED;
                            break;
                        }

                        uint32_t cbActual = 0;

                        if (g_pfnExtension)
                        {
                            VBOXCLIPBOARDEXTPARMS parms;

                            parms.u32Format = u32Format;
                            parms.u.pvData = pv;
                            parms.cbData = cb;

                            g_fReadingData = true;
                            rc = g_pfnExtension (g_pvExtension, VBOX_CLIPBOARD_EXT_FN_DATA_READ, &parms, sizeof (parms));
                            LogFlowFunc(("DATA: g_fDelayedAnnouncement = %d, g_u32DelayedFormats = 0x%x\n", g_fDelayedAnnouncement, g_u32DelayedFormats));
                            if (g_fDelayedAnnouncement)
                            {
                                vboxSvcClipboardReportMsg (g_pClientData, VBOX_SHARED_CLIPBOARD_HOST_MSG_REPORT_FORMATS, g_u32DelayedFormats);
                                g_fDelayedAnnouncement = false;
                                g_u32DelayedFormats = 0;
                            }
                            g_fReadingData = false;

                            if (RT_SUCCESS (rc))
                            {
                                cbActual = parms.cbData;
                            }
                        }
                        else
                        {
                            /* Release any other pending read, as we only
                             * support one pending read at one time. */
                            vboxSvcClipboardCompleteReadData(pClientData, VERR_NO_DATA, 0);
                            rc = VBoxClipboardSvcImplReadData (pClientData, u32Format, pv, cb, &cbActual);
                        }

                        /* Remember our read request until it is completed.
                         * See the protocol description above for more
                         * information. */
                        if (rc == VINF_HGCM_ASYNC_EXECUTE)
                        {
                            if (VBoxSvcClipboardLock())
                            {
                                pClientData->State.asyncRead.callHandle = callHandle;
                                pClientData->State.asyncRead.paParms    = paParms;
                                pClientData->State.fReadPending         = true;
                                fAsynchronousProcessing = true;
                                VBoxSvcClipboardUnlock();
                            }
                            else
                                rc = VERR_NOT_SUPPORTED;
                        }
                        else if (RT_SUCCESS (rc))
                        {
                            VBoxHGCMParmUInt32Set(&paParms[2], cbActual);
                        }
                    }
                }
            }
        } break;

        case VBOX_SHARED_CLIPBOARD_FN_WRITE_DATA:
        {
            /* The guest writes the requested data. */
            LogFunc(("VBOX_SHARED_CLIPBOARD_FN_WRITE_DATA\n"));

            if (cParms != VBOX_SHARED_CLIPBOARD_CPARMS_WRITE_DATA)
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else if (   paParms[0].type != VBOX_HGCM_SVC_PARM_32BIT   /* format */
                     || paParms[1].type != VBOX_HGCM_SVC_PARM_PTR     /* ptr */
                    )
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                void *pv;
                uint32_t cb;
                uint32_t u32Format;

                rc = VBoxHGCMParmUInt32Get(&paParms[0], &u32Format);

                if (RT_SUCCESS (rc))
                {
                    rc = VBoxHGCMParmPtrGet(&paParms[1], &pv, &cb);

                    if (RT_SUCCESS (rc))
                    {
                        if (   vboxSvcClipboardGetMode () != VBOX_SHARED_CLIPBOARD_MODE_GUEST_TO_HOST
                            && vboxSvcClipboardGetMode () != VBOX_SHARED_CLIPBOARD_MODE_BIDIRECTIONAL)
                        {
                            rc = VERR_NOT_SUPPORTED;
                            break;
                        }

                        if (g_pfnExtension)
                        {
                            VBOXCLIPBOARDEXTPARMS parms;

                            parms.u32Format = u32Format;
                            parms.u.pvData = pv;
                            parms.cbData = cb;

                            g_pfnExtension (g_pvExtension, VBOX_CLIPBOARD_EXT_FN_DATA_WRITE, &parms, sizeof (parms));
                        }
                        else
                        {
                            VBoxClipboardSvcImplWriteData (pClientData, pv, cb, u32Format);
                        }
                    }
                }
            }
        } break;

        default:
        {
#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
            rc = vboxClipboardSvcURIHandler(u32ClientID, pvClient, u32Function, cParms, paParms, tsArrival,
                                            &fAsynchronousProcessing);
#else
            rc = VERR_NOT_IMPLEMENTED;
#endif
        }
    }

    LogFlowFunc(("rc = %Rrc\n", rc));

    if (!fAsynchronousProcessing)
    {
        g_pHelpers->pfnCallComplete(callHandle, rc);
    }
}

/** If the client in the guest is waiting for a read operation to complete
 * then complete it, otherwise return.  See the protocol description in the
 * shared clipboard module description. */
void vboxSvcClipboardCompleteReadData(PVBOXCLIPBOARDCLIENTDATA pClientData, int rc, uint32_t cbActual)
{
    VBOXHGCMCALLHANDLE callHandle = NULL;
    VBOXHGCMSVCPARM *paParms = NULL;
    bool fReadPending = false;
    if (VBoxSvcClipboardLock())  /* if not can we do anything useful? */
    {
        callHandle   = pClientData->State.asyncRead.callHandle;
        paParms      = pClientData->State.asyncRead.paParms;
        fReadPending = pClientData->State.fReadPending;
        pClientData->State.fReadPending = false;
        VBoxSvcClipboardUnlock();
    }
    if (fReadPending)
    {
        VBoxHGCMParmUInt32Set(&paParms[2], cbActual);
        g_pHelpers->pfnCallComplete(callHandle, rc);
    }
}

/*
 * We differentiate between a function handler for the guest and one for the host.
 */
static DECLCALLBACK(int) svcHostCall(void *,
                                     uint32_t u32Function,
                                     uint32_t cParms,
                                     VBOXHGCMSVCPARM paParms[])
{
    int rc = VINF_SUCCESS;

    LogFunc(("svcHostCall: fn = %d, cParms = %d, pparms = %d\n",
             u32Function, cParms, paParms));

    switch (u32Function)
    {
        case VBOX_SHARED_CLIPBOARD_HOST_FN_SET_MODE:
        {
            LogFunc(("VBOX_SHARED_CLIPBOARD_HOST_FN_SET_MODE\n"));

            if (cParms != 1)
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else if (   paParms[0].type != VBOX_HGCM_SVC_PARM_32BIT   /* mode */
                    )
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                uint32_t u32Mode = VBOX_SHARED_CLIPBOARD_MODE_OFF;

                rc = VBoxHGCMParmUInt32Get (&paParms[0], &u32Mode);

                /* The setter takes care of invalid values. */
                vboxSvcClipboardModeSet (u32Mode);
            }
        } break;

        case VBOX_SHARED_CLIPBOARD_HOST_FN_SET_HEADLESS:
        {
            uint32_t u32Headless = g_fHeadless;

            rc = VERR_INVALID_PARAMETER;
            if (cParms != 1)
                break;
            rc = VBoxHGCMParmUInt32Get (&paParms[0], &u32Headless);
            if (RT_SUCCESS(rc))
                LogFlowFunc(("VBOX_SHARED_CLIPBOARD_HOST_FN_SET_HEADLESS, u32Headless=%u\n",
                            (unsigned) u32Headless));
            g_fHeadless = RT_BOOL(u32Headless);
        } break;

        default:
        {
#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
            rc = vboxClipboardSvcURIHostHandler(u32Function, cParms, paParms);
#endif
        } break;
    }

    LogFlowFunc(("svcHostCall: rc = %Rrc\n", rc));
    return rc;
}

#ifndef UNIT_TEST
/**
 * SSM descriptor table for the VBOXCLIPBOARDCLIENTDATA structure.
 */
static SSMFIELD const g_aClipboardClientDataFields[] =
{
    SSMFIELD_ENTRY(VBOXCLIPBOARDCLIENTSTATE, u32ClientID),  /* for validation purposes */
    SSMFIELD_ENTRY(VBOXCLIPBOARDCLIENTSTATE, fHostMsgQuit),
    SSMFIELD_ENTRY(VBOXCLIPBOARDCLIENTSTATE, fHostMsgReadData),
    SSMFIELD_ENTRY(VBOXCLIPBOARDCLIENTSTATE, fHostMsgFormats),
    SSMFIELD_ENTRY(VBOXCLIPBOARDCLIENTSTATE, u32RequestedFormat),
    SSMFIELD_ENTRY_TERM()
};
#endif

static DECLCALLBACK(int) svcSaveState(void *, uint32_t u32ClientID, void *pvClient, PSSMHANDLE pSSM)
{
    RT_NOREF(u32ClientID);

#ifndef UNIT_TEST
    /*
     * When the state will be restored, pending requests will be reissued
     * by VMMDev. The service therefore must save state as if there were no
     * pending request.
     * Pending requests, if any, will be completed in svcDisconnect.
     */
    LogFunc(("u32ClientID = %d\n", u32ClientID));

    PVBOXCLIPBOARDCLIENTDATA pClientData = (PVBOXCLIPBOARDCLIENTDATA)pvClient;

    /* This field used to be the length. We're using it as a version field
       with the high bit set. */
    SSMR3PutU32(pSSM, UINT32_C(0x80000002));
    int rc = SSMR3PutStructEx(pSSM, pClientData, sizeof(*pClientData), 0 /*fFlags*/, &g_aClipboardClientDataFields[0], NULL);
    AssertRCReturn(rc, rc);

#else  /* UNIT_TEST */
    RT_NOREF3(u32ClientID, pvClient, pSSM);
#endif /* UNIT_TEST */
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) svcLoadState(void *, uint32_t u32ClientID, void *pvClient, PSSMHANDLE pSSM, uint32_t uVersion)
{
#ifndef UNIT_TEST
    RT_NOREF(u32ClientID, uVersion);

    LogFunc(("u32ClientID = %d\n", u32ClientID));

    PVBOXCLIPBOARDCLIENTDATA pClientData   = (PVBOXCLIPBOARDCLIENTDATA)pvClient;
    AssertPtr(pClientData);

    /* Existing client can not be in async state yet. */
    Assert(!pClientData->State.fAsync);

    /* Save the client ID for data validation. */
    /** @todo isn't this the same as u32ClientID? Playing safe for now... */
    uint32_t const u32ClientIDOld = pClientData->State.u32ClientID;

    /* Restore the client data. */
    uint32_t lenOrVer;
    int rc = SSMR3GetU32(pSSM, &lenOrVer);
    AssertRCReturn(rc, rc);
    if (lenOrVer == UINT32_C(0x80000002))
    {
        rc = SSMR3GetStructEx(pSSM, pClientData, sizeof(*pClientData), 0 /*fFlags*/, &g_aClipboardClientDataFields[0], NULL);
        AssertRCReturn(rc, rc);
    }
    else
    {
        LogFunc(("Client data size mismatch: got %#x\n", lenOrVer));
        return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
    }

    /* Verify the client ID. */
    if (pClientData->State.u32ClientID != u32ClientIDOld)
    {
        LogFunc(("Client ID mismatch: expected %d, got %d\n", u32ClientIDOld, pClientData->State.u32ClientID));
        pClientData->State.u32ClientID = u32ClientIDOld;
        return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
    }

    /* Actual host data are to be reported to guest (SYNC). */
    VBoxClipboardSvcImplSync(pClientData);

#else  /* UNIT_TEST*/
    RT_NOREF(u32ClientID, pvClient, pSSM, uVersion);
#endif /* UNIT_TEST */
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) extCallback(uint32_t u32Function, uint32_t u32Format, void *pvData, uint32_t cbData)
{
    RT_NOREF2(pvData, cbData);
    if (g_pClientData != NULL)
    {
        switch (u32Function)
        {
            case VBOX_CLIPBOARD_EXT_FN_FORMAT_ANNOUNCE:
            {
                LogFlowFunc(("ANNOUNCE: g_fReadingData = %d\n", g_fReadingData));
                if (g_fReadingData)
                {
                    g_fDelayedAnnouncement = true;
                    g_u32DelayedFormats = u32Format;
                }
                else
                {
                    vboxSvcClipboardReportMsg(g_pClientData, VBOX_SHARED_CLIPBOARD_HOST_MSG_REPORT_FORMATS, u32Format);
                }
            } break;

            case VBOX_CLIPBOARD_EXT_FN_DATA_READ:
            {
                vboxSvcClipboardReportMsg(g_pClientData, VBOX_SHARED_CLIPBOARD_HOST_MSG_READ_DATA, u32Format);
            } break;

            default:
                return VERR_NOT_SUPPORTED;
        }
    }

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) svcRegisterExtension(void *, PFNHGCMSVCEXT pfnExtension, void *pvExtension)
{
    LogFlowFunc(("pfnExtension=%p\n", pfnExtension));

    VBOXCLIPBOARDEXTPARMS parms;

    if (pfnExtension)
    {
        /* Install extension. */
        g_pfnExtension = pfnExtension;
        g_pvExtension = pvExtension;

        parms.u.pfnCallback = extCallback;
        g_pfnExtension(g_pvExtension, VBOX_CLIPBOARD_EXT_FN_SET_CALLBACK, &parms, sizeof(parms));
    }
    else
    {
        if (g_pfnExtension)
        {
            parms.u.pfnCallback = NULL;
            g_pfnExtension(g_pvExtension, VBOX_CLIPBOARD_EXT_FN_SET_CALLBACK, &parms, sizeof(parms));
        }

        /* Uninstall extension. */
        g_pfnExtension = NULL;
        g_pvExtension = NULL;
    }

    return VINF_SUCCESS;
}

extern "C" DECLCALLBACK(DECLEXPORT(int)) VBoxHGCMSvcLoad(VBOXHGCMSVCFNTABLE *ptable)
{
    int rc = VINF_SUCCESS;

    LogFlowFunc(("ptable=%p\n", ptable));

    if (!ptable)
    {
        rc = VERR_INVALID_PARAMETER;
    }
    else
    {
        LogFunc(("ptable->cbSize = %d, ptable->u32Version = 0x%08X\n", ptable->cbSize, ptable->u32Version));

        if (   ptable->cbSize     != sizeof (VBOXHGCMSVCFNTABLE)
            || ptable->u32Version != VBOX_HGCM_SVC_VERSION)
        {
            rc = VERR_INVALID_PARAMETER;
        }
        else
        {
            g_pHelpers = ptable->pHelpers;

            ptable->cbClient = sizeof(VBOXCLIPBOARDCLIENTDATA);

            ptable->pfnUnload     = svcUnload;
            ptable->pfnConnect    = svcConnect;
            ptable->pfnDisconnect = svcDisconnect;
            ptable->pfnCall       = svcCall;
            ptable->pfnHostCall   = svcHostCall;
            ptable->pfnSaveState  = svcSaveState;
            ptable->pfnLoadState  = svcLoadState;
            ptable->pfnRegisterExtension  = svcRegisterExtension;
            ptable->pfnNotify     = NULL;
            ptable->pvService     = NULL;

            /* Service specific initialization. */
            rc = svcInit();
        }
    }

    return rc;
}
