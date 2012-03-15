/* $Id$ */

/** @file
 * VBox Miniport device extension header
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

#ifndef VBOXMPDEVEXT_H
#define VBOXMPDEVEXT_H

#include "VBoxMPUtils.h"
#include <VBox/VBoxVideoGuest.h>

#ifdef VBOX_XPDM_MINIPORT
# include <miniport.h>
# include <ntddvdeo.h>
# include <video.h>
# include "common/xpdm/VBoxVideoPortAPI.h"
#endif

#ifdef VBOX_WDDM_MINIPORT
# include "wddm/VBoxMPTypes.h"
#endif

typedef struct VBOXMP_COMMON
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
} VBOXMP_COMMON, *PVBOXMP_COMMON;

typedef struct _VBOXMP_DEVEXT
{
   struct _VBOXMP_DEVEXT *pNext;               /* Next extension in the DualView extension list.
                                                * The primary extension is the first one.
                                                */
#ifdef VBOX_XPDM_MINIPORT
   struct _VBOXMP_DEVEXT *pPrimary;            /* Pointer to the primary device extension. */

   ULONG iDevice;                              /* Device index: 0 for primary, otherwise a secondary device. */

   ULONG CurrentMode;                          /* Saved information about video modes */
   ULONG CurrentModeWidth;
   ULONG CurrentModeHeight;
   ULONG CurrentModeBPP;

   ULONG ulFrameBufferOffset;                  /* The framebuffer position in the VRAM. */
   ULONG ulFrameBufferSize;                    /* The size of the current framebuffer. */
#endif /*VBOX_XPDM_MINIPORT*/

#ifdef VBOX_WDDM_MINIPORT
   PDEVICE_OBJECT pPDO;
   UNICODE_STRING RegKeyName;
   UNICODE_STRING VideoGuid;

   uint8_t * pvVisibleVram;

   VBOXVIDEOCM_MGR CmMgr;
   /* hgsmi allocation manager */
   VBOXVIDEOCM_ALLOC_MGR AllocMgr;
   VBOXVDMADDI_NODE aNodes[VBOXWDDM_NUM_NODES];
   LIST_ENTRY DpcCmdQueue;
   LIST_ENTRY SwapchainList3D;
   /* mutex for context list operations */
   FAST_MUTEX ContextMutex;
   KSPIN_LOCK SynchLock;
   volatile uint32_t cContexts3D;
   volatile uint32_t cUnlockedVBVADisabled;

   HVBOXCRCTL hCrCtl;
   uint32_t cCrCtlRefs;

   VBOXWDDM_GLOBAL_POINTER_INFO PointerInfo;

   VBOXVTLIST CtlList;
   VBOXVTLIST DmaCmdList;
#ifdef VBOX_WITH_VIDEOHWACCEL
   VBOXVTLIST VhwaCmdList;
#endif
   BOOL bNotifyDxDpc;

#if 0
   FAST_MUTEX ShRcTreeMutex;
   AVLPVTREE ShRcTree;
#endif

   VBOXWDDM_SOURCE aSources[VBOX_VIDEO_MAX_SCREENS];
   VBOXWDDM_TARGET aTargets[VBOX_VIDEO_MAX_SCREENS];
#endif /*VBOX_WDDM_MINIPORT*/

   union {
       /* Information that is only relevant to the primary device or is the same for all devices. */
       struct {

           void *pvReqFlush;                   /* Pointer to preallocated generic request structure for
                                                * VMMDevReq_VideoAccelFlush. Allocated when VBVA status
                                                * is changed. Deallocated on HwReset.
                                                */
           ULONG ulVbvaEnabled;                /* Indicates that VBVA mode is enabled. */
           ULONG ulMaxFrameBufferSize;         /* The size of the VRAM allocated for the a single framebuffer. */
           BOOLEAN fMouseHidden;               /* Has the mouse cursor been hidden by the guest? */
           VBOXMP_COMMON commonInfo;
#ifdef VBOX_XPDM_MINIPORT
           /* Video Port API dynamically picked up at runtime for binary backwards compatibility with older NT versions */
           VBOXVIDEOPORTPROCS VideoPortProcs;
#endif

#ifdef VBOX_WDDM_MINIPORT
           VBOXVDMAINFO Vdma;
# ifdef VBOXVDMA_WITH_VBVA
           VBOXVBVAINFO Vbva;
# endif
           D3DKMDT_HVIDPN hCommittedVidPn;      /* committed VidPn handle */
           DXGKRNL_INTERFACE DxgkInterface;     /* Display Port handle and callbacks */
#endif
       } primary;

       /* Secondary device information. */
       struct {
           BOOLEAN bEnabled;                   /* Device enabled flag */
       } secondary;
   } u;

   HGSMIAREA areaDisplay;                      /* Entire VRAM chunk for this display device. */
   BOOLEAN fAnyX;                              /* Unrestricted horizontal resolution flag. */
} VBOXMP_DEVEXT, *PVBOXMP_DEVEXT;

DECLINLINE(PVBOXMP_DEVEXT) VBoxCommonToPrimaryExt(PVBOXMP_COMMON pCommon)
{
    return RT_FROM_MEMBER(pCommon, VBOXMP_DEVEXT, u.primary.commonInfo);
}

DECLINLINE(PVBOXMP_COMMON) VBoxCommonFromDeviceExt(PVBOXMP_DEVEXT pExt)
{
#ifdef VBOX_XPDM_MINIPORT
    return &pExt->pPrimary->u.primary.commonInfo;
#else
    return &pExt->u.primary.commonInfo;
#endif
}

#ifdef VBOX_WDDM_MINIPORT
DECLINLINE(ULONG) vboxWddmVramCpuVisibleSize(PVBOXMP_DEVEXT pDevExt)
{
#ifdef VBOXWDDM_RENDER_FROM_SHADOW
    /* all memory layout info should be initialized */
    Assert(pDevExt->aSources[0].Vbva.offVBVA);
    /* page aligned */
    Assert(!(pDevExt->aSources[0].Vbva.offVBVA & 0xfff));

    return (ULONG)(pDevExt->aSources[0].Vbva.offVBVA & ~0xfffULL);
#else
    /* all memory layout info should be initialized */
    Assert(pDevExt->u.primary.Vdma.CmdHeap.Heap.area.offBase);
    /* page aligned */
    Assert(!(pDevExt->u.primary.Vdma.CmdHeap.Heap.area.offBase & 0xfff));

    return pDevExt->u.primary.Vdma.CmdHeap.Heap.area.offBase & ~0xfffUL;
#endif
}

DECLINLINE(ULONG) vboxWddmVramCpuVisibleSegmentSize(PVBOXMP_DEVEXT pDevExt)
{
    return vboxWddmVramCpuVisibleSize(pDevExt);
}

#ifdef VBOXWDDM_RENDER_FROM_SHADOW
DECLINLINE(ULONG) vboxWddmVramCpuInvisibleSegmentSize(PVBOXMP_DEVEXT pDevExt)
{
    return vboxWddmVramCpuVisibleSegmentSize(pDevExt);
}

DECLINLINE(bool) vboxWddmCmpSurfDescsBase(VBOXWDDM_SURFACE_DESC *pDesc1, VBOXWDDM_SURFACE_DESC *pDesc2)
{
    if (pDesc1->width != pDesc2->width)
        return false;
    if (pDesc1->height != pDesc2->height)
        return false;
    if (pDesc1->format != pDesc2->format)
        return false;
    if (pDesc1->bpp != pDesc2->bpp)
        return false;
    if (pDesc1->pitch != pDesc2->pitch)
        return false;
    return true;
}

DECLINLINE(void) vboxWddmAssignShadow(PVBOXMP_DEVEXT pDevExt, PVBOXWDDM_SOURCE pSource, PVBOXWDDM_ALLOCATION pAllocation, D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId)
{
    if (pSource->pShadowAllocation == pAllocation)
    {
        Assert(pAllocation->bAssigned);
        return;
    }

    if (pSource->pShadowAllocation)
    {
        PVBOXWDDM_ALLOCATION pOldAlloc = pSource->pShadowAllocation;
        /* clear the visibility info fo the current primary */
        pOldAlloc->bVisible = FALSE;
        pOldAlloc->bAssigned = FALSE;
        Assert(pOldAlloc->SurfDesc.VidPnSourceId == srcId);
        /* release the shadow surface */
        pOldAlloc->SurfDesc.VidPnSourceId = D3DDDI_ID_UNINITIALIZED;
    }

    if (pAllocation)
    {
        Assert(!pAllocation->bAssigned);
        Assert(!pAllocation->bVisible);
        pAllocation->bVisible = FALSE;
        /* this check ensures the shadow is not used for other source simultaneously */
        Assert(pAllocation->SurfDesc.VidPnSourceId == D3DDDI_ID_UNINITIALIZED);
        pAllocation->SurfDesc.VidPnSourceId = srcId;
        pAllocation->bAssigned = TRUE;
        if (!vboxWddmCmpSurfDescsBase(&pSource->SurfDesc, &pAllocation->SurfDesc))
            pSource->offVram = VBOXVIDEOOFFSET_VOID; /* force guest->host notification */
        pSource->SurfDesc = pAllocation->SurfDesc;
    }

    pSource->pShadowAllocation = pAllocation;
}
#endif

DECLINLINE(VOID) vboxWddmAssignPrimary(PVBOXMP_DEVEXT pDevExt, PVBOXWDDM_SOURCE pSource, PVBOXWDDM_ALLOCATION pAllocation, D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId)
{
    if (pSource->pPrimaryAllocation == pAllocation)
        return;

    if (pSource->pPrimaryAllocation)
    {
        PVBOXWDDM_ALLOCATION pOldAlloc = pSource->pPrimaryAllocation;
        /* clear the visibility info fo the current primary */
        pOldAlloc->bVisible = FALSE;
        pOldAlloc->bAssigned = FALSE;
        Assert(pOldAlloc->SurfDesc.VidPnSourceId == srcId);
    }

    if (pAllocation)
    {
        pAllocation->bVisible = FALSE;
        Assert(pAllocation->SurfDesc.VidPnSourceId == srcId);
        pAllocation->SurfDesc.VidPnSourceId = srcId;
        pAllocation->bAssigned = TRUE;
    }

    pSource->pPrimaryAllocation = pAllocation;
}
#endif /*VBOX_WDDM_MINIPORT*/

#endif /*VBOXMPDEVEXT_H*/
