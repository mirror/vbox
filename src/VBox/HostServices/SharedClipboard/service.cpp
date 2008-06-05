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
    return VBOX_SUCCESS(RTCritSectEnter (&critsect));
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
        if (VBOX_FAILURE (rc))
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

    if (VBOX_SUCCESS (rc))
    {
        g_pClient = pClient;
    }

    Log(("vboxClipboardConnect: rc = %Vrc\n", rc));

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
                
                if (VBOX_SUCCESS (rc))
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
                
                if (VBOX_SUCCESS (rc))
                {
                    rc = VBoxHGCMParmPtrGet (&paParms[1], &pv, &cb);

                    if (VBOX_SUCCESS (rc))
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
                            parms.pvData = pv;
                            parms.cbData = cb;
                            
                            rc = g_pfnExtension (g_pvExtension, VBOX_CLIPBOARD_EXT_FN_DATA_READ, &parms, sizeof (parms));
                            
                            if (VBOX_SUCCESS (rc))
                            {
                                cbActual = parms.cbData;
                            }
                        }
                        else
                        {
                            rc = vboxClipboardReadData (pClient, u32Format, pv, cb, &cbActual);
                        }
                        
                        if (VBOX_SUCCESS (rc))
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

                if (VBOX_SUCCESS (rc))
                {
                    rc = VBoxHGCMParmPtrGet (&paParms[1], &pv, &cb);

                    if (VBOX_SUCCESS (rc))
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
                            parms.pvData = pv;
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

    LogFlow(("svcCall: rc = %Vrc\n", rc));

    if (!fAsynchronousProcessing)
    {
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

    LogFlow(("svcHostCall: rc = %Vrc\n", rc));
    return rc;
}

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

    /* Save client structure length & contents */
    int rc = SSMR3PutU32(pSSM, sizeof(*pClient));
    AssertRCReturn(rc, rc);

    rc = SSMR3PutMem(pSSM, pClient, sizeof(*pClient));
    AssertRCReturn(rc, rc);

    if (pClient->fAsync)
    {
        g_pHelpers->pfnCallComplete (pClient->async.callHandle, VINF_SUCCESS /* error code is not important here. */);
        pClient->fAsync = false;
    }

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

    if (len != sizeof(VBOXCLIPBOARDCLIENTDATA))
    {
        Log(("Client len mismatch: %d %d\n", len, sizeof (VBOXCLIPBOARDCLIENTDATA)));
        return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
    }

    VBOXCLIPBOARDCLIENTDATA client;
    rc = SSMR3GetMem(pSSM, &client, sizeof(client));
    AssertRCReturn(rc, rc);

    /* Verify the loaded clients data and update the pClient. */
    if (pClient->u32ClientID != client.u32ClientID)
    {
        Log(("Client ID mismatch: %d %d\n", pClient->u32ClientID, client.u32ClientID));
        return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
    }

    pClient->fMsgQuit     = client.fMsgQuit;
    pClient->fMsgReadData = client.fMsgReadData;
    pClient->fMsgFormats  = client.fMsgFormats;
    pClient->u32RequestedFormat = client.u32RequestedFormat;

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
                vboxSvcClipboardReportMsg (g_pClient, VBOX_SHARED_CLIPBOARD_HOST_MSG_FORMATS, u32Format);
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
        
        parms.pvData = (void *)extCallback;
        g_pfnExtension (g_pvExtension, VBOX_CLIPBOARD_EXT_FN_SET_CALLBACK, &parms, sizeof (parms));
    }
    else
    {
        if (g_pfnExtension)
        {
            parms.pvData = NULL;
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
