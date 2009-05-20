/** @file
 *
 * Shared Clipboard:
 * Host service entry points.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */


/** @page pg_hostclip       The Shared Clipboard Host Service
 *
 * The shared clipboard host service provides a proxy between the host's
 * clipboard and a similar proxy running on a guest.  The service is split
 * into a platform-independant core and platform-specific backends.  The
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
 * VBOX_SHARED_CLIPBOARD_HOST_MSG_FORMATS (to notify the guest that new
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

#include <VBox/HostServices/VBoxClipboardSvc.h>
#include <VBox/HostServices/VBoxClipboardExt.h>

#include <iprt/alloc.h>
#include <iprt/string.h>
#include <iprt/assert.h>
#include <iprt/critsect.h>
#include <VBox/ssm.h>

#include "VBoxClipboard.h"

static void VBoxHGCMParmUInt32Set (VBOXHGCMSVCPARM *pParm, uint32_t u32)
{
    pParm->type = VBOX_HGCM_SVC_PARM_32BIT;
    pParm->u.uint32 = u32;
}

static int VBoxHGCMParmUInt32Get (VBOXHGCMSVCPARM *pParm, uint32_t *pu32)
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

static int VBoxHGCMParmPtrGet (VBOXHGCMSVCPARM *pParm, void **ppv, uint32_t *pcb)
{
    if (pParm->type == VBOX_HGCM_SVC_PARM_PTR)
    {
        *ppv = pParm->u.pointer.addr;
        *pcb = pParm->u.pointer.size;
        return VINF_SUCCESS;
    }

    return VERR_INVALID_PARAMETER;
}

static PVBOXHGCMSVCHELPERS g_pHelpers;

static RTCRITSECT critsect;
static uint32_t g_u32Mode;

static PFNHGCMSVCEXT g_pfnExtension;
static void *g_pvExtension;

static VBOXCLIPBOARDCLIENTDATA *g_pClient;

/* Serialization of data reading and format announcements from the RDP client. */
static bool g_fReadingData = false;
static bool g_fDelayedAnnouncement = false;
static uint32_t g_u32DelayedFormats = 0;

static uint32_t vboxSvcClipboardMode (void)
{
    return g_u32Mode;
}

static void vboxSvcClipboardModeSet (uint32_t u32Mode)
{
    switch (u32Mode)
    {
        case VBOX_SHARED_CLIPBOARD_MODE_OFF:
        case VBOX_SHARED_CLIPBOARD_MODE_HOST_TO_GUEST:
        case VBOX_SHARED_CLIPBOARD_MODE_GUEST_TO_HOST:
        case VBOX_SHARED_CLIPBOARD_MODE_BIDIRECTIONAL:
            g_u32Mode = u32Mode;
            break;

        default:
            g_u32Mode = VBOX_SHARED_CLIPBOARD_MODE_OFF;
    }
}

bool vboxSvcClipboardLock (void)
{
    return RT_SUCCESS(RTCritSectEnter (&critsect));
}

void vboxSvcClipboardUnlock (void)
{
    RTCritSectLeave (&critsect);
}

/* Set the HGCM parameters according to pending messages.
 * Executed under the clipboard lock.
 */
static bool vboxSvcClipboardReturnMsg (VBOXCLIPBOARDCLIENTDATA *pClient, VBOXHGCMSVCPARM paParms[])
{
    /* Message priority is taken into account. */
    if (pClient->fMsgQuit)
    {
        LogFlow(("vboxSvcClipboardReturnMsg: Quit\n"));
        VBoxHGCMParmUInt32Set (&paParms[0], VBOX_SHARED_CLIPBOARD_HOST_MSG_QUIT);
        VBoxHGCMParmUInt32Set (&paParms[1], 0);
        pClient->fMsgQuit = false;
    }
    else if (pClient->fMsgReadData)
    {
        LogFlow(("vboxSvcClipboardReturnMsg: ReadData %02X\n", pClient->u32RequestedFormat));
        VBoxHGCMParmUInt32Set (&paParms[0], VBOX_SHARED_CLIPBOARD_HOST_MSG_READ_DATA);
        VBoxHGCMParmUInt32Set (&paParms[1], pClient->u32RequestedFormat);
        pClient->fMsgReadData = false;
    }
    else if (pClient->fMsgFormats)
    {
        LogFlow(("vboxSvcClipboardReturnMsg: Formats %02X\n", pClient->u32AvailableFormats));
        VBoxHGCMParmUInt32Set (&paParms[0], VBOX_SHARED_CLIPBOARD_HOST_MSG_FORMATS);
        VBoxHGCMParmUInt32Set (&paParms[1], pClient->u32AvailableFormats);
        pClient->fMsgFormats = false;
    }
    else
    {
        /* No pending messages. */
        LogFlow(("vboxSvcClipboardReturnMsg: no message\n"));
        return false;
    }

    /* Message information assigned. */
    return true;
}

void vboxSvcClipboardReportMsg (VBOXCLIPBOARDCLIENTDATA *pClient, uint32_t u32Msg, uint32_t u32Formats)
{
    if (vboxSvcClipboardLock ())
    {
        switch (u32Msg)
        {
            case VBOX_SHARED_CLIPBOARD_HOST_MSG_QUIT:
            {
                LogFlow(("vboxSvcClipboardReportMsg: Quit\n"));
                pClient->fMsgQuit = true;
            } break;
            case VBOX_SHARED_CLIPBOARD_HOST_MSG_READ_DATA:
            {
                if (   vboxSvcClipboardMode () != VBOX_SHARED_CLIPBOARD_MODE_GUEST_TO_HOST
                    && vboxSvcClipboardMode () != VBOX_SHARED_CLIPBOARD_MODE_BIDIRECTIONAL)
                {
                    /* Skip the message. */
                    break;
                }

                LogFlow(("vboxSvcClipboardReportMsg: ReadData %02X\n", u32Formats));
                pClient->u32RequestedFormat = u32Formats;
                pClient->fMsgReadData = true;
            } break;
            case VBOX_SHARED_CLIPBOARD_HOST_MSG_FORMATS:
            {
                if (   vboxSvcClipboardMode () != VBOX_SHARED_CLIPBOARD_MODE_HOST_TO_GUEST
                    && vboxSvcClipboardMode () != VBOX_SHARED_CLIPBOARD_MODE_BIDIRECTIONAL)
                {
                    /* Skip the message. */
                    break;
                }

                LogFlow(("vboxSvcClipboardReportMsg: Formats %02X\n", u32Formats));
                pClient->u32AvailableFormats = u32Formats;
                pClient->fMsgFormats = true;
            } break;
            default:
            {
                /* Invalid message. */
                LogFlow(("vboxSvcClipboardReportMsg: invalid message %d\n", u32Msg));
            } break;
        }

        if (pClient->fAsync)
        {
            /* The client waits for a responce. */
            bool fMessageReturned = vboxSvcClipboardReturnMsg (pClient, pClient->async.paParms);

            /* Make a copy of the handle. */
            VBOXHGCMCALLHANDLE callHandle = pClient->async.callHandle;

            if (fMessageReturned)
            {
                /* There is a responce. */
                pClient->fAsync = false;
            }

            vboxSvcClipboardUnlock ();

            if (fMessageReturned)
            {
                LogFlow(("vboxSvcClipboardReportMsg: CallComplete\n"));
                g_pHelpers->pfnCallComplete (callHandle, VINF_SUCCESS);
            }
        }
        else
        {
            vboxSvcClipboardUnlock ();
        }
    }
}

static int svcInit (void)
{
    int rc = RTCritSectInit (&critsect);

    if (RT_SUCCESS (rc))
    {
        vboxSvcClipboardModeSet (VBOX_SHARED_CLIPBOARD_MODE_OFF);

        rc = vboxClipboardInit ();

        /* Clean up on failure, because 'svnUnload' will not be called
         * if the 'svcInit' returns an error.
         */
        if (RT_FAILURE (rc))
        {
            RTCritSectDelete (&critsect);
        }
    }

    return rc;
}

static DECLCALLBACK(int) svcUnload (void *)
{
    vboxClipboardDestroy ();
    RTCritSectDelete (&critsect);
    return VINF_SUCCESS;
}

/**
 * Disconnect the host side of the shared clipboard and send a "host disconnected" message
 * to the guest side.
 */
static DECLCALLBACK(int) svcDisconnect (void *, uint32_t u32ClientID, void *pvClient)
{
    VBOXCLIPBOARDCLIENTDATA *pClient = (VBOXCLIPBOARDCLIENTDATA *)pvClient;

    vboxSvcClipboardReportMsg (pClient, VBOX_SHARED_CLIPBOARD_HOST_MSG_QUIT, 0);

    vboxClipboardDisconnect (pClient);

    memset (pClient, 0, sizeof (*pClient));

    g_pClient = NULL;

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) svcConnect (void *, uint32_t u32ClientID, void *pvClient)
{
    VBOXCLIPBOARDCLIENTDATA *pClient = (VBOXCLIPBOARDCLIENTDATA *)pvClient;

    int rc = VINF_SUCCESS;

    /* If there is already a client connected then we want to release it first. */
    if (g_pClient != NULL)
    {
        uint32_t u32ClientID = g_pClient->u32ClientID;

        svcDisconnect(NULL, u32ClientID, g_pClient);
        /* And free the resources in the hgcm subsystem. */
        g_pHelpers->pfnDisconnectClient(g_pHelpers->pvInstance, u32ClientID);
    }

    /* Register the client. */
    memset (pClient, 0, sizeof (*pClient));

    pClient->u32ClientID = u32ClientID;

    rc = vboxClipboardConnect (pClient);

    if (RT_SUCCESS (rc))
    {
        g_pClient = pClient;
    }

    Log(("vboxClipboardConnect: rc = %Rrc\n", rc));

    return rc;
}

static DECLCALLBACK(void) svcCall (void *,
                                   VBOXHGCMCALLHANDLE callHandle,
                                   uint32_t u32ClientID,
                                   void *pvClient,
                                   uint32_t u32Function,
                                   uint32_t cParms,
                                   VBOXHGCMSVCPARM paParms[])
{
    int rc = VINF_SUCCESS;

    Log(("svcCall: u32ClientID = %d, fn = %d, cParms = %d, pparms = %d\n",
         u32ClientID, u32Function, cParms, paParms));

    VBOXCLIPBOARDCLIENTDATA *pClient = (VBOXCLIPBOARDCLIENTDATA *)pvClient;

    bool fAsynchronousProcessing = false;

#ifdef DEBUG
    uint32_t i;

    for (i = 0; i < cParms; i++)
    {
        /** @todo parameters other than 32 bit */
        Log(("    pparms[%d]: type %d value %d\n", i, paParms[i].type, paParms[i].u.uint32));
    }
#endif

    switch (u32Function)
    {
        case VBOX_SHARED_CLIPBOARD_FN_GET_HOST_MSG:
        {
            /* The quest requests a host message. */
            Log(("svcCall: VBOX_SHARED_CLIPBOARD_FN_GET_HOST_MSG\n"));

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
                if (vboxSvcClipboardLock ())
                {
                    bool fMessageReturned = vboxSvcClipboardReturnMsg (pClient, paParms);

                    if (fMessageReturned)
                    {
                        /* Just return to the caller. */
                        pClient->fAsync = false;
                    }
                    else
                    {
                        /* No event available at the time. Process asynchronously. */
                        fAsynchronousProcessing = true;

                        pClient->fAsync           = true;
                        pClient->async.callHandle = callHandle;
                        pClient->async.paParms    = paParms;

                        Log(("svcCall: async.\n"));
                    }

                    vboxSvcClipboardUnlock ();
                }
                else
                {
                    rc = VERR_NOT_SUPPORTED;
                }
            }
        } break;

        case VBOX_SHARED_CLIPBOARD_FN_FORMATS:
        {
            /* The guest reports that some formats are available. */
            Log(("svcCall: VBOX_SHARED_CLIPBOARD_FN_FORMATS\n"));

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

                rc = VBoxHGCMParmUInt32Get (&paParms[0], &u32Formats);

                if (RT_SUCCESS (rc))
                {
                    if (   vboxSvcClipboardMode () != VBOX_SHARED_CLIPBOARD_MODE_GUEST_TO_HOST
                        && vboxSvcClipboardMode () != VBOX_SHARED_CLIPBOARD_MODE_BIDIRECTIONAL)
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
                        vboxClipboardFormatAnnounce (pClient, u32Formats);
                    }
                }
            }
        } break;

        case VBOX_SHARED_CLIPBOARD_FN_READ_DATA:
        {
            /* The guest wants to read data in the given format. */
            Log(("svcCall: VBOX_SHARED_CLIPBOARD_FN_READ_DATA\n"));

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

                rc = VBoxHGCMParmUInt32Get (&paParms[0], &u32Format);

                if (RT_SUCCESS (rc))
                {
                    rc = VBoxHGCMParmPtrGet (&paParms[1], &pv, &cb);

                    if (RT_SUCCESS (rc))
                    {
                        if (   vboxSvcClipboardMode () != VBOX_SHARED_CLIPBOARD_MODE_HOST_TO_GUEST
                            && vboxSvcClipboardMode () != VBOX_SHARED_CLIPBOARD_MODE_BIDIRECTIONAL)
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
                            LogFlow(("DATA: g_fDelayedAnnouncement = %d, g_u32DelayedFormats = 0x%x\n", g_fDelayedAnnouncement, g_u32DelayedFormats));
                            if (g_fDelayedAnnouncement)
                            {
                                vboxSvcClipboardReportMsg (g_pClient, VBOX_SHARED_CLIPBOARD_HOST_MSG_FORMATS, g_u32DelayedFormats);
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
                            vboxSvcClipboardCompleteReadData(pClient, VERR_NO_DATA, 0);
                            rc = vboxClipboardReadData (pClient, u32Format, pv, cb, &cbActual);
                        }

                        /* Remember our read request until it is completed.
                         * See the protocol description above for more
                         * information. */
                        if (rc == VINF_HGCM_ASYNC_EXECUTE)
                        {
                            if (vboxSvcClipboardLock())
                            {
                                pClient->asyncRead.callHandle = callHandle;
                                pClient->asyncRead.paParms    = paParms;
                                pClient->fReadPending         = true;
                                fAsynchronousProcessing = true;
                                vboxSvcClipboardUnlock();
                            }
                            else
                                rc = VERR_NOT_SUPPORTED;
                        }
                        else if (RT_SUCCESS (rc))
                        {
                            VBoxHGCMParmUInt32Set (&paParms[2], cbActual);
                        }
                    }
                }
            }
        } break;

        case VBOX_SHARED_CLIPBOARD_FN_WRITE_DATA:
        {
            /* The guest writes the requested data. */
            Log(("svcCall: VBOX_SHARED_CLIPBOARD_FN_WRITE_DATA\n"));

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

                rc = VBoxHGCMParmUInt32Get (&paParms[0], &u32Format);

                if (RT_SUCCESS (rc))
                {
                    rc = VBoxHGCMParmPtrGet (&paParms[1], &pv, &cb);

                    if (RT_SUCCESS (rc))
                    {
                        if (   vboxSvcClipboardMode () != VBOX_SHARED_CLIPBOARD_MODE_GUEST_TO_HOST
                            && vboxSvcClipboardMode () != VBOX_SHARED_CLIPBOARD_MODE_BIDIRECTIONAL)
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
                            vboxClipboardWriteData (pClient, pv, cb, u32Format);
                        }
                    }
                }
            }
        } break;

        default:
        {
            rc = VERR_NOT_IMPLEMENTED;
        }
    }

    LogFlow(("svcCall: rc = %Rrc\n", rc));

    if (!fAsynchronousProcessing)
    {
        g_pHelpers->pfnCallComplete (callHandle, rc);
    }
}

/** If the client in the guest is waiting for a read operation to complete
 * then complete it, otherwise return.  See the protocol description in the
 * shared clipboard module description. */
void vboxSvcClipboardCompleteReadData(VBOXCLIPBOARDCLIENTDATA *pClient, int rc, uint32_t cbActual)
{
    VBOXHGCMCALLHANDLE callHandle = NULL;
    VBOXHGCMSVCPARM *paParms = NULL;
    bool fReadPending = false;
    if (vboxSvcClipboardLock())  /* if not can we do anything useful? */
    {
        callHandle   = pClient->asyncRead.callHandle;
        paParms      = pClient->asyncRead.paParms;
        fReadPending = pClient->fReadPending;
        pClient->fReadPending = false;
        vboxSvcClipboardUnlock();
    }
    if (fReadPending)
    {
        VBoxHGCMParmUInt32Set (&paParms[2], cbActual);
        g_pHelpers->pfnCallComplete (callHandle, rc);
    }
}

/*
 * We differentiate between a function handler for the guest and one for the host.
 */
static DECLCALLBACK(int) svcHostCall (void *,
                                      uint32_t u32Function,
                                      uint32_t cParms,
                                      VBOXHGCMSVCPARM paParms[])
{
    int rc = VINF_SUCCESS;

    Log(("svcHostCall: fn = %d, cParms = %d, pparms = %d\n",
         u32Function, cParms, paParms));

    switch (u32Function)
    {
        case VBOX_SHARED_CLIPBOARD_HOST_FN_SET_MODE:
        {
            Log(("svcCall: VBOX_SHARED_CLIPBOARD_HOST_FN_SET_MODE\n"));

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

        default:
            break;
    }

    LogFlow(("svcHostCall: rc = %Rrc\n", rc));
    return rc;
}

/** This structure corresponds to the original layout of the
 * VBOXCLIPBOARDCLIENTDATA structure.  As the structure was saved as a whole
 * when saving state, we need to remember it forever in order to preserve
 * compatibility.
 * @todo the first person who needs to make an incompatible change to the
 *       saved state should switch to saving individual data members.  So far,
 *       there are only three we care about anyway! */
typedef struct _CLIPSAVEDSTATEDATA
{
    struct _CLIPSAVEDSTATEDATA *pNext;
    struct _CLIPSAVEDSTATEDATA *pPrev;

    VBOXCLIPBOARDCONTEXT *pCtx;

    uint32_t u32ClientID;

    bool fAsync: 1; /* Guest is waiting for a message. */

    bool fMsgQuit: 1;
    bool fMsgReadData: 1;
    bool fMsgFormats: 1;

    struct {
        VBOXHGCMCALLHANDLE callHandle;
        VBOXHGCMSVCPARM *paParms;
    } async;

    struct {
         void *pv;
         uint32_t cb;
         uint32_t u32Format;
    } data;

    uint32_t u32AvailableFormats;
    uint32_t u32RequestedFormat;

} CLIPSAVEDSTATEDATA;

static DECLCALLBACK(int) svcSaveState(void *, uint32_t u32ClientID, void *pvClient, PSSMHANDLE pSSM)
{
    /* If there are any pending requests, they must be completed here. Since
     * the service is single threaded, there could be only requests
     * which the service itself has postponed.
     *
     * HGCM knows that the state is being saved and that the pfnComplete
     * calls are just clean ups. These requests are saved by the VMMDev.
     *
     * When the state will be restored, these requests will be reissued
     * by VMMDev. The service therefore must save state as if there were no
     * pending request.
     */
    Log(("svcSaveState: u32ClientID = %d\n", u32ClientID));

    VBOXCLIPBOARDCLIENTDATA *pClient = (VBOXCLIPBOARDCLIENTDATA *)pvClient;

    CLIPSAVEDSTATEDATA savedState = { 0 };
    /* Save client structure length & contents */
    int rc = SSMR3PutU32(pSSM, sizeof(savedState));
    AssertRCReturn(rc, rc);

    savedState.u32ClientID        = pClient->u32ClientID;
    savedState.fMsgQuit           = pClient->fMsgQuit;
    savedState.fMsgReadData       = pClient->fMsgReadData;
    savedState.fMsgFormats        = pClient->fMsgFormats;
    savedState.u32RequestedFormat = pClient->u32RequestedFormat;
    rc = SSMR3PutMem(pSSM, &savedState, sizeof(savedState));
    AssertRCReturn(rc, rc);

    if (pClient->fAsync)
    {
        g_pHelpers->pfnCallComplete (pClient->async.callHandle, VINF_SUCCESS /* error code is not important here. */);
        pClient->fAsync = false;
    }

    vboxSvcClipboardCompleteReadData(pClient, VINF_SUCCESS, 0);

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) svcLoadState(void *, uint32_t u32ClientID, void *pvClient, PSSMHANDLE pSSM)
{
    Log(("svcLoadState: u32ClientID = %d\n", u32ClientID));

    VBOXCLIPBOARDCLIENTDATA *pClient = (VBOXCLIPBOARDCLIENTDATA *)pvClient;

    /* Existing client can not be in async state yet. */
    Assert(!pClient->fAsync);

    /* Restore the client data. */
    uint32_t len;
    int rc = SSMR3GetU32(pSSM, &len);
    AssertRCReturn(rc, rc);

    if (len != sizeof(CLIPSAVEDSTATEDATA))
    {
        Log(("Client data size mismatch: expected %d, got %d\n", sizeof (CLIPSAVEDSTATEDATA), len));
        return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
    }

    CLIPSAVEDSTATEDATA savedState;
    rc = SSMR3GetMem(pSSM, &savedState, sizeof(savedState));
    AssertRCReturn(rc, rc);

    /* Verify the loaded clients data and update the pClient. */
    if (pClient->u32ClientID != savedState.u32ClientID)
    {
        Log(("Client ID mismatch: expected %d, got %d\n", pClient->u32ClientID, savedState.u32ClientID));
        return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
    }

    pClient->fMsgQuit           = savedState.fMsgQuit;
    pClient->fMsgReadData       = savedState.fMsgReadData;
    pClient->fMsgFormats        = savedState.fMsgFormats;
    pClient->u32RequestedFormat = savedState.u32RequestedFormat;

    /* Actual host data are to be reported to guest (SYNC). */
    vboxClipboardSync (pClient);

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) extCallback (uint32_t u32Function, uint32_t u32Format, void *pvData, uint32_t cbData)
{
    if (g_pClient != NULL)
    {
        switch (u32Function)
        {
            case VBOX_CLIPBOARD_EXT_FN_FORMAT_ANNOUNCE:
            {
                LogFlow(("ANNOUNCE: g_fReadingData = %d\n", g_fReadingData));
                if (g_fReadingData)
                {
                    g_fDelayedAnnouncement = true;
                    g_u32DelayedFormats = u32Format;
                }
                else
                {
                    vboxSvcClipboardReportMsg (g_pClient, VBOX_SHARED_CLIPBOARD_HOST_MSG_FORMATS, u32Format);
                }
            } break;

            case VBOX_CLIPBOARD_EXT_FN_DATA_READ:
            {
                vboxSvcClipboardReportMsg (g_pClient, VBOX_SHARED_CLIPBOARD_HOST_MSG_READ_DATA, u32Format);
            } break;

            default:
                return VERR_NOT_SUPPORTED;
        }
    }

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) svcRegisterExtension(void *, PFNHGCMSVCEXT pfnExtension, void *pvExtension)
{
    LogFlowFunc(("pfnExtension = %p\n", pfnExtension));

    VBOXCLIPBOARDEXTPARMS parms;

    if (pfnExtension)
    {
        /* Install extension. */
        g_pfnExtension = pfnExtension;
        g_pvExtension = pvExtension;

        parms.u.pfnCallback = extCallback;
        g_pfnExtension (g_pvExtension, VBOX_CLIPBOARD_EXT_FN_SET_CALLBACK, &parms, sizeof (parms));
    }
    else
    {
        if (g_pfnExtension)
        {
            parms.u.pfnCallback = NULL;
            g_pfnExtension (g_pvExtension, VBOX_CLIPBOARD_EXT_FN_SET_CALLBACK, &parms, sizeof (parms));
        }

        /* Uninstall extension. */
        g_pfnExtension = NULL;
        g_pvExtension = NULL;
    }

    return VINF_SUCCESS;
}

extern "C" DECLCALLBACK(DECLEXPORT(int)) VBoxHGCMSvcLoad (VBOXHGCMSVCFNTABLE *ptable)
{
    int rc = VINF_SUCCESS;

    LogFlowFunc(("ptable = %p\n", ptable));

    if (!ptable)
    {
        rc = VERR_INVALID_PARAMETER;
    }
    else
    {
        Log(("VBoxHGCMSvcLoad: ptable->cbSize = %d, ptable->u32Version = 0x%08X\n", ptable->cbSize, ptable->u32Version));

        if (   ptable->cbSize != sizeof (VBOXHGCMSVCFNTABLE)
            || ptable->u32Version != VBOX_HGCM_SVC_VERSION)
        {
            rc = VERR_INVALID_PARAMETER;
        }
        else
        {
            g_pHelpers = ptable->pHelpers;

            ptable->cbClient = sizeof (VBOXCLIPBOARDCLIENTDATA);

            ptable->pfnUnload     = svcUnload;
            ptable->pfnConnect    = svcConnect;
            ptable->pfnDisconnect = svcDisconnect;
            ptable->pfnCall       = svcCall;
            ptable->pfnHostCall   = svcHostCall;
            ptable->pfnSaveState  = svcSaveState;
            ptable->pfnLoadState  = svcLoadState;
            ptable->pfnRegisterExtension  = svcRegisterExtension;
            ptable->pvService     = NULL;

            /* Service specific initialization. */
            rc = svcInit ();
        }
    }

    return rc;
}
