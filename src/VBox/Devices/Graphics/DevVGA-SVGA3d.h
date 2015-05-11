/** @file
 * DevVMWare - VMWare SVGA device - 3D part.
 */

/*
 * Copyright (C) 2013-2015 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___DEVVMWARE3D_H___
#define ___DEVVMWARE3D_H___

#include "vmsvga/svga_reg.h"
#include "vmsvga/svga3d_reg.h"
#include "vmsvga/svga_escape.h"
#include "vmsvga/svga_overlay.h"

#if defined(RT_OS_WINDOWS) && defined(IN_RING3)
# include <Windows.h>

# define WM_VMSVGA3D_WAKEUP                     (WM_APP+1)
# define WM_VMSVGA3D_CREATEWINDOW               (WM_APP+2)
# define WM_VMSVGA3D_DESTROYWINDOW              (WM_APP+3)
# define WM_VMSVGA3D_RESIZEWINDOW               (WM_APP+4)
# define WM_VMSVGA3D_EXIT                       (WM_APP+5)

DECLCALLBACK(int) vmsvga3dWindowThread(RTTHREAD ThreadSelf, void *pvUser);
int vmsvga3dSendThreadMessage(RTTHREAD pWindowThread, RTSEMEVENT WndRequestSem, UINT msg, WPARAM wParam, LPARAM lParam);

#endif

/** Arbitrary limit */
#define SVGA3D_MAX_SHADER_IDS                   0x800
/** D3D allows up to 8 texture stages. */
#define SVGA3D_MAX_TEXTURE_STAGE                8
/** Arbitrary upper limit; seen 8 so far. */
#define SVGA3D_MAX_LIGHTS                       32

void vmsvgaGMRFree(PVGASTATE pThis, uint32_t idGMR);
int vmsvgaGMRTransfer(PVGASTATE pThis, const SVGA3dTransferType enmTransferType, uint8_t *pDest, int32_t cbDestPitch,
                      SVGAGuestPtr src, uint32_t offSrc, int32_t cbSrcPitch, uint32_t cbWidth, uint32_t cHeight);

int vmsvga3dInit(PVGASTATE pThis);
int vmsvga3dPowerOn(PVGASTATE pThis);
int vmsvga3dLoadExec(PVGASTATE pThis, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass);
int vmsvga3dSaveExec(PVGASTATE pThis, PSSMHANDLE pSSM);
int vmsvga3dTerminate(PVGASTATE pThis);
int vmsvga3dReset(PVGASTATE pThis);
int vmsvga3dQueryCaps(PVGASTATE pThis, uint32_t idx3dCaps, uint32_t *pu32Val);

int vmsvga3dSurfaceDefine(PVGASTATE pThis, uint32_t sid, uint32_t surfaceFlags, SVGA3dSurfaceFormat format, SVGA3dSurfaceFace face[SVGA3D_MAX_SURFACE_FACES], uint32_t multisampleCount, SVGA3dTextureFilter autogenFilter, uint32_t cMipLevels, SVGA3dSize *pMipLevelSize);
int vmsvga3dSurfaceDestroy(PVGASTATE pThis, uint32_t sid);
int vmsvga3dSurfaceCopy(PVGASTATE pThis, SVGA3dSurfaceImageId dest, SVGA3dSurfaceImageId src, uint32_t cCopyBoxes, SVGA3dCopyBox *pBox);
int vmsvga3dSurfaceStretchBlt(PVGASTATE pThis, SVGA3dSurfaceImageId dest, SVGA3dBox destBox, SVGA3dSurfaceImageId src, SVGA3dBox srcBox, SVGA3dStretchBltMode mode);
int vmsvga3dSurfaceDMA(PVGASTATE pThis, SVGA3dGuestImage guest, SVGA3dSurfaceImageId host, SVGA3dTransferType transfer, uint32_t cCopyBoxes, SVGA3dCopyBox *pBoxes);
int vmsvga3dSurfaceBlitToScreen(PVGASTATE pThis, uint32_t dest, SVGASignedRect destRect, SVGA3dSurfaceImageId src, SVGASignedRect srcRect, uint32_t cRects, SVGASignedRect *pRect);

int vmsvga3dContextDefine(PVGASTATE pThis, uint32_t cid);
int vmsvga3dContextDestroy(PVGASTATE pThis, uint32_t cid);

int vmsvga3dChangeMode(PVGASTATE pThis);

int vmsvga3dSetTransform(PVGASTATE pThis, uint32_t cid, SVGA3dTransformType type, float matrix[16]);
int vmsvga3dSetZRange(PVGASTATE pThis, uint32_t cid, SVGA3dZRange zRange);
int vmsvga3dSetRenderState(PVGASTATE pThis, uint32_t cid, uint32_t cRenderStates, SVGA3dRenderState *pRenderState);
int vmsvga3dSetRenderTarget(PVGASTATE pThis, uint32_t cid, SVGA3dRenderTargetType type, SVGA3dSurfaceImageId target);
int vmsvga3dSetTextureState(PVGASTATE pThis, uint32_t cid, uint32_t cTextureStates, SVGA3dTextureState *pTextureState);
int vmsvga3dSetMaterial(PVGASTATE pThis, uint32_t cid, SVGA3dFace face, SVGA3dMaterial *pMaterial);
int vmsvga3dSetLightData(PVGASTATE pThis, uint32_t cid, uint32_t index, SVGA3dLightData *pData);
int vmsvga3dSetLightEnabled(PVGASTATE pThis, uint32_t cid, uint32_t index, uint32_t enabled);
int vmsvga3dSetViewPort(PVGASTATE pThis, uint32_t cid, SVGA3dRect *pRect);
int vmsvga3dSetClipPlane(PVGASTATE pThis, uint32_t cid,  uint32_t index, float plane[4]);
int vmsvga3dCommandClear(PVGASTATE pThis, uint32_t cid, SVGA3dClearFlag clearFlag, uint32_t color, float depth, uint32_t stencil, uint32_t cRects, SVGA3dRect *pRect);
int vmsvga3dCommandPresent(PVGASTATE pThis, uint32_t sid, uint32_t cRects, SVGA3dCopyRect *pRect);
int vmsvga3dDrawPrimitives(PVGASTATE pThis, uint32_t cid, uint32_t numVertexDecls, SVGA3dVertexDecl *pVertexDecl, uint32_t numRanges, SVGA3dPrimitiveRange *pNumRange, uint32_t cVertexDivisor, SVGA3dVertexDivisor *pVertexDivisor);
int vmsvga3dSetScissorRect(PVGASTATE pThis, uint32_t cid, SVGA3dRect *pRect);
int vmsvga3dGenerateMipmaps(PVGASTATE pThis, uint32_t sid, SVGA3dTextureFilter filter);

int vmsvga3dShaderDefine(PVGASTATE pThis, uint32_t cid, uint32_t shid, SVGA3dShaderType type, uint32_t cbData, uint32_t *pShaderData);
int vmsvga3dShaderDestroy(PVGASTATE pThis, uint32_t cid, uint32_t shid, SVGA3dShaderType type);
int vmsvga3dShaderSet(PVGASTATE pThis, struct VMSVGA3DCONTEXT *pContext, uint32_t cid, SVGA3dShaderType type, uint32_t shid);
int vmsvga3dShaderSetConst(PVGASTATE pThis, uint32_t cid, uint32_t reg, SVGA3dShaderType type, SVGA3dShaderConstType ctype, uint32_t cRegisters, uint32_t *pValues);

int vmsvga3dQueryBegin(PVGASTATE pThis, uint32_t cid, SVGA3dQueryType type);
int vmsvga3dQueryEnd(PVGASTATE pThis, uint32_t cid, SVGA3dQueryType type, SVGAGuestPtr guestResult);
int vmsvga3dQueryWait(PVGASTATE pThis, uint32_t cid, SVGA3dQueryType type, SVGAGuestPtr guestResult);


uint32_t vmsvga3dSurfaceFormatSize(SVGA3dSurfaceFormat format);

#ifdef LOG_ENABLED
const char *vmsvga3dGetCapString(uint32_t idxCap);
const char *vmsvga3dGet3dFormatString(uint32_t format);
const char *vmsvga3dGetRenderStateName(uint32_t state);
const char *vmsvga3dTextureStateToString(SVGA3dTextureStateName textureState);
const char *vmsvgaTransformToString(SVGA3dTransformType type);
const char *vmsvgaDeclUsage2String(SVGA3dDeclUsage usage);
const char *vmsvgaDeclType2String(SVGA3dDeclType type);
const char *vmsvgaDeclMethod2String(SVGA3dDeclMethod method);
const char *vmsvgaSurfaceType2String(SVGA3dSurfaceFormat format);
const char *vmsvga3dPrimitiveType2String(SVGA3dPrimitiveType PrimitiveType);
#endif

#endif  /* !___DEVVMWARE3D_H___ */

