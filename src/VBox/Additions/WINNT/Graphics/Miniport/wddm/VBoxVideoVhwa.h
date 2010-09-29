/*
 * Copyright (C) 2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */
#ifndef ___VBoxVideoVhwa_h___
#define ___VBoxVideoVhwa_h___

#include <iprt/cdefs.h>

#include "VBoxVideoShgsmi.h"

VBOXVHWACMD* vboxVhwaCommandCreate(PDEVICE_EXTENSION pDevExt,
        D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId,
        VBOXVHWACMD_TYPE enmCmd,
        VBOXVHWACMD_LENGTH cbCmd);

void vboxVhwaCommandFree(PDEVICE_EXTENSION pDevExt, VBOXVHWACMD* pCmd);
int vboxVhwaCommandSubmit(PDEVICE_EXTENSION pDevExt, VBOXVHWACMD* pCmd);
int vboxVhwaCommandSubmit(PDEVICE_EXTENSION pDevExt, VBOXVHWACMD* pCmd);
void vboxVhwaCommandSubmitAsynchAndComplete(PDEVICE_EXTENSION pDevExt, VBOXVHWACMD* pCmd);

#ifndef VBOXVHWA_WITH_SHGSMI
typedef DECLCALLBACK(void) FNVBOXVHWACMDCOMPLETION(PDEVICE_EXTENSION pDevExt, VBOXVHWACMD * pCmd, void * pContext);
typedef FNVBOXVHWACMDCOMPLETION *PFNVBOXVHWACMDCOMPLETION;

void vboxVhwaCommandSubmitAsynch(PDEVICE_EXTENSION pDevExt, VBOXVHWACMD* pCmd, PFNVBOXVHWACMDCOMPLETION pfnCompletion, void * pContext);
void vboxVhwaCommandSubmitAsynchByEvent(PDEVICE_EXTENSION pDevExt, VBOXVHWACMD* pCmd, RTSEMEVENT hEvent);

#define VBOXVHWA_CMD2LISTENTRY(_pCmd) ((PVBOXSHGSMILIST_ENTRY)&(_pCmd)->u.pNext)
#define VBOXVHWA_LISTENTRY2CMD(_pEntry) ( (VBOXVHWACMD*)((uint8_t *)(_pEntry) - RT_OFFSETOF(VBOXVHWACMD, u.pNext)) )

DECLINLINE(void) vboxVhwaPutList(VBOXSHGSMILIST *pList, VBOXVHWACMD* pCmd)
{
    vboxSHGSMIListPut(pList, VBOXVHWA_CMD2LISTENTRY(pCmd), VBOXVHWA_CMD2LISTENTRY(pCmd));
}

void vboxVhwaCompletionListProcess(PDEVICE_EXTENSION pDevExt, VBOXSHGSMILIST *pList);
#endif

void vboxVhwaFreeHostInfo1(PDEVICE_EXTENSION pDevExt, VBOXVHWACMD_QUERYINFO1* pInfo);
void vboxVhwaFreeHostInfo2(PDEVICE_EXTENSION pDevExt, VBOXVHWACMD_QUERYINFO2* pInfo);
VBOXVHWACMD_QUERYINFO1* vboxVhwaQueryHostInfo1(PDEVICE_EXTENSION pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId);
VBOXVHWACMD_QUERYINFO2* vboxVhwaQueryHostInfo2(PDEVICE_EXTENSION pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId, uint32_t numFourCC);
int vboxVhwaEnable(PDEVICE_EXTENSION pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId);
int vboxVhwaDisable(PDEVICE_EXTENSION pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId);
void vboxVhwaInit(PDEVICE_EXTENSION pDevExt);
void vboxVhwaFree(PDEVICE_EXTENSION pDevExt);

int vboxVhwaHlpOverlayFlip(PVBOXWDDM_OVERLAY pOverlay, const DXGKARG_FLIPOVERLAY *pFlipInfo);
int vboxVhwaHlpOverlayUpdate(PVBOXWDDM_OVERLAY pOverlay, const DXGK_OVERLAYINFO *pOverlayInfo);
int vboxVhwaHlpOverlayDestroy(PVBOXWDDM_OVERLAY pOverlay);
int vboxVhwaHlpOverlayCreate(PDEVICE_EXTENSION pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId, DXGK_OVERLAYINFO *pOverlayInfo, /* OUT */ PVBOXWDDM_OVERLAY pOverlay);

int vboxVhwaHlpColorFill(PVBOXWDDM_OVERLAY pOverlay, PVBOXWDDM_DMA_PRIVATEDATA_CLRFILL pCF);

int vboxVhwaHlpGetSurfInfo(PDEVICE_EXTENSION pDevExt, PVBOXWDDM_ALLOCATION pSurf);

BOOLEAN vboxVhwaHlpOverlayListIsEmpty(PDEVICE_EXTENSION pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId);
void vboxVhwaHlpOverlayListAdd(PDEVICE_EXTENSION pDevExt, PVBOXWDDM_OVERLAY pOverlay);
void vboxVhwaHlpOverlayListRemove(PDEVICE_EXTENSION pDevExt, PVBOXWDDM_OVERLAY pOverlay);
void vboxVhwaHlpOverlayDstRectUnion(PDEVICE_EXTENSION pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId, RECT *pRect);
void vboxVhwaHlpOverlayDstRectGet(PDEVICE_EXTENSION pDevExt, PVBOXWDDM_OVERLAY pOverlay, RECT *pRect);

#endif /* #ifndef ___VBoxVideoVhwa_h___ */
