/* $Id$ */
/** @file
 * DevVMWare - VMWare SVGA device - 3D part.
 */

/*
 * Copyright (C) 2013-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VBOX_INCLUDED_SRC_Graphics_DevVGA_SVGA3d_h
#define VBOX_INCLUDED_SRC_Graphics_DevVGA_SVGA3d_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "DevVGA-SVGA.h"


/** Arbitrary limit */
#define SVGA3D_MAX_SHADER_IDS                   0x800
/** D3D allows up to 8 texture stages. */
#define SVGA3D_MAX_TEXTURE_STAGES               8
/** Samplers: 16 Pixel Shader + 1 Displacement Map + 4 Vertex Shader */
#define SVGA3D_MAX_SAMPLERS_PS         16
#define SVGA3D_MAX_SAMPLERS_DMAP       1
#define SVGA3D_MAX_SAMPLERS_VS         4
#define SVGA3D_MAX_SAMPLERS            (SVGA3D_MAX_SAMPLERS_PS + SVGA3D_MAX_SAMPLERS_DMAP + SVGA3D_MAX_SAMPLERS_VS)
/** Arbitrary upper limit; seen 8 so far. */
#define SVGA3D_MAX_LIGHTS                       32
/** Arbitrary upper limit; 2GB enough for 32768x16384*4. */
#define SVGA3D_MAX_SURFACE_MEM_SIZE             0x80000000


/**@def FLOAT_FMT_STR
 * Format string bits to go with FLOAT_FMT_ARGS. */
#define FLOAT_FMT_STR                           "%s%u.%06u"
/** @def FLOAT_FMT_ARGS
 * Format arguments for a float value, corresponding to FLOAT_FMT_STR.
 * @param   r       The floating point value to format.  */
#define FLOAT_FMT_ARGS(r)                       (r) >= 0.0f ? "" : "-", (unsigned)RT_ABS(r), (unsigned)(RT_ABS((r) - (unsigned)(r)) * 1000000.0f)


typedef enum VMSVGA3D_SURFACE_MAP
{
    VMSVGA3D_SURFACE_MAP_READ,
    VMSVGA3D_SURFACE_MAP_WRITE,
    VMSVGA3D_SURFACE_MAP_READ_WRITE,
    VMSVGA3D_SURFACE_MAP_WRITE_DISCARD,
} VMSVGA3D_SURFACE_MAP;

typedef struct VMSVGA3D_MAPPED_SURFACE
{
    VMSVGA3D_SURFACE_MAP enmMapType;
    SVGA3dBox box;
    uint32_t cbPixel;
    uint32_t cbRowPitch;
    uint32_t cbDepthPitch;
    void *pvData;
} VMSVGA3D_MAPPED_SURFACE;

#ifdef DUMP_BITMAPS
void vmsvga3dMapWriteBmpFile(VMSVGA3D_MAPPED_SURFACE const *pMap, char const *pszPrefix);
#endif

/* DevVGA-SVGA.cpp: */
void vmsvgaR33dSurfaceUpdateHeapBuffersOnFifoThread(PPDMDEVINS pDevIns, PVGASTATE pThis, PVGASTATECC pThisCC, uint32_t sid);


/* DevVGA-SVGA3d-ogl.cpp & DevVGA-SVGA3d-win.cpp: */
int vmsvga3dInit(PPDMDEVINS pDevIns, PVGASTATE pThis, PVGASTATECC pThisCC);
int vmsvga3dPowerOn(PPDMDEVINS pDevIns, PVGASTATE pThis, PVGASTATECC pThisCC);
int vmsvga3dLoadExec(PPDMDEVINS pDevIns, PVGASTATE pThis, PVGASTATECC pThisCC, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass);
int vmsvga3dSaveExec(PPDMDEVINS pDevIns, PVGASTATECC pThisCC, PSSMHANDLE pSSM);
int vmsvga3dTerminate(PVGASTATECC pThisCC);
int vmsvga3dReset(PVGASTATECC pThisCC);
void vmsvga3dUpdateHostScreenViewport(PVGASTATECC pThisCC, uint32_t idScreen, VMSVGAVIEWPORT const *pOldViewport);
int vmsvga3dQueryCaps(PVGASTATECC pThisCC, SVGA3dDevCapIndex idx3dCaps, uint32_t *pu32Val);

int vmsvga3dSurfaceDefine(PVGASTATECC pThisCC, uint32_t sid, SVGA3dSurfaceFlags surfaceFlags, SVGA3dSurfaceFormat format,
                          uint32_t multisampleCount, SVGA3dTextureFilter autogenFilter,
                          uint32_t cMipLevels, SVGA3dSize const *pMipLevel0Size);
int vmsvga3dSurfaceDestroy(PVGASTATECC pThisCC, uint32_t sid);
int vmsvga3dSurfaceCopy(PVGASTATECC pThisCC, SVGA3dSurfaceImageId dest, SVGA3dSurfaceImageId src,
                        uint32_t cCopyBoxes, SVGA3dCopyBox *pBox);
int vmsvga3dSurfaceStretchBlt(PVGASTATE pThis, PVGASTATECC pThisCC,
                              SVGA3dSurfaceImageId const *pDstSfcImg, SVGA3dBox const *pDstBox,
                              SVGA3dSurfaceImageId const *pSrcSfcImg, SVGA3dBox const *pSrcBox, SVGA3dStretchBltMode enmMode);
int vmsvga3dSurfaceDMA(PVGASTATE pThis, PVGASTATECC pThisCC, SVGAGuestImage guest, SVGA3dSurfaceImageId host, SVGA3dTransferType transfer, uint32_t cCopyBoxes, SVGA3dCopyBox *pBoxes);
int vmsvga3dSurfaceBlitToScreen(PVGASTATE pThis, PVGASTATECC pThisCC, uint32_t dest, SVGASignedRect destRect, SVGA3dSurfaceImageId srcImage, SVGASignedRect srcRect, uint32_t cRects, SVGASignedRect *pRect);

int vmsvga3dContextDefine(PVGASTATECC pThisCC, uint32_t cid);
int vmsvga3dContextDestroy(PVGASTATECC pThisCC, uint32_t cid);

int vmsvga3dChangeMode(PVGASTATECC pThisCC);

int vmsvga3dDefineScreen(PVGASTATE pThis, PVGASTATECC pThisCC, VMSVGASCREENOBJECT *pScreen);
int vmsvga3dDestroyScreen(PVGASTATECC pThisCC, VMSVGASCREENOBJECT *pScreen);

int vmsvga3dSetTransform(PVGASTATECC pThisCC, uint32_t cid, SVGA3dTransformType type, float matrix[16]);
int vmsvga3dSetZRange(PVGASTATECC pThisCC, uint32_t cid, SVGA3dZRange zRange);
int vmsvga3dSetRenderState(PVGASTATECC pThisCC, uint32_t cid, uint32_t cRenderStates, SVGA3dRenderState *pRenderState);
int vmsvga3dSetRenderTarget(PVGASTATECC pThisCC, uint32_t cid, SVGA3dRenderTargetType type, SVGA3dSurfaceImageId target);
int vmsvga3dSetTextureState(PVGASTATECC pThisCC, uint32_t cid, uint32_t cTextureStates, SVGA3dTextureState *pTextureState);
int vmsvga3dSetMaterial(PVGASTATECC pThisCC, uint32_t cid, SVGA3dFace face, SVGA3dMaterial *pMaterial);
int vmsvga3dSetLightData(PVGASTATECC pThisCC, uint32_t cid, uint32_t index, SVGA3dLightData *pData);
int vmsvga3dSetLightEnabled(PVGASTATECC pThisCC, uint32_t cid, uint32_t index, uint32_t enabled);
int vmsvga3dSetViewPort(PVGASTATECC pThisCC, uint32_t cid, SVGA3dRect *pRect);
int vmsvga3dSetClipPlane(PVGASTATECC pThisCC, uint32_t cid,  uint32_t index, float plane[4]);
int vmsvga3dCommandClear(PVGASTATECC pThisCC, uint32_t cid, SVGA3dClearFlag clearFlag, uint32_t color, float depth, uint32_t stencil, uint32_t cRects, SVGA3dRect *pRect);
int vmsvga3dCommandPresent(PVGASTATE pThis, PVGASTATECC pThisCC, uint32_t sid, uint32_t cRects, SVGA3dCopyRect *pRect);
int vmsvga3dDrawPrimitives(PVGASTATECC pThisCC, uint32_t cid, uint32_t numVertexDecls, SVGA3dVertexDecl *pVertexDecl, uint32_t numRanges, SVGA3dPrimitiveRange *pNumRange, uint32_t cVertexDivisor, SVGA3dVertexDivisor *pVertexDivisor);
int vmsvga3dSetScissorRect(PVGASTATECC pThisCC, uint32_t cid, SVGA3dRect *pRect);
int vmsvga3dGenerateMipmaps(PVGASTATECC pThisCC, uint32_t sid, SVGA3dTextureFilter filter);

int vmsvga3dShaderDefine(PVGASTATECC pThisCC, uint32_t cid, uint32_t shid, SVGA3dShaderType type, uint32_t cbData, uint32_t *pShaderData);
int vmsvga3dShaderDestroy(PVGASTATECC pThisCC, uint32_t cid, uint32_t shid, SVGA3dShaderType type);
int vmsvga3dShaderSet(PVGASTATECC pThisCC, struct VMSVGA3DCONTEXT *pContext, uint32_t cid, SVGA3dShaderType type, uint32_t shid);
int vmsvga3dShaderSetConst(PVGASTATECC pThisCC, uint32_t cid, uint32_t reg, SVGA3dShaderType type, SVGA3dShaderConstType ctype, uint32_t cRegisters, uint32_t *pValues);

int vmsvga3dQueryBegin(PVGASTATECC pThisCC, uint32_t cid, SVGA3dQueryType type);
int vmsvga3dQueryEnd(PVGASTATECC pThisCC, uint32_t cid, SVGA3dQueryType type, SVGAGuestPtr guestResult);
int vmsvga3dQueryWait(PVGASTATE pThis, PVGASTATECC pThisCC, uint32_t cid, SVGA3dQueryType type, SVGAGuestPtr guestResult);

int vmsvga3dScreenTargetBind(PVGASTATECC pThisCC, VMSVGASCREENOBJECT *pScreen, uint32_t sid);
int vmsvga3dScreenTargetUpdate(PVGASTATECC pThisCC, VMSVGASCREENOBJECT *pScreen, SVGA3dRect const *pRect);

int vmsvga3dSurfaceMap(PVGASTATECC pThisCC, SVGA3dSurfaceImageId const *pImage, SVGA3dBox const *pBox, VMSVGA3D_SURFACE_MAP enmMapType, VMSVGA3D_MAPPED_SURFACE *pMap);
int vmsvga3dSurfaceUnmap(PVGASTATECC pThisCC, SVGA3dSurfaceImageId const *pImage, VMSVGA3D_MAPPED_SURFACE *pMap, bool fWritten);

/* DevVGA-SVGA3d-shared.h: */
#if defined(RT_OS_WINDOWS) && defined(IN_RING3)
# include <iprt/win/windows.h>

# define WM_VMSVGA3D_WAKEUP                     (WM_APP+1)
# define WM_VMSVGA3D_CREATEWINDOW               (WM_APP+2)
# define WM_VMSVGA3D_DESTROYWINDOW              (WM_APP+3)
# define WM_VMSVGA3D_EXIT                       (WM_APP+5)
# if 0
#  define WM_VMSVGA3D_CREATE_DEVICE             (WM_APP+6)
typedef struct VMSVGA3DCREATEDEVICEPARAMS
{
    struct VMSVGA3DSTATE   *pState;
    struct VMSVGA3DCONTEXT *pContext;
    struct _D3DPRESENT_PARAMETERS_ *pPresParams;
    HRESULT                 hrc;
} VMSVGA3DCREATEDEVICEPARAMS;
# endif

DECLCALLBACK(int) vmsvga3dWindowThread(RTTHREAD ThreadSelf, void *pvUser);
int vmsvga3dSendThreadMessage(RTTHREAD pWindowThread, RTSEMEVENT WndRequestSem, UINT msg, WPARAM wParam, LPARAM lParam);
int vmsvga3dContextWindowCreate(HINSTANCE hInstance, RTTHREAD pWindowThread, RTSEMEVENT WndRequestSem, HWND *pHwnd);

#endif

void vmsvga3dUpdateHeapBuffersForSurfaces(PVGASTATECC pThisCC, uint32_t sid);
void vmsvga3dInfoContextWorker(PVGASTATECC pThisCC, PCDBGFINFOHLP pHlp, uint32_t cid, bool fVerbose);
void vmsvga3dInfoSurfaceWorker(PPDMDEVINS pDevIns, PVGASTATE pThis, PVGASTATECC pThisCC, PCDBGFINFOHLP pHlp, uint32_t sid,
                               bool fVerbose, uint32_t cxAscii, bool fInvY, const char *pszBitmapPath);

/* DevVGA-SVGA3d-shared.cpp: */

/**
 * Structure for use with vmsvga3dInfoU32Flags.
 */
typedef struct VMSVGAINFOFLAGS32
{
    /** The flags. */
    uint32_t    fFlags;
    /** The corresponding mnemonic. */
    const char *pszJohnny;
} VMSVGAINFOFLAGS32;
/** Pointer to a read-only flag translation entry. */
typedef VMSVGAINFOFLAGS32 const *PCVMSVGAINFOFLAGS32;
void vmsvga3dInfoU32Flags(PCDBGFINFOHLP pHlp, uint32_t fFlags, const char *pszPrefix, PCVMSVGAINFOFLAGS32 paFlags, uint32_t cFlags);

/**
 * Structure for use with vmsvgaFormatEnumValueEx and vmsvgaFormatEnumValue.
 */
typedef struct VMSVGAINFOENUM
{
    /** The enum value. */
    int32_t     iValue;
    /** The corresponding value name. */
    const char *pszName;
} VMSVGAINFOENUM;
/** Pointer to a read-only enum value translation entry. */
typedef VMSVGAINFOENUM const *PCVMSVGAINFOENUM;
/**
 * Structure for use with vmsvgaFormatEnumValueEx and vmsvgaFormatEnumValue.
 */
typedef struct VMSVGAINFOENUMMAP
{
    /** Pointer to the value mapping array. */
    PCVMSVGAINFOENUM    paValues;
    /** The number of value mappings. */
    size_t              cValues;
    /** The prefix. */
    const char         *pszPrefix;
#ifdef RT_STRICT
    /** Indicates whether we've checked that it's sorted or not. */
    bool               *pfAsserted;
#endif
} VMSVGAINFOENUMMAP;
typedef VMSVGAINFOENUMMAP const *PCVMSVGAINFOENUMMAP;
/** @def VMSVGAINFOENUMMAP_MAKE
 * Macro for defining a VMSVGAINFOENUMMAP, silently dealing with pfAsserted.
 *
 * @param   a_Scope     The scope. RT_NOTHING or static.
 * @param   a_VarName   The variable name for this map.
 * @param   a_aValues   The variable name of the value mapping array.
 * @param   a_pszPrefix The value name prefix.
 */
#ifdef VBOX_STRICT
# define VMSVGAINFOENUMMAP_MAKE(a_Scope, a_VarName, a_aValues, a_pszPrefix) \
    static bool RT_CONCAT(a_VarName,_AssertedSorted) = false; \
    a_Scope VMSVGAINFOENUMMAP const a_VarName = { \
        a_aValues, RT_ELEMENTS(a_aValues), a_pszPrefix, &RT_CONCAT(a_VarName,_AssertedSorted) \
    }
#else
# define VMSVGAINFOENUMMAP_MAKE(a_Scope, a_VarName, a_aValues, a_pszPrefix) \
    a_Scope VMSVGAINFOENUMMAP const a_VarName = { a_aValues, RT_ELEMENTS(a_aValues), a_pszPrefix }
#endif
extern VMSVGAINFOENUMMAP const g_SVGA3dSurfaceFormat2String;
const char *vmsvgaLookupEnum(int32_t iValue, PCVMSVGAINFOENUMMAP pEnumMap);
char *vmsvgaFormatEnumValueEx(char *pszBuffer, size_t cbBuffer, const char *pszName, int32_t iValue,
                              bool fPrefix, PCVMSVGAINFOENUMMAP pEnumMap);
char *vmsvgaFormatEnumValue(char *pszBuffer, size_t cbBuffer, const char *pszName, uint32_t uValue,
                            const char *pszPrefix, const char * const *papszValues, size_t cValues);

/**
 * ASCII "art" scanline printer callback.
 *
 * @param   pszLine         The line to output.
 * @param   pvUser          The user argument.
 */
typedef DECLCALLBACKTYPE(void, FNVMSVGAASCIIPRINTLN,(const char *pszLine, void *pvUser));
/** Pointer to an ASCII "art" print line callback. */
typedef FNVMSVGAASCIIPRINTLN *PFNVMSVGAASCIIPRINTLN;
void vmsvga3dAsciiPrint(PFNVMSVGAASCIIPRINTLN pfnPrintLine, void *pvUser, void const *pvImage, size_t cbImage,
                        uint32_t cx, uint32_t cy, uint32_t cbScanline, SVGA3dSurfaceFormat enmFormat, bool fInvY,
                        uint32_t cchMaxX, uint32_t cchMaxY);
DECLCALLBACK(void) vmsvga3dAsciiPrintlnInfo(const char *pszLine, void *pvUser);
DECLCALLBACK(void) vmsvga3dAsciiPrintlnLog(const char *pszLine, void *pvUser);

char *vmsvga3dFormatRenderState(char *pszBuffer, size_t cbBuffer, SVGA3dRenderState const *pRenderState);
char *vmsvga3dFormatTextureState(char *pszBuffer, size_t cbBuffer, SVGA3dTextureState const *pTextureState);
void vmsvga3dInfoHostWindow(PCDBGFINFOHLP pHlp, uint64_t idHostWindow);

uint32_t vmsvga3dSurfaceFormatSize(SVGA3dSurfaceFormat format,
                                   uint32_t *pu32BlockWidth,
                                   uint32_t *pu32BlockHeight);

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

#define VMSVGA3D_BACKEND_INTERFACE_NAME_DX "DX"
typedef struct
{
    DECLCALLBACKMEMBER(void, pfnDXDefineContext,             (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXDestroyContext,            (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXBindContext,               (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXReadbackContext,           (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXInvalidateContext,         (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXSetSingleConstantBuffer,   (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXSetShaderResources,        (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXSetShader,                 (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXSetSamplers,               (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXDraw,                      (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXDrawIndexed,               (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXDrawInstanced,             (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXDrawIndexedInstanced,      (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXDrawAuto,                  (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXSetInputLayout,            (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXSetVertexBuffers,          (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXSetIndexBuffer,            (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXSetTopology,               (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXSetRenderTargets,          (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXSetBlendState,             (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXSetDepthStencilState,      (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXSetRasterizerState,        (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXDefineQuery,               (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXDestroyQuery,              (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXBindQuery,                 (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXSetQueryOffset,            (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXBeginQuery,                (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXEndQuery,                  (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXReadbackQuery,             (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXSetPredication,            (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXSetSOTargets,              (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXSetViewports,              (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXSetScissorRects,           (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXClearRenderTargetView,     (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXClearDepthStencilView,     (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXPredCopyRegion,            (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXPredCopy,                  (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXStretchBlt,                (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXGenMips,                   (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXUpdateSubResource,         (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXReadbackSubResource,       (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXInvalidateSubResource,     (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXDefineShaderResourceView,  (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXDestroyShaderResourceView, (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXDefineRenderTargetView,    (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXDestroyRenderTargetView,   (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXDefineDepthStencilView,    (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXDestroyDepthStencilView,   (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXDefineElementLayout,       (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXDestroyElementLayout,      (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXDefineBlendState,          (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXDestroyBlendState,         (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXDefineDepthStencilState,   (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXDestroyDepthStencilState,  (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXDefineRasterizerState,     (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXDestroyRasterizerState,    (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXDefineSamplerState,        (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXDestroySamplerState,       (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXDefineShader,              (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXDestroyShader,             (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXBindShader,                (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXDefineStreamOutput,        (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXDestroyStreamOutput,       (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXSetStreamOutput,           (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXSetCOTable,                (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXReadbackCOTable,           (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXBufferCopy,                (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXTransferFromBuffer,        (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXSurfaceCopyAndReadback,    (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXMoveQuery,                 (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXBindAllQuery,              (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXReadbackAllQuery,          (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXPredTransferFromBuffer,    (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXMobFence64,                (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXBindAllShader,             (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXHint,                      (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXBufferUpdate,              (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXSetVSConstantBufferOffset, (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXSetPSConstantBufferOffset, (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXSetGSConstantBufferOffset, (PVMSVGA3DSTATE p3dState));
    DECLCALLBACKMEMBER(void, pfnDXCondBindAllShader,         (PVMSVGA3DSTATE p3dState));
} VMSVGA3DBACKENDFUNCSDX;

int vmsvga3dQueryInterface(PVGASTATECC pThisCC, char const *pszInterfaceName, void *pvInterfaceFuncs, size_t cbInterfaceFuncs);

#endif /* !VBOX_INCLUDED_SRC_Graphics_DevVGA_SVGA3d_h */

