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

void vboxVHWAFreeHostInfo1(PDEVICE_EXTENSION pDevExt, VBOXVHWACMD_QUERYINFO1* pInfo);
void vboxVHWAFreeHostInfo2(PDEVICE_EXTENSION pDevExt, VBOXVHWACMD_QUERYINFO2* pInfo);
VBOXVHWACMD_QUERYINFO1* vboxVHWAQueryHostInfo1(PDEVICE_EXTENSION pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId);
VBOXVHWACMD_QUERYINFO2* vboxVHWAQueryHostInfo2(PDEVICE_EXTENSION pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId, uint32_t numFourCC);
int vboxVHWAEnable(PDEVICE_EXTENSION pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId);
int vboxVHWADisable(PDEVICE_EXTENSION pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId);
void vboxVHWAInit(PDEVICE_EXTENSION pDevExt);

#endif /* #ifndef ___VBoxVideoVhwa_h___ */
