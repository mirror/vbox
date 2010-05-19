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

/*
 * with wddm 2D commands are split into two categories:
 * 1. control commands: those that are not (and actually can not be)
 * handled.trasfered as DMA commands in terms of WDDM model
 * these are:
 *  VBOXVHWACMD_TYPE_QUERY_INFO1,
 *  VBOXVHWACMD_TYPE_QUERY_INFO2,
 *  VBOXVHWACMD_TYPE_ENABLE,
 *  VBOXVHWACMD_TYPE_DISABLE,
 * 2. DMA commands: all the rest are handled as DMA commands
 * (i.e. using vboxVdmaXxx API)
 * This is opaque to the host 2D frontend
 * */
#define VBOXVHWA_MAX_FORMATS 64
typedef struct VBOXVHWA_INFO
{
    bool bEnabled;
    uint32_t cOverlaysSupported;
    uint32_t cFormats;
    D3DDDIFORMAT aFormats[VBOXVHWA_MAX_FORMATS];
} VBOXVHWA_INFO;
DECLINLINE(void) vboxVhwaInitHdr(VBOXVHWACMD* pHdr, D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId, VBOXVHWACMD_TYPE enmCmd)
{
    memset(pHdr, 0, sizeof(VBOXVHWACMD));
    pHdr->iDisplay = srcId;
    pHdr->rc = VERR_GENERAL_FAILURE;
    pHdr->enmCmd = enmCmd;
    /* no need for the cRefs since we're using SHGSMI that has its own ref counting */
//    pHdr->cRefs = 1;
}

VBOXVHWACMD* vboxVhwaCtlCommandCreate(PDEVICE_EXTENSION pDevExt,
        D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId,
        VBOXVHWACMD_TYPE enmCmd,
        VBOXVHWACMD_LENGTH cbCmd);

void vboxVhwaCtlCommandFree(PDEVICE_EXTENSION pDevExt, VBOXVHWACMD* pCmd);
int vboxVhwaCtlCommandSubmit(PDEVICE_EXTENSION pDevExt, VBOXVHWACMD* pCmd);
int vboxVhwaCtlCommandSubmit(PDEVICE_EXTENSION pDevExt, VBOXVHWACMD* pCmd);
void vboxVHWAFreeHostInfo1(PDEVICE_EXTENSION pDevExt, VBOXVHWACMD_QUERYINFO1* pInfo);
void vboxVHWAFreeHostInfo2(PDEVICE_EXTENSION pDevExt, VBOXVHWACMD_QUERYINFO2* pInfo);
VBOXVHWACMD_QUERYINFO1* vboxVHWAQueryHostInfo1(PDEVICE_EXTENSION pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId);
VBOXVHWACMD_QUERYINFO2* vboxVHWAQueryHostInfo2(PDEVICE_EXTENSION pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId, uint32_t numFourCC);
int vboxVHWAEnable(PDEVICE_EXTENSION pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId);
int vboxVHWADisable(PDEVICE_EXTENSION pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId);
void vboxVHWAInit(PDEVICE_EXTENSION pDevExt);


#endif /* #ifndef ___VBoxVideoVhwa_h___ */
