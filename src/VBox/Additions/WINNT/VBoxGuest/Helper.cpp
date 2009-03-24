/** @file
 * VBoxGuest -- VirtualBox Win32 guest support driver
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

//#define LOG_ENABLED

#include "VBoxGuest_Internal.h"
#include "Helper.h"
#include <VBox/err.h>
#include <VBox/log.h>
#include <VBox/VBoxGuestLib.h>

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, VBoxScanPCIResourceList)
#endif

/* CM_RESOURCE_MEMORY_* flags which were used on XP or earlier. */
#define VBOX_CM_PRE_VISTA_MASK (0x3f)

/**
 * Helper to scan the PCI resource list and remember stuff.
 *
 * @param pResList  Resource list
 * @param pDevExt   Device extension
 */
NTSTATUS VBoxScanPCIResourceList(PCM_RESOURCE_LIST pResList, PVBOXGUESTDEVEXT pDevExt)
{
    NTSTATUS rc = STATUS_SUCCESS;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR partialData;

    // enumerate the resource list
    dprintf(("found %d resources\n", pResList->List->PartialResourceList.Count));
    ULONG rangeCount = 0;
    ULONG cMMIORange = 0;
    PBASE_ADDRESS baseAddress = pDevExt->baseAddress;
    for (ULONG i = 0; i < pResList->List->PartialResourceList.Count; i++)
    {
        partialData = &pResList->List->PartialResourceList.PartialDescriptors[i];
        switch (partialData->Type)
        {
            case CmResourceTypePort:
            {
                // overflow protection
                if (rangeCount < PCI_TYPE0_ADDRESSES)
                {
                    dprintf(("I/O range:     Base = %08x : %08x   Length = %08x \n",
                            partialData->u.Port.Start.HighPart,
                            partialData->u.Port.Start.LowPart,
                            partialData->u.Port.Length));
                    //@todo not so gut
                    dprintf(("I got all I want, my dear port, oh!\n"));
                    pDevExt->startPortAddress = (ULONG)partialData->u.Port.Start.LowPart;
                    // save resource information
                    baseAddress->RangeStart     = partialData->u.Port.Start;
                    baseAddress->RangeLength    = partialData->u.Port.Length;
                    baseAddress->RangeInMemory  = FALSE;
                    baseAddress->ResourceMapped = FALSE;
                    // next item
                    rangeCount++; baseAddress++;
                }
                break;
            }

            case CmResourceTypeInterrupt:
            {
                dprintf(("Interrupt:  Level = %x    Vector = %x    Mode = %x \n",
                        partialData->u.Interrupt.Level,
                        partialData->u.Interrupt.Vector,
                        partialData->Flags));
                // save information
                pDevExt->interruptLevel    = partialData->u.Interrupt.Level;
                pDevExt->interruptVector   = partialData->u.Interrupt.Vector;
                pDevExt->interruptAffinity = partialData->u.Interrupt.Affinity;
                // check interrupt mode
                if (partialData->Flags & CM_RESOURCE_INTERRUPT_LATCHED)
                {
                    pDevExt->interruptMode = Latched;
                }
                else
                {
                    pDevExt->interruptMode = LevelSensitive;
                }
                break;
            }

            case CmResourceTypeMemory:
            {
                // overflow protection
                if (rangeCount < PCI_TYPE0_ADDRESSES)
                {
                    dprintf(("Memory range:     Base = %08x : %08x   Length = %08x \n",
                            partialData->u.Memory.Start.HighPart,
                            partialData->u.Memory.Start.LowPart,
                            partialData->u.Memory.Length));
                    // we only care about read/write memory
                    /** @todo reconsider memory type */
                    if (    cMMIORange == 0 /* only care about the first mmio range (!!!) */
                        && (partialData->Flags & VBOX_CM_PRE_VISTA_MASK) == CM_RESOURCE_MEMORY_READ_WRITE)
                    {
                        pDevExt->memoryAddress = partialData->u.Memory.Start;
                        pDevExt->memoryLength = (ULONG)partialData->u.Memory.Length;
                        // save resource information
                        baseAddress->RangeStart     = partialData->u.Memory.Start;
                        baseAddress->RangeLength    = partialData->u.Memory.Length;
                        baseAddress->RangeInMemory  = TRUE;
                        baseAddress->ResourceMapped = FALSE;
                        // next item
                        rangeCount++; baseAddress++;cMMIORange++;
                    } else
                    {
                        dprintf(("Ignoring memory: flags = %08x \n", partialData->Flags));
                    }
                }
                break;
            }

            case CmResourceTypeDma:
            {
                dprintf(("DMA resource found. Hmm...\n"));
                break;
            }

            default:
            {
                dprintf(("Unexpected resource found %d. Hmm...\n", partialData->Type));
                break;
            }
        }
    }
    // memorize the number of resources found
    pDevExt->addressCount = rangeCount;

    return rc;
}


NTSTATUS hlpVBoxMapVMMDevMemory (PVBOXGUESTDEVEXT pDevExt)
{
    NTSTATUS rc = STATUS_SUCCESS;

    if (pDevExt->memoryLength != 0)
    {
         pDevExt->pVMMDevMemory = (VMMDevMemory *)MmMapIoSpace (pDevExt->memoryAddress, pDevExt->memoryLength, MmNonCached);
         dprintf(("VBoxGuest::VBoxGuestPnp: VMMDevMemory: ptr = 0x%x\n", pDevExt->pVMMDevMemory));
         if (pDevExt->pVMMDevMemory)
         {
             dprintf(("VBoxGuest::VBoxGuestPnp: VMMDevMemory: version = 0x%x, size = %d\n", pDevExt->pVMMDevMemory->u32Version, pDevExt->pVMMDevMemory->u32Size));

             /* Check version of the structure */
             if (pDevExt->pVMMDevMemory->u32Version != VMMDEV_MEMORY_VERSION)
             {
                 /* Not our version, refuse operation and unmap the memory */
                 hlpVBoxUnmapVMMDevMemory (pDevExt);

                 rc = STATUS_UNSUCCESSFUL;
             }
         }
         else
         {
             rc = STATUS_UNSUCCESSFUL;
         }
    }

    return rc;
}

void hlpVBoxUnmapVMMDevMemory (PVBOXGUESTDEVEXT pDevExt)
{
    if (pDevExt->pVMMDevMemory)
    {
        MmUnmapIoSpace (pDevExt->pVMMDevMemory, pDevExt->memoryLength);
        pDevExt->pVMMDevMemory = NULL;
    }

    pDevExt->memoryAddress.QuadPart = 0;
    pDevExt->memoryLength = 0;
}

NTSTATUS hlpVBoxReportGuestInfo (PVBOXGUESTDEVEXT pDevExt)
{
    VMMDevReportGuestInfo *req = NULL;

    int rc = VbglGRAlloc ((VMMDevRequestHeader **)&req, sizeof (VMMDevReportGuestInfo), VMMDevReq_ReportGuestInfo);

    dprintf(("hlpVBoxReportGuestInfo: VbglGRAlloc rc = %d\n", rc));

    if (RT_SUCCESS(rc))
    {
        req->guestInfo.additionsVersion = VMMDEV_VERSION;

        /* we've already determined the Windows product before */
        switch (winVersion)
        {
            case WINNT4:
                req->guestInfo.osType = VBOXOSTYPE_WinNT4;
                break;
            case WIN2K:
                req->guestInfo.osType = VBOXOSTYPE_Win2k;
                break;
            case WINXP:
                req->guestInfo.osType = VBOXOSTYPE_WinXP;
                break;
            case WIN2K3:
                req->guestInfo.osType = VBOXOSTYPE_Win2k3;
                break;
            case WINVISTA:
                req->guestInfo.osType = VBOXOSTYPE_WinVista;
                break;
            default:
                /* we don't know, therefore NT family */
                req->guestInfo.osType = VBOXOSTYPE_WinNT;
                break;
        }

        /** @todo registry lookup for additional information */


        rc = VbglGRPerform (&req->header);

        if (RT_FAILURE(rc) || RT_FAILURE(req->header.rc))
        {
            dprintf(("VBoxGuest::hlpVBoxReportGuestInfo: error reporting guest info to VMMDev."
                      "rc = %d, VMMDev rc = %Rrc\n", rc, req->header.rc));
        }

        rc = RT_SUCCESS(rc) ? req->header.rc : rc;

        VbglGRFree (&req->header);
    }

    return RT_FAILURE(rc) ? STATUS_UNSUCCESSFUL : STATUS_SUCCESS;
}

