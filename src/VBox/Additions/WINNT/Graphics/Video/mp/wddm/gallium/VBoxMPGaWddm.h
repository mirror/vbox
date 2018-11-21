/* $Id$ */
/** @file
 * VirtualBox Windows Guest Mesa3D - Gallium driver interface for WDDM kernel mode driver.
 */

/*
 * Copyright (C) 2016-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___VBoxMPGaWddm_h__
#define ___VBoxMPGaWddm_h__

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
                        uint32_t u32Height);

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

NTSTATUS APIENTRY GaDxgkDdiBuildPagingBuffer(const HANDLE hAdapter,
                                             DXGKARG_BUILDPAGINGBUFFER *pBuildPagingBuffer);
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

#endif
