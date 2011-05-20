/* $Id$ */

/** @file
 * VBox Mouse filter internal functions
 */

/*
 * Copyright (C) 2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "VBoxMF.h"
#include <VBox/VBoxGuestLib.h>

typedef struct _VBoxGlobalContext
{
    volatile LONG cDevicesStarted;
    volatile LONG fVBGLInited;
    volatile LONG fVBGLInitFailed;
    volatile LONG fHostInformed;
    volatile LONG fHostMouseFound;
} VBoxGlobalContext;

static VBoxGlobalContext g_ctx = {0, FALSE, FALSE, FALSE, FALSE};

static BOOLEAN vboxIsVBGLInited(void)
{
   return InterlockedCompareExchange(&g_ctx.fVBGLInited, TRUE, TRUE) == TRUE;
}

static BOOLEAN vboxIsVBGLInitFailed (void)
{
   return InterlockedCompareExchange(&g_ctx.fVBGLInitFailed, TRUE, TRUE) == TRUE;
}

static BOOLEAN vboxIsHostInformed(void)
{
   return InterlockedCompareExchange(&g_ctx.fHostInformed, TRUE, TRUE) == TRUE;
}

static BOOLEAN vboxIsHostMouseFound(void)
{
   return InterlockedCompareExchange(&g_ctx.fHostMouseFound, TRUE, TRUE) == TRUE;
}

VOID VBoxDeviceAdded(PVBOXMOUSE_DEVEXT pDevExt)
{
    LOGF_ENTER();
    LONG callCnt = InterlockedIncrement(&g_ctx.cDevicesStarted);

    /* One time Vbgl initialization */
    if (callCnt == 1)
    {
        if (!vboxIsVBGLInited() && !vboxIsVBGLInitFailed())
        {
            int rc = VbglInit();

            if (RT_SUCCESS(rc))
            {
                InterlockedExchange(&g_ctx.fVBGLInited, TRUE);
                LOG(("VBGL init OK"));
            }
            else
            {
                InterlockedExchange (&g_ctx.fVBGLInitFailed, TRUE);
                WARN(("VBGL init failed with rc=%#x", rc));
            }
        }
    }

    if (!vboxIsHostMouseFound())
    {
        NTSTATUS rc;
        UCHAR buffer[512];
        CM_RESOURCE_LIST *pResourceList = (CM_RESOURCE_LIST *)&buffer[0];
        ULONG cbWritten=0;
        BOOLEAN bDetected = FALSE;

        rc = IoGetDeviceProperty(pDevExt->pdoMain, DevicePropertyBootConfiguration,
                                 sizeof(buffer), &buffer[0], &cbWritten);
        if (!NT_SUCCESS(rc))
        {
            WARN(("IoGetDeviceProperty failed with rc=%#x", rc));
            return;
        }

        LOG(("Number of descriptors: %d", pResourceList->Count));
        
        /* Check if device claims IO port 0x60 or int12 */
        for (ULONG i=0; i<pResourceList->Count; ++i)
        {
            CM_FULL_RESOURCE_DESCRIPTOR *pFullDescriptor = &pResourceList->List[i];

            LOG(("FullDescriptor[%i]: IfType %d, Bus %d, Ver %d, Rev %d, Count %d",
                 i, pFullDescriptor->InterfaceType, pFullDescriptor->BusNumber,
                 pFullDescriptor->PartialResourceList.Version, pFullDescriptor->PartialResourceList.Revision,
                 pFullDescriptor->PartialResourceList.Count));

            for (ULONG j=0; j<pFullDescriptor->PartialResourceList.Count; ++j)
            {
                CM_PARTIAL_RESOURCE_DESCRIPTOR *pPartialDescriptor = &pFullDescriptor->PartialResourceList.PartialDescriptors[j];
                LOG(("PartialDescriptor[%d]: type %d, ShareDisposition %d, Flags 0x%04X, Start 0x%llx, length 0x%x",
                     j, pPartialDescriptor->Type, pPartialDescriptor->ShareDisposition, pPartialDescriptor->Flags,
                     pPartialDescriptor->u.Generic.Start.QuadPart, pPartialDescriptor->u.Generic.Length));

                switch(pPartialDescriptor->Type)
                {
                    case CmResourceTypePort:
                    {
                        LOG(("CmResourceTypePort %#x", pPartialDescriptor->u.Port.Start.QuadPart));
                        if (pPartialDescriptor->u.Port.Start.QuadPart == 0x60)
                        {
                            bDetected = TRUE;
                        }
                        break;
                    }
                    case CmResourceTypeInterrupt:
                    {
                        LOG(("CmResourceTypeInterrupt %ld", pPartialDescriptor->u.Interrupt.Vector));
                        if (pPartialDescriptor->u.Interrupt.Vector == 0xC)
                        {
                            bDetected = TRUE;
                        }
                        break;
                    }
                    default:
                    {
                        break;
                    }
                }
            }
        }

        if (bDetected)
        {
            /* It's the emulated 8042 PS/2 mouse/kbd device, so mark it as the Host one.
             * For this device the filter will query absolute mouse coords from the host.
             */
            InterlockedExchange(&g_ctx.fHostMouseFound, TRUE);

            pDevExt->bHostMouse = TRUE;
            LOG(("Host mouse found"));
        }
    }
    LOGF_LEAVE();
}

VOID VBoxInformHost(PVBOXMOUSE_DEVEXT pDevExt)
{
    LOGF_ENTER();
    
    if (!vboxIsVBGLInited())
    {
        WARN(("!vboxIsVBGLInited"));
        return;
    }

    /* Inform host we support absolute coordinates */
    if (pDevExt->bHostMouse && !vboxIsHostInformed())
    {
        VMMDevReqMouseStatus *req = NULL;
        int rc = VbglGRAlloc((VMMDevRequestHeader **)&req, sizeof(VMMDevReqMouseStatus), VMMDevReq_SetMouseStatus);

        if (RT_SUCCESS(rc))
        {
            req->mouseFeatures = VMMDEV_MOUSE_GUEST_CAN_ABSOLUTE;
            req->pointerXPos = 0;
            req->pointerYPos = 0;

            rc = VbglGRPerform(&req->header);

            if (RT_SUCCESS(rc))
            {
                InterlockedExchange(&g_ctx.fHostInformed, TRUE);
            }
            else
            {
                WARN(("VbglGRPerform failed with rc=%#x", rc));
            }

            VbglGRFree(&req->header);
        }
        else
        {
            WARN(("VbglGRAlloc failed with rc=%#x", rc));
        }
    }

    /* Preallocate request to be used in VBoxServiceCB*/
    if (pDevExt->bHostMouse && !pDevExt->pSCReq)
    {
        VMMDevReqMouseStatus *req = NULL;

        int rc = VbglGRAlloc((VMMDevRequestHeader **)&req, sizeof(VMMDevReqMouseStatus), VMMDevReq_GetMouseStatus);

        if (RT_SUCCESS(rc))
        {
            InterlockedExchangePointer((PVOID volatile *)&pDevExt->pSCReq, req);
        }
        else
        {
            WARN(("VbglGRAlloc for service callback failed with rc=%#x", rc));
        }
    }
    
    LOGF_LEAVE();
}

VOID VBoxDeviceRemoved(PVBOXMOUSE_DEVEXT pDevExt)
{
    LOGF_ENTER();

    /* Save the allocated request pointer and clear the devExt. */
    VMMDevReqMouseStatus *pSCReq = (VMMDevReqMouseStatus *) InterlockedExchangePointer((PVOID volatile *)&pDevExt->pSCReq, NULL);

    if (pDevExt->bHostMouse && vboxIsHostInformed())
    {
        // tell the VMM that from now on we can't handle absolute coordinates anymore
        VMMDevReqMouseStatus *req = NULL;

        int rc = VbglGRAlloc((VMMDevRequestHeader **)&req, sizeof(VMMDevReqMouseStatus), VMMDevReq_SetMouseStatus);

        if (RT_SUCCESS(rc))
        {
            req->mouseFeatures = 0;
            req->pointerXPos = 0;
            req->pointerYPos = 0;

            rc = VbglGRPerform(&req->header);

            if (RT_FAILURE(rc))
            {
                WARN(("VbglGRPerform failed with rc=%#x", rc));
            }

            VbglGRFree(&req->header);
        }
        else
        {
            WARN(("VbglGRAlloc failed with rc=%#x", rc));
        }

        InterlockedExchange(&g_ctx.fHostInformed, FALSE);
    }

    if (pSCReq)
    {
        VbglGRFree(&pSCReq->header);
    }

    LONG callCnt = InterlockedDecrement(&g_ctx.cDevicesStarted);

    if (callCnt == 0)
    {
        if (vboxIsVBGLInited())
        {
            /* Set the flag to prevent reinitializing of the VBGL. */
            InterlockedExchange(&g_ctx.fVBGLInitFailed, TRUE);

            VbglTerminate();

            /* The VBGL is now in the not initialized state. */
            InterlockedExchange(&g_ctx.fVBGLInited, FALSE);
            InterlockedExchange(&g_ctx.fVBGLInitFailed, FALSE);
        }
    }

    LOGF_LEAVE();
}
