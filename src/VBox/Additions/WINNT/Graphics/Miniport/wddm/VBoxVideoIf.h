/** @file
 * Contains base definitions of constants & structures used
 * to control & perform rendering,
 * such as DMA commands types, allocation types, escape codes, etc.
 * used by both miniport & display drivers.
 *
 * The latter uses these and only these defs to communicate with the former
 * by posting appropriate requests via D3D RT Krnl Svc accessing callbacks.
 */
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
#ifndef ___VBoxVideoIf_h___
#define ___VBoxVideoIf_h___

#include <VBox/VBoxVideo.h>

/* @todo: implement a check to ensure display & miniport versions match.
 * One would increase this whenever definitions in this file are changed */
#define VBOXVIDEOIF_VERSION 2

/* create allocation func */
typedef enum
{
    VBOXWDDM_ALLOC_TYPE_UNEFINED = 0,
    VBOXWDDM_ALLOC_TYPE_STD_SHAREDPRIMARYSURFACE,
    VBOXWDDM_ALLOC_TYPE_STD_SHADOWSURFACE,
    VBOXWDDM_ALLOC_TYPE_STD_STAGINGSURFACE,
    /* this one is win 7-specific and hence unused for now */
    VBOXWDDM_ALLOC_TYPE_STD_GDISURFACE
    /* custom allocation types requested from user-mode d3d module will go here */
    , VBOXWDDM_ALLOC_TYPE_UMD_RC_GENERIC
} VBOXWDDM_ALLOC_TYPE;

typedef struct VBOXWDDM_SURFACE_DESC
{
    UINT width;
    UINT height;
    D3DDDIFORMAT format;
    UINT bpp;
    UINT pitch;
    UINT depth;
    UINT slicePitch;
    UINT cbSize;
    D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId;
    D3DDDI_RATIONAL RefreshRate;
} VBOXWDDM_SURFACE_DESC, *PVBOXWDDM_SURFACE_DESC;

typedef struct VBOXWDDM_ALLOCINFO
{
    VBOXWDDM_ALLOC_TYPE enmType;
    VBOXWDDM_SURFACE_DESC SurfDesc;
} VBOXWDDM_ALLOCINFO, *PVBOXWDDM_ALLOCINFO;

//#define VBOXWDDM_ALLOCINFO_HEADSIZE() (sizeof (VBOXWDDM_ALLOCINFO))
//#define VBOXWDDM_ALLOCINFO_SIZE_FROMBODYSIZE(_s) (VBOXWDDM_ALLOCINFO_HEADSIZE() + (_s))
//#define VBOXWDDM_ALLOCINFO_SIZE(_tCmd) (VBOXWDDM_ALLOCINFO_SIZE_FROMBODYSIZE(sizeof(_tCmd)))
//#define VBOXWDDM_ALLOCINFO_BODY(_p, _t) ( (_t*)(((uint8_t*)(_p)) + VBOXWDDM_ALLOCINFO_HEADSIZE()) )
//#define VBOXWDDM_ALLOCINFO_HEAD(_pb) ((VBOXWDDM_ALLOCINFO*)((uint8_t *)(_pb) - VBOXWDDM_ALLOCATION_HEADSIZE()))

/* this resource is OpenResource'd rather than CreateResource'd */
#define VBOXWDDM_RESOURCE_F_OPENNED      0x00000001
/* identifies this is a resource created with CreateResource, the VBOXWDDMDISP_RESOURCE::fRcFlags is valid */
#define VBOXWDDM_RESOURCE_F_TYPE_GENERIC 0x00000002

typedef struct VBOXWDDM_RC_DESC
{
    D3DDDI_RESOURCEFLAGS fFlags;
    D3DDDIFORMAT enmFormat;
    D3DDDI_POOL enmPool;
    D3DDDIMULTISAMPLE_TYPE enmMultisampleType;
    UINT MultisampleQuality;
    UINT MipLevels;
    UINT Fvf;
    D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId;
    D3DDDI_RATIONAL RefreshRate;
    D3DDDI_ROTATION enmRotation;
} VBOXWDDM_RC_DESC, *PVBOXWDDM_RC_DESC;

typedef struct VBOXWDDM_RCINFO
{
    uint32_t fFlags;
    VBOXWDDM_RC_DESC RcDesc;
    uint32_t cAllocInfos;
//    VBOXWDDM_ALLOCINFO aAllocInfos[1];
} VBOXWDDM_RCINFO, *PVBOXWDDM_RCINFO;

#define VBOXVHWA_F_ENABLED  0x00000001
#define VBOXVHWA_F_CKEY_DST 0x00000002
#define VBOXVHWA_F_CKEY_SRC 0x00000004

#define VBOXVHWA_MAX_FORMATS 8

typedef struct VBOXVHWA_INFO
{
    uint32_t fFlags;
    uint32_t cOverlaysSupported;
    uint32_t cFormats;
    D3DDDIFORMAT aFormats[VBOXVHWA_MAX_FORMATS];
} VBOXVHWA_INFO;

/* query info func */
typedef struct VBOXWDDM_QI
{
    uint32_t u32Version;
    uint32_t cInfos;
    VBOXVHWA_INFO aInfos[VBOX_VIDEO_MAX_SCREENS];
} VBOXWDDM_QI;

/* submit cmd func */




/* tooling */
DECLINLINE(UINT) vboxWddmCalcBitsPerPixel(D3DDDIFORMAT format)
{
    switch (format)
    {
        case D3DDDIFMT_R8G8B8:
            return 24;
        case D3DDDIFMT_A8R8G8B8:
        case D3DDDIFMT_X8R8G8B8:
            return 32;
        case D3DDDIFMT_R5G6B5:
        case D3DDDIFMT_X1R5G5B5:
        case D3DDDIFMT_A1R5G5B5:
        case D3DDDIFMT_A4R4G4B4:
            return 16;
        case D3DDDIFMT_R3G3B2:
        case D3DDDIFMT_A8:
            return 8;
        case D3DDDIFMT_A8R3G3B2:
        case D3DDDIFMT_X4R4G4B4:
            return 16;
        case D3DDDIFMT_A2B10G10R10:
        case D3DDDIFMT_A8B8G8R8:
        case D3DDDIFMT_X8B8G8R8:
        case D3DDDIFMT_G16R16:
        case D3DDDIFMT_A2R10G10B10:
            return 32;
        case D3DDDIFMT_A16B16G16R16:
            return 64;
        case D3DDDIFMT_A8P8:
            return 16;
        case D3DDDIFMT_P8:
            return 8;
        default:
            AssertBreakpoint();
            return 0;
    }
}

DECLINLINE(uint32_t) vboxWddmFormatToFourcc(D3DDDIFORMAT format)
{
    uint32_t uFormat = (uint32_t)format;
    /* assume that in case both four bytes are non-zero, this is a fourcc */
    if ((format & 0xff000000)
            && (format & 0x00ff0000)
            && (format & 0x0000ff00)
            && (format & 0x000000ff)
            )
        return uFormat;
    return 0;
}

#define VBOXWDDM_ROUNDBOUND(_v, _b) (((_v) + ((_b) - 1)) & ~((_b) - 1))

DECLINLINE(UINT) vboxWddmCalcPitch(UINT w, UINT bitsPerPixel)
{
    UINT Pitch = bitsPerPixel * w;
    /* pitch is now in bits, translate in bytes */
    return VBOXWDDM_ROUNDBOUND(Pitch, 8) >> 3;
}

#endif /* #ifndef ___VBoxVideoIf_h___ */
