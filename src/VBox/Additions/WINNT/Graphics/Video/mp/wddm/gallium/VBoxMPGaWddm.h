/* $Id$ */
/** @file
 * VirtualBox Windows Guest Mesa3D - Gallium driver interface for WDDM kernel mode driver.
 */

/*
 * Copyright (C) 2016-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_gallium_VBoxMPGaWddm_h
#define GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_gallium_VBoxMPGaWddm_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "common/VBoxMPDevExt.h"

NTSTATUS GaAdapterStart(PVBOXMP_DEVEXT pDevExt);
void GaAdapterStop(PVBOXMP_DEVEXT pDevExt);

NTSTATUS GaQueryInfo(PVBOXWDDM_EXT_GA pGaDevExt,
                     VBOXVIDEO_HWTYPE enmHwType,
                     VBOXGAHWINFO *pHWInfo);

NTSTATUS GaScreenDefine(PVBOXWDDM_EXT_GA pGaDevExt,
                        uint32_t u32Offset,
                        uint32_t u32ScreenId,
                        int32_t xOrigin,
                        int32_t yOrigin,
                        uint32_t u32Width,
                        uint32_t u32Height,
                        bool fBlank);
NTSTATUS GaScreenDestroy(PVBOXWDDM_EXT_GA pGaDevExt,
                         uint32_t u32ScreenId);

NTSTATUS GaDeviceCreate(PVBOXWDDM_EXT_GA pGaDevExt,
                        PVBOXWDDM_DEVICE pDevice);
void GaDeviceDestroy(PVBOXWDDM_EXT_GA pGaDevExt,
                     PVBOXWDDM_DEVICE pDevice);

NTSTATUS GaContextCreate(PVBOXWDDM_EXT_GA pGaDevExt,
                         PVBOXWDDM_CREATECONTEXT_INFO pInfo,
                         PVBOXWDDM_CONTEXT pContext);
NTSTATUS GaContextDestroy(PVBOXWDDM_EXT_GA pGaDevExt,
                          PVBOXWDDM_CONTEXT pContext);
NTSTATUS GaUpdate(PVBOXWDDM_EXT_GA pGaDevExt,
                  uint32_t u32X,
                  uint32_t u32Y,
                  uint32_t u32Width,
                  uint32_t u32Height);

NTSTATUS GaDefineCursor(PVBOXWDDM_EXT_GA pGaDevExt,
                        uint32_t u32HotspotX,
                        uint32_t u32HotspotY,
                        uint32_t u32Width,
                        uint32_t u32Height,
                        uint32_t u32AndMaskDepth,
                        uint32_t u32XorMaskDepth,
                        void const *pvAndMask,
                        uint32_t cbAndMask,
                        void const *pvXorMask,
                        uint32_t cbXorMask);

NTSTATUS GaDefineAlphaCursor(PVBOXWDDM_EXT_GA pGaDevExt,
                             uint32_t u32HotspotX,
                             uint32_t u32HotspotY,
                             uint32_t u32Width,
                             uint32_t u32Height,
                             void const *pvImage,
                             uint32_t cbImage);

NTSTATUS APIENTRY GaDxgkDdiBuildPagingBuffer(const HANDLE hAdapter,
                                             DXGKARG_BUILDPAGINGBUFFER *pBuildPagingBuffer);
NTSTATUS APIENTRY GaDxgkDdiPresentDisplayOnly(const HANDLE hAdapter,
                                              const DXGKARG_PRESENT_DISPLAYONLY *pPresentDisplayOnly);
NTSTATUS APIENTRY GaDxgkDdiPresent(const HANDLE hContext,
                                   DXGKARG_PRESENT *pPresent);
NTSTATUS APIENTRY GaDxgkDdiRender(const HANDLE hContext,
                                  DXGKARG_RENDER *pRender);
NTSTATUS APIENTRY GaDxgkDdiPatch(const HANDLE hAdapter,
                                 const DXGKARG_PATCH *pPatch);
NTSTATUS APIENTRY GaDxgkDdiSubmitCommand(const HANDLE hAdapter,
                                         const DXGKARG_SUBMITCOMMAND *pSubmitCommand);
NTSTATUS APIENTRY GaDxgkDdiPreemptCommand(const HANDLE hAdapter,
                                          const DXGKARG_PREEMPTCOMMAND *pPreemptCommand);
NTSTATUS APIENTRY GaDxgkDdiQueryCurrentFence(const HANDLE hAdapter,
                                             DXGKARG_QUERYCURRENTFENCE *pCurrentFence);
BOOLEAN GaDxgkDdiInterruptRoutine(const PVOID MiniportDeviceContext,
                                  ULONG MessageNumber);
VOID GaDxgkDdiDpcRoutine(const PVOID MiniportDeviceContext);
NTSTATUS APIENTRY GaDxgkDdiEscape(const HANDLE hAdapter,
                                  const DXGKARG_ESCAPE *pEscape);

DECLINLINE(bool) GaContextTypeIs(PVBOXWDDM_CONTEXT pContext, VBOXWDDM_CONTEXT_TYPE enmType)
{
    return (pContext && pContext->enmType == enmType);
}

DECLINLINE(bool) GaContextHwTypeIs(PVBOXWDDM_CONTEXT pContext, VBOXVIDEO_HWTYPE enmHwType)
{
    return (pContext && pContext->pDevice->pAdapter->enmHwType == enmHwType);
}

NTSTATUS GaVidPnSourceReport(PVBOXMP_DEVEXT pDevExt, VBOXWDDM_SOURCE *pSource);
NTSTATUS GaVidPnSourceCheckPos(PVBOXMP_DEVEXT pDevExt, UINT iSource);

#endif /* !GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_gallium_VBoxMPGaWddm_h */
