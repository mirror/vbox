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


#define VBOXWDDMENTRY_2_SWAPCHAIN(_pE) ((PVBOXWDDM_SWAPCHAIN)((uint8_t*)(_pE) - RT_OFFSETOF(VBOXWDDM_SWAPCHAIN, DevExtListEntry)))

#ifdef VBOXWDDM_RENDER_FROM_SHADOW
# define VBOXWDDM_FB_ALLOCATION(_pDevExt, _pSrc) ((_pDevExt)->fRenderToShadowDisabled ? (_pSrc)->pPrimaryAllocation : (_pSrc)->pShadowAllocation)
#else
# define VBOXWDDM_FB_ALLOCATION(_pDevExt, _pSrc) ((_pSrc)->pPrimaryAllocation)
#endif

#endif /* #ifndef ___VBoxMPWddm_h___ */

