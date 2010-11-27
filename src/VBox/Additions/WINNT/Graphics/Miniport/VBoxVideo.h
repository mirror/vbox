/** @file
 * VirtualBox Video miniport driver
 *
 * Copyright (C) 2006-2007 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VBOXVIDEO_H
#define VBOXVIDEO_H

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <iprt/assert.h>

//#include <iprt/thread.h>

#include <VBox/Hardware/VBoxVideoVBE.h>
#include <VBox/VBoxVideoGuest.h>
#include <VBox/VBoxVideo.h>
#include "VBoxHGSMI.h"

/* This looks absolutely unnecessary to me, but harmless. */
#define VBE_DISPI_TOTAL_VIDEO_MEMORY_MB 4
#define VBE_DISPI_TOTAL_VIDEO_MEMORY_KB         (VBE_DISPI_TOTAL_VIDEO_MEMORY_MB * 1024)
#define VBE_DISPI_TOTAL_VIDEO_MEMORY_BYTES      (VBE_DISPI_TOTAL_VIDEO_MEMORY_KB * 1024)

typedef struct VBOXVIDEO_COMMON
{
    int cDisplays;                      /* Number of displays. */

    uint32_t cbVRAM;                    /* The VRAM size. */

    uint32_t cbMiniportHeap;            /* The size of reserved VRAM for miniport driver heap.
                                         * It is at offset:
                                         *   cbAdapterMemorySize - VBOX_VIDEO_ADAPTER_INFORMATION_SIZE - cbMiniportHeap
                                         */
    void *pvMiniportHeap;               /* The pointer to the miniport heap VRAM.
                                         * This is mapped by miniport separately.
                                         */
    void *pvAdapterInformation;         /* The pointer to the last 4K of VRAM.
                                         * This is mapped by miniport separately.
                                         */

    /** Whether HGSMI is enabled. */
    bool bHGSMI;
    /** Context information needed to receive commands from the host. */
    HGSMIHOSTCOMMANDCONTEXT hostCtx;
    /** Context information needed to submit commands to the host. */
    HGSMIGUESTCOMMANDCONTEXT guestCtx;
} VBOXVIDEO_COMMON, *PVBOXVIDEO_COMMON;

extern "C"
{
/** Signal an event in a guest-OS-specific way.  pvEvent will be re-cast to
 * something OS-specific. */
void VBoxVideoCmnSignalEvent(PVBOXVIDEO_COMMON pCommon, uint64_t pvEvent);

/** Allocate memory to be used in normal driver operation (dispatch level in
 * Windows) but not necessarily in IRQ context. */
void *VBoxVideoCmnMemAllocDriver(PVBOXVIDEO_COMMON pCommon, size_t cb);

/** Free memory allocated by @a VBoxVideoCmnMemAllocDriver */
void VBoxVideoCmnMemFreeDriver(PVBOXVIDEO_COMMON pCommon, void *pv);

int VBoxMapAdapterMemory (PVBOXVIDEO_COMMON pCommon,
                          void **ppv,
                          uint32_t ulOffset,
                          uint32_t ulSize);

void VBoxUnmapAdapterMemory (PVBOXVIDEO_COMMON pCommon, void **ppv);

typedef bool(*PFNVIDEOIRQSYNC)(void *);
bool VBoxSyncToVideoIRQ(PVBOXVIDEO_COMMON pCommon, PFNVIDEOIRQSYNC pfnSync,
                        void *pvUser);

void VBoxSetupDisplaysHGSMI (PVBOXVIDEO_COMMON pCommon,
                             uint32_t AdapterMemorySize, uint32_t fCaps);

void VBoxFreeDisplaysHGSMI(PVBOXVIDEO_COMMON pCommon);
#ifndef VBOX_WITH_WDDM
DECLCALLBACK(void) hgsmiHostCmdComplete (HVBOXVIDEOHGSMI hHGSMI, struct _VBVAHOSTCMD * pCmd);
DECLCALLBACK(int) hgsmiHostCmdRequest (HVBOXVIDEOHGSMI hHGSMI, uint8_t u8Channel, uint32_t iDisplay, struct _VBVAHOSTCMD ** ppCmd);
#endif


int vboxVBVAChannelDisplayEnable(PVBOXVIDEO_COMMON pCommon,
        int iDisplay, /* negative would mean this is a miniport handler */
        uint8_t u8Channel);

} /* extern "C" */

#endif /* VBOXVIDEO_H */
