/* $Id$ */

/** @file
 * VBox Miniport HGSMI related functions
 */

/*
 * Copyright (C) 2011-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "VBoxMPHGSMI.h"
#include "VBoxMPCommon.h"
#include <VBox/VMMDev.h>

/**
 * Helper function to register secondary displays (DualView). Note that this will not
 * be available on pre-XP versions, and some editions on XP will fail because they are
 * intentionally crippled.
 *
 * HGSMI variant is a bit different because it uses only HGSMI interface (VBVA channel)
 * to talk to the host.
 */
void VBoxSetupDisplaysHGSMI(PVBOXMP_COMMON pCommon, PHYSICAL_ADDRESS phVRAM, uint32_t ulApertureSize,
                            uint32_t cbVRAM, uint32_t fCaps)
{
    /** @todo I simply converted this from Windows error codes.  That is wrong,
     * but we currently freely mix and match those (failure == rc > 0) and iprt
     * ones (failure == rc < 0) anyway.  This needs to be fully reviewed and
     * fixed. */
    int rc = VINF_SUCCESS;
    uint32_t offVRAMBaseMapping, cbMapping, offGuestHeapMemory, cbGuestHeapMemory,
             offHostFlags, offVRAMHostArea, cbHostArea;
    LOGF_ENTER();

    memset(pCommon, 0, sizeof(*pCommon));
    pCommon->phVRAM = phVRAM;
    pCommon->ulApertureSize = ulApertureSize;
    pCommon->cbVRAM    = cbVRAM;
    pCommon->cDisplays = 1;
    pCommon->bHGSMI    = VBoxHGSMIIsSupported();

    if (pCommon->bHGSMI)
    {
        VBoxHGSMIGetBaseMappingInfo(pCommon->cbVRAM, &offVRAMBaseMapping,
                                    &cbMapping, &offGuestHeapMemory,
                                    &cbGuestHeapMemory, &offHostFlags);

        /* Map the adapter information. It will be needed for HGSMI IO. */
        rc = VBoxMPCmnMapAdapterMemory(pCommon, &pCommon->pvAdapterInformation, offVRAMBaseMapping, cbMapping);
        if (RT_FAILURE(rc))
        {
            LOG(("VBoxMPCmnMapAdapterMemory failed rc = %d", rc));
            pCommon->bHGSMI = false;
        }
        else
        {
            /* Setup an HGSMI heap within the adapter information area. */
            rc = VBoxHGSMISetupGuestContext(&pCommon->guestCtx,
                                            pCommon->pvAdapterInformation,
                                            cbGuestHeapMemory,
                                              offVRAMBaseMapping
                                            + offGuestHeapMemory);

            if (RT_FAILURE(rc))
            {
                LOG(("HGSMIHeapSetup failed rc = %d", rc));
                pCommon->bHGSMI = false;
            }
        }
    }

    /* Setup the host heap and the adapter memory. */
    if (pCommon->bHGSMI)
    {
        VBoxHGSMIGetHostAreaMapping(&pCommon->guestCtx, pCommon->cbVRAM,
                                    offVRAMBaseMapping, &offVRAMHostArea,
                                    &cbHostArea);
        if (cbHostArea)
        {

            /* Map the heap region.
             *
             * Note: the heap will be used for the host buffers submitted to the guest.
             *       The miniport driver is responsible for reading FIFO and notifying
             *       display drivers.
             */
            pCommon->cbMiniportHeap = cbHostArea;
            rc = VBoxMPCmnMapAdapterMemory (pCommon, &pCommon->pvMiniportHeap,
                                       offVRAMHostArea, cbHostArea);
            if (RT_FAILURE(rc))
            {
                pCommon->pvMiniportHeap = NULL;
                pCommon->cbMiniportHeap = 0;
                pCommon->bHGSMI = false;
            }
            else
                VBoxHGSMISetupHostContext(&pCommon->hostCtx,
                                          pCommon->pvAdapterInformation,
                                          offHostFlags,
                                          pCommon->pvMiniportHeap,
                                          offVRAMHostArea, cbHostArea);
        }
        else
        {
            /* Host has not requested a heap. */
            pCommon->pvMiniportHeap = NULL;
            pCommon->cbMiniportHeap = 0;
        }
    }

    if (pCommon->bHGSMI)
    {
        /* Setup the information for the host. */
        rc = VBoxHGSMISendHostCtxInfo(&pCommon->guestCtx,
                                      offVRAMBaseMapping + offHostFlags,
                                      fCaps, offVRAMHostArea,
                                      pCommon->cbMiniportHeap);

        if (RT_FAILURE(rc))
        {
            pCommon->bHGSMI = false;
        }
    }

    /* Check whether the guest supports multimonitors. */
    if (pCommon->bHGSMI)
    {
        /* Query the configured number of displays. */
        pCommon->cDisplays = VBoxHGSMIGetMonitorCount(&pCommon->guestCtx);
    }
    else
    {
        VBoxFreeDisplaysHGSMI(pCommon);
    }

    LOGF_LEAVE();
}

static bool VBoxUnmapAdpInfoCallback(void *pvCommon)
{
    PVBOXMP_COMMON pCommon = (PVBOXMP_COMMON)pvCommon;

    pCommon->hostCtx.pfHostFlags = NULL;
    return true;
}

void VBoxFreeDisplaysHGSMI(PVBOXMP_COMMON pCommon)
{
    VBoxMPCmnUnmapAdapterMemory(pCommon, &pCommon->pvMiniportHeap);
#ifdef VBOX_WDDM_MINIPORT
    VBoxSHGSMITerm(&pCommon->guestCtx.heapCtx);
#else
    HGSMIHeapDestroy(&pCommon->guestCtx.heapCtx);
#endif

    /* Unmap the adapter information needed for HGSMI IO. */
    VBoxMPCmnSyncToVideoIRQ(pCommon, VBoxUnmapAdpInfoCallback, pCommon);
    VBoxMPCmnUnmapAdapterMemory(pCommon, &pCommon->pvAdapterInformation);
}
