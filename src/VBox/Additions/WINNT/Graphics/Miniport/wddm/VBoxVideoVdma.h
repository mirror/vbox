/*
 * Copyright (C) 2010 Sun Microsystems, Inc.
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
#ifndef ___VBoxVideoVdma_h___
#define ___VBoxVideoVdma_h___

#include <iprt/cdefs.h>
#include <VBox/VBoxVideo.h>

/* DMA commands are currently submitted over HGSMI */
typedef struct VBOXVDMAINFO
{
    HGSMIHEAP CmdHeap;
    UINT      uLastCompletedPagingBufferCmdFenceId;
    BOOL      fEnabled;
} VBOXVDMAINFO, *PVBOXVDMAINFO;

int vboxVdmaCreate (struct _DEVICE_EXTENSION* pDevExt, VBOXVDMAINFO *pInfo, ULONG offBuffer, ULONG cbBuffer);
int vboxVdmaDisable (struct _DEVICE_EXTENSION* pDevExt, PVBOXVDMAINFO pInfo);
int vboxVdmaEnable (struct _DEVICE_EXTENSION* pDevExt, PVBOXVDMAINFO pInfo);
int vboxVdmaFlush (struct _DEVICE_EXTENSION* pDevExt, PVBOXVDMAINFO pInfo);
int vboxVdmaDestroy (struct _DEVICE_EXTENSION* pDevExt, PVBOXVDMAINFO pInfo);
void vboxVdmaCBufDrSubmit (struct _DEVICE_EXTENSION* pDevExt, PVBOXVDMAINFO pInfo, PVBOXVDMACBUF_DR pDr);
struct VBOXVDMACBUF_DR* vboxVdmaCBufDrCreate (PVBOXVDMAINFO pInfo, uint32_t cbTrailingData);
void vboxVdmaCBufDrFree (PVBOXVDMAINFO pInfo, struct VBOXVDMACBUF_DR* pDr);

#endif /* #ifndef ___VBoxVideoVdma_h___ */
