/* $Id$ */
/** @file
 * VirtualBox Windows Guest Mesa3D - Gallium driver interface.
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

#ifndef ___VBoxGaDriver_h__
#define ___VBoxGaDriver_h__
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBoxGaHWInfo.h>
#include <VBoxGaTypes.h>

#include <iprt/win/windows.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct WDDMGalliumDriverEnv
{
    /* Size of the structure. */
    DWORD cb;
    const VBOXGAHWINFO *pHWInfo;
    /* The environment context pointer to use in the following callbacks. */
    void *pvEnv;
    /* The callbacks to use by the driver. */
    DECLCALLBACKMEMBER(uint32_t, pfnContextCreate)(void *pvEnv,
                                                   boolean extended,
                                                   boolean vgpu10);
    DECLCALLBACKMEMBER(void, pfnContextDestroy)(void *pvEnv,
                                                uint32_t u32Cid);
    DECLCALLBACKMEMBER(int, pfnSurfaceDefine)(void *pvEnv,
                                              GASURFCREATE *pCreateParms,
                                              GASURFSIZE *paSizes,
                                              uint32_t cSizes,
                                              uint32_t *pu32Sid);
    DECLCALLBACKMEMBER(void, pfnSurfaceDestroy)(void *pvEnv,
                                                uint32_t u32Sid);
    DECLCALLBACKMEMBER(int, pfnRender)(void *pvEnv,
                                       uint32_t u32Cid,
                                       void *pvCommands,
                                       uint32_t cbCommands,
                                       GAFENCEQUERY *pFenceQuery);
    DECLCALLBACKMEMBER(void, pfnFenceUnref)(void *pvEnv,
                                            uint32_t u32FenceHandle);
    DECLCALLBACKMEMBER(int, pfnFenceQuery)(void *pvEnv,
                                           uint32_t u32FenceHandle,
                                           GAFENCEQUERY *pFenceQuery);
    DECLCALLBACKMEMBER(int, pfnFenceWait)(void *pvEnv,
                                          uint32_t u32FenceHandle,
                                          uint32_t u32TimeoutUS);
    DECLCALLBACKMEMBER(int, pfnRegionCreate)(void *pvEnv,
                                             uint32_t u32RegionSize,
                                             uint32_t *pu32GmrId,
                                             void **ppvMap);
    DECLCALLBACKMEMBER(void, pfnRegionDestroy)(void *pvEnv,
                                               uint32_t u32GmrId,
                                               void *pvMap);
} WDDMGalliumDriverEnv;

typedef struct pipe_screen * WINAPI FNGaDrvScreenCreate(const WDDMGalliumDriverEnv *pEnv);
typedef FNGaDrvScreenCreate *PFNGaDrvScreenCreate;

typedef void WINAPI FNGaDrvScreenDestroy(struct pipe_screen *s);
typedef FNGaDrvScreenDestroy *PFNGaDrvScreenDestroy;

typedef const WDDMGalliumDriverEnv * WINAPI FNGaDrvGetWDDMEnv(struct pipe_screen *pScreen);
typedef FNGaDrvGetWDDMEnv *PFNGaDrvGetWDDMEnv;

typedef uint32_t WINAPI FNGaDrvGetContextId(struct pipe_context *pPipeContext);
typedef FNGaDrvGetContextId *PFNGaDrvGetContextId;

typedef uint32_t WINAPI FNGaDrvGetSurfaceId(struct pipe_screen *pScreen, struct pipe_resource *pResource);
typedef FNGaDrvGetSurfaceId *PFNGaDrvGetSurfaceId;

typedef void WINAPI FNGaDrvContextFlush(struct pipe_context *pPipeContext);
typedef FNGaDrvContextFlush *PFNGaDrvContextFlush;

#ifdef __cplusplus
}
#endif

#endif

