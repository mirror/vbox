/* $Id$ */
/** @file
 * VBox WDDM Miniport driver
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

#ifndef ___VBoxMPWddm_h___
#define ___VBoxMPWddm_h___

#ifndef DEBUG_misha
# ifdef Assert
#  error "VBoxMPWddm.h must be included first."
# endif
# define RT_NO_STRICT
#endif
#include "common/VBoxMPUtils.h"
#include "common/VBoxMPDevExt.h"
#include "../../common/VBoxVideoTools.h"

//#define VBOXWDDM_DEBUG_VIDPN

#define VBOXWDDM_CFG_LOG_UM_BACKDOOR 0x00000001
#define VBOXWDDM_CFG_LOG_UM_DBGPRINT 0x00000002
#define VBOXWDDM_CFG_STR_LOG_UM L"VBoxLogUm"
extern DWORD g_VBoxLogUm;
#ifdef VBOX_WDDM_WIN8
extern DWORD g_VBoxDisplayOnly;
#endif

RT_C_DECLS_BEGIN
NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING RegistryPath);
RT_C_DECLS_END

PVOID vboxWddmMemAlloc(IN SIZE_T cbSize);
PVOID vboxWddmMemAllocZero(IN SIZE_T cbSize);
VOID vboxWddmMemFree(PVOID pvMem);

NTSTATUS vboxWddmCallIsr(PVBOXMP_DEVEXT pDevExt);

DECLINLINE(PVBOXWDDM_RESOURCE) vboxWddmResourceForAlloc(PVBOXWDDM_ALLOCATION pAlloc)
{
#if 0
    if(pAlloc->iIndex == VBOXWDDM_ALLOCATIONINDEX_VOID)
        return NULL;
    PVBOXWDDM_RESOURCE pRc = (PVBOXWDDM_RESOURCE)(((uint8_t*)pAlloc) - RT_OFFSETOF(VBOXWDDM_RESOURCE, aAllocations[pAlloc->iIndex]));
    return pRc;
#else
    return pAlloc->pResource;
#endif
}

VOID vboxWddmAllocationDestroy(PVBOXWDDM_ALLOCATION pAllocation);

DECLINLINE(VOID) vboxWddmAllocationRelease(PVBOXWDDM_ALLOCATION pAllocation)
{
    uint32_t cRefs = ASMAtomicDecU32(&pAllocation->cRefs);
    Assert(cRefs < UINT32_MAX/2);
    if (!cRefs)
    {
        vboxWddmAllocationDestroy(pAllocation);
    }
}

DECLINLINE(VOID) vboxWddmAllocationRetain(PVBOXWDDM_ALLOCATION pAllocation)
{
    ASMAtomicIncU32(&pAllocation->cRefs);
}

DECLINLINE(VOID) vboxWddmAddrSetVram(PVBOXWDDM_ADDR pAddr, UINT SegmentId, VBOXVIDEOOFFSET offVram)
{
    pAddr->SegmentId = SegmentId;
    pAddr->offVram = offVram;
}

DECLINLINE(bool) vboxWddmAddrVramEqual(PVBOXWDDM_ADDR pAddr1, PVBOXWDDM_ADDR pAddr2)
{
    return pAddr1->SegmentId == pAddr2->SegmentId && pAddr1->offVram == pAddr2->offVram;
}

DECLINLINE(VBOXVIDEOOFFSET) vboxWddmVramAddrToOffset(PVBOXMP_DEVEXT pDevExt, PHYSICAL_ADDRESS Addr)
{
    PVBOXMP_COMMON pCommon = VBoxCommonFromDeviceExt(pDevExt);
    AssertRelease(pCommon->phVRAM.QuadPart <= Addr.QuadPart);
    return (VBOXVIDEOOFFSET)Addr.QuadPart - pCommon->phVRAM.QuadPart;
}

#ifdef VBOXWDDM_RENDER_FROM_SHADOW
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
        Assert(pOldAlloc->AllocData.SurfDesc.VidPnSourceId == srcId);
        /* release the shadow surface */
        pOldAlloc->AllocData.SurfDesc.VidPnSourceId = D3DDDI_ID_UNINITIALIZED;
    }

    if (pAllocation)
    {
        Assert(!pAllocation->bAssigned);
        Assert(!pAllocation->bVisible);
        pAllocation->bVisible = FALSE;
        /* this check ensures the shadow is not used for other source simultaneously */
        Assert(pAllocation->AllocData.SurfDesc.VidPnSourceId == D3DDDI_ID_UNINITIALIZED);
        pAllocation->AllocData.SurfDesc.VidPnSourceId = srcId;
        pAllocation->bAssigned = TRUE;
        if(!vboxWddmAddrVramEqual(&pSource->AllocData.Addr, &pAllocation->AllocData.Addr))
            pSource->bGhSynced = FALSE; /* force guest->host notification */
        pSource->AllocData.Addr = pAllocation->AllocData.Addr;
    }

    pSource->pShadowAllocation = pAllocation;
}
#endif

DECLINLINE(VOID) vboxWddmAssignPrimary(PVBOXMP_DEVEXT pDevExt, PVBOXWDDM_SOURCE pSource, PVBOXWDDM_ALLOCATION pAllocation, D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId)
{
    /* vboxWddmAssignPrimary can not be run in reentrant order, so safely do a direct unlocked check here */
    if (pSource->pPrimaryAllocation == pAllocation)
        return;

    if (pSource->pPrimaryAllocation)
    {
        PVBOXWDDM_ALLOCATION pOldAlloc = pSource->pPrimaryAllocation;
        /* clear the visibility info fo the current primary */
        pOldAlloc->bVisible = FALSE;
        pOldAlloc->bAssigned = FALSE;
        Assert(pOldAlloc->AllocData.SurfDesc.VidPnSourceId == srcId);

        vboxWddmAllocationRelease(pOldAlloc);
    }

    if (pAllocation)
    {
        pAllocation->bVisible = FALSE;
        Assert(pAllocation->AllocData.SurfDesc.VidPnSourceId == srcId);
        pAllocation->bAssigned = TRUE;

        if(!vboxWddmAddrVramEqual(&pSource->AllocData.Addr, &pAllocation->AllocData.Addr))
            pSource->bGhSynced = FALSE; /* force guest->host notification */
        pSource->AllocData.Addr = pAllocation->AllocData.Addr;

        vboxWddmAllocationRetain(pAllocation);
    }

    KIRQL OldIrql;
    KeAcquireSpinLock(&pSource->AllocationLock, &OldIrql);
    pSource->pPrimaryAllocation = pAllocation;
    KeReleaseSpinLock(&pSource->AllocationLock, OldIrql);
}

DECLINLINE(PVBOXWDDM_ALLOCATION) vboxWddmAquirePrimary(PVBOXMP_DEVEXT pDevExt, PVBOXWDDM_SOURCE pSource, D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId)
{
    PVBOXWDDM_ALLOCATION pPrimary;
    KIRQL OldIrql;
    KeAcquireSpinLock(&pSource->AllocationLock, &OldIrql);
    pPrimary = pSource->pPrimaryAllocation;
    if (pPrimary)
        vboxWddmAllocationRetain(pPrimary);
    KeReleaseSpinLock(&pSource->AllocationLock, OldIrql);
    return pPrimary;
}

#define VBOXWDDMENTRY_2_SWAPCHAIN(_pE) ((PVBOXWDDM_SWAPCHAIN)((uint8_t*)(_pE) - RT_OFFSETOF(VBOXWDDM_SWAPCHAIN, DevExtListEntry)))

#ifdef VBOXWDDM_RENDER_FROM_SHADOW
# ifdef VBOX_WDDM_WIN8
#  define VBOXWDDM_FB_ALLOCATION(_pDevExt, _pSrc) ( (g_VBoxDisplayOnly || (_pDevExt)->fRenderToShadowDisabled) ? (_pSrc)->pPrimaryAllocation : (_pSrc)->pShadowAllocation)
# else
#  define VBOXWDDM_FB_ALLOCATION(_pDevExt, _pSrc) ((_pDevExt)->fRenderToShadowDisabled ? (_pSrc)->pPrimaryAllocation : (_pSrc)->pShadowAllocation)
# endif
#else
# define VBOXWDDM_FB_ALLOCATION(_pDevExt, _pSrc) ((_pSrc)->pPrimaryAllocation)
#endif

#endif /* #ifndef ___VBoxMPWddm_h___ */

