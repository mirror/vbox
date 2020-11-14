/* $Id$ */
/** @file
 * DevVMWare - VMWare SVGA device
 */

/*
 * Copyright (C) 2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_DEV_VMSVGA
#include <VBox/AssertGuest.h>
#include <VBox/log.h>
#include <VBox/vmm/pdmdev.h>
#include <VBox/vmm/pgm.h>

#include <iprt/assert.h>
#include <iprt/errcore.h>
#include <iprt/mem.h>

#include <VBoxVideo.h> /* required by DevVGA.h */
#include <VBoxVideo3D.h>

/* should go BEFORE any other DevVGA include to make all DevVGA.h config defines be visible */
#include "DevVGA.h"

#include "DevVGA-SVGA.h"
#include "DevVGA-SVGA3d.h"
#include "DevVGA-SVGA3d-internal.h"

#include <d3d11.h>


/* What kind of resource has been created for the VMSVGA3D surface. */
typedef enum VMSVGA3DBACKRESTYPE
{
    VMSVGA3D_RESTYPE_NONE           = 0,
    VMSVGA3D_RESTYPE_SCREEN_TARGET  = 1,
    VMSVGA3D_RESTYPE_SURFACE        = 2,
    VMSVGA3D_RESTYPE_TEXTURE        = 3,
    VMSVGA3D_RESTYPE_CUBE_TEXTURE   = 4,
    VMSVGA3D_RESTYPE_VOLUME_TEXTURE = 5,
    VMSVGA3D_RESTYPE_VERTEX_BUFFER  = 6,
    VMSVGA3D_RESTYPE_INDEX_BUFFER   = 7,
} VMSVGA3DBACKRESTYPE;

struct VMSVGA3DBACKENDSURFACE
{
    VMSVGA3DBACKRESTYPE enmResType;
    union
    {
        struct
        {
            ID3D11Texture2D    *pTexture;         /* The texture for the screen content. */
            ID3D11Texture2D    *pDynamicTexture;  /* For screen updates from memory. */ /** @todo One for all screens. */
            ID3D11Texture2D    *pStagingTexture;  /* For Reading the screen content. */ /** @todo One for all screens. */
        } ScreenTarget;
        struct
        {
        } Texture;
    } u;
} VMSVGA3DBACKENDSURFACE;

typedef struct VMSVGAHWSCREEN
{
    ID3D11Texture2D            *pTexture;         /* Shared texture for the screen content. Only used as CopyResource target. */
    IDXGIResource              *pDxgiResource;    /* Interface of the texture. */
    IDXGIKeyedMutex            *pDXGIKeyedMutex;  /* Synchronization interface for the render device. */
    HANDLE                      SharedHandle;     /* The shared handle of this structure. */
    uint32_t                    sidScreenTarget;  /* The source surface for this screen. */
} VMSVGAHWSCREEN;

struct VMSVGA3DBACKEND
{
    RTLDRMOD                    hD3D11;
    PFN_D3D11_CREATE_DEVICE     pfnD3D11CreateDevice;

    ID3D11Device               *pDevice;               /* Device for the VMSVGA3D context independent operation. */
    ID3D11DeviceContext        *pImmediateContext;     /* Corresponding context. */
    IDXGIFactory               *pDxgiFactory;          /* DXGI Factory. */
    D3D_FEATURE_LEVEL           FeatureLevel;
} VMSVGA3DBACKEND;


static DXGI_FORMAT vmsvgaDXScreenTargetFormat2Dxgi(SVGA3dSurfaceFormat format)
{
    switch (format)
    {
        /** @todo More formats required? */
        case SVGA3D_X8R8G8B8:                   return DXGI_FORMAT_B8G8R8A8_UNORM;
        case SVGA3D_A8R8G8B8:                   return DXGI_FORMAT_B8G8R8A8_UNORM;
        default:
            AssertFailed();
            break;
    }
    return DXGI_FORMAT_UNKNOWN;
}


static DXGI_FORMAT vmsvgaDXSurfaceFormat2Dxgi(SVGA3dSurfaceFormat format)
{
#define DXGI_FORMAT_ DXGI_FORMAT_UNKNOWN
    /** @todo More formats. */
    switch (format)
    {
        case SVGA3D_X8R8G8B8:                   return DXGI_FORMAT_B8G8R8X8_UNORM;
        case SVGA3D_A8R8G8B8:                   return DXGI_FORMAT_B8G8R8A8_UNORM;
        case SVGA3D_R5G6B5:                     return DXGI_FORMAT_B5G6R5_UNORM;
        case SVGA3D_X1R5G5B5:                   return DXGI_FORMAT_B5G5R5A1_UNORM;
        case SVGA3D_A1R5G5B5:                   return DXGI_FORMAT_B5G5R5A1_UNORM;
        case SVGA3D_A4R4G4B4:                   break; // 11.1 return DXGI_FORMAT_B4G4R4A4_UNORM;
        case SVGA3D_Z_D32:                      break;
        case SVGA3D_Z_D16:                      return DXGI_FORMAT_D16_UNORM;
        case SVGA3D_Z_D24S8:                    return DXGI_FORMAT_D24_UNORM_S8_UINT;
        case SVGA3D_Z_D15S1:                    break;
        case SVGA3D_LUMINANCE8:                 return DXGI_FORMAT_;
        case SVGA3D_LUMINANCE4_ALPHA4:          return DXGI_FORMAT_;
        case SVGA3D_LUMINANCE16:                return DXGI_FORMAT_;
        case SVGA3D_LUMINANCE8_ALPHA8:          return DXGI_FORMAT_;
        case SVGA3D_DXT1:                       return DXGI_FORMAT_;
        case SVGA3D_DXT2:                       return DXGI_FORMAT_;
        case SVGA3D_DXT3:                       return DXGI_FORMAT_;
        case SVGA3D_DXT4:                       return DXGI_FORMAT_;
        case SVGA3D_DXT5:                       return DXGI_FORMAT_;
        case SVGA3D_BUMPU8V8:                   return DXGI_FORMAT_;
        case SVGA3D_BUMPL6V5U5:                 return DXGI_FORMAT_;
        case SVGA3D_BUMPX8L8V8U8:               return DXGI_FORMAT_;
        case SVGA3D_FORMAT_DEAD1:               break;
        case SVGA3D_ARGB_S10E5:                 return DXGI_FORMAT_;
        case SVGA3D_ARGB_S23E8:                 return DXGI_FORMAT_;
        case SVGA3D_A2R10G10B10:                return DXGI_FORMAT_;
        case SVGA3D_V8U8:                       return DXGI_FORMAT_;
        case SVGA3D_Q8W8V8U8:                   return DXGI_FORMAT_;
        case SVGA3D_CxV8U8:                     return DXGI_FORMAT_;
        case SVGA3D_X8L8V8U8:                   return DXGI_FORMAT_;
        case SVGA3D_A2W10V10U10:                return DXGI_FORMAT_;
        case SVGA3D_ALPHA8:                     return DXGI_FORMAT_;
        case SVGA3D_R_S10E5:                    return DXGI_FORMAT_;
        case SVGA3D_R_S23E8:                    return DXGI_FORMAT_;
        case SVGA3D_RG_S10E5:                   return DXGI_FORMAT_;
        case SVGA3D_RG_S23E8:                   return DXGI_FORMAT_;
        case SVGA3D_BUFFER:                     return DXGI_FORMAT_;
        case SVGA3D_Z_D24X8:                    return DXGI_FORMAT_;
        case SVGA3D_V16U16:                     return DXGI_FORMAT_;
        case SVGA3D_G16R16:                     return DXGI_FORMAT_;
        case SVGA3D_A16B16G16R16:               return DXGI_FORMAT_;
        case SVGA3D_UYVY:                       return DXGI_FORMAT_;
        case SVGA3D_YUY2:                       return DXGI_FORMAT_;
        case SVGA3D_NV12:                       return DXGI_FORMAT_;
        case SVGA3D_AYUV:                       return DXGI_FORMAT_;
        case SVGA3D_R32G32B32A32_TYPELESS:      return DXGI_FORMAT_R32G32B32A32_TYPELESS;
        case SVGA3D_R32G32B32A32_UINT:          return DXGI_FORMAT_R32G32B32A32_UINT;
        case SVGA3D_R32G32B32A32_SINT:          return DXGI_FORMAT_R32G32B32A32_SINT;
        case SVGA3D_R32G32B32_TYPELESS:         return DXGI_FORMAT_R32G32B32_TYPELESS;
        case SVGA3D_R32G32B32_FLOAT:            return DXGI_FORMAT_R32G32B32_FLOAT;
        case SVGA3D_R32G32B32_UINT:             return DXGI_FORMAT_R32G32B32_UINT;
        case SVGA3D_R32G32B32_SINT:             return DXGI_FORMAT_R32G32B32_SINT;
        case SVGA3D_R16G16B16A16_TYPELESS:      return DXGI_FORMAT_R16G16B16A16_TYPELESS;
        case SVGA3D_R16G16B16A16_UINT:          return DXGI_FORMAT_R16G16B16A16_UINT;
        case SVGA3D_R16G16B16A16_SNORM:         return DXGI_FORMAT_R16G16B16A16_SNORM;
        case SVGA3D_R16G16B16A16_SINT:          return DXGI_FORMAT_R16G16B16A16_SINT;
        case SVGA3D_R32G32_TYPELESS:            return DXGI_FORMAT_R32G32_TYPELESS;
        case SVGA3D_R32G32_UINT:                return DXGI_FORMAT_R32G32_UINT;
        case SVGA3D_R32G32_SINT:                return DXGI_FORMAT_R32G32_SINT;
        case SVGA3D_R32G8X24_TYPELESS:          return DXGI_FORMAT_R32G8X24_TYPELESS;
        case SVGA3D_D32_FLOAT_S8X24_UINT:       return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
        case SVGA3D_R32_FLOAT_X8X24:            return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
        case SVGA3D_X32_G8X24_UINT:             return DXGI_FORMAT_X32_TYPELESS_G8X24_UINT;
        case SVGA3D_R10G10B10A2_TYPELESS:       return DXGI_FORMAT_R10G10B10A2_TYPELESS;
        case SVGA3D_R10G10B10A2_UINT:           return DXGI_FORMAT_R10G10B10A2_UINT;
        case SVGA3D_R11G11B10_FLOAT:            return DXGI_FORMAT_R11G11B10_FLOAT;
        case SVGA3D_R8G8B8A8_TYPELESS:          return DXGI_FORMAT_R8G8B8A8_TYPELESS;
        case SVGA3D_R8G8B8A8_UNORM:             return DXGI_FORMAT_R8G8B8A8_UNORM;
        case SVGA3D_R8G8B8A8_UNORM_SRGB:        return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        case SVGA3D_R8G8B8A8_UINT:              return DXGI_FORMAT_R8G8B8A8_UINT;
        case SVGA3D_R8G8B8A8_SINT:              return DXGI_FORMAT_R8G8B8A8_SINT;
        case SVGA3D_R16G16_TYPELESS:            return DXGI_FORMAT_R16G16_TYPELESS;
        case SVGA3D_R16G16_UINT:                return DXGI_FORMAT_R16G16_UINT;
        case SVGA3D_R16G16_SINT:                return DXGI_FORMAT_R16G16_SINT;
        case SVGA3D_R32_TYPELESS:               return DXGI_FORMAT_R32_TYPELESS;
        case SVGA3D_D32_FLOAT:                  return DXGI_FORMAT_D32_FLOAT;
        case SVGA3D_R32_UINT:                   return DXGI_FORMAT_R32_UINT;
        case SVGA3D_R32_SINT:                   return DXGI_FORMAT_R32_SINT;
        case SVGA3D_R24G8_TYPELESS:             return DXGI_FORMAT_R24G8_TYPELESS;
        case SVGA3D_D24_UNORM_S8_UINT:          return DXGI_FORMAT_D24_UNORM_S8_UINT;
        case SVGA3D_R24_UNORM_X8:               return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        case SVGA3D_X24_G8_UINT:                return DXGI_FORMAT_X24_TYPELESS_G8_UINT;
        case SVGA3D_R8G8_TYPELESS:              return DXGI_FORMAT_R8G8_TYPELESS;
        case SVGA3D_R8G8_UNORM:                 return DXGI_FORMAT_R8G8_UNORM;
        case SVGA3D_R8G8_UINT:                  return DXGI_FORMAT_R8G8_UINT;
        case SVGA3D_R8G8_SINT:                  return DXGI_FORMAT_R8G8_SINT;
        case SVGA3D_R16_TYPELESS:               return DXGI_FORMAT_R16_TYPELESS;
        case SVGA3D_R16_UNORM:                  return DXGI_FORMAT_R16_UNORM;
        case SVGA3D_R16_UINT:                   return DXGI_FORMAT_R16_UINT;
        case SVGA3D_R16_SNORM:                  return DXGI_FORMAT_R16_SNORM;
        case SVGA3D_R16_SINT:                   return DXGI_FORMAT_R16_SINT;
        case SVGA3D_R8_TYPELESS:                return DXGI_FORMAT_R8_TYPELESS;
        case SVGA3D_R8_UNORM:                   return DXGI_FORMAT_R8_UNORM;
        case SVGA3D_R8_UINT:                    return DXGI_FORMAT_R8_UINT;
        case SVGA3D_R8_SNORM:                   return DXGI_FORMAT_R8_SNORM;
        case SVGA3D_R8_SINT:                    return DXGI_FORMAT_R8_SINT;
        case SVGA3D_P8:                         break;
        case SVGA3D_R9G9B9E5_SHAREDEXP:         return DXGI_FORMAT_R9G9B9E5_SHAREDEXP;
        case SVGA3D_R8G8_B8G8_UNORM:            return DXGI_FORMAT_R8G8_B8G8_UNORM;
        case SVGA3D_G8R8_G8B8_UNORM:            return DXGI_FORMAT_G8R8_G8B8_UNORM;
        case SVGA3D_BC1_TYPELESS:               return DXGI_FORMAT_BC1_TYPELESS;
        case SVGA3D_BC1_UNORM_SRGB:             return DXGI_FORMAT_BC1_UNORM_SRGB;
        case SVGA3D_BC2_TYPELESS:               return DXGI_FORMAT_BC2_TYPELESS;
        case SVGA3D_BC2_UNORM_SRGB:             return DXGI_FORMAT_BC2_UNORM_SRGB;
        case SVGA3D_BC3_TYPELESS:               return DXGI_FORMAT_BC3_TYPELESS;
        case SVGA3D_BC3_UNORM_SRGB:             return DXGI_FORMAT_BC3_UNORM_SRGB;
        case SVGA3D_BC4_TYPELESS:               return DXGI_FORMAT_BC4_TYPELESS;
        case SVGA3D_ATI1:                       break;
        case SVGA3D_BC4_SNORM:                  return DXGI_FORMAT_BC4_SNORM;
        case SVGA3D_BC5_TYPELESS:               return DXGI_FORMAT_BC5_TYPELESS;
        case SVGA3D_ATI2:                       break;
        case SVGA3D_BC5_SNORM:                  return DXGI_FORMAT_BC5_SNORM;
        case SVGA3D_R10G10B10_XR_BIAS_A2_UNORM: return DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM;
        case SVGA3D_B8G8R8A8_TYPELESS:          return DXGI_FORMAT_B8G8R8A8_TYPELESS;
        case SVGA3D_B8G8R8A8_UNORM_SRGB:        return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
        case SVGA3D_B8G8R8X8_TYPELESS:          return DXGI_FORMAT_B8G8R8X8_TYPELESS;
        case SVGA3D_B8G8R8X8_UNORM_SRGB:        return DXGI_FORMAT_B8G8R8X8_UNORM_SRGB;
        case SVGA3D_Z_DF16:                     break;
        case SVGA3D_Z_DF24:                     break;
        case SVGA3D_Z_D24S8_INT:                return DXGI_FORMAT_D24_UNORM_S8_UINT;
        case SVGA3D_YV12:                       break;
        case SVGA3D_R32G32B32A32_FLOAT:         return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case SVGA3D_R16G16B16A16_FLOAT:         return DXGI_FORMAT_R16G16B16A16_FLOAT;
        case SVGA3D_R16G16B16A16_UNORM:         return DXGI_FORMAT_R16G16B16A16_UNORM;
        case SVGA3D_R32G32_FLOAT:               return DXGI_FORMAT_R32G32_FLOAT;
        case SVGA3D_R10G10B10A2_UNORM:          return DXGI_FORMAT_R10G10B10A2_UNORM;
        case SVGA3D_R8G8B8A8_SNORM:             return DXGI_FORMAT_R8G8B8A8_SNORM;
        case SVGA3D_R16G16_FLOAT:               return DXGI_FORMAT_R16G16_FLOAT;
        case SVGA3D_R16G16_UNORM:               return DXGI_FORMAT_R16G16_UNORM;
        case SVGA3D_R16G16_SNORM:               return DXGI_FORMAT_R16G16_SNORM;
        case SVGA3D_R32_FLOAT:                  return DXGI_FORMAT_R32_FLOAT;
        case SVGA3D_R8G8_SNORM:                 return DXGI_FORMAT_R8G8_SNORM;
        case SVGA3D_R16_FLOAT:                  return DXGI_FORMAT_R16_FLOAT;
        case SVGA3D_D16_UNORM:                  return DXGI_FORMAT_D16_UNORM;
        case SVGA3D_A8_UNORM:                   return DXGI_FORMAT_A8_UNORM;
        case SVGA3D_BC1_UNORM:                  return DXGI_FORMAT_BC1_UNORM;
        case SVGA3D_BC2_UNORM:                  return DXGI_FORMAT_BC2_UNORM;
        case SVGA3D_BC3_UNORM:                  return DXGI_FORMAT_BC3_UNORM;
        case SVGA3D_B5G6R5_UNORM:               return DXGI_FORMAT_B5G6R5_UNORM;
        case SVGA3D_B5G5R5A1_UNORM:             return DXGI_FORMAT_B5G5R5A1_UNORM;
        case SVGA3D_B8G8R8A8_UNORM:             return DXGI_FORMAT_B8G8R8A8_UNORM;
        case SVGA3D_B8G8R8X8_UNORM:             return DXGI_FORMAT_B8G8R8X8_UNORM;
        case SVGA3D_BC4_UNORM:                  return DXGI_FORMAT_BC4_UNORM;
        case SVGA3D_BC5_UNORM:                  return DXGI_FORMAT_BC5_UNORM;

        case SVGA3D_FORMAT_INVALID:
        case SVGA3D_FORMAT_MAX:                 break;
    }
    // AssertFailed();
    return DXGI_FORMAT_UNKNOWN;
#undef DXGI_FORMAT_
}


static SVGA3dSurfaceFormat vmsvgaDXDevCapSurfaceFmt2Format(SVGA3dDevCapIndex enmDevCap)
{
    switch (enmDevCap)
    {
        case SVGA3D_DEVCAP_SURFACEFMT_X8R8G8B8:                 return SVGA3D_X8R8G8B8;
        case SVGA3D_DEVCAP_SURFACEFMT_A8R8G8B8:                 return SVGA3D_A8R8G8B8;
        case SVGA3D_DEVCAP_SURFACEFMT_A2R10G10B10:              return SVGA3D_A2R10G10B10;
        case SVGA3D_DEVCAP_SURFACEFMT_X1R5G5B5:                 return SVGA3D_X1R5G5B5;
        case SVGA3D_DEVCAP_SURFACEFMT_A1R5G5B5:                 return SVGA3D_A1R5G5B5;
        case SVGA3D_DEVCAP_SURFACEFMT_A4R4G4B4:                 return SVGA3D_A4R4G4B4;
        case SVGA3D_DEVCAP_SURFACEFMT_R5G6B5:                   return SVGA3D_R5G6B5;
        case SVGA3D_DEVCAP_SURFACEFMT_LUMINANCE16:              return SVGA3D_LUMINANCE16;
        case SVGA3D_DEVCAP_SURFACEFMT_LUMINANCE8_ALPHA8:        return SVGA3D_LUMINANCE8_ALPHA8;
        case SVGA3D_DEVCAP_SURFACEFMT_ALPHA8:                   return SVGA3D_ALPHA8;
        case SVGA3D_DEVCAP_SURFACEFMT_LUMINANCE8:               return SVGA3D_LUMINANCE8;
        case SVGA3D_DEVCAP_SURFACEFMT_Z_D16:                    return SVGA3D_Z_D16;
        case SVGA3D_DEVCAP_SURFACEFMT_Z_D24S8:                  return SVGA3D_Z_D24S8;
        case SVGA3D_DEVCAP_SURFACEFMT_Z_D24X8:                  return SVGA3D_Z_D24X8;
        case SVGA3D_DEVCAP_SURFACEFMT_DXT1:                     return SVGA3D_DXT1;
        case SVGA3D_DEVCAP_SURFACEFMT_DXT2:                     return SVGA3D_DXT2;
        case SVGA3D_DEVCAP_SURFACEFMT_DXT3:                     return SVGA3D_DXT3;
        case SVGA3D_DEVCAP_SURFACEFMT_DXT4:                     return SVGA3D_DXT4;
        case SVGA3D_DEVCAP_SURFACEFMT_DXT5:                     return SVGA3D_DXT5;
        case SVGA3D_DEVCAP_SURFACEFMT_BUMPX8L8V8U8:             return SVGA3D_BUMPX8L8V8U8;
        case SVGA3D_DEVCAP_SURFACEFMT_A2W10V10U10:              return SVGA3D_A2W10V10U10;
        case SVGA3D_DEVCAP_SURFACEFMT_BUMPU8V8:                 return SVGA3D_BUMPU8V8;
        case SVGA3D_DEVCAP_SURFACEFMT_Q8W8V8U8:                 return SVGA3D_Q8W8V8U8;
        case SVGA3D_DEVCAP_SURFACEFMT_CxV8U8:                   return SVGA3D_CxV8U8;
        case SVGA3D_DEVCAP_SURFACEFMT_R_S10E5:                  return SVGA3D_R_S10E5;
        case SVGA3D_DEVCAP_SURFACEFMT_R_S23E8:                  return SVGA3D_R_S23E8;
        case SVGA3D_DEVCAP_SURFACEFMT_RG_S10E5:                 return SVGA3D_RG_S10E5;
        case SVGA3D_DEVCAP_SURFACEFMT_RG_S23E8:                 return SVGA3D_RG_S23E8;
        case SVGA3D_DEVCAP_SURFACEFMT_ARGB_S10E5:               return SVGA3D_ARGB_S10E5;
        case SVGA3D_DEVCAP_SURFACEFMT_ARGB_S23E8:               return SVGA3D_ARGB_S23E8;
        case SVGA3D_DEVCAP_SURFACEFMT_V16U16:                   return SVGA3D_V16U16;
        case SVGA3D_DEVCAP_SURFACEFMT_G16R16:                   return SVGA3D_G16R16;
        case SVGA3D_DEVCAP_SURFACEFMT_A16B16G16R16:             return SVGA3D_A16B16G16R16;
        case SVGA3D_DEVCAP_SURFACEFMT_UYVY:                     return SVGA3D_UYVY;
        case SVGA3D_DEVCAP_SURFACEFMT_YUY2:                     return SVGA3D_YUY2;
        case SVGA3D_DEVCAP_SURFACEFMT_NV12:                     return SVGA3D_NV12;
        case SVGA3D_DEVCAP_SURFACEFMT_AYUV:                     return SVGA3D_AYUV;
        case SVGA3D_DEVCAP_SURFACEFMT_Z_DF16:                   return SVGA3D_Z_DF16;
        case SVGA3D_DEVCAP_SURFACEFMT_Z_DF24:                   return SVGA3D_Z_DF24;
        case SVGA3D_DEVCAP_SURFACEFMT_Z_D24S8_INT:              return SVGA3D_Z_D24S8_INT;
        case SVGA3D_DEVCAP_SURFACEFMT_ATI1:                     return SVGA3D_ATI1;
        case SVGA3D_DEVCAP_SURFACEFMT_ATI2:                     return SVGA3D_ATI2;
        case SVGA3D_DEVCAP_SURFACEFMT_YV12:                     return SVGA3D_YV12;
        default:
            AssertFailed();
            break;
    }
    return SVGA3D_FORMAT_INVALID;
}


static SVGA3dSurfaceFormat vmsvgaDXDevCapDxfmt2Format(SVGA3dDevCapIndex enmDevCap)
{
    switch (enmDevCap)
    {
        case SVGA3D_DEVCAP_DXFMT_X8R8G8B8:                      return SVGA3D_X8R8G8B8;
        case SVGA3D_DEVCAP_DXFMT_A8R8G8B8:                      return SVGA3D_A8R8G8B8;
        case SVGA3D_DEVCAP_DXFMT_R5G6B5:                        return SVGA3D_R5G6B5;
        case SVGA3D_DEVCAP_DXFMT_X1R5G5B5:                      return SVGA3D_X1R5G5B5;
        case SVGA3D_DEVCAP_DXFMT_A1R5G5B5:                      return SVGA3D_A1R5G5B5;
        case SVGA3D_DEVCAP_DXFMT_A4R4G4B4:                      return SVGA3D_A4R4G4B4;
        case SVGA3D_DEVCAP_DXFMT_Z_D32:                         return SVGA3D_Z_D32;
        case SVGA3D_DEVCAP_DXFMT_Z_D16:                         return SVGA3D_Z_D16;
        case SVGA3D_DEVCAP_DXFMT_Z_D24S8:                       return SVGA3D_Z_D24S8;
        case SVGA3D_DEVCAP_DXFMT_Z_D15S1:                       return SVGA3D_Z_D15S1;
        case SVGA3D_DEVCAP_DXFMT_LUMINANCE8:                    return SVGA3D_LUMINANCE8;
        case SVGA3D_DEVCAP_DXFMT_LUMINANCE4_ALPHA4:             return SVGA3D_LUMINANCE4_ALPHA4;
        case SVGA3D_DEVCAP_DXFMT_LUMINANCE16:                   return SVGA3D_LUMINANCE16;
        case SVGA3D_DEVCAP_DXFMT_LUMINANCE8_ALPHA8:             return SVGA3D_LUMINANCE8_ALPHA8;
        case SVGA3D_DEVCAP_DXFMT_DXT1:                          return SVGA3D_DXT1;
        case SVGA3D_DEVCAP_DXFMT_DXT2:                          return SVGA3D_DXT2;
        case SVGA3D_DEVCAP_DXFMT_DXT3:                          return SVGA3D_DXT3;
        case SVGA3D_DEVCAP_DXFMT_DXT4:                          return SVGA3D_DXT4;
        case SVGA3D_DEVCAP_DXFMT_DXT5:                          return SVGA3D_DXT5;
        case SVGA3D_DEVCAP_DXFMT_BUMPU8V8:                      return SVGA3D_BUMPU8V8;
        case SVGA3D_DEVCAP_DXFMT_BUMPL6V5U5:                    return SVGA3D_BUMPL6V5U5;
        case SVGA3D_DEVCAP_DXFMT_BUMPX8L8V8U8:                  return SVGA3D_BUMPX8L8V8U8;
        case SVGA3D_DEVCAP_DXFMT_FORMAT_DEAD1:                  return SVGA3D_FORMAT_DEAD1;
        case SVGA3D_DEVCAP_DXFMT_ARGB_S10E5:                    return SVGA3D_ARGB_S10E5;
        case SVGA3D_DEVCAP_DXFMT_ARGB_S23E8:                    return SVGA3D_ARGB_S23E8;
        case SVGA3D_DEVCAP_DXFMT_A2R10G10B10:                   return SVGA3D_A2R10G10B10;
        case SVGA3D_DEVCAP_DXFMT_V8U8:                          return SVGA3D_V8U8;
        case SVGA3D_DEVCAP_DXFMT_Q8W8V8U8:                      return SVGA3D_Q8W8V8U8;
        case SVGA3D_DEVCAP_DXFMT_CxV8U8:                        return SVGA3D_CxV8U8;
        case SVGA3D_DEVCAP_DXFMT_X8L8V8U8:                      return SVGA3D_X8L8V8U8;
        case SVGA3D_DEVCAP_DXFMT_A2W10V10U10:                   return SVGA3D_A2W10V10U10;
        case SVGA3D_DEVCAP_DXFMT_ALPHA8:                        return SVGA3D_ALPHA8;
        case SVGA3D_DEVCAP_DXFMT_R_S10E5:                       return SVGA3D_R_S10E5;
        case SVGA3D_DEVCAP_DXFMT_R_S23E8:                       return SVGA3D_R_S23E8;
        case SVGA3D_DEVCAP_DXFMT_RG_S10E5:                      return SVGA3D_RG_S10E5;
        case SVGA3D_DEVCAP_DXFMT_RG_S23E8:                      return SVGA3D_RG_S23E8;
        case SVGA3D_DEVCAP_DXFMT_BUFFER:                        return SVGA3D_BUFFER;
        case SVGA3D_DEVCAP_DXFMT_Z_D24X8:                       return SVGA3D_Z_D24X8;
        case SVGA3D_DEVCAP_DXFMT_V16U16:                        return SVGA3D_V16U16;
        case SVGA3D_DEVCAP_DXFMT_G16R16:                        return SVGA3D_G16R16;
        case SVGA3D_DEVCAP_DXFMT_A16B16G16R16:                  return SVGA3D_A16B16G16R16;
        case SVGA3D_DEVCAP_DXFMT_UYVY:                          return SVGA3D_UYVY;
        case SVGA3D_DEVCAP_DXFMT_YUY2:                          return SVGA3D_YUY2;
        case SVGA3D_DEVCAP_DXFMT_NV12:                          return SVGA3D_NV12;
        case SVGA3D_DEVCAP_DXFMT_AYUV:                          return SVGA3D_AYUV;
        case SVGA3D_DEVCAP_DXFMT_R32G32B32A32_TYPELESS:         return SVGA3D_R32G32B32A32_TYPELESS;
        case SVGA3D_DEVCAP_DXFMT_R32G32B32A32_UINT:             return SVGA3D_R32G32B32A32_UINT;
        case SVGA3D_DEVCAP_DXFMT_R32G32B32A32_SINT:             return SVGA3D_R32G32B32A32_SINT;
        case SVGA3D_DEVCAP_DXFMT_R32G32B32_TYPELESS:            return SVGA3D_R32G32B32_TYPELESS;
        case SVGA3D_DEVCAP_DXFMT_R32G32B32_FLOAT:               return SVGA3D_R32G32B32_FLOAT;
        case SVGA3D_DEVCAP_DXFMT_R32G32B32_UINT:                return SVGA3D_R32G32B32_UINT;
        case SVGA3D_DEVCAP_DXFMT_R32G32B32_SINT:                return SVGA3D_R32G32B32_SINT;
        case SVGA3D_DEVCAP_DXFMT_R16G16B16A16_TYPELESS:         return SVGA3D_R16G16B16A16_TYPELESS;
        case SVGA3D_DEVCAP_DXFMT_R16G16B16A16_UINT:             return SVGA3D_R16G16B16A16_UINT;
        case SVGA3D_DEVCAP_DXFMT_R16G16B16A16_SNORM:            return SVGA3D_R16G16B16A16_SNORM;
        case SVGA3D_DEVCAP_DXFMT_R16G16B16A16_SINT:             return SVGA3D_R16G16B16A16_SINT;
        case SVGA3D_DEVCAP_DXFMT_R32G32_TYPELESS:               return SVGA3D_R32G32_TYPELESS;
        case SVGA3D_DEVCAP_DXFMT_R32G32_UINT:                   return SVGA3D_R32G32_UINT;
        case SVGA3D_DEVCAP_DXFMT_R32G32_SINT:                   return SVGA3D_R32G32_SINT;
        case SVGA3D_DEVCAP_DXFMT_R32G8X24_TYPELESS:             return SVGA3D_R32G8X24_TYPELESS;
        case SVGA3D_DEVCAP_DXFMT_D32_FLOAT_S8X24_UINT:          return SVGA3D_D32_FLOAT_S8X24_UINT;
        case SVGA3D_DEVCAP_DXFMT_R32_FLOAT_X8X24_TYPELESS:      return SVGA3D_R32_FLOAT_X8X24;
        case SVGA3D_DEVCAP_DXFMT_X32_TYPELESS_G8X24_UINT:       return SVGA3D_X32_G8X24_UINT;
        case SVGA3D_DEVCAP_DXFMT_R10G10B10A2_TYPELESS:          return SVGA3D_R10G10B10A2_TYPELESS;
        case SVGA3D_DEVCAP_DXFMT_R10G10B10A2_UINT:              return SVGA3D_R10G10B10A2_UINT;
        case SVGA3D_DEVCAP_DXFMT_R11G11B10_FLOAT:               return SVGA3D_R11G11B10_FLOAT;
        case SVGA3D_DEVCAP_DXFMT_R8G8B8A8_TYPELESS:             return SVGA3D_R8G8B8A8_TYPELESS;
        case SVGA3D_DEVCAP_DXFMT_R8G8B8A8_UNORM:                return SVGA3D_R8G8B8A8_UNORM;
        case SVGA3D_DEVCAP_DXFMT_R8G8B8A8_UNORM_SRGB:           return SVGA3D_R8G8B8A8_UNORM_SRGB;
        case SVGA3D_DEVCAP_DXFMT_R8G8B8A8_UINT:                 return SVGA3D_R8G8B8A8_UINT;
        case SVGA3D_DEVCAP_DXFMT_R8G8B8A8_SINT:                 return SVGA3D_R8G8B8A8_SINT;
        case SVGA3D_DEVCAP_DXFMT_R16G16_TYPELESS:               return SVGA3D_R16G16_TYPELESS;
        case SVGA3D_DEVCAP_DXFMT_R16G16_UINT:                   return SVGA3D_R16G16_UINT;
        case SVGA3D_DEVCAP_DXFMT_R16G16_SINT:                   return SVGA3D_R16G16_SINT;
        case SVGA3D_DEVCAP_DXFMT_R32_TYPELESS:                  return SVGA3D_R32_TYPELESS;
        case SVGA3D_DEVCAP_DXFMT_D32_FLOAT:                     return SVGA3D_D32_FLOAT;
        case SVGA3D_DEVCAP_DXFMT_R32_UINT:                      return SVGA3D_R32_UINT;
        case SVGA3D_DEVCAP_DXFMT_R32_SINT:                      return SVGA3D_R32_SINT;
        case SVGA3D_DEVCAP_DXFMT_R24G8_TYPELESS:                return SVGA3D_R24G8_TYPELESS;
        case SVGA3D_DEVCAP_DXFMT_D24_UNORM_S8_UINT:             return SVGA3D_D24_UNORM_S8_UINT;
        case SVGA3D_DEVCAP_DXFMT_R24_UNORM_X8_TYPELESS:         return SVGA3D_R24_UNORM_X8;
        case SVGA3D_DEVCAP_DXFMT_X24_TYPELESS_G8_UINT:          return SVGA3D_X24_G8_UINT;
        case SVGA3D_DEVCAP_DXFMT_R8G8_TYPELESS:                 return SVGA3D_R8G8_TYPELESS;
        case SVGA3D_DEVCAP_DXFMT_R8G8_UNORM:                    return SVGA3D_R8G8_UNORM;
        case SVGA3D_DEVCAP_DXFMT_R8G8_UINT:                     return SVGA3D_R8G8_UINT;
        case SVGA3D_DEVCAP_DXFMT_R8G8_SINT:                     return SVGA3D_R8G8_SINT;
        case SVGA3D_DEVCAP_DXFMT_R16_TYPELESS:                  return SVGA3D_R16_TYPELESS;
        case SVGA3D_DEVCAP_DXFMT_R16_UNORM:                     return SVGA3D_R16_UNORM;
        case SVGA3D_DEVCAP_DXFMT_R16_UINT:                      return SVGA3D_R16_UINT;
        case SVGA3D_DEVCAP_DXFMT_R16_SNORM:                     return SVGA3D_R16_SNORM;
        case SVGA3D_DEVCAP_DXFMT_R16_SINT:                      return SVGA3D_R16_SINT;
        case SVGA3D_DEVCAP_DXFMT_R8_TYPELESS:                   return SVGA3D_R8_TYPELESS;
        case SVGA3D_DEVCAP_DXFMT_R8_UNORM:                      return SVGA3D_R8_UNORM;
        case SVGA3D_DEVCAP_DXFMT_R8_UINT:                       return SVGA3D_R8_UINT;
        case SVGA3D_DEVCAP_DXFMT_R8_SNORM:                      return SVGA3D_R8_SNORM;
        case SVGA3D_DEVCAP_DXFMT_R8_SINT:                       return SVGA3D_R8_SINT;
        case SVGA3D_DEVCAP_DXFMT_P8:                            return SVGA3D_P8;
        case SVGA3D_DEVCAP_DXFMT_R9G9B9E5_SHAREDEXP:            return SVGA3D_R9G9B9E5_SHAREDEXP;
        case SVGA3D_DEVCAP_DXFMT_R8G8_B8G8_UNORM:               return SVGA3D_R8G8_B8G8_UNORM;
        case SVGA3D_DEVCAP_DXFMT_G8R8_G8B8_UNORM:               return SVGA3D_G8R8_G8B8_UNORM;
        case SVGA3D_DEVCAP_DXFMT_BC1_TYPELESS:                  return SVGA3D_BC1_TYPELESS;
        case SVGA3D_DEVCAP_DXFMT_BC1_UNORM_SRGB:                return SVGA3D_BC1_UNORM_SRGB;
        case SVGA3D_DEVCAP_DXFMT_BC2_TYPELESS:                  return SVGA3D_BC2_TYPELESS;
        case SVGA3D_DEVCAP_DXFMT_BC2_UNORM_SRGB:                return SVGA3D_BC2_UNORM_SRGB;
        case SVGA3D_DEVCAP_DXFMT_BC3_TYPELESS:                  return SVGA3D_BC3_TYPELESS;
        case SVGA3D_DEVCAP_DXFMT_BC3_UNORM_SRGB:                return SVGA3D_BC3_UNORM_SRGB;
        case SVGA3D_DEVCAP_DXFMT_BC4_TYPELESS:                  return SVGA3D_BC4_TYPELESS;
        case SVGA3D_DEVCAP_DXFMT_ATI1:                          return SVGA3D_ATI1;
        case SVGA3D_DEVCAP_DXFMT_BC4_SNORM:                     return SVGA3D_BC4_SNORM;
        case SVGA3D_DEVCAP_DXFMT_BC5_TYPELESS:                  return SVGA3D_BC5_TYPELESS;
        case SVGA3D_DEVCAP_DXFMT_ATI2:                          return SVGA3D_ATI2;
        case SVGA3D_DEVCAP_DXFMT_BC5_SNORM:                     return SVGA3D_BC5_SNORM;
        case SVGA3D_DEVCAP_DXFMT_R10G10B10_XR_BIAS_A2_UNORM:    return SVGA3D_R10G10B10_XR_BIAS_A2_UNORM;
        case SVGA3D_DEVCAP_DXFMT_B8G8R8A8_TYPELESS:             return SVGA3D_B8G8R8A8_TYPELESS;
        case SVGA3D_DEVCAP_DXFMT_B8G8R8A8_UNORM_SRGB:           return SVGA3D_B8G8R8A8_UNORM_SRGB;
        case SVGA3D_DEVCAP_DXFMT_B8G8R8X8_TYPELESS:             return SVGA3D_B8G8R8X8_TYPELESS;
        case SVGA3D_DEVCAP_DXFMT_B8G8R8X8_UNORM_SRGB:           return SVGA3D_B8G8R8X8_UNORM_SRGB;
        case SVGA3D_DEVCAP_DXFMT_Z_DF16:                        return SVGA3D_Z_DF16;
        case SVGA3D_DEVCAP_DXFMT_Z_DF24:                        return SVGA3D_Z_DF24;
        case SVGA3D_DEVCAP_DXFMT_Z_D24S8_INT:                   return SVGA3D_Z_D24S8_INT;
        case SVGA3D_DEVCAP_DXFMT_YV12:                          return SVGA3D_YV12;
        case SVGA3D_DEVCAP_DXFMT_R32G32B32A32_FLOAT:            return SVGA3D_R32G32B32A32_FLOAT;
        case SVGA3D_DEVCAP_DXFMT_R16G16B16A16_FLOAT:            return SVGA3D_R16G16B16A16_FLOAT;
        case SVGA3D_DEVCAP_DXFMT_R16G16B16A16_UNORM:            return SVGA3D_R16G16B16A16_UNORM;
        case SVGA3D_DEVCAP_DXFMT_R32G32_FLOAT:                  return SVGA3D_R32G32_FLOAT;
        case SVGA3D_DEVCAP_DXFMT_R10G10B10A2_UNORM:             return SVGA3D_R10G10B10A2_UNORM;
        case SVGA3D_DEVCAP_DXFMT_R8G8B8A8_SNORM:                return SVGA3D_R8G8B8A8_SNORM;
        case SVGA3D_DEVCAP_DXFMT_R16G16_FLOAT:                  return SVGA3D_R16G16_FLOAT;
        case SVGA3D_DEVCAP_DXFMT_R16G16_UNORM:                  return SVGA3D_R16G16_UNORM;
        case SVGA3D_DEVCAP_DXFMT_R16G16_SNORM:                  return SVGA3D_R16G16_SNORM;
        case SVGA3D_DEVCAP_DXFMT_R32_FLOAT:                     return SVGA3D_R32_FLOAT;
        case SVGA3D_DEVCAP_DXFMT_R8G8_SNORM:                    return SVGA3D_R8G8_SNORM;
        case SVGA3D_DEVCAP_DXFMT_R16_FLOAT:                     return SVGA3D_R16_FLOAT;
        case SVGA3D_DEVCAP_DXFMT_D16_UNORM:                     return SVGA3D_D16_UNORM;
        case SVGA3D_DEVCAP_DXFMT_A8_UNORM:                      return SVGA3D_A8_UNORM;
        case SVGA3D_DEVCAP_DXFMT_BC1_UNORM:                     return SVGA3D_BC1_UNORM;
        case SVGA3D_DEVCAP_DXFMT_BC2_UNORM:                     return SVGA3D_BC2_UNORM;
        case SVGA3D_DEVCAP_DXFMT_BC3_UNORM:                     return SVGA3D_BC3_UNORM;
        case SVGA3D_DEVCAP_DXFMT_B5G6R5_UNORM:                  return SVGA3D_B5G6R5_UNORM;
        case SVGA3D_DEVCAP_DXFMT_B5G5R5A1_UNORM:                return SVGA3D_B5G5R5A1_UNORM;
        case SVGA3D_DEVCAP_DXFMT_B8G8R8A8_UNORM:                return SVGA3D_B8G8R8A8_UNORM;
        case SVGA3D_DEVCAP_DXFMT_B8G8R8X8_UNORM:                return SVGA3D_B8G8R8X8_UNORM;
        case SVGA3D_DEVCAP_DXFMT_BC4_UNORM:                     return SVGA3D_BC4_UNORM;
        case SVGA3D_DEVCAP_DXFMT_BC5_UNORM:                     return SVGA3D_BC5_UNORM;
        default:
            AssertFailed();
            break;
    }
    return SVGA3D_FORMAT_INVALID;
}


static int vmsvgaDXCheckFormatSupportPreDX(PVMSVGA3DSTATE pState, SVGA3dSurfaceFormat enmFormat, uint32_t *pu32DevCap)
{
    int rc = VINF_SUCCESS;

    *pu32DevCap = 0;

    DXGI_FORMAT const dxgiFormat = vmsvgaDXSurfaceFormat2Dxgi(enmFormat);
    if (dxgiFormat != DXGI_FORMAT_UNKNOWN)
    {
        RT_NOREF(pState);
        /** @todo Implement */
    }
    else
        rc = VERR_NOT_SUPPORTED;
    return rc;
}

static int vmsvgaDXCheckFormatSupport(PVMSVGA3DSTATE pState, SVGA3dSurfaceFormat enmFormat, uint32_t *pu32DevCap)
{
    int rc = VINF_SUCCESS;

    *pu32DevCap = 0;

    DXGI_FORMAT const dxgiFormat = vmsvgaDXSurfaceFormat2Dxgi(enmFormat);
    if (dxgiFormat != DXGI_FORMAT_UNKNOWN)
    {
        ID3D11Device *pDevice = pState->pBackend->pDevice;
        UINT FormatSupport = 0;
        HRESULT hr = pDevice->CheckFormatSupport(dxgiFormat, &FormatSupport);
        if (SUCCEEDED(hr))
        {
            *pu32DevCap |= SVGA3D_DXFMT_SUPPORTED;

            if (FormatSupport & D3D11_FORMAT_SUPPORT_SHADER_SAMPLE)
                *pu32DevCap |= SVGA3D_DXFMT_SHADER_SAMPLE;

            if (FormatSupport & D3D11_FORMAT_SUPPORT_RENDER_TARGET)
                *pu32DevCap |= SVGA3D_DXFMT_COLOR_RENDERTARGET;

            if (FormatSupport & D3D11_FORMAT_SUPPORT_DEPTH_STENCIL)
                *pu32DevCap |= SVGA3D_DXFMT_DEPTH_RENDERTARGET;

            if (FormatSupport & D3D11_FORMAT_SUPPORT_BLENDABLE)
                *pu32DevCap |= SVGA3D_DXFMT_BLENDABLE;

            if (FormatSupport & D3D11_FORMAT_SUPPORT_MIP)
                *pu32DevCap |= SVGA3D_DXFMT_MIPS;

            if (FormatSupport & D3D11_FORMAT_SUPPORT_TEXTURECUBE)
                *pu32DevCap |= SVGA3D_DXFMT_ARRAY;

            if (FormatSupport & D3D11_FORMAT_SUPPORT_TEXTURE3D)
                *pu32DevCap |= SVGA3D_DXFMT_VOLUME;

            if (FormatSupport & D3D11_FORMAT_SUPPORT_IA_VERTEX_BUFFER)
                *pu32DevCap |= SVGA3D_DXFMT_DX_VERTEX_BUFFER;

            UINT NumQualityLevels;
            hr = pDevice->CheckMultisampleQualityLevels(dxgiFormat, 2, &NumQualityLevels);
            if (SUCCEEDED(hr) && NumQualityLevels != 0)
                *pu32DevCap |= SVGADX_DXFMT_MULTISAMPLE_2;

            hr = pDevice->CheckMultisampleQualityLevels(dxgiFormat, 4, &NumQualityLevels);
            if (SUCCEEDED(hr) && NumQualityLevels != 0)
                *pu32DevCap |= SVGADX_DXFMT_MULTISAMPLE_4;

            hr = pDevice->CheckMultisampleQualityLevels(dxgiFormat, 8, &NumQualityLevels);
            if (SUCCEEDED(hr) && NumQualityLevels != 0)
                *pu32DevCap |= SVGADX_DXFMT_MULTISAMPLE_8;
        }
        else
            rc = VERR_NOT_SUPPORTED;
    }
    else
        rc = VERR_NOT_SUPPORTED;
    return rc;
}


int vmsvga3dInit(PPDMDEVINS pDevIns, PVGASTATE pThis, PVGASTATECC pThisCC)
{
    RT_NOREF(pDevIns, pThis);

    PVMSVGA3DSTATE pState = (PVMSVGA3DSTATE)RTMemAllocZ(sizeof(VMSVGA3DSTATE));
    AssertReturn(pState, VERR_NO_MEMORY);
    pThisCC->svga.p3dState = pState;

    PVMSVGA3DBACKEND pBackend = (PVMSVGA3DBACKEND)RTMemAllocZ(sizeof(VMSVGA3DBACKEND));
    AssertReturn(pBackend, VERR_NO_MEMORY);
    pState->pBackend = pBackend;

    int rc = RTLdrLoadSystem("d3d11", /* fNoUnload = */ true, &pBackend->hD3D11);
    AssertRC(rc);
    if (RT_SUCCESS(rc))
    {
        rc = RTLdrGetSymbol(pBackend->hD3D11, "D3D11CreateDevice", (void **)&pBackend->pfnD3D11CreateDevice);
        AssertRC(rc);
    }

    return rc;
}


int vmsvga3dPowerOn(PPDMDEVINS pDevIns, PVGASTATE pThis, PVGASTATECC pThisCC)
{
    RT_NOREF(pDevIns, pThis);

    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    PVMSVGA3DBACKEND pBackend = pState->pBackend;
    AssertReturn(pBackend, VERR_INVALID_STATE);

    int rc = VINF_SUCCESS;

    IDXGIAdapter *pAdapter = NULL; /* Default adapter. */
    static D3D_FEATURE_LEVEL const s_aFeatureLevels[] =
    {
        /// @todo Requires a Windows 8+ _SDKS: D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0
    };
    UINT Flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef DEBUG
    Flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    HRESULT hr = pBackend->pfnD3D11CreateDevice(pAdapter,
                                                D3D_DRIVER_TYPE_HARDWARE,
                                                NULL,
                                                Flags,
                                                s_aFeatureLevels,
                                                RT_ELEMENTS(s_aFeatureLevels),
                                                D3D11_SDK_VERSION,
                                                &pBackend->pDevice,
                                                &pBackend->FeatureLevel,
                                                &pBackend->pImmediateContext);
    if (SUCCEEDED(hr))
    {
        LogRel(("VMSVGA: Feature level %#x\n", pBackend->FeatureLevel));

        IDXGIDevice *pDxgiDevice = 0;
        hr = pBackend->pDevice->QueryInterface(__uuidof(IDXGIDevice), (void**)&pDxgiDevice);
        if (SUCCEEDED(hr))
        {
            IDXGIAdapter *pDxgiAdapter = 0;
            hr = pDxgiDevice->GetParent(__uuidof(IDXGIAdapter), (void**)&pDxgiAdapter);
            if (SUCCEEDED(hr))
            {
                hr = pDxgiAdapter->GetParent(__uuidof(IDXGIFactory), (void**)&pBackend->pDxgiFactory);
                D3D_RELEASE(pDxgiAdapter);
            }

            D3D_RELEASE(pDxgiDevice);
        }
    }

    if (FAILED(hr))
        rc = VERR_NOT_SUPPORTED;

    return rc;
}


int vmsvga3dTerminate(PVGASTATECC pThisCC)
{
    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    if (pState->pBackend)
    {
        D3D_RELEASE(pState->pBackend->pDevice);
        D3D_RELEASE(pState->pBackend->pImmediateContext);
        D3D_RELEASE(pState->pBackend->pDxgiFactory);

        RTMemFree(pState->pBackend);
        pState->pBackend = NULL;
    }

    return VINF_SUCCESS;
}


int vmsvga3dReset(PVGASTATECC pThisCC)
{
    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    /** @todo Cleanup all resources and recreate Device, ImmediateContext etc to be at the same state as after poweron. */

    return VINF_SUCCESS;
}


static int vmsvga3dDrvNotifyDefineScreen(PVGASTATECC pThisCC, VMSVGASCREENOBJECT *pScreen)
{
    /** @todo Such structures must be in VBoxVideo3D.h */
    typedef struct VBOX3DNOTIFYDEFINESCREEN
    {
        VBOX3DNOTIFY Core;
        uint32_t cWidth;
        uint32_t cHeight;
        int32_t  xRoot;
        int32_t  yRoot;
        uint32_t fPrimary;
        uint32_t cDpi;
    } VBOX3DNOTIFYDEFINESCREEN;

    VBOX3DNOTIFYDEFINESCREEN n;
    n.Core.enmNotification = VBOX3D_NOTIFY_TYPE_HW_SCREEN_CREATED;
    n.Core.iDisplay        = pScreen->idScreen;
    n.Core.u32Reserved     = 0;
    n.Core.cbData          = sizeof(n) - RT_UOFFSETOF(VBOX3DNOTIFY, au8Data);
    RT_ZERO(n.Core.au8Data);
    n.cWidth               = pScreen->cWidth;
    n.cHeight              = pScreen->cHeight;
    n.xRoot                = pScreen->xOrigin;
    n.yRoot                = pScreen->yOrigin;
    n.fPrimary             = RT_BOOL(pScreen->fuScreen & SVGA_SCREEN_IS_PRIMARY);
    n.cDpi                 = pScreen->cDpi;

    return pThisCC->pDrv->pfn3DNotifyProcess(pThisCC->pDrv, &n.Core);
}


static int vmsvga3dDrvNotifyDestroyScreen(PVGASTATECC pThisCC, VMSVGASCREENOBJECT *pScreen)
{
    VBOX3DNOTIFY n;
    n.enmNotification = VBOX3D_NOTIFY_TYPE_HW_SCREEN_DESTROYED;
    n.iDisplay        = pScreen->idScreen;
    n.u32Reserved     = 0;
    n.cbData          = sizeof(n) - RT_UOFFSETOF(VBOX3DNOTIFY, au8Data);
    RT_ZERO(n.au8Data);

    return pThisCC->pDrv->pfn3DNotifyProcess(pThisCC->pDrv, &n);
}


static int vmsvga3dDrvNotifyBindSurface(PVGASTATECC pThisCC, VMSVGASCREENOBJECT *pScreen, HANDLE hSharedSurface)
{
    VBOX3DNOTIFY n;
    n.enmNotification = VBOX3D_NOTIFY_TYPE_HW_SCREEN_BIND_SURFACE;
    n.iDisplay        = pScreen->idScreen;
    n.u32Reserved     = 0;
    n.cbData          = sizeof(n) - RT_UOFFSETOF(VBOX3DNOTIFY, au8Data);
    *(uint64_t *)&n.au8Data[0] = (uint64_t)hSharedSurface;

    return pThisCC->pDrv->pfn3DNotifyProcess(pThisCC->pDrv, &n);
}


static int vmsvga3dDrvNotifyUpdate(PVGASTATECC pThisCC, VMSVGASCREENOBJECT *pScreen,
                                   uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    typedef struct VBOX3DNOTIFYUPDATE
    {
        VBOX3DNOTIFY Core;
        uint32_t x;
        uint32_t y;
        uint32_t w;
        uint32_t h;
    } VBOX3DNOTIFYUPDATE;

    VBOX3DNOTIFYUPDATE n;
    n.Core.enmNotification = VBOX3D_NOTIFY_TYPE_HW_SCREEN_UPDATE_END;
    n.Core.iDisplay        = pScreen->idScreen;
    n.Core.u32Reserved     = 0;
    n.Core.cbData          = sizeof(n) - RT_UOFFSETOF(VBOX3DNOTIFY, au8Data);
    RT_ZERO(n.Core.au8Data);
    n.x = x;
    n.y = y;
    n.w = w;
    n.h = h;

    return pThisCC->pDrv->pfn3DNotifyProcess(pThisCC->pDrv, &n.Core);
}

static int vmsvga3dHwScreenCreate(PVMSVGA3DSTATE pState, uint32_t cWidth, uint32_t cHeight, VMSVGAHWSCREEN *p)
{
    PVMSVGA3DBACKEND pBackend = pState->pBackend;

    D3D11_TEXTURE2D_DESC td;
    RT_ZERO(td);
    td.Width              = cWidth;
    td.Height             = cHeight;
    td.MipLevels          = 1;
    td.ArraySize          = 1;
    td.Format             = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count   = 1;
    td.SampleDesc.Quality = 0;
    td.Usage              = D3D11_USAGE_DEFAULT;
    td.BindFlags          = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    td.CPUAccessFlags     = 0;
    td.MiscFlags          = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

    HRESULT hr = pBackend->pDevice->CreateTexture2D(&td, 0, &p->pTexture);
    if (SUCCEEDED(hr))
    {
        /* Get the shared handle. */
        hr = p->pTexture->QueryInterface(__uuidof(IDXGIResource), (void**)&p->pDxgiResource);
        if (SUCCEEDED(hr))
        {
            hr = p->pDxgiResource->GetSharedHandle(&p->SharedHandle);
            if (SUCCEEDED(hr))
                hr = p->pTexture->QueryInterface(__uuidof(IDXGIKeyedMutex), (void**)&p->pDXGIKeyedMutex);
        }
    }

    if (SUCCEEDED(hr))
        return VINF_SUCCESS;

    AssertFailed();
    return VERR_NOT_SUPPORTED;
}


static void vmsvga3dHwScreenDestroy(PVMSVGA3DSTATE pState, VMSVGAHWSCREEN *p)
{
    RT_NOREF(pState);
    D3D_RELEASE(p->pDXGIKeyedMutex);
    D3D_RELEASE(p->pDxgiResource);
    D3D_RELEASE(p->pTexture);
    p->SharedHandle = 0;
    p->sidScreenTarget = SVGA_ID_INVALID;
}


int vmsvga3dBackDefineScreen(PVGASTATE pThis, PVGASTATECC pThisCC, VMSVGASCREENOBJECT *pScreen)
{
    RT_NOREF(pThis, pThisCC, pScreen);

    LogRel4(("VMSVGA: vmsvga3dBackDefineScreen: screen %u\n", pScreen->idScreen));

    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    PVMSVGA3DBACKEND pBackend = pState->pBackend;
    AssertReturn(pBackend, VERR_INVALID_STATE);

    Assert(pScreen->pHwScreen == NULL);

    VMSVGAHWSCREEN *p = (VMSVGAHWSCREEN *)RTMemAllocZ(sizeof(VMSVGAHWSCREEN));
    AssertPtrReturn(p, VERR_NO_MEMORY);

    p->sidScreenTarget = SVGA_ID_INVALID;

    int rc = vmsvga3dDrvNotifyDefineScreen(pThisCC, pScreen);
    if (RT_SUCCESS(rc))
    {
        /* The frontend supports the screen. Create the actual resource. */
        rc = vmsvga3dHwScreenCreate(pState, pScreen->cWidth, pScreen->cHeight, p);
        if (RT_SUCCESS(rc))
            LogRel4(("VMSVGA: vmsvga3dBackDefineScreen: created\n"));
    }

    if (RT_SUCCESS(rc))
    {
        LogRel(("VMSVGA: Using HW accelerated screen %u\n", pScreen->idScreen));
        pScreen->pHwScreen = p;
    }
    else
    {
        LogRel4(("VMSVGA: vmsvga3dBackDefineScreen: %Rrc\n", rc));
        vmsvga3dHwScreenDestroy(pState, p);
        RTMemFree(p);
    }

    return rc;
}


int vmsvga3dBackDestroyScreen(PVGASTATECC pThisCC, VMSVGASCREENOBJECT *pScreen)
{
    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    vmsvga3dDrvNotifyDestroyScreen(pThisCC, pScreen);

    if (pScreen->pHwScreen)
    {
        vmsvga3dHwScreenDestroy(pState, pScreen->pHwScreen);
        RTMemFree(pScreen->pHwScreen);
        pScreen->pHwScreen = NULL;
    }

    return VINF_SUCCESS;
}


int vmsvga3dBackSurfaceBlitToScreen(PVGASTATECC pThisCC, VMSVGASCREENOBJECT *pScreen,
                                    SVGASignedRect destRect, SVGA3dSurfaceImageId srcImage,
                                    SVGASignedRect srcRect, uint32_t cRects, SVGASignedRect *paRects)
{
    RT_NOREF(pThisCC, pScreen, destRect, srcImage, srcRect, cRects, paRects);

    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    PVMSVGA3DBACKEND pBackend = pState->pBackend;
    AssertReturn(pBackend, VERR_INVALID_STATE);

    VMSVGAHWSCREEN *p = pScreen->pHwScreen;
    AssertReturn(p, VERR_NOT_SUPPORTED);

    PVMSVGA3DSURFACE pSurface;
    int rc = vmsvga3dSurfaceFromSid(pState, srcImage.sid, &pSurface);
    AssertRCReturn(rc, rc);

    /** @todo Implement. */
    AssertFailed();
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dSurfaceMap(PVGASTATECC pThisCC, SVGA3dSurfaceImageId const *pImage, SVGA3dBox const *pBox,
                                            VMSVGA3D_SURFACE_MAP enmMapType, VMSVGA3D_MAPPED_SURFACE *pMap)
{
    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    PVMSVGA3DBACKEND pBackend = pState->pBackend;
    AssertReturn(pBackend, VERR_INVALID_STATE);
    AssertReturn(pBackend->pImmediateContext, VERR_INVALID_STATE);

    PVMSVGA3DSURFACE pSurface;
    int rc = vmsvga3dSurfaceFromSid(pState, pImage->sid, &pSurface);
    AssertRCReturn(rc, rc);

    PVMSVGA3DBACKENDSURFACE pBackendSurface = pSurface->pBackendSurface;
    if (!pBackendSurface)
        return VERR_INVALID_PARAMETER;

    SVGA3dBox clipBox;
    if (pBox)
    {
        clipBox = *pBox;
        vmsvgaR3ClipBox(&pSurface->paMipmapLevels[0].mipmapSize, &clipBox);
        ASSERT_GUEST_RETURN(clipBox.w && clipBox.h && clipBox.d, VERR_INVALID_PARAMETER);
    }
    else
    {
        clipBox.x = 0;
        clipBox.y = 0;
        clipBox.z = 0;
        clipBox.w = pSurface->paMipmapLevels[0].mipmapSize.width;
        clipBox.h = pSurface->paMipmapLevels[0].mipmapSize.height;
        clipBox.d = pSurface->paMipmapLevels[0].mipmapSize.depth;
    }

    /** @todo D3D11_MAP_WRITE does not work with D3D11_USAGE_DYNAMIC resources. Probably VMSVGA3D_SURFACE_MAP_WRITE is not necessary. */
    D3D11_MAP d3d11MapType;
    switch (enmMapType)
    {
        case VMSVGA3D_SURFACE_MAP_READ:          d3d11MapType = D3D11_MAP_READ; break;
        case VMSVGA3D_SURFACE_MAP_WRITE:         d3d11MapType = D3D11_MAP_WRITE; break;
        case VMSVGA3D_SURFACE_MAP_READ_WRITE:    d3d11MapType = D3D11_MAP_READ_WRITE; break;
        case VMSVGA3D_SURFACE_MAP_WRITE_DISCARD: d3d11MapType = D3D11_MAP_WRITE_DISCARD; break;
        default:
            AssertFailed();
            return VERR_INVALID_PARAMETER;
    }

    D3D11_MAPPED_SUBRESOURCE mappedResource;
    RT_ZERO(mappedResource);

    if (pBackendSurface->enmResType == VMSVGA3D_RESTYPE_SCREEN_TARGET)
    {
        ID3D11Texture2D *pMappedTexture;
        if (enmMapType == VMSVGA3D_SURFACE_MAP_READ)
        {
            pMappedTexture = pBackendSurface->u.ScreenTarget.pStagingTexture;

            /* Copy the texture content to the staging texture. */
            pBackend->pImmediateContext->CopyResource(pBackendSurface->u.ScreenTarget.pStagingTexture, pBackendSurface->u.ScreenTarget.pTexture);
        }
        else
            pMappedTexture = pBackendSurface->u.ScreenTarget.pDynamicTexture;

        UINT const Subresource = 0; /* Screen target surfaces have only one subresource. */
        HRESULT hr = pBackend->pImmediateContext->Map(pMappedTexture, Subresource,
                                                      d3d11MapType, /* MapFlags =  */ 0, &mappedResource);
        if (SUCCEEDED(hr))
        {
            pMap->enmMapType   = enmMapType;
            pMap->box          = clipBox;
            pMap->cbPixel      = pSurface->cbBlock;
            pMap->cbRowPitch   = mappedResource.RowPitch;
            pMap->cbDepthPitch = mappedResource.DepthPitch;
            pMap->pvData       = (uint8_t *)mappedResource.pData
                               + pMap->box.x * pMap->cbPixel
                               + pMap->box.y * pMap->cbRowPitch
                               + pMap->box.z * pMap->cbDepthPitch;
        }
        else
            rc = VERR_NOT_SUPPORTED;
    }
    else
    {
        // UINT D3D11CalcSubresource(UINT MipSlice, UINT ArraySlice, UINT MipLevels);
        /** @todo Implement. */
        AssertFailed();
        rc = VERR_NOT_IMPLEMENTED;
    }

    return rc;
}


static DECLCALLBACK(int) vmsvga3dSurfaceUnmap(PVGASTATECC pThisCC, SVGA3dSurfaceImageId const *pImage, VMSVGA3D_MAPPED_SURFACE *pMap, bool fWritten)
{
    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    PVMSVGA3DBACKEND pBackend = pState->pBackend;
    AssertReturn(pBackend, VERR_INVALID_STATE);
    AssertReturn(pBackend->pImmediateContext, VERR_INVALID_STATE);

    PVMSVGA3DSURFACE pSurface;
    int rc = vmsvga3dSurfaceFromSid(pState, pImage->sid, &pSurface);
    AssertRCReturn(rc, rc);

    /* The called should not use the function for system memory surfaces. */
    PVMSVGA3DBACKENDSURFACE pBackendSurface = pSurface->pBackendSurface;
    AssertReturn(pBackendSurface, VERR_INVALID_PARAMETER);

    if (pBackendSurface->enmResType == VMSVGA3D_RESTYPE_SCREEN_TARGET)
    {
        ID3D11Texture2D *pMappedTexture;
        if (pMap->enmMapType == VMSVGA3D_SURFACE_MAP_READ)
            pMappedTexture = pBackendSurface->u.ScreenTarget.pStagingTexture;
        else
            pMappedTexture = pBackendSurface->u.ScreenTarget.pDynamicTexture;

        UINT const Subresource = 0; /* Screen target surfaces have only one subresource. */
        pBackend->pImmediateContext->Unmap(pMappedTexture, Subresource);

        if (   fWritten
            && (   pMap->enmMapType == VMSVGA3D_SURFACE_MAP_WRITE
                || pMap->enmMapType == VMSVGA3D_SURFACE_MAP_READ_WRITE
                || pMap->enmMapType == VMSVGA3D_SURFACE_MAP_WRITE_DISCARD))
        {
            ID3D11Resource *pDstResource = pBackendSurface->u.ScreenTarget.pTexture;
            UINT DstSubresource = Subresource;
            UINT DstX = pMap->box.x;
            UINT DstY = pMap->box.y;
            UINT DstZ = pMap->box.z;
            ID3D11Resource *pSrcResource = pBackendSurface->u.ScreenTarget.pDynamicTexture;
            UINT SrcSubresource = Subresource;
            D3D11_BOX SrcBox;
            SrcBox.left   = pMap->box.x;
            SrcBox.top    = pMap->box.y;
            SrcBox.front  = pMap->box.z;
            SrcBox.right  = pMap->box.x + pMap->box.w;
            SrcBox.bottom = pMap->box.y + pMap->box.h;
            SrcBox.back   = pMap->box.z + pMap->box.d;
            pBackend->pImmediateContext->CopySubresourceRegion(pDstResource, DstSubresource, DstX, DstY, DstZ,
                                                               pSrcResource, SrcSubresource, &SrcBox);
        }
    }
    else
    {
        AssertFailed();
        rc = VERR_NOT_IMPLEMENTED;
    }

    return rc;
}


int vmsvga3dBackSurfaceCreateScreenTarget(PVMSVGA3DSTATE pState, PVMSVGA3DSURFACE pSurface)
{
    PVMSVGA3DBACKEND pBackend = pState->pBackend;
    AssertReturn(pBackend, VERR_INVALID_STATE);
    AssertReturn(pBackend->pDevice, VERR_INVALID_STATE);

    /* Surface must have SCREEN_TARGET flag. */
    ASSERT_GUEST_RETURN(RT_BOOL(pSurface->surfaceFlags & SVGA3D_SURFACE_SCREENTARGET), VERR_INVALID_PARAMETER);

    if (VMSVGA3DSURFACE_HAS_HW_SURFACE(pSurface))
    {
        AssertFailed(); /* Should the function not be used like that? */
        vmsvga3dBackSurfaceDestroy(pState, pSurface);
    }

    PVMSVGA3DBACKENDSURFACE pBackendSurface = (PVMSVGA3DBACKENDSURFACE)RTMemAllocZ(sizeof(VMSVGA3DBACKENDSURFACE));
    AssertPtrReturn(pBackendSurface, VERR_NO_MEMORY);

    D3D11_TEXTURE2D_DESC td;
    RT_ZERO(td);
    td.Width              = pSurface->paMipmapLevels[0].mipmapSize.width;
    td.Height             = pSurface->paMipmapLevels[0].mipmapSize.height;
    Assert(pSurface->cLevels == 1);
    td.MipLevels          = 1;
    td.ArraySize          = 1;
    td.Format             = vmsvgaDXScreenTargetFormat2Dxgi(pSurface->format); // DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count   = 1;
    td.SampleDesc.Quality = 0;
    td.Usage              = D3D11_USAGE_DEFAULT;
    td.BindFlags          = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    td.CPUAccessFlags     = 0;
    td.MiscFlags          = 0;

    HRESULT hr = pBackend->pDevice->CreateTexture2D(&td, 0, &pBackendSurface->u.ScreenTarget.pTexture);

    if (SUCCEEDED(hr))
    {
        /* Map-able texture. */
        td.Usage          = D3D11_USAGE_DYNAMIC;
        td.BindFlags      = D3D11_BIND_SHADER_RESOURCE; /* Have to specify a supported flag, otherwise E_INVALIDARG will be returned. */
        td.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        td.MiscFlags      = 0;
        hr = pBackend->pDevice->CreateTexture2D(&td, 0, &pBackendSurface->u.ScreenTarget.pDynamicTexture);
    }

    if (SUCCEEDED(hr))
    {
        /* Staging texture. */
        td.Usage          = D3D11_USAGE_STAGING;
        td.BindFlags      = 0; /* No flags allowed. */
        td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        td.MiscFlags      = 0;
        hr = pBackend->pDevice->CreateTexture2D(&td, 0, &pBackendSurface->u.ScreenTarget.pStagingTexture);
    }

    if (SUCCEEDED(hr))
    {
        /*
         * Success.
         */
        pBackendSurface->enmResType = VMSVGA3D_RESTYPE_SCREEN_TARGET;
        pSurface->pBackendSurface = pBackendSurface;
        return VINF_SUCCESS;
    }

    /* Failure. */
    D3D_RELEASE(pBackendSurface->u.ScreenTarget.pStagingTexture);
    D3D_RELEASE(pBackendSurface->u.ScreenTarget.pDynamicTexture);
    D3D_RELEASE(pBackendSurface->u.ScreenTarget.pTexture);
    RTMemFree(pBackendSurface);
    return VERR_NO_MEMORY;
}


static DECLCALLBACK(int) vmsvga3dScreenTargetBind(PVGASTATECC pThisCC, VMSVGASCREENOBJECT *pScreen, uint32_t sid)
{
    int rc = VINF_SUCCESS;

    PVMSVGA3DSURFACE pSurface;
    if (sid != SVGA_ID_INVALID)
    {
        /* Create the surface if does not yet exist. */
        PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
        AssertReturn(pState, VERR_INVALID_STATE);

        rc = vmsvga3dSurfaceFromSid(pState, sid, &pSurface);
        AssertRCReturn(rc, rc);

        if (!VMSVGA3DSURFACE_HAS_HW_SURFACE(pSurface))
        {
            /* Create the actual texture. */
            rc = vmsvga3dBackSurfaceCreateScreenTarget(pState, pSurface);
            AssertRCReturn(rc, rc);
        }
    }
    else
        pSurface = NULL;

    /* Notify the HW accelerated screen if it is used. */
    VMSVGAHWSCREEN *pHwScreen = pScreen->pHwScreen;
    if (!pHwScreen)
        return VINF_SUCCESS;

    /* Same surface -> do nothing. */
    if (pHwScreen->sidScreenTarget == sid)
        return VINF_SUCCESS;

    if (sid != SVGA_ID_INVALID)
    {
        AssertReturn(   pSurface->pBackendSurface
                     && pSurface->pBackendSurface->enmResType == VMSVGA3D_RESTYPE_SCREEN_TARGET, VERR_INVALID_PARAMETER);

        HANDLE const hSharedSurface = pHwScreen->SharedHandle;
        rc = vmsvga3dDrvNotifyBindSurface(pThisCC, pScreen, hSharedSurface);
    }

    if (RT_SUCCESS(rc))
    {
        pHwScreen->sidScreenTarget = sid;
    }

    return rc;
}


static DECLCALLBACK(int) vmsvga3dScreenTargetUpdate(PVGASTATECC pThisCC, VMSVGASCREENOBJECT *pScreen, SVGA3dRect const *pRect)
{
    VMSVGAHWSCREEN *pHwScreen = pScreen->pHwScreen;
    AssertReturn(pHwScreen, VERR_NOT_SUPPORTED);

    if (pHwScreen->sidScreenTarget == SVGA_ID_INVALID)
        return VINF_SUCCESS; /* No surface bound. */

    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    PVMSVGA3DBACKEND pBackend = pState->pBackend;
    AssertReturn(pBackend, VERR_INVALID_STATE);

    PVMSVGA3DSURFACE pSurface;
    int rc = vmsvga3dSurfaceFromSid(pState, pHwScreen->sidScreenTarget, &pSurface);
    AssertRCReturn(rc, rc);

    PVMSVGA3DBACKENDSURFACE pBackendSurface = pSurface->pBackendSurface;
    AssertReturn(pBackendSurface, VERR_INVALID_PARAMETER);

    SVGA3dRect boundRect;
    boundRect.x = 0;
    boundRect.y = 0;
    boundRect.w = pSurface->paMipmapLevels[0].mipmapSize.width;
    boundRect.h = pSurface->paMipmapLevels[0].mipmapSize.height;
    SVGA3dRect clipRect = *pRect;
    vmsvgaR3Clip3dRect(&boundRect, &clipRect);
    ASSERT_GUEST_RETURN(clipRect.w && clipRect.h, VERR_INVALID_PARAMETER);

    /* Copy the screen texture to the shared surface. */
    DWORD result = pHwScreen->pDXGIKeyedMutex->AcquireSync(0, 10000);
    if (result == WAIT_OBJECT_0)
    {
        ID3D11Query *pQuery = 0;
        D3D11_QUERY_DESC qd;
        RT_ZERO(qd);
        qd.Query = D3D11_QUERY_EVENT;
        HRESULT hr2 = pBackend->pDevice->CreateQuery(&qd, &pQuery);
        Assert(hr2 == S_OK); RT_NOREF(hr2);

        pBackend->pImmediateContext->CopyResource(pHwScreen->pTexture, pBackendSurface->u.ScreenTarget.pTexture);

        pBackend->pImmediateContext->Flush();

        pBackend->pImmediateContext->End(pQuery);

        BOOL queryData;
        while (pBackend->pImmediateContext->GetData(pQuery, &queryData, sizeof(queryData), 0) != S_OK)
        {
            RTThreadYield();
        }
        D3D_RELEASE(pQuery);

        result = pHwScreen->pDXGIKeyedMutex->ReleaseSync(1);
    }
    else
        AssertFailed();

    rc = vmsvga3dDrvNotifyUpdate(pThisCC, pScreen, pRect->x, pRect->y, pRect->w, pRect->h);
    return rc;
}


void vmsvga3dUpdateHostScreenViewport(PVGASTATECC pThisCC, uint32_t idScreen, VMSVGAVIEWPORT const *pOldViewport)
{
    RT_NOREF(pThisCC, idScreen, pOldViewport);
    /** @todo Scroll the screen content without requiring the guest to redraw. */
}


int vmsvga3dQueryCaps(PVGASTATECC pThisCC, SVGA3dDevCapIndex idx3dCaps, uint32_t *pu32Val)
{
    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    int rc = VINF_SUCCESS;

    *pu32Val = 0;

    /* Most values are taken from:
     * https://docs.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-devices-downlevel-intro
     *
     * Shader values are from
     * https://docs.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-models
     */

    switch (idx3dCaps)
    {
    case SVGA3D_DEVCAP_3D:
        *pu32Val = 1;
        break;

    case SVGA3D_DEVCAP_MAX_LIGHTS:
        *pu32Val = SVGA3D_NUM_LIGHTS; /* VGPU9. Not applicable to DX11. */
        break;

    case SVGA3D_DEVCAP_MAX_TEXTURES:
        *pu32Val = SVGA3D_NUM_TEXTURE_UNITS; /* VGPU9. Not applicable to DX11. */
        break;

    case SVGA3D_DEVCAP_MAX_CLIP_PLANES:
        *pu32Val = SVGA3D_NUM_CLIPPLANES;
        break;

    case SVGA3D_DEVCAP_VERTEX_SHADER_VERSION:
        if (pState->pBackend->FeatureLevel >= D3D_FEATURE_LEVEL_10_0)
            *pu32Val = SVGA3DVSVERSION_40;
        else
            *pu32Val = SVGA3DVSVERSION_30;
        break;

    case SVGA3D_DEVCAP_VERTEX_SHADER:
        *pu32Val = 1;
        break;

    case SVGA3D_DEVCAP_FRAGMENT_SHADER_VERSION:
        if (pState->pBackend->FeatureLevel >= D3D_FEATURE_LEVEL_10_0)
            *pu32Val = SVGA3DPSVERSION_40;
        else
            *pu32Val = SVGA3DPSVERSION_30;
        break;

    case SVGA3D_DEVCAP_FRAGMENT_SHADER:
        *pu32Val = 1;
        break;

    case SVGA3D_DEVCAP_MAX_RENDER_TARGETS:
        if (pState->pBackend->FeatureLevel >= D3D_FEATURE_LEVEL_10_0)
            *pu32Val = 8;
        else
            *pu32Val = 4;
        break;

    case SVGA3D_DEVCAP_S23E8_TEXTURES:
    case SVGA3D_DEVCAP_S10E5_TEXTURES:
        /* Must be obsolete by now; surface format caps specify the same thing. */
        break;

    case SVGA3D_DEVCAP_MAX_FIXED_VERTEXBLEND:
        /* Obsolete */
        break;

    /*
     *   2. The BUFFER_FORMAT capabilities are deprecated, and they always
     *      return TRUE. Even on physical hardware that does not support
     *      these formats natively, the SVGA3D device will provide an emulation
     *      which should be invisible to the guest OS.
     */
    case SVGA3D_DEVCAP_D16_BUFFER_FORMAT:
    case SVGA3D_DEVCAP_D24S8_BUFFER_FORMAT:
    case SVGA3D_DEVCAP_D24X8_BUFFER_FORMAT:
        *pu32Val = 1;
        break;

    case SVGA3D_DEVCAP_QUERY_TYPES:
        /* Obsolete */
        break;

    case SVGA3D_DEVCAP_TEXTURE_GRADIENT_SAMPLING:
        /* Obsolete */
        break;

    case SVGA3D_DEVCAP_MAX_POINT_SIZE:
        AssertCompile(sizeof(uint32_t) == sizeof(float));
        *(float *)pu32Val = 256.0f;  /* VGPU9. Not applicable to DX11. */
        break;

    case SVGA3D_DEVCAP_MAX_SHADER_TEXTURES:
        /* Obsolete */
        break;

    case SVGA3D_DEVCAP_MAX_TEXTURE_WIDTH:
    case SVGA3D_DEVCAP_MAX_TEXTURE_HEIGHT:
        if (pState->pBackend->FeatureLevel >= D3D_FEATURE_LEVEL_11_0)
            *pu32Val = 16384;
        else if (pState->pBackend->FeatureLevel >= D3D_FEATURE_LEVEL_10_0)
            *pu32Val = 8192;
        else if (pState->pBackend->FeatureLevel >= D3D_FEATURE_LEVEL_9_3)
            *pu32Val = 4096;
        else
            *pu32Val = 2048;
        break;

    case SVGA3D_DEVCAP_MAX_VOLUME_EXTENT:
        if (pState->pBackend->FeatureLevel >= D3D_FEATURE_LEVEL_10_0)
            *pu32Val = 2048;
        else
            *pu32Val = 256;
        break;

    case SVGA3D_DEVCAP_MAX_TEXTURE_REPEAT:
        if (pState->pBackend->FeatureLevel >= D3D_FEATURE_LEVEL_11_0)
            *pu32Val = 16384;
        else if (pState->pBackend->FeatureLevel >= D3D_FEATURE_LEVEL_9_3)
            *pu32Val = 8192;
        else if (pState->pBackend->FeatureLevel >= D3D_FEATURE_LEVEL_9_2)
            *pu32Val = 2048;
        else
            *pu32Val = 128;
        break;

    case SVGA3D_DEVCAP_MAX_TEXTURE_ASPECT_RATIO:
        /* Obsolete */
        break;

    case SVGA3D_DEVCAP_MAX_TEXTURE_ANISOTROPY:
        if (pState->pBackend->FeatureLevel >= D3D_FEATURE_LEVEL_9_2)
            *pu32Val = D3D11_REQ_MAXANISOTROPY;
        else
            *pu32Val = 2; // D3D_FL9_1_DEFAULT_MAX_ANISOTROPY;
        break;

    case SVGA3D_DEVCAP_MAX_PRIMITIVE_COUNT:
        if (pState->pBackend->FeatureLevel >= D3D_FEATURE_LEVEL_10_0)
            *pu32Val = UINT32_MAX;
        else if (pState->pBackend->FeatureLevel >= D3D_FEATURE_LEVEL_9_2)
            *pu32Val = 1048575; // D3D_FL9_2_IA_PRIMITIVE_MAX_COUNT;
        else
            *pu32Val = 65535; // D3D_FL9_1_IA_PRIMITIVE_MAX_COUNT;
        break;

    case SVGA3D_DEVCAP_MAX_VERTEX_INDEX:
        if (pState->pBackend->FeatureLevel >= D3D_FEATURE_LEVEL_10_0)
            *pu32Val = UINT32_MAX;
        else if (pState->pBackend->FeatureLevel >= D3D_FEATURE_LEVEL_9_2)
            *pu32Val = 1048575;
        else
            *pu32Val = 65534;
        break;

    case SVGA3D_DEVCAP_MAX_VERTEX_SHADER_INSTRUCTIONS:
        if (pState->pBackend->FeatureLevel >= D3D_FEATURE_LEVEL_10_0)
            *pu32Val = UINT32_MAX;
        else
            *pu32Val = 512;
        break;

    case SVGA3D_DEVCAP_MAX_FRAGMENT_SHADER_INSTRUCTIONS:
        if (pState->pBackend->FeatureLevel >= D3D_FEATURE_LEVEL_10_0)
            *pu32Val = UINT32_MAX;
        else
            *pu32Val = 512;
        break;

    case SVGA3D_DEVCAP_MAX_VERTEX_SHADER_TEMPS:
        if (pState->pBackend->FeatureLevel >= D3D_FEATURE_LEVEL_10_0)
            *pu32Val = 4096;
        else
            *pu32Val = 32;
        break;

    case SVGA3D_DEVCAP_MAX_FRAGMENT_SHADER_TEMPS:
        if (pState->pBackend->FeatureLevel >= D3D_FEATURE_LEVEL_10_0)
            *pu32Val = 4096;
        else
            *pu32Val = 32;
        break;

    case SVGA3D_DEVCAP_TEXTURE_OPS:
        /* Obsolete */
        break;

    case SVGA3D_DEVCAP_SURFACEFMT_X8R8G8B8:
    case SVGA3D_DEVCAP_SURFACEFMT_A8R8G8B8:
    case SVGA3D_DEVCAP_SURFACEFMT_A2R10G10B10:
    case SVGA3D_DEVCAP_SURFACEFMT_X1R5G5B5:
    case SVGA3D_DEVCAP_SURFACEFMT_A1R5G5B5:
    case SVGA3D_DEVCAP_SURFACEFMT_A4R4G4B4:
    case SVGA3D_DEVCAP_SURFACEFMT_R5G6B5:
    case SVGA3D_DEVCAP_SURFACEFMT_LUMINANCE16:
    case SVGA3D_DEVCAP_SURFACEFMT_LUMINANCE8_ALPHA8:
    case SVGA3D_DEVCAP_SURFACEFMT_ALPHA8:
    case SVGA3D_DEVCAP_SURFACEFMT_LUMINANCE8:
    case SVGA3D_DEVCAP_SURFACEFMT_Z_D16:
    case SVGA3D_DEVCAP_SURFACEFMT_Z_D24S8:
    case SVGA3D_DEVCAP_SURFACEFMT_Z_D24X8:
    case SVGA3D_DEVCAP_SURFACEFMT_DXT1:
    case SVGA3D_DEVCAP_SURFACEFMT_DXT2:
    case SVGA3D_DEVCAP_SURFACEFMT_DXT3:
    case SVGA3D_DEVCAP_SURFACEFMT_DXT4:
    case SVGA3D_DEVCAP_SURFACEFMT_DXT5:
    case SVGA3D_DEVCAP_SURFACEFMT_BUMPX8L8V8U8:
    case SVGA3D_DEVCAP_SURFACEFMT_A2W10V10U10:
    case SVGA3D_DEVCAP_SURFACEFMT_BUMPU8V8:
    case SVGA3D_DEVCAP_SURFACEFMT_Q8W8V8U8:
    case SVGA3D_DEVCAP_SURFACEFMT_CxV8U8:
    case SVGA3D_DEVCAP_SURFACEFMT_R_S10E5:
    case SVGA3D_DEVCAP_SURFACEFMT_R_S23E8:
    case SVGA3D_DEVCAP_SURFACEFMT_RG_S10E5:
    case SVGA3D_DEVCAP_SURFACEFMT_RG_S23E8:
    case SVGA3D_DEVCAP_SURFACEFMT_ARGB_S10E5:
    case SVGA3D_DEVCAP_SURFACEFMT_ARGB_S23E8:
    case SVGA3D_DEVCAP_SURFACEFMT_V16U16:
    case SVGA3D_DEVCAP_SURFACEFMT_G16R16:
    case SVGA3D_DEVCAP_SURFACEFMT_A16B16G16R16:
    case SVGA3D_DEVCAP_SURFACEFMT_UYVY:
    case SVGA3D_DEVCAP_SURFACEFMT_YUY2:
    case SVGA3D_DEVCAP_SURFACEFMT_NV12:
    case SVGA3D_DEVCAP_SURFACEFMT_AYUV:
    case SVGA3D_DEVCAP_SURFACEFMT_Z_DF16:
    case SVGA3D_DEVCAP_SURFACEFMT_Z_DF24:
    case SVGA3D_DEVCAP_SURFACEFMT_Z_D24S8_INT:
    case SVGA3D_DEVCAP_SURFACEFMT_ATI1:
    case SVGA3D_DEVCAP_SURFACEFMT_ATI2:
    case SVGA3D_DEVCAP_SURFACEFMT_YV12:
    {
        SVGA3dSurfaceFormat const enmFormat = vmsvgaDXDevCapSurfaceFmt2Format(idx3dCaps);
        rc = vmsvgaDXCheckFormatSupportPreDX(pState, enmFormat, pu32Val);
        break;
    }

    case SVGA3D_DEVCAP_MISSING62:
        /* Unused */
        break;

    case SVGA3D_DEVCAP_MAX_VERTEX_SHADER_TEXTURES:
        /* Obsolete */
        break;

    case SVGA3D_DEVCAP_MAX_SIMULTANEOUS_RENDER_TARGETS:
        if (pState->pBackend->FeatureLevel >= D3D_FEATURE_LEVEL_10_0)
            *pu32Val = 8;
        else if (pState->pBackend->FeatureLevel >= D3D_FEATURE_LEVEL_9_3)
            *pu32Val = 4; // D3D_FL9_3_SIMULTANEOUS_RENDER_TARGET_COUNT
        else
            *pu32Val = 1; // D3D_FL9_1_SIMULTANEOUS_RENDER_TARGET_COUNT
        break;

    case SVGA3D_DEVCAP_MULTISAMPLE_NONMASKABLESAMPLES:
    case SVGA3D_DEVCAP_MULTISAMPLE_MASKABLESAMPLES:
        *pu32Val = (1 << (2-1)) | (1 << (4-1)) | (1 << (8-1)); /* 2x, 4x, 8x */
        break;

    case SVGA3D_DEVCAP_ALPHATOCOVERAGE:
        /* Obsolete? */
        break;

    case SVGA3D_DEVCAP_SUPERSAMPLE:
        /* Obsolete? */
        break;

    case SVGA3D_DEVCAP_AUTOGENMIPMAPS:
        *pu32Val = 1;
        break;

    case SVGA3D_DEVCAP_MAX_CONTEXT_IDS:
        *pu32Val = SVGA3D_MAX_CONTEXT_IDS;
        break;

    case SVGA3D_DEVCAP_MAX_SURFACE_IDS:
        *pu32Val = SVGA3D_MAX_SURFACE_IDS;
        break;

    case SVGA3D_DEVCAP_DEAD1:
        /* Obsolete */
        break;

    case SVGA3D_DEVCAP_VIDEO_DECODE:
        /* Obsolete */
        break;

    case SVGA3D_DEVCAP_VIDEO_PROCESS:
        /* Obsolete */
        break;

    case SVGA3D_DEVCAP_LINE_AA:
        *pu32Val = 1;
        break;

    case SVGA3D_DEVCAP_LINE_STIPPLE:
        *pu32Val = 0; /* DX11 does not seem to support this directly. */
        break;

    case SVGA3D_DEVCAP_MAX_LINE_WIDTH:
        AssertCompile(sizeof(uint32_t) == sizeof(float));
        *(float *)pu32Val = 1.0f;
        break;

    case SVGA3D_DEVCAP_MAX_AA_LINE_WIDTH:
        AssertCompile(sizeof(uint32_t) == sizeof(float));
        *(float *)pu32Val = 1.0f;
        break;

    case SVGA3D_DEVCAP_LOGICOPS:
        /* Deprecated. */
        AssertCompile(SVGA3D_DEVCAP_LOGICOPS == 92); /* Newer SVGA headers redefine this. */
        *pu32Val = 0; /* Supported starting with Direct3D 11.1 */
        break;

    case SVGA3D_DEVCAP_TS_COLOR_KEY:
        *pu32Val = 0; /* DX11 does not seem to support this directly. */
        break;

    case SVGA3D_DEVCAP_DEAD2:
        break;

    case SVGA3D_DEVCAP_DX:
        *pu32Val = 1;
        break;

    case SVGA3D_DEVCAP_MAX_TEXTURE_ARRAY_SIZE:
        *pu32Val = D3D11_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION;
        break;

    case SVGA3D_DEVCAP_DX_MAX_VERTEXBUFFERS:
        *pu32Val = D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT;
        break;

    case SVGA3D_DEVCAP_DX_MAX_CONSTANT_BUFFERS:
        *pu32Val = D3D11_COMMONSHADER_CONSTANT_BUFFER_HW_SLOT_COUNT;
        break;

    case SVGA3D_DEVCAP_DX_PROVOKING_VERTEX:
        *pu32Val = 0; /* boolean */
        break;

    case SVGA3D_DEVCAP_DXFMT_X8R8G8B8:
    case SVGA3D_DEVCAP_DXFMT_A8R8G8B8:
    case SVGA3D_DEVCAP_DXFMT_R5G6B5:
    case SVGA3D_DEVCAP_DXFMT_X1R5G5B5:
    case SVGA3D_DEVCAP_DXFMT_A1R5G5B5:
    case SVGA3D_DEVCAP_DXFMT_A4R4G4B4:
    case SVGA3D_DEVCAP_DXFMT_Z_D32:
    case SVGA3D_DEVCAP_DXFMT_Z_D16:
    case SVGA3D_DEVCAP_DXFMT_Z_D24S8:
    case SVGA3D_DEVCAP_DXFMT_Z_D15S1:
    case SVGA3D_DEVCAP_DXFMT_LUMINANCE8:
    case SVGA3D_DEVCAP_DXFMT_LUMINANCE4_ALPHA4:
    case SVGA3D_DEVCAP_DXFMT_LUMINANCE16:
    case SVGA3D_DEVCAP_DXFMT_LUMINANCE8_ALPHA8:
    case SVGA3D_DEVCAP_DXFMT_DXT1:
    case SVGA3D_DEVCAP_DXFMT_DXT2:
    case SVGA3D_DEVCAP_DXFMT_DXT3:
    case SVGA3D_DEVCAP_DXFMT_DXT4:
    case SVGA3D_DEVCAP_DXFMT_DXT5:
    case SVGA3D_DEVCAP_DXFMT_BUMPU8V8:
    case SVGA3D_DEVCAP_DXFMT_BUMPL6V5U5:
    case SVGA3D_DEVCAP_DXFMT_BUMPX8L8V8U8:
    case SVGA3D_DEVCAP_DXFMT_FORMAT_DEAD1:
    case SVGA3D_DEVCAP_DXFMT_ARGB_S10E5:
    case SVGA3D_DEVCAP_DXFMT_ARGB_S23E8:
    case SVGA3D_DEVCAP_DXFMT_A2R10G10B10:
    case SVGA3D_DEVCAP_DXFMT_V8U8:
    case SVGA3D_DEVCAP_DXFMT_Q8W8V8U8:
    case SVGA3D_DEVCAP_DXFMT_CxV8U8:
    case SVGA3D_DEVCAP_DXFMT_X8L8V8U8:
    case SVGA3D_DEVCAP_DXFMT_A2W10V10U10:
    case SVGA3D_DEVCAP_DXFMT_ALPHA8:
    case SVGA3D_DEVCAP_DXFMT_R_S10E5:
    case SVGA3D_DEVCAP_DXFMT_R_S23E8:
    case SVGA3D_DEVCAP_DXFMT_RG_S10E5:
    case SVGA3D_DEVCAP_DXFMT_RG_S23E8:
    case SVGA3D_DEVCAP_DXFMT_BUFFER:
    case SVGA3D_DEVCAP_DXFMT_Z_D24X8:
    case SVGA3D_DEVCAP_DXFMT_V16U16:
    case SVGA3D_DEVCAP_DXFMT_G16R16:
    case SVGA3D_DEVCAP_DXFMT_A16B16G16R16:
    case SVGA3D_DEVCAP_DXFMT_UYVY:
    case SVGA3D_DEVCAP_DXFMT_YUY2:
    case SVGA3D_DEVCAP_DXFMT_NV12:
    case SVGA3D_DEVCAP_DXFMT_AYUV:
    case SVGA3D_DEVCAP_DXFMT_R32G32B32A32_TYPELESS:
    case SVGA3D_DEVCAP_DXFMT_R32G32B32A32_UINT:
    case SVGA3D_DEVCAP_DXFMT_R32G32B32A32_SINT:
    case SVGA3D_DEVCAP_DXFMT_R32G32B32_TYPELESS:
    case SVGA3D_DEVCAP_DXFMT_R32G32B32_FLOAT:
    case SVGA3D_DEVCAP_DXFMT_R32G32B32_UINT:
    case SVGA3D_DEVCAP_DXFMT_R32G32B32_SINT:
    case SVGA3D_DEVCAP_DXFMT_R16G16B16A16_TYPELESS:
    case SVGA3D_DEVCAP_DXFMT_R16G16B16A16_UINT:
    case SVGA3D_DEVCAP_DXFMT_R16G16B16A16_SNORM:
    case SVGA3D_DEVCAP_DXFMT_R16G16B16A16_SINT:
    case SVGA3D_DEVCAP_DXFMT_R32G32_TYPELESS:
    case SVGA3D_DEVCAP_DXFMT_R32G32_UINT:
    case SVGA3D_DEVCAP_DXFMT_R32G32_SINT:
    case SVGA3D_DEVCAP_DXFMT_R32G8X24_TYPELESS:
    case SVGA3D_DEVCAP_DXFMT_D32_FLOAT_S8X24_UINT:
    case SVGA3D_DEVCAP_DXFMT_R32_FLOAT_X8X24_TYPELESS:
    case SVGA3D_DEVCAP_DXFMT_X32_TYPELESS_G8X24_UINT:
    case SVGA3D_DEVCAP_DXFMT_R10G10B10A2_TYPELESS:
    case SVGA3D_DEVCAP_DXFMT_R10G10B10A2_UINT:
    case SVGA3D_DEVCAP_DXFMT_R11G11B10_FLOAT:
    case SVGA3D_DEVCAP_DXFMT_R8G8B8A8_TYPELESS:
    case SVGA3D_DEVCAP_DXFMT_R8G8B8A8_UNORM:
    case SVGA3D_DEVCAP_DXFMT_R8G8B8A8_UNORM_SRGB:
    case SVGA3D_DEVCAP_DXFMT_R8G8B8A8_UINT:
    case SVGA3D_DEVCAP_DXFMT_R8G8B8A8_SINT:
    case SVGA3D_DEVCAP_DXFMT_R16G16_TYPELESS:
    case SVGA3D_DEVCAP_DXFMT_R16G16_UINT:
    case SVGA3D_DEVCAP_DXFMT_R16G16_SINT:
    case SVGA3D_DEVCAP_DXFMT_R32_TYPELESS:
    case SVGA3D_DEVCAP_DXFMT_D32_FLOAT:
    case SVGA3D_DEVCAP_DXFMT_R32_UINT:
    case SVGA3D_DEVCAP_DXFMT_R32_SINT:
    case SVGA3D_DEVCAP_DXFMT_R24G8_TYPELESS:
    case SVGA3D_DEVCAP_DXFMT_D24_UNORM_S8_UINT:
    case SVGA3D_DEVCAP_DXFMT_R24_UNORM_X8_TYPELESS:
    case SVGA3D_DEVCAP_DXFMT_X24_TYPELESS_G8_UINT:
    case SVGA3D_DEVCAP_DXFMT_R8G8_TYPELESS:
    case SVGA3D_DEVCAP_DXFMT_R8G8_UNORM:
    case SVGA3D_DEVCAP_DXFMT_R8G8_UINT:
    case SVGA3D_DEVCAP_DXFMT_R8G8_SINT:
    case SVGA3D_DEVCAP_DXFMT_R16_TYPELESS:
    case SVGA3D_DEVCAP_DXFMT_R16_UNORM:
    case SVGA3D_DEVCAP_DXFMT_R16_UINT:
    case SVGA3D_DEVCAP_DXFMT_R16_SNORM:
    case SVGA3D_DEVCAP_DXFMT_R16_SINT:
    case SVGA3D_DEVCAP_DXFMT_R8_TYPELESS:
    case SVGA3D_DEVCAP_DXFMT_R8_UNORM:
    case SVGA3D_DEVCAP_DXFMT_R8_UINT:
    case SVGA3D_DEVCAP_DXFMT_R8_SNORM:
    case SVGA3D_DEVCAP_DXFMT_R8_SINT:
    case SVGA3D_DEVCAP_DXFMT_P8:
    case SVGA3D_DEVCAP_DXFMT_R9G9B9E5_SHAREDEXP:
    case SVGA3D_DEVCAP_DXFMT_R8G8_B8G8_UNORM:
    case SVGA3D_DEVCAP_DXFMT_G8R8_G8B8_UNORM:
    case SVGA3D_DEVCAP_DXFMT_BC1_TYPELESS:
    case SVGA3D_DEVCAP_DXFMT_BC1_UNORM_SRGB:
    case SVGA3D_DEVCAP_DXFMT_BC2_TYPELESS:
    case SVGA3D_DEVCAP_DXFMT_BC2_UNORM_SRGB:
    case SVGA3D_DEVCAP_DXFMT_BC3_TYPELESS:
    case SVGA3D_DEVCAP_DXFMT_BC3_UNORM_SRGB:
    case SVGA3D_DEVCAP_DXFMT_BC4_TYPELESS:
    case SVGA3D_DEVCAP_DXFMT_ATI1:
    case SVGA3D_DEVCAP_DXFMT_BC4_SNORM:
    case SVGA3D_DEVCAP_DXFMT_BC5_TYPELESS:
    case SVGA3D_DEVCAP_DXFMT_ATI2:
    case SVGA3D_DEVCAP_DXFMT_BC5_SNORM:
    case SVGA3D_DEVCAP_DXFMT_R10G10B10_XR_BIAS_A2_UNORM:
    case SVGA3D_DEVCAP_DXFMT_B8G8R8A8_TYPELESS:
    case SVGA3D_DEVCAP_DXFMT_B8G8R8A8_UNORM_SRGB:
    case SVGA3D_DEVCAP_DXFMT_B8G8R8X8_TYPELESS:
    case SVGA3D_DEVCAP_DXFMT_B8G8R8X8_UNORM_SRGB:
    case SVGA3D_DEVCAP_DXFMT_Z_DF16:
    case SVGA3D_DEVCAP_DXFMT_Z_DF24:
    case SVGA3D_DEVCAP_DXFMT_Z_D24S8_INT:
    case SVGA3D_DEVCAP_DXFMT_YV12:
    case SVGA3D_DEVCAP_DXFMT_R32G32B32A32_FLOAT:
    case SVGA3D_DEVCAP_DXFMT_R16G16B16A16_FLOAT:
    case SVGA3D_DEVCAP_DXFMT_R16G16B16A16_UNORM:
    case SVGA3D_DEVCAP_DXFMT_R32G32_FLOAT:
    case SVGA3D_DEVCAP_DXFMT_R10G10B10A2_UNORM:
    case SVGA3D_DEVCAP_DXFMT_R8G8B8A8_SNORM:
    case SVGA3D_DEVCAP_DXFMT_R16G16_FLOAT:
    case SVGA3D_DEVCAP_DXFMT_R16G16_UNORM:
    case SVGA3D_DEVCAP_DXFMT_R16G16_SNORM:
    case SVGA3D_DEVCAP_DXFMT_R32_FLOAT:
    case SVGA3D_DEVCAP_DXFMT_R8G8_SNORM:
    case SVGA3D_DEVCAP_DXFMT_R16_FLOAT:
    case SVGA3D_DEVCAP_DXFMT_D16_UNORM:
    case SVGA3D_DEVCAP_DXFMT_A8_UNORM:
    case SVGA3D_DEVCAP_DXFMT_BC1_UNORM:
    case SVGA3D_DEVCAP_DXFMT_BC2_UNORM:
    case SVGA3D_DEVCAP_DXFMT_BC3_UNORM:
    case SVGA3D_DEVCAP_DXFMT_B5G6R5_UNORM:
    case SVGA3D_DEVCAP_DXFMT_B5G5R5A1_UNORM:
    case SVGA3D_DEVCAP_DXFMT_B8G8R8A8_UNORM:
    case SVGA3D_DEVCAP_DXFMT_B8G8R8X8_UNORM:
    case SVGA3D_DEVCAP_DXFMT_BC4_UNORM:
    case SVGA3D_DEVCAP_DXFMT_BC5_UNORM:
    {
        SVGA3dSurfaceFormat const enmFormat = vmsvgaDXDevCapDxfmt2Format(idx3dCaps);
        rc = vmsvgaDXCheckFormatSupport(pState, enmFormat, pu32Val);
        break;
    }

    case SVGA3D_DEVCAP_MAX:
    case SVGA3D_DEVCAP_INVALID:
        rc = VERR_NOT_SUPPORTED;
        break;
    }

    return rc;
}


/* Handle resize */
int vmsvga3dChangeMode(PVGASTATECC pThisCC)
{
    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    return VINF_SUCCESS;
}


int vmsvga3dSurfaceCopy(PVGASTATECC pThisCC, SVGA3dSurfaceImageId dest, SVGA3dSurfaceImageId src,
                        uint32_t cCopyBoxes, SVGA3dCopyBox *pBox)
{
    RT_NOREF(dest, src, cCopyBoxes, pBox);

    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    AssertFailed();
    return VERR_NOT_IMPLEMENTED;
}


/**
 * Create a new 3d context
 *
 * @returns VBox status code.
 * @param   pThisCC         The VGA/VMSVGA state for ring-3.
 * @param   cid             Context id
 */
int vmsvga3dContextDefine(PVGASTATECC pThisCC, uint32_t cid)
{
    RT_NOREF(cid);

    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    AssertFailed();
    return VERR_NOT_IMPLEMENTED;
}


/**
 * Destroy an existing 3d context
 *
 * @returns VBox status code.
 * @param   pThisCC         The VGA/VMSVGA state for ring-3.
 * @param   cid             Context id
 */
int vmsvga3dContextDestroy(PVGASTATECC pThisCC, uint32_t cid)
{
    RT_NOREF(cid);

    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    AssertFailed();
    return VINF_SUCCESS;
}


int vmsvga3dSetTransform(PVGASTATECC pThisCC, uint32_t cid, SVGA3dTransformType type, float matrix[16])
{
    RT_NOREF(cid, type, matrix);

    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    AssertFailed();
    return VINF_SUCCESS;
}


int vmsvga3dSetZRange(PVGASTATECC pThisCC, uint32_t cid, SVGA3dZRange zRange)
{
    RT_NOREF(cid, zRange);

    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    AssertFailed();
    return VINF_SUCCESS;
}


int vmsvga3dSetRenderState(PVGASTATECC pThisCC, uint32_t cid, uint32_t cRenderStates, SVGA3dRenderState *pRenderState)
{
    RT_NOREF(cid, cRenderStates, pRenderState);

    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    AssertFailed();
    return VINF_SUCCESS;
}


int vmsvga3dSetRenderTarget(PVGASTATECC pThisCC, uint32_t cid, SVGA3dRenderTargetType type, SVGA3dSurfaceImageId target)
{
    RT_NOREF(cid, type, target);

    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    AssertFailed();
    return VINF_SUCCESS;
}


int vmsvga3dSetTextureState(PVGASTATECC pThisCC, uint32_t cid, uint32_t cTextureStates, SVGA3dTextureState *pTextureState)
{
    RT_NOREF(cid, cTextureStates, pTextureState);

    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    AssertFailed();
    return VINF_SUCCESS;
}


int vmsvga3dSetMaterial(PVGASTATECC pThisCC, uint32_t cid, SVGA3dFace face, SVGA3dMaterial *pMaterial)
{
    RT_NOREF(cid, face, pMaterial);

    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    AssertFailed();
    return VINF_SUCCESS;
}


int vmsvga3dSetLightData(PVGASTATECC pThisCC, uint32_t cid, uint32_t index, SVGA3dLightData *pData)
{
    RT_NOREF(cid, index, pData);

    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    AssertFailed();
    return VINF_SUCCESS;
}


int vmsvga3dSetLightEnabled(PVGASTATECC pThisCC, uint32_t cid, uint32_t index, uint32_t enabled)
{
    RT_NOREF(cid, index, enabled);

    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    AssertFailed();
    return VINF_SUCCESS;
}


int vmsvga3dSetViewPort(PVGASTATECC pThisCC, uint32_t cid, SVGA3dRect *pRect)
{
    RT_NOREF(cid, pRect);

    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    AssertFailed();
    return VINF_SUCCESS;
}


int vmsvga3dSetClipPlane(PVGASTATECC pThisCC, uint32_t cid, uint32_t index, float plane[4])
{
    RT_NOREF(cid, index, plane);

    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    AssertFailed();
    return VINF_SUCCESS;
}


int vmsvga3dCommandClear(PVGASTATECC pThisCC, uint32_t cid, SVGA3dClearFlag clearFlag, uint32_t color, float depth,
                         uint32_t stencil, uint32_t cRects, SVGA3dRect *pRect)
{
    /* From SVGA3D_BeginClear comments:
     *
     *      Clear is not affected by clipping, depth test, or other
     *      render state which affects the fragment pipeline.
     *
     * Therefore this code must ignore the current scissor rect.
     */

    RT_NOREF(cid, clearFlag, color, depth, stencil, cRects, pRect);

    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    AssertFailed();
    return VINF_SUCCESS;
}


int vmsvga3dDrawPrimitives(PVGASTATECC pThisCC, uint32_t cid, uint32_t numVertexDecls, SVGA3dVertexDecl *pVertexDecl,
                           uint32_t numRanges, SVGA3dPrimitiveRange *pRange,
                           uint32_t cVertexDivisor, SVGA3dVertexDivisor *pVertexDivisor)
{
    RT_NOREF(cid, numVertexDecls, pVertexDecl, numRanges, pRange, cVertexDivisor, pVertexDivisor);

    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    AssertFailed();
    return VINF_SUCCESS;
}


int vmsvga3dSetScissorRect(PVGASTATECC pThisCC, uint32_t cid, SVGA3dRect *pRect)
{
    RT_NOREF(cid, pRect);

    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    AssertFailed();
    return VINF_SUCCESS;
}


int vmsvga3dGenerateMipmaps(PVGASTATECC pThisCC, uint32_t sid, SVGA3dTextureFilter filter)
{
    RT_NOREF(sid, filter);

    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    AssertFailed();
    return VINF_SUCCESS;
}


int vmsvga3dShaderDefine(PVGASTATECC pThisCC, uint32_t cid, uint32_t shid, SVGA3dShaderType type,
                         uint32_t cbData, uint32_t *pShaderData)
{
    RT_NOREF(cid, shid, type, cbData, pShaderData);

    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    AssertFailed();
    return VINF_SUCCESS;
}


int vmsvga3dShaderDestroy(PVGASTATECC pThisCC, uint32_t cid, uint32_t shid, SVGA3dShaderType type)
{
    RT_NOREF(cid, shid, type);

    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    AssertFailed();
    return VINF_SUCCESS;
}


int vmsvga3dShaderSet(PVGASTATECC pThisCC, PVMSVGA3DCONTEXT pContext, uint32_t cid, SVGA3dShaderType type, uint32_t shid)
{
    RT_NOREF(pContext, cid, type, shid);

    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    AssertFailed();
    return VINF_SUCCESS;
}


int vmsvga3dShaderSetConst(PVGASTATECC pThisCC, uint32_t cid, uint32_t reg, SVGA3dShaderType type,
                           SVGA3dShaderConstType ctype, uint32_t cRegisters, uint32_t *pValues)
{
    RT_NOREF(cid, reg, type, ctype, cRegisters, pValues);

    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    AssertFailed();
    return VINF_SUCCESS;
}


/**
 * Destroy backend specific surface bits (part of SVGA_3D_CMD_SURFACE_DESTROY).
 *
 * @param   pState              The VMSVGA3d state.
 * @param   pSurface            The surface being destroyed.
 */
void vmsvga3dBackSurfaceDestroy(PVMSVGA3DSTATE pState, PVMSVGA3DSURFACE pSurface)
{
    PVMSVGA3DBACKEND pBackend = pState->pBackend;
    AssertReturnVoid(pBackend);
    AssertReturnVoid(pBackend->pImmediateContext);

    /* The caller should not use the function for system memory surfaces. */
    PVMSVGA3DBACKENDSURFACE pBackendSurface = pSurface->pBackendSurface;
    AssertReturnVoid(pBackendSurface);
    pSurface->pBackendSurface = NULL;

    if (pBackendSurface->enmResType == VMSVGA3D_RESTYPE_SCREEN_TARGET)
    {
        D3D_RELEASE(pBackendSurface->u.ScreenTarget.pStagingTexture);
        D3D_RELEASE(pBackendSurface->u.ScreenTarget.pDynamicTexture);
        D3D_RELEASE(pBackendSurface->u.ScreenTarget.pTexture);
    }
    else
    {
        AssertFailed();
    }

    RTMemFree(pBackendSurface);
}


/**
 * Backend worker for implementing SVGA_3D_CMD_SURFACE_STRETCHBLT.
 *
 * @returns VBox status code.
 * @param   pThis               The VGA device instance.
 * @param   pState              The VMSVGA3d state.
 * @param   pDstSurface         The destination host surface.
 * @param   uDstFace            The destination face (valid).
 * @param   uDstMipmap          The destination mipmap level (valid).
 * @param   pDstBox             The destination box.
 * @param   pSrcSurface         The source host surface.
 * @param   uSrcFace            The destination face (valid).
 * @param   uSrcMipmap          The source mimap level (valid).
 * @param   pSrcBox             The source box.
 * @param   enmMode             The strecht blt mode .
 * @param   pContext            The VMSVGA3d context (already current for OGL).
 */
int vmsvga3dBackSurfaceStretchBlt(PVGASTATE pThis, PVMSVGA3DSTATE pState,
                                  PVMSVGA3DSURFACE pDstSurface, uint32_t uDstFace, uint32_t uDstMipmap, SVGA3dBox const *pDstBox,
                                  PVMSVGA3DSURFACE pSrcSurface, uint32_t uSrcFace, uint32_t uSrcMipmap, SVGA3dBox const *pSrcBox,
                                  SVGA3dStretchBltMode enmMode, PVMSVGA3DCONTEXT pContext)
{
    RT_NOREF(pThis, pState, pDstSurface, uDstFace, uDstMipmap, pDstBox,
             pSrcSurface, uSrcFace, uSrcMipmap, pSrcBox, enmMode, pContext);

    AssertFailed();
    return VINF_SUCCESS;
}


/**
 * Backend worker for implementing SVGA_3D_CMD_SURFACE_DMA that copies one box.
 *
 * @returns Failure status code or @a rc.
 * @param   pThis               The shared VGA/VMSVGA instance data.
 * @param   pThisCC             The VGA/VMSVGA state for ring-3.
 * @param   pState              The VMSVGA3d state.
 * @param   pSurface            The host surface.
 * @param   pMipLevel           Mipmap level. The caller knows it already.
 * @param   uHostFace           The host face (valid).
 * @param   uHostMipmap         The host mipmap level (valid).
 * @param   GuestPtr            The guest pointer.
 * @param   cbGuestPitch        The guest pitch.
 * @param   transfer            The transfer direction.
 * @param   pBox                The box to copy (clipped, valid, except for guest's srcx, srcy, srcz).
 * @param   pContext            The context (for OpenGL).
 * @param   rc                  The current rc for all boxes.
 * @param   iBox                The current box number (for Direct 3D).
 */
int vmsvga3dBackSurfaceDMACopyBox(PVGASTATE pThis, PVGASTATECC pThisCC, PVMSVGA3DSTATE pState, PVMSVGA3DSURFACE pSurface,
                                  PVMSVGA3DMIPMAPLEVEL pMipLevel, uint32_t uHostFace, uint32_t uHostMipmap,
                                  SVGAGuestPtr GuestPtr, uint32_t cbGuestPitch, SVGA3dTransferType transfer,
                                  SVGA3dCopyBox const *pBox, PVMSVGA3DCONTEXT pContext, int rc, int iBox)
{
    RT_NOREF(pState, pMipLevel, pContext, iBox);

    /* The called should not use the function for system memory surfaces. */
    PVMSVGA3DBACKENDSURFACE pBackendSurface = pSurface->pBackendSurface;
    AssertReturn(pBackendSurface, VERR_INVALID_PARAMETER);

    if (pBackendSurface->enmResType == VMSVGA3D_RESTYPE_SCREEN_TARGET)
    {
        /** @todo This is generic code and should be in DevVGA-SVGA3d.cpp for backends which support Map/Unmap. */
        AssertReturn(uHostFace == 0 && uHostMipmap == 0, VERR_INVALID_PARAMETER);
        AssertReturn(transfer == SVGA3D_WRITE_HOST_VRAM, VERR_NOT_IMPLEMENTED); /** @todo Implement */

        uint32_t const u32GuestBlockX = pBox->srcx / pSurface->cxBlock;
        uint32_t const u32GuestBlockY = pBox->srcy / pSurface->cyBlock;
        Assert(u32GuestBlockX * pSurface->cxBlock == pBox->srcx);
        Assert(u32GuestBlockY * pSurface->cyBlock == pBox->srcy);
        uint32_t const cBlocksX = (pBox->w + pSurface->cxBlock - 1) / pSurface->cxBlock;
        uint32_t const cBlocksY = (pBox->h + pSurface->cyBlock - 1) / pSurface->cyBlock;
        AssertMsgReturn(cBlocksX && cBlocksY, ("Empty box %dx%d\n", pBox->w, pBox->h), VERR_INTERNAL_ERROR);

        /* vmsvgaR3GmrTransfer verifies uGuestOffset.
         * srcx(u32GuestBlockX) and srcy(u32GuestBlockY) have been verified in vmsvga3dSurfaceDMA
         * to not cause 32 bit overflow when multiplied by cbBlock and cbGuestPitch.
         */
        uint64_t const uGuestOffset = u32GuestBlockX * pSurface->cbBlock + u32GuestBlockY * cbGuestPitch;
        AssertReturn(uGuestOffset < UINT32_MAX, VERR_INVALID_PARAMETER);

        SVGA3dSurfaceImageId image;
        image.sid = pSurface->id;
        image.face = uHostFace;
        image.mipmap = uHostMipmap;

        SVGA3dBox box;
        box.x = pBox->x;
        box.y = pBox->y;
        box.z = 0;
        box.w = pBox->w;
        box.h = pBox->h;
        box.d = 1;

        VMSVGA3D_MAPPED_SURFACE map;
        rc = vmsvga3dSurfaceMap(pThisCC, &image, &box, VMSVGA3D_SURFACE_MAP_WRITE_DISCARD, &map);
        if (RT_SUCCESS(rc))
        {
            /* Prepare parameters for vmsvgaR3GmrTransfer, which needs the host buffer address, size
             * and offset of the first scanline.
             */
            uint32_t const cbLockedBuf = map.cbRowPitch * cBlocksY;
            uint8_t *pu8LockedBuf = (uint8_t *)map.pvData;
            uint32_t const offLockedBuf = 0;

            rc = vmsvgaR3GmrTransfer(pThis,
                                     pThisCC,
                                     transfer,
                                     pu8LockedBuf,
                                     cbLockedBuf,
                                     offLockedBuf,
                                     map.cbRowPitch,
                                     GuestPtr,
                                     (uint32_t)uGuestOffset,
                                     cbGuestPitch,
                                     cBlocksX * pSurface->cbBlock,
                                     cBlocksY);
            AssertRC(rc);

            // Log4(("first line:\n%.*Rhxd\n", cBlocksX * pSurface->cbBlock, LockedRect.pBits));

            //vmsvga3dMapWriteBmpFile(&map, "Dynamic");

            vmsvga3dSurfaceUnmap(pThisCC, &image, &map, /* fWritten = */ true);
        }
#if 0
//ASMBreakpoint();
            rc = vmsvga3dSurfaceMap(pThisCC, &image, NULL, VMSVGA3D_SURFACE_MAP_READ, &map);
            if (RT_SUCCESS(rc))
            {
                vmsvga3dMapWriteBmpFile(&map, "Staging");

                vmsvga3dSurfaceUnmap(pThisCC, &image, &map, /* fWritten =  */ false);
            }
#endif
    }
    else
    {
        AssertMsgFailed(("Unsupported surface type %d\n", pBackendSurface->enmResType));
        rc = VERR_NOT_IMPLEMENTED;
    }

    return rc;
}


/**
 * Create D3D/OpenGL texture object for the specified surface.
 *
 * Surfaces are created when needed.
 *
 * @param   pState              The VMSVGA3d state.
 * @param   pContext            The context.
 * @param   idAssociatedContext Probably the same as pContext->id.
 * @param   pSurface            The surface to create the texture for.
 */
int vmsvga3dBackCreateTexture(PVMSVGA3DSTATE pState, PVMSVGA3DCONTEXT pContext, uint32_t idAssociatedContext,
                              PVMSVGA3DSURFACE pSurface)

{
    RT_NOREF(pState, pContext, idAssociatedContext, pSurface);

    AssertFailed();
    return VINF_SUCCESS;
}


int vmsvga3dOcclusionQueryCreate(PVMSVGA3DSTATE pState, PVMSVGA3DCONTEXT pContext)
{
    RT_NOREF(pState, pContext);
    AssertFailed();
    return VINF_SUCCESS;
}


int vmsvga3dOcclusionQueryBegin(PVMSVGA3DSTATE pState, PVMSVGA3DCONTEXT pContext)
{
    RT_NOREF(pState, pContext);
    AssertFailed();
    return VINF_SUCCESS;
}


int vmsvga3dOcclusionQueryEnd(PVMSVGA3DSTATE pState, PVMSVGA3DCONTEXT pContext)
{
    RT_NOREF(pState, pContext);
    AssertFailed();
    return VINF_SUCCESS;
}


int vmsvga3dOcclusionQueryGetData(PVMSVGA3DSTATE pState, PVMSVGA3DCONTEXT pContext, uint32_t *pu32Pixels)
{
    RT_NOREF(pState, pContext);
    *pu32Pixels = 0;
    AssertFailed();
    return VINF_SUCCESS;
}


int vmsvga3dOcclusionQueryDelete(PVMSVGA3DSTATE pState, PVMSVGA3DCONTEXT pContext)
{
    RT_NOREF(pState, pContext);
    AssertFailed();
    return VINF_SUCCESS;
}


static DECLCALLBACK(void) vmsvga3dDXDefineContext(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXDestroyContext(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXBindContext(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXReadbackContext(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXInvalidateContext(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXSetSingleConstantBuffer(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXSetShaderResources(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXSetShader(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXSetSamplers(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXDraw(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXDrawIndexed(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXDrawInstanced(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXDrawIndexedInstanced(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXDrawAuto(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXSetInputLayout(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXSetVertexBuffers(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXSetIndexBuffer(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXSetTopology(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXSetRenderTargets(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXSetBlendState(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXSetDepthStencilState(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXSetRasterizerState(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXDefineQuery(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXDestroyQuery(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXBindQuery(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXSetQueryOffset(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXBeginQuery(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXEndQuery(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXReadbackQuery(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXSetPredication(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXSetSOTargets(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXSetViewports(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXSetScissorRects(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXClearRenderTargetView(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXClearDepthStencilView(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXPredCopyRegion(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXPredCopy(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXStretchBlt(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXGenMips(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXUpdateSubResource(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXReadbackSubResource(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXInvalidateSubResource(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXDefineShaderResourceView(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXDestroyShaderResourceView(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXDefineRenderTargetView(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXDestroyRenderTargetView(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXDefineDepthStencilView(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXDestroyDepthStencilView(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXDefineElementLayout(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXDestroyElementLayout(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXDefineBlendState(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXDestroyBlendState(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXDefineDepthStencilState(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXDestroyDepthStencilState(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXDefineRasterizerState(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXDestroyRasterizerState(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXDefineSamplerState(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXDestroySamplerState(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXDefineShader(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXDestroyShader(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXBindShader(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXDefineStreamOutput(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXDestroyStreamOutput(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXSetStreamOutput(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXSetCOTable(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXReadbackCOTable(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXBufferCopy(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXTransferFromBuffer(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXSurfaceCopyAndReadback(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXMoveQuery(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXBindAllQuery(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXReadbackAllQuery(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXPredTransferFromBuffer(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXMobFence64(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXBindAllShader(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXHint(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXBufferUpdate(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXSetVSConstantBufferOffset(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXSetPSConstantBufferOffset(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXSetGSConstantBufferOffset(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


static DECLCALLBACK(void) vmsvga3dDXCondBindAllShader(PVMSVGA3DSTATE p3dState)
{
    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;

    RT_NOREF(pBackend);
    AssertFailed(); /** @todo Implement */
}


int vmsvga3dQueryInterface(PVGASTATECC pThisCC, char const *pszInterfaceName, void *pvInterfaceFuncs, size_t cbInterfaceFuncs)
{
    PVMSVGA3DSTATE p3dState = pThisCC->svga.p3dState;
    AssertReturn(p3dState, VERR_NOT_SUPPORTED);

    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;
    AssertReturn(pBackend, VERR_NOT_SUPPORTED);

    if (pvInterfaceFuncs)
        RT_BZERO(pvInterfaceFuncs, cbInterfaceFuncs);

    int rc = VINF_SUCCESS;
    if (RTStrCmp(pszInterfaceName, VMSVGA3D_BACKEND_INTERFACE_NAME_DX) == 0)
    {
        if (cbInterfaceFuncs == sizeof(VMSVGA3DBACKENDFUNCSDX))
        {
            if (pvInterfaceFuncs)
            {
                VMSVGA3DBACKENDFUNCSDX *p = (VMSVGA3DBACKENDFUNCSDX *)pvInterfaceFuncs;
                p->pfnDXDefineContext             = vmsvga3dDXDefineContext;
                p->pfnDXDestroyContext            = vmsvga3dDXDestroyContext;
                p->pfnDXBindContext               = vmsvga3dDXBindContext;
                p->pfnDXReadbackContext           = vmsvga3dDXReadbackContext;
                p->pfnDXInvalidateContext         = vmsvga3dDXInvalidateContext;
                p->pfnDXSetSingleConstantBuffer   = vmsvga3dDXSetSingleConstantBuffer;
                p->pfnDXSetShaderResources        = vmsvga3dDXSetShaderResources;
                p->pfnDXSetShader                 = vmsvga3dDXSetShader;
                p->pfnDXSetSamplers               = vmsvga3dDXSetSamplers;
                p->pfnDXDraw                      = vmsvga3dDXDraw;
                p->pfnDXDrawIndexed               = vmsvga3dDXDrawIndexed;
                p->pfnDXDrawInstanced             = vmsvga3dDXDrawInstanced;
                p->pfnDXDrawIndexedInstanced      = vmsvga3dDXDrawIndexedInstanced;
                p->pfnDXDrawAuto                  = vmsvga3dDXDrawAuto;
                p->pfnDXSetInputLayout            = vmsvga3dDXSetInputLayout;
                p->pfnDXSetVertexBuffers          = vmsvga3dDXSetVertexBuffers;
                p->pfnDXSetIndexBuffer            = vmsvga3dDXSetIndexBuffer;
                p->pfnDXSetTopology               = vmsvga3dDXSetTopology;
                p->pfnDXSetRenderTargets          = vmsvga3dDXSetRenderTargets;
                p->pfnDXSetBlendState             = vmsvga3dDXSetBlendState;
                p->pfnDXSetDepthStencilState      = vmsvga3dDXSetDepthStencilState;
                p->pfnDXSetRasterizerState        = vmsvga3dDXSetRasterizerState;
                p->pfnDXDefineQuery               = vmsvga3dDXDefineQuery;
                p->pfnDXDestroyQuery              = vmsvga3dDXDestroyQuery;
                p->pfnDXBindQuery                 = vmsvga3dDXBindQuery;
                p->pfnDXSetQueryOffset            = vmsvga3dDXSetQueryOffset;
                p->pfnDXBeginQuery                = vmsvga3dDXBeginQuery;
                p->pfnDXEndQuery                  = vmsvga3dDXEndQuery;
                p->pfnDXReadbackQuery             = vmsvga3dDXReadbackQuery;
                p->pfnDXSetPredication            = vmsvga3dDXSetPredication;
                p->pfnDXSetSOTargets              = vmsvga3dDXSetSOTargets;
                p->pfnDXSetViewports              = vmsvga3dDXSetViewports;
                p->pfnDXSetScissorRects           = vmsvga3dDXSetScissorRects;
                p->pfnDXClearRenderTargetView     = vmsvga3dDXClearRenderTargetView;
                p->pfnDXClearDepthStencilView     = vmsvga3dDXClearDepthStencilView;
                p->pfnDXPredCopyRegion            = vmsvga3dDXPredCopyRegion;
                p->pfnDXPredCopy                  = vmsvga3dDXPredCopy;
                p->pfnDXStretchBlt                = vmsvga3dDXStretchBlt;
                p->pfnDXGenMips                   = vmsvga3dDXGenMips;
                p->pfnDXUpdateSubResource         = vmsvga3dDXUpdateSubResource;
                p->pfnDXReadbackSubResource       = vmsvga3dDXReadbackSubResource;
                p->pfnDXInvalidateSubResource     = vmsvga3dDXInvalidateSubResource;
                p->pfnDXDefineShaderResourceView  = vmsvga3dDXDefineShaderResourceView;
                p->pfnDXDestroyShaderResourceView = vmsvga3dDXDestroyShaderResourceView;
                p->pfnDXDefineRenderTargetView    = vmsvga3dDXDefineRenderTargetView;
                p->pfnDXDestroyRenderTargetView   = vmsvga3dDXDestroyRenderTargetView;
                p->pfnDXDefineDepthStencilView    = vmsvga3dDXDefineDepthStencilView;
                p->pfnDXDestroyDepthStencilView   = vmsvga3dDXDestroyDepthStencilView;
                p->pfnDXDefineElementLayout       = vmsvga3dDXDefineElementLayout;
                p->pfnDXDestroyElementLayout      = vmsvga3dDXDestroyElementLayout;
                p->pfnDXDefineBlendState          = vmsvga3dDXDefineBlendState;
                p->pfnDXDestroyBlendState         = vmsvga3dDXDestroyBlendState;
                p->pfnDXDefineDepthStencilState   = vmsvga3dDXDefineDepthStencilState;
                p->pfnDXDestroyDepthStencilState  = vmsvga3dDXDestroyDepthStencilState;
                p->pfnDXDefineRasterizerState     = vmsvga3dDXDefineRasterizerState;
                p->pfnDXDestroyRasterizerState    = vmsvga3dDXDestroyRasterizerState;
                p->pfnDXDefineSamplerState        = vmsvga3dDXDefineSamplerState;
                p->pfnDXDestroySamplerState       = vmsvga3dDXDestroySamplerState;
                p->pfnDXDefineShader              = vmsvga3dDXDefineShader;
                p->pfnDXDestroyShader             = vmsvga3dDXDestroyShader;
                p->pfnDXBindShader                = vmsvga3dDXBindShader;
                p->pfnDXDefineStreamOutput        = vmsvga3dDXDefineStreamOutput;
                p->pfnDXDestroyStreamOutput       = vmsvga3dDXDestroyStreamOutput;
                p->pfnDXSetStreamOutput           = vmsvga3dDXSetStreamOutput;
                p->pfnDXSetCOTable                = vmsvga3dDXSetCOTable;
                p->pfnDXReadbackCOTable           = vmsvga3dDXReadbackCOTable;
                p->pfnDXBufferCopy                = vmsvga3dDXBufferCopy;
                p->pfnDXTransferFromBuffer        = vmsvga3dDXTransferFromBuffer;
                p->pfnDXSurfaceCopyAndReadback    = vmsvga3dDXSurfaceCopyAndReadback;
                p->pfnDXMoveQuery                 = vmsvga3dDXMoveQuery;
                p->pfnDXBindAllQuery              = vmsvga3dDXBindAllQuery;
                p->pfnDXReadbackAllQuery          = vmsvga3dDXReadbackAllQuery;
                p->pfnDXPredTransferFromBuffer    = vmsvga3dDXPredTransferFromBuffer;
                p->pfnDXMobFence64                = vmsvga3dDXMobFence64;
                p->pfnDXBindAllShader             = vmsvga3dDXBindAllShader;
                p->pfnDXHint                      = vmsvga3dDXHint;
                p->pfnDXBufferUpdate              = vmsvga3dDXBufferUpdate;
                p->pfnDXSetVSConstantBufferOffset = vmsvga3dDXSetVSConstantBufferOffset;
                p->pfnDXSetPSConstantBufferOffset = vmsvga3dDXSetPSConstantBufferOffset;
                p->pfnDXSetGSConstantBufferOffset = vmsvga3dDXSetGSConstantBufferOffset;
                p->pfnDXCondBindAllShader         = vmsvga3dDXCondBindAllShader;
            }
        }
        else
        {
            AssertFailed();
            rc = VERR_INVALID_PARAMETER;
        }
    }
    else if (RTStrCmp(pszInterfaceName, VMSVGA3D_BACKEND_INTERFACE_NAME_MAP) == 0)
    {
        if (cbInterfaceFuncs == sizeof(VMSVGA3DBACKENDFUNCSMAP))
        {
            if (pvInterfaceFuncs)
            {
                VMSVGA3DBACKENDFUNCSMAP *p = (VMSVGA3DBACKENDFUNCSMAP *)pvInterfaceFuncs;
                p->pfnSurfaceMap   = vmsvga3dSurfaceMap;
                p->pfnSurfaceUnmap = vmsvga3dSurfaceUnmap;
            }
        }
        else
        {
            AssertFailed();
            rc = VERR_INVALID_PARAMETER;
        }
    }
    else if (RTStrCmp(pszInterfaceName, VMSVGA3D_BACKEND_INTERFACE_NAME_GBO) == 0)
    {
        if (cbInterfaceFuncs == sizeof(VMSVGA3DBACKENDFUNCSGBO))
        {
            if (pvInterfaceFuncs)
            {
                VMSVGA3DBACKENDFUNCSGBO *p = (VMSVGA3DBACKENDFUNCSGBO *)pvInterfaceFuncs;
                p->pfnScreenTargetBind   = vmsvga3dScreenTargetBind;
                p->pfnScreenTargetUpdate = vmsvga3dScreenTargetUpdate;
            }
        }
        else
        {
            AssertFailed();
            rc = VERR_INVALID_PARAMETER;
        }
    }
    else
        rc = VERR_NOT_IMPLEMENTED;
    return rc;
}
