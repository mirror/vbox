/* $Id$ */
/** @file
 * VBox Miniport common functions used by XPDM/WDDM drivers
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

#ifndef VBOXMPCOMMON_H
#define VBOXMPCOMMON_H

#include "VBoxMPDevExt.h"

RT_C_DECLS_BEGIN

int VBoxMPCmnMapAdapterMemory(PVBOXMP_COMMON pCommon, void **ppv, uint32_t ulOffset, uint32_t ulSize);
void VBoxMPCmnUnmapAdapterMemory(PVBOXMP_COMMON pCommon, void **ppv);

typedef bool(*PFNVIDEOIRQSYNC)(void *);
bool VBoxMPCmnSyncToVideoIRQ(PVBOXMP_COMMON pCommon, PFNVIDEOIRQSYNC pfnSync, void *pvUser);

/* Video modes related */
#define VBOXMP_MAX_VIDEO_MODES 128
void VBoxMPCmnInitCustomVideoModes(PVBOXMP_DEVEXT pExt);
VIDEO_MODE_INFORMATION* VBoxMPCmnGetCustomVideoModeInfo(ULONG ulIndex);
VIDEO_MODE_INFORMATION* VBoxMPCmnGetVideoModeInfo(ULONG ulIndex);

#ifdef VBOX_XPDM_MINIPORT
VIDEO_MODE_INFORMATION* VBoxMPXpdmCurrentVideoMode(PVBOXMP_DEVEXT pExt);
ULONG VBoxMPXpdmGetVideoModesCount();
void VBoxMPXpdmBuildVideoModesTable(PVBOXMP_DEVEXT pExt);
#endif

#ifdef VBOX_WDDM_MINIPORT
void VBoxWddmInvalidateAllVideoModesInfos(PVBOXMP_DEVEXT pExt);
void VBoxWddmInvalidateVideoModesInfo(PVBOXMP_DEVEXT pExt, D3DDDI_VIDEO_PRESENT_TARGET_ID VidPnTargetId);
PVBOXWDDM_VIDEOMODES_INFO VBoxWddmUpdateVideoModesInfoByMask(PVBOXMP_DEVEXT pExt, uint8_t *pScreenIdMask);
PVBOXWDDM_VIDEOMODES_INFO VBoxWddmUpdateAllVideoModesInfos(PVBOXMP_DEVEXT pExt);
NTSTATUS VBoxWddmGetModesForResolution(VIDEO_MODE_INFORMATION *pAllModes, uint32_t cAllModes, int iSearchPreferredMode,
                                       const D3DKMDT_2DREGION *pResolution, VIDEO_MODE_INFORMATION * pModes,
                                       uint32_t cModes, uint32_t *pcModes, int32_t *piPreferrableMode);
PVBOXWDDM_VIDEOMODES_INFO VBoxWddmGetAllVideoModesInfos(PVBOXMP_DEVEXT pExt);
PVBOXWDDM_VIDEOMODES_INFO VBoxWddmGetVideoModesInfo(PVBOXMP_DEVEXT pExt, D3DDDI_VIDEO_PRESENT_TARGET_ID VidPnTargetId);
bool VBoxWddmFillMode(VIDEO_MODE_INFORMATION *pInfo, D3DDDIFORMAT enmFormat, ULONG w, ULONG h);
bool VBoxWddmFillMode(VIDEO_MODE_INFORMATION *pInfo, D3DDDIFORMAT enmFormat, ULONG w, ULONG h);
void VBoxWddmAdjustMode(PVBOXMP_DEVEXT pExt, PVBOXWDDM_ADJUSTVIDEOMODE pMode);
void VBoxWddmAdjustModes(PVBOXMP_DEVEXT pExt, uint32_t cModes, PVBOXWDDM_ADJUSTVIDEOMODE aModes);
#endif

/* Registry access */
#ifdef VBOX_XPDM_MINIPORT
typedef PVBOXMP_DEVEXT VBOXMPCMNREGISTRY;
#else
typedef HANDLE VBOXMPCMNREGISTRY;
#endif

VP_STATUS VBoxMPCmnRegInit(IN PVBOXMP_DEVEXT pExt, OUT VBOXMPCMNREGISTRY *pReg);
VP_STATUS VBoxMPCmnRegFini(IN VBOXMPCMNREGISTRY Reg);
VP_STATUS VBoxMPCmnRegSetDword(IN VBOXMPCMNREGISTRY Reg, PWSTR pName, uint32_t Val);
VP_STATUS VBoxMPCmnRegQueryDword(IN VBOXMPCMNREGISTRY Reg, PWSTR pName, uint32_t *pVal);

/* Pointer related */
inline bool VBoxMPCmnUpdatePointerShape(PVBOXMP_COMMON pCommon, PVIDEO_POINTER_ATTRIBUTES pAttrs, uint32_t cbLength)
{
    return VBoxHGSMIUpdatePointerShape(&pCommon->guestCtx,
                                       pAttrs->Enable & 0x0000FFFF,
                                       (pAttrs->Enable >> 16) & 0xFF,
                                       (pAttrs->Enable >> 24) & 0xFF,
                                       pAttrs->Width, pAttrs->Height, pAttrs->Pixels,
                                       cbLength - sizeof(VIDEO_POINTER_ATTRIBUTES));
}

RT_C_DECLS_END

#endif /*VBOXMPCOMMON_H*/
