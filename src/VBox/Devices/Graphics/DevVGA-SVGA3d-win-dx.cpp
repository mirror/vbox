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
#include "DevVGA-SVGA3d-dx-shader.h"

#include <d3d11.h>

#define DX_RELEASE_ARRAY(a_Count, a_papArray) do { \
    for (uint32_t i = 0; i < (a_Count); ++i) \
        D3D_RELEASE((a_papArray)[i]); \
} while (0)

typedef struct DXDEVICE
{
    ID3D11Device               *pDevice;               /* Device. */
    ID3D11DeviceContext        *pImmediateContext;     /* Corresponding context. */
    IDXGIFactory               *pDxgiFactory;          /* DXGI Factory. */
    D3D_FEATURE_LEVEL           FeatureLevel;
} DXDEVICE;

/* What kind of resource has been created for the VMSVGA3D surface. */
typedef enum VMSVGA3DBACKRESTYPE
{
    VMSVGA3D_RESTYPE_NONE           = 0,
    VMSVGA3D_RESTYPE_SCREEN_TARGET  = 1,
    VMSVGA3D_RESTYPE_SURFACE        = 2,
    VMSVGA3D_RESTYPE_TEXTURE        = 3,
    VMSVGA3D_RESTYPE_CUBE_TEXTURE   = 4,
    VMSVGA3D_RESTYPE_VOLUME_TEXTURE = 5,
    VMSVGA3D_RESTYPE_BUFFER         = 6,
} VMSVGA3DBACKRESTYPE;

typedef struct VMSVGA3DBACKENDSURFACE
{
    VMSVGA3DBACKRESTYPE enmResType;
    DXGI_FORMAT enmDxgiFormat;
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
            ID3D11Texture2D    *pTexture;         /* The texture for the screen content. */
            ID3D11Texture2D    *pDynamicTexture;  /* For screen updates from memory. */ /** @todo One for all screens. */
            ID3D11Texture2D    *pStagingTexture;  /* For Reading the screen content. */ /** @todo One for all screens. */
        } Texture;
        struct
        {
             ID3D11Buffer      *pBuffer;
        } Buffer;
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


typedef struct DXELEMENTLAYOUT
{
    ID3D11InputLayout          *pElementLayout;
    uint32_t                    cElementDesc;
    D3D11_INPUT_ELEMENT_DESC    aElementDesc[32];
} DXELEMENTLAYOUT;

typedef struct DXSHADER
{
    SVGA3dShaderType enmShaderType;
    union
    {
        ID3D11DeviceChild    *pShader;            /* All. */
        ID3D11VertexShader   *pVertexShader;      /* SVGA3D_SHADERTYPE_VS */
        ID3D11PixelShader    *pPixelShader;       /* SVGA3D_SHADERTYPE_PS */
        ID3D11GeometryShader *pGeometryShader;    /* SVGA3D_SHADERTYPE_GS */
        ID3D11HullShader     *pHullShader;        /* SVGA3D_SHADERTYPE_HS */
        ID3D11DomainShader   *pDomainShader;      /* SVGA3D_SHADERTYPE_DS */
        ID3D11ComputeShader  *pComputeShader;     /* SVGA3D_SHADERTYPE_CS */
    };
    void *pvDXBC;
    uint32_t cbDXBC;
} DXSHADER;

typedef struct VMSVGA3DBACKENDDXCONTEXT
{
    DXDEVICE                    device;                /* Device for the this context operation. */

    /* Arrays for Context-Object Tables. Number of entries depends on COTable size. */
    uint32_t                   cBlendState;            /* Number of entries in the papBlendState array. */
    uint32_t                   cDepthStencilState;     /* papDepthStencilState */
    uint32_t                   cSamplerState;          /* papSamplerState */
    uint32_t                   cRasterizerState;       /* papRasterizerState */
    uint32_t                   cElementLayout;         /* papElementLayout */
    uint32_t                   cRenderTargetView;      /* papRenderTargetView */
    uint32_t                   cDepthStencilView;      /* papDepthStencilView */
    uint32_t                   cShaderResourceView;    /* papShaderResourceView */
    uint32_t                   cQuery;                 /* papQuery */
    uint32_t                   cShader;                /* papShader */
    ID3D11BlendState         **papBlendState;
    ID3D11DepthStencilState  **papDepthStencilState;
    ID3D11SamplerState       **papSamplerState;
    ID3D11RasterizerState    **papRasterizerState;
    DXELEMENTLAYOUT           *paElementLayout;
    ID3D11RenderTargetView   **papRenderTargetView;
    ID3D11DepthStencilView   **papDepthStencilView;
    ID3D11ShaderResourceView **papShaderResourceView;
    ID3D11Query              **papQuery;
    DXSHADER                 *paShader;
} VMSVGA3DBACKENDDXCONTEXT;

typedef HRESULT FN_D3D_DISASSEMBLE(LPCVOID pSrcData, SIZE_T SrcDataSize, UINT Flags, LPCSTR szComments, ID3D10Blob **ppDisassembly);
typedef FN_D3D_DISASSEMBLE *PFN_D3D_DISASSEMBLE;

typedef struct VMSVGA3DBACKEND
{
    RTLDRMOD                    hD3D11;
    PFN_D3D11_CREATE_DEVICE     pfnD3D11CreateDevice;

    RTLDRMOD                    hD3DCompiler;
    PFN_D3D_DISASSEMBLE         pfnD3DDisassemble;

    ID3D11Device               *pDevice;               /* Device for the VMSVGA3D context independent operation. */
    ID3D11DeviceContext        *pImmediateContext;     /* Corresponding context. */
    IDXGIFactory               *pDxgiFactory;          /* DXGI Factory. */
    D3D_FEATURE_LEVEL           FeatureLevel;
} VMSVGA3DBACKEND;


static DXGI_FORMAT vmsvgaDXScreenTargetFormat2Dxgi(SVGA3dSurfaceFormat format)
{
    switch (format)
    {
        /** @todo More formats. */
        case SVGA3D_X8R8G8B8:                   return DXGI_FORMAT_B8G8R8A8_UNORM;
        case SVGA3D_A8R8G8B8:                   return DXGI_FORMAT_B8G8R8A8_UNORM;
        case SVGA3D_B8G8R8X8_UNORM:             return DXGI_FORMAT_B8G8R8A8_UNORM;
        default:
            AssertFailed();
            break;
    }
    return DXGI_FORMAT_UNKNOWN;
}


static DXGI_FORMAT vmsvgaDXSurfaceFormat2Dxgi(SVGA3dSurfaceFormat format)
{
    /* Ensure that correct headers are used.
     * SVGA3D_AYUV was equal to 45, then replaced with SVGA3D_FORMAT_DEAD2 = 45, and redefined as SVGA3D_AYUV = 152.
     */
    AssertCompile(SVGA3D_AYUV == 152);

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
        case SVGA3D_FORMAT_DEAD2:               break; /* Old SVGA3D_AYUV */
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

        case SVGA3D_B4G4R4A4_UNORM:             return DXGI_FORMAT_;
        case SVGA3D_BC6H_TYPELESS:              return DXGI_FORMAT_;
        case SVGA3D_BC6H_UF16:                  return DXGI_FORMAT_;
        case SVGA3D_BC6H_SF16:                  return DXGI_FORMAT_;
        case SVGA3D_BC7_TYPELESS:               return DXGI_FORMAT_;
        case SVGA3D_BC7_UNORM:                  return DXGI_FORMAT_;
        case SVGA3D_BC7_UNORM_SRGB:             return DXGI_FORMAT_;
        case SVGA3D_AYUV:                       return DXGI_FORMAT_;

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
        case SVGA3D_DEVCAP_DEAD10:                              return SVGA3D_FORMAT_DEAD2; /* SVGA3D_DEVCAP_SURFACEFMT_AYUV -> SVGA3D_AYUV */
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
        case SVGA3D_DEVCAP_DXFMT_FORMAT_DEAD2:                  return SVGA3D_FORMAT_DEAD2; /* SVGA3D_DEVCAP_DXFMT_AYUV -> SVGA3D_AYUV */
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
        case SVGA3D_DEVCAP_DXFMT_R32_FLOAT_X8X24:               return SVGA3D_R32_FLOAT_X8X24;
        case SVGA3D_DEVCAP_DXFMT_X32_G8X24_UINT:                return SVGA3D_X32_G8X24_UINT;
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
        case SVGA3D_DEVCAP_DXFMT_R24_UNORM_X8:                  return SVGA3D_R24_UNORM_X8;
        case SVGA3D_DEVCAP_DXFMT_X24_G8_UINT:                   return SVGA3D_X24_G8_UINT;
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
        case SVGA3D_DEVCAP_DXFMT_BC6H_TYPELESS:                 return SVGA3D_BC6H_TYPELESS;
        case SVGA3D_DEVCAP_DXFMT_BC6H_UF16:                     return SVGA3D_BC6H_UF16;
        case SVGA3D_DEVCAP_DXFMT_BC6H_SF16:                     return SVGA3D_BC6H_SF16;
        case SVGA3D_DEVCAP_DXFMT_BC7_TYPELESS:                  return SVGA3D_BC7_TYPELESS;
        case SVGA3D_DEVCAP_DXFMT_BC7_UNORM:                     return SVGA3D_BC7_UNORM;
        case SVGA3D_DEVCAP_DXFMT_BC7_UNORM_SRGB:                return SVGA3D_BC7_UNORM_SRGB;
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
                *pu32DevCap |= SVGA3D_DXFMT_MULTISAMPLE;
        }
        else
            AssertFailedStmt(rc = VERR_NOT_SUPPORTED);
    }
    else
        rc = VERR_NOT_SUPPORTED;
    return rc;
}


static int dxDeviceCreate(PVMSVGA3DBACKEND pBackend, DXDEVICE *pDevice)
{
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
                                                &pDevice->pDevice,
                                                &pDevice->FeatureLevel,
                                                &pDevice->pImmediateContext);
    if (SUCCEEDED(hr))
    {
        LogRel(("VMSVGA: Feature level %#x\n", pDevice->FeatureLevel));

        IDXGIDevice *pDxgiDevice = 0;
        hr = pBackend->pDevice->QueryInterface(__uuidof(IDXGIDevice), (void**)&pDxgiDevice);
        if (SUCCEEDED(hr))
        {
            IDXGIAdapter *pDxgiAdapter = 0;
            hr = pDxgiDevice->GetParent(__uuidof(IDXGIAdapter), (void**)&pDxgiAdapter);
            if (SUCCEEDED(hr))
            {
                hr = pDxgiAdapter->GetParent(__uuidof(IDXGIFactory), (void**)&pDevice->pDxgiFactory);
                D3D_RELEASE(pDxgiAdapter);
            }

            D3D_RELEASE(pDxgiDevice);
        }
    }

    if (FAILED(hr))
        rc = VERR_NOT_SUPPORTED;

    return rc;
}


static void dxDeviceDestroy(PVMSVGA3DBACKEND pBackend, DXDEVICE *pDevice)
{
    RT_NOREF(pBackend);
    D3D_RELEASE(pDevice->pDevice);
    D3D_RELEASE(pDevice->pImmediateContext);
    D3D_RELEASE(pDevice->pDxgiFactory);
    RT_ZERO(*pDevice);
}


/** @todo AssertCompile for types like D3D11_COMPARISON_FUNC and SVGA3dComparisonFunc */
static HRESULT dxBlendStateCreate(DXDEVICE *pDevice, SVGACOTableDXBlendStateEntry const *pEntry, ID3D11BlendState **pp)
{
    D3D11_BLEND_DESC BlendDesc;
    BlendDesc.AlphaToCoverageEnable = RT_BOOL(pEntry->alphaToCoverageEnable);
    BlendDesc.IndependentBlendEnable = RT_BOOL(pEntry->independentBlendEnable);
    for (int i = 0; i < SVGA3D_MAX_RENDER_TARGETS; ++i)
    {
        BlendDesc.RenderTarget[i].BlendEnable           = RT_BOOL(pEntry->perRT[i].blendEnable);
        BlendDesc.RenderTarget[i].SrcBlend              = (D3D11_BLEND)pEntry->perRT[i].srcBlend;
        BlendDesc.RenderTarget[i].DestBlend             = (D3D11_BLEND)pEntry->perRT[i].destBlend;
        BlendDesc.RenderTarget[i].BlendOp               = (D3D11_BLEND_OP)pEntry->perRT[i].blendOp;
        BlendDesc.RenderTarget[i].SrcBlendAlpha         = (D3D11_BLEND)pEntry->perRT[i].srcBlendAlpha;
        BlendDesc.RenderTarget[i].DestBlendAlpha        = (D3D11_BLEND)pEntry->perRT[i].destBlendAlpha;
        BlendDesc.RenderTarget[i].BlendOpAlpha          = (D3D11_BLEND_OP)pEntry->perRT[i].blendOpAlpha;
        BlendDesc.RenderTarget[i].RenderTargetWriteMask = pEntry->perRT[i].renderTargetWriteMask;
        /** @todo logicOpEnable and logicOp */
    }

    HRESULT hr = pDevice->pDevice->CreateBlendState(&BlendDesc, pp);
    Assert(SUCCEEDED(hr));
    return hr;
}


static HRESULT dxDepthStencilStateCreate(DXDEVICE *pDevice, SVGACOTableDXDepthStencilEntry const *pEntry, ID3D11DepthStencilState **pp)
{
    D3D11_DEPTH_STENCIL_DESC desc;
    desc.DepthEnable                  = pEntry->depthEnable;
    desc.DepthWriteMask               = (D3D11_DEPTH_WRITE_MASK)pEntry->depthWriteMask;
    desc.DepthFunc                    = (D3D11_COMPARISON_FUNC)pEntry->depthFunc;
    desc.StencilEnable                = pEntry->stencilEnable;
    desc.StencilReadMask              = pEntry->stencilReadMask;
    desc.StencilWriteMask             = pEntry->stencilWriteMask;
    desc.FrontFace.StencilFailOp      = (D3D11_STENCIL_OP)pEntry->frontStencilFailOp;
    desc.FrontFace.StencilDepthFailOp = (D3D11_STENCIL_OP)pEntry->frontStencilDepthFailOp;
    desc.FrontFace.StencilPassOp      = (D3D11_STENCIL_OP)pEntry->frontStencilPassOp;
    desc.FrontFace.StencilFunc        = (D3D11_COMPARISON_FUNC)pEntry->frontStencilFunc;
    desc.BackFace.StencilFailOp       = (D3D11_STENCIL_OP)pEntry->backStencilFailOp;
    desc.BackFace.StencilDepthFailOp  = (D3D11_STENCIL_OP)pEntry->backStencilDepthFailOp;
    desc.BackFace.StencilPassOp       = (D3D11_STENCIL_OP)pEntry->backStencilPassOp;
    desc.BackFace.StencilFunc         = (D3D11_COMPARISON_FUNC)pEntry->backStencilFunc;
    /** @todo frontEnable, backEnable */

    HRESULT hr = pDevice->pDevice->CreateDepthStencilState(&desc, pp);
    Assert(SUCCEEDED(hr));
    return hr;
}


static HRESULT dxSamplerStateCreate(DXDEVICE *pDevice, SVGACOTableDXSamplerEntry const *pEntry, ID3D11SamplerState **pp)
{
    D3D11_SAMPLER_DESC desc;
    desc.Filter         = (D3D11_FILTER)pEntry->filter;
    desc.AddressU       = (D3D11_TEXTURE_ADDRESS_MODE)pEntry->addressU;
    desc.AddressV       = (D3D11_TEXTURE_ADDRESS_MODE)pEntry->addressV;
    desc.AddressW       = (D3D11_TEXTURE_ADDRESS_MODE)pEntry->addressW;
    desc.MipLODBias     = pEntry->mipLODBias;
    desc.MaxAnisotropy  = RT_CLAMP(pEntry->maxAnisotropy, 1, 16); /* "Valid values are between 1 and 16" */
    desc.ComparisonFunc = (D3D11_COMPARISON_FUNC)pEntry->comparisonFunc;
    desc.BorderColor[0] = pEntry->borderColor.value[0];
    desc.BorderColor[1] = pEntry->borderColor.value[1];
    desc.BorderColor[2] = pEntry->borderColor.value[2];
    desc.BorderColor[3] = pEntry->borderColor.value[3];
    desc.MinLOD         = pEntry->minLOD;
    desc.MaxLOD         = pEntry->maxLOD;

    HRESULT hr = pDevice->pDevice->CreateSamplerState(&desc, pp);
    Assert(SUCCEEDED(hr));
    return hr;
}


static HRESULT dxRasterizerStateCreate(DXDEVICE *pDevice, SVGACOTableDXRasterizerStateEntry const *pEntry, ID3D11RasterizerState **pp)
{
    D3D11_RASTERIZER_DESC desc;
    desc.FillMode              = (D3D11_FILL_MODE)pEntry->fillMode;
    desc.CullMode              = (D3D11_CULL_MODE)pEntry->cullMode;
    desc.FrontCounterClockwise = pEntry->frontCounterClockwise;
    /** provokingVertexLast */
    desc.DepthBias             = pEntry->depthBias;
    desc.DepthBiasClamp        = pEntry->depthBiasClamp;
    desc.SlopeScaledDepthBias  = pEntry->slopeScaledDepthBias;
    desc.DepthClipEnable       = pEntry->depthClipEnable;
    desc.ScissorEnable         = pEntry->scissorEnable;
    desc.MultisampleEnable     = pEntry->multisampleEnable;
    desc.AntialiasedLineEnable = pEntry->antialiasedLineEnable;
    /** @todo lineWidth lineStippleEnable lineStippleFactor lineStipplePattern forcedSampleCount */

    HRESULT hr = pDevice->pDevice->CreateRasterizerState(&desc, pp);
    Assert(SUCCEEDED(hr));
    return hr;
}


static HRESULT dxRenderTargetViewCreate(DXDEVICE *pDevice, SVGACOTableDXRTViewEntry const *pEntry, VMSVGA3DBACKENDSURFACE *pBackendSurface, ID3D11RenderTargetView **pp)
{
    ID3D11Resource *pResource = pBackendSurface->u.Texture.pTexture;

    D3D11_RENDER_TARGET_VIEW_DESC desc;
    RT_ZERO(desc);
    desc.Format = vmsvgaDXSurfaceFormat2Dxgi(pEntry->format);
    AssertReturn(desc.Format != DXGI_FORMAT_UNKNOWN, E_FAIL);
    switch (pEntry->resourceDimension)
    {
        case SVGA3D_RESOURCE_BUFFER:
            desc.ViewDimension = D3D11_RTV_DIMENSION_BUFFER;
            desc.Buffer.FirstElement = pEntry->desc.buffer.firstElement;
            desc.Buffer.NumElements = pEntry->desc.buffer.numElements;
            break;
        case SVGA3D_RESOURCE_TEXTURE1D:
            if (pEntry->desc.tex.arraySize <= 1)
            {
                desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE1D;
                desc.Texture1D.MipSlice = pEntry->desc.tex.mipSlice;
            }
            else
            {
                desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE1DARRAY;
                desc.Texture1DArray.MipSlice = pEntry->desc.tex.mipSlice;
                desc.Texture1DArray.FirstArraySlice = pEntry->desc.tex.firstArraySlice;
                desc.Texture1DArray.ArraySize = pEntry->desc.tex.arraySize;
            }
            break;
        case SVGA3D_RESOURCE_TEXTURE2D:
            if (pEntry->desc.tex.arraySize <= 1)
            {
                desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
                desc.Texture2D.MipSlice = pEntry->desc.tex.mipSlice;
            }
            else
            {
                desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
                desc.Texture2DArray.MipSlice = pEntry->desc.tex.mipSlice;
                desc.Texture2DArray.FirstArraySlice = pEntry->desc.tex.firstArraySlice;
                desc.Texture2DArray.ArraySize = pEntry->desc.tex.arraySize;
            }
            break;
        case SVGA3D_RESOURCE_TEXTURE3D:
            desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE3D;
            desc.Texture3D.MipSlice = pEntry->desc.tex3D.mipSlice;
            desc.Texture3D.FirstWSlice = pEntry->desc.tex3D.firstW;
            desc.Texture3D.WSize = pEntry->desc.tex3D.wSize;
            break;
        case SVGA3D_RESOURCE_TEXTURECUBE:
            AssertFailed(); /** @todo test. Probably not applicable to a render target view. */
            desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
            desc.Texture2DArray.MipSlice = pEntry->desc.tex.mipSlice;
            desc.Texture2DArray.FirstArraySlice = 0;
            desc.Texture2DArray.ArraySize = 6;
            break;
        case SVGA3D_RESOURCE_BUFFEREX:
            AssertFailed(); /** @todo test. Probably not applicable to a render target view. */
            desc.ViewDimension = D3D11_RTV_DIMENSION_BUFFER;
            desc.Buffer.FirstElement = pEntry->desc.buffer.firstElement;
            desc.Buffer.NumElements = pEntry->desc.buffer.numElements;
            break;
        default:
            ASSERT_GUEST_FAILED_RETURN(E_INVALIDARG);
    }

    HRESULT hr = pDevice->pDevice->CreateRenderTargetView(pResource, &desc, pp);
    Assert(SUCCEEDED(hr));
    return hr;
}


static HRESULT dxShaderResourceViewCreate(DXDEVICE *pDevice, SVGACOTableDXSRViewEntry const *pEntry, VMSVGA3DBACKENDSURFACE *pBackendSurface, ID3D11ShaderResourceView **pp)
{
    ID3D11Resource *pResource = pBackendSurface->u.Texture.pTexture;

    D3D11_SHADER_RESOURCE_VIEW_DESC desc;
    RT_ZERO(desc);
    desc.Format = vmsvgaDXSurfaceFormat2Dxgi(pEntry->format);
    AssertReturn(desc.Format != DXGI_FORMAT_UNKNOWN, E_FAIL);

    switch (pEntry->resourceDimension)
    {
        case SVGA3D_RESOURCE_BUFFER:
            desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
            desc.Buffer.FirstElement = pEntry->desc.buffer.firstElement;
            desc.Buffer.NumElements = pEntry->desc.buffer.numElements;
            break;
        case SVGA3D_RESOURCE_TEXTURE1D:
            if (pEntry->desc.tex.arraySize <= 1)
            {
                desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1D;
                desc.Texture1D.MostDetailedMip = pEntry->desc.tex.mostDetailedMip;
                desc.Texture1D.MipLevels = pEntry->desc.tex.mipLevels;
            }
            else
            {
                desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1DARRAY;
                desc.Texture1DArray.MostDetailedMip = pEntry->desc.tex.mostDetailedMip;
                desc.Texture1DArray.MipLevels = pEntry->desc.tex.mipLevels;
                desc.Texture1DArray.FirstArraySlice = pEntry->desc.tex.firstArraySlice;
                desc.Texture1DArray.ArraySize = pEntry->desc.tex.arraySize;
            }
            break;
        case SVGA3D_RESOURCE_TEXTURE2D:
            if (pEntry->desc.tex.arraySize <= 1)
            {
                desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
                desc.Texture2D.MostDetailedMip = pEntry->desc.tex.mostDetailedMip;
                desc.Texture2D.MipLevels = pEntry->desc.tex.mipLevels;
            }
            else
            {
                desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
                desc.Texture2DArray.MostDetailedMip = pEntry->desc.tex.mostDetailedMip;
                desc.Texture2DArray.MipLevels = pEntry->desc.tex.mipLevels;
                desc.Texture2DArray.FirstArraySlice = pEntry->desc.tex.firstArraySlice;
                desc.Texture2DArray.ArraySize = pEntry->desc.tex.arraySize;
            }
            break;
        case SVGA3D_RESOURCE_TEXTURE3D:
            desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
            desc.Texture3D.MostDetailedMip = pEntry->desc.tex.mostDetailedMip;
            desc.Texture3D.MipLevels = pEntry->desc.tex.mipLevels;
            break;
        case SVGA3D_RESOURCE_TEXTURECUBE:
            AssertFailed(); /** @todo test. */
            desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
            desc.TextureCube.MostDetailedMip = pEntry->desc.tex.mostDetailedMip;
            desc.TextureCube.MipLevels = pEntry->desc.tex.mipLevels;
            break;
        case SVGA3D_RESOURCE_BUFFEREX:
            AssertFailed(); /** @todo test. */
            desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
            desc.BufferEx.FirstElement = pEntry->desc.bufferex.firstElement;
            desc.BufferEx.NumElements = pEntry->desc.bufferex.numElements;
            desc.BufferEx.Flags = pEntry->desc.bufferex.flags;
            break;
        default:
            ASSERT_GUEST_FAILED_RETURN(E_INVALIDARG);
    }

    HRESULT hr = pDevice->pDevice->CreateShaderResourceView(pResource, &desc, pp);
    Assert(SUCCEEDED(hr));
    return hr;
}


static HRESULT dxDepthStencilViewCreate(DXDEVICE *pDevice, SVGACOTableDXDSViewEntry const *pEntry, VMSVGA3DBACKENDSURFACE *pBackendSurface, ID3D11DepthStencilView **pp)
{
    ID3D11Resource *pResource = pBackendSurface->u.Texture.pTexture;

    D3D11_DEPTH_STENCIL_VIEW_DESC desc;
    RT_ZERO(desc);
    desc.Format = vmsvgaDXSurfaceFormat2Dxgi(pEntry->format);
    AssertReturn(desc.Format != DXGI_FORMAT_UNKNOWN, E_FAIL);
    desc.Flags = pEntry->flags;
    switch (pEntry->resourceDimension)
    {
        case SVGA3D_RESOURCE_TEXTURE1D:
            if (pEntry->arraySize <= 1)
            {
                desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE1D;
                desc.Texture1D.MipSlice = pEntry->mipSlice;
            }
            else
            {
                desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE1DARRAY;
                desc.Texture1DArray.MipSlice = pEntry->mipSlice;
                desc.Texture1DArray.FirstArraySlice = pEntry->firstArraySlice;
                desc.Texture1DArray.ArraySize = pEntry->arraySize;
            }
            break;
        case SVGA3D_RESOURCE_TEXTURE2D:
            if (pEntry->arraySize <= 1)
            {
                desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
                desc.Texture2D.MipSlice = pEntry->mipSlice;
            }
            else
            {
                desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
                desc.Texture2DArray.MipSlice = pEntry->mipSlice;
                desc.Texture2DArray.FirstArraySlice = pEntry->firstArraySlice;
                desc.Texture2DArray.ArraySize = pEntry->arraySize;
            }
            break;
        default:
            ASSERT_GUEST_FAILED_RETURN(E_INVALIDARG);
    }

    HRESULT hr = pDevice->pDevice->CreateDepthStencilView(pResource, &desc, pp);
    Assert(SUCCEEDED(hr));
    return hr;
}


static HRESULT dxShaderCreate(DXDEVICE *pDevice, PVMSVGA3DSHADER pShader, DXSHADER *pDXShader)
{
    HRESULT hr = S_OK;

    switch (pShader->type)
    {
        case SVGA3D_SHADERTYPE_VS:
            hr = pDevice->pDevice->CreateVertexShader(pDXShader->pvDXBC, pDXShader->cbDXBC, NULL, &pDXShader->pVertexShader);
            Assert(SUCCEEDED(hr));
            break;
        case SVGA3D_SHADERTYPE_PS:
            hr = pDevice->pDevice->CreatePixelShader(pDXShader->pvDXBC, pDXShader->cbDXBC, NULL, &pDXShader->pPixelShader);
            Assert(SUCCEEDED(hr));
            break;
        case SVGA3D_SHADERTYPE_GS:
        case SVGA3D_SHADERTYPE_HS:
        case SVGA3D_SHADERTYPE_DS:
        case SVGA3D_SHADERTYPE_CS:
        default:
            ASSERT_GUEST_FAILED_RETURN(E_INVALIDARG);
    }

    return hr;
}


static void dxShaderSet(DXDEVICE *pDevice, PVMSVGA3DSHADER pShader, DXSHADER *pDXShader)
{
    switch (pShader->type)
    {
        case SVGA3D_SHADERTYPE_VS:
            pDevice->pImmediateContext->VSSetShader(pDXShader->pVertexShader, NULL, 0);
            break;
        case SVGA3D_SHADERTYPE_PS:
            pDevice->pImmediateContext->PSSetShader(pDXShader->pPixelShader, NULL, 0);
            break;
        case SVGA3D_SHADERTYPE_GS:
        case SVGA3D_SHADERTYPE_HS:
        case SVGA3D_SHADERTYPE_DS:
        case SVGA3D_SHADERTYPE_CS:
        default:
            ASSERT_GUEST_FAILED_RETURN_VOID();
    }
}


static void dxConstantBufferSet(DXDEVICE *pDevice, uint32_t slot, SVGA3dShaderType type, ID3D11Buffer *pConstantBuffer)
{
    switch (type)
    {
        case SVGA3D_SHADERTYPE_VS:
            pDevice->pImmediateContext->VSSetConstantBuffers(slot, 1, &pConstantBuffer);
            break;
        case SVGA3D_SHADERTYPE_PS:
            pDevice->pImmediateContext->PSSetConstantBuffers(slot, 1, &pConstantBuffer);
            break;
        case SVGA3D_SHADERTYPE_GS:
        case SVGA3D_SHADERTYPE_HS:
        case SVGA3D_SHADERTYPE_DS:
        case SVGA3D_SHADERTYPE_CS:
        default:
            ASSERT_GUEST_FAILED_RETURN_VOID();
    }
}


static void dxSamplerSet(DXDEVICE *pDevice, SVGA3dShaderType type, uint32_t startSampler, uint32_t cSampler, ID3D11SamplerState * const *papSampler)
{
    switch (type)
    {
        case SVGA3D_SHADERTYPE_VS:
            pDevice->pImmediateContext->VSSetSamplers(startSampler, cSampler, papSampler);
            break;
        case SVGA3D_SHADERTYPE_PS:
            pDevice->pImmediateContext->PSSetSamplers(startSampler, cSampler, papSampler);
            break;
        case SVGA3D_SHADERTYPE_GS:
        case SVGA3D_SHADERTYPE_HS:
        case SVGA3D_SHADERTYPE_DS:
        case SVGA3D_SHADERTYPE_CS:
        default:
            ASSERT_GUEST_FAILED_RETURN_VOID();
    }
}


static void dxShaderResourceViewSet(DXDEVICE *pDevice, SVGA3dShaderType type, uint32_t startView, uint32_t cShaderResourceView, ID3D11ShaderResourceView * const *papShaderResourceView)
{
    switch (type)
    {
        case SVGA3D_SHADERTYPE_VS:
            pDevice->pImmediateContext->VSSetShaderResources(startView, cShaderResourceView, papShaderResourceView);
            break;
        case SVGA3D_SHADERTYPE_PS:
            pDevice->pImmediateContext->PSSetShaderResources(startView, cShaderResourceView, papShaderResourceView);
            break;
        case SVGA3D_SHADERTYPE_GS:
        case SVGA3D_SHADERTYPE_HS:
        case SVGA3D_SHADERTYPE_DS:
        case SVGA3D_SHADERTYPE_CS:
        default:
            ASSERT_GUEST_FAILED_RETURN_VOID();
    }
}



static int vmsvga3dBackSurfaceCreateScreenTarget(PVMSVGA3DSTATE pState, PVMSVGA3DSURFACE pSurface)
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
    Assert(SUCCEEDED(hr));
    if (SUCCEEDED(hr))
    {
        /* Map-able texture. */
        td.Usage          = D3D11_USAGE_DYNAMIC;
        td.BindFlags      = D3D11_BIND_SHADER_RESOURCE; /* Have to specify a supported flag, otherwise E_INVALIDARG will be returned. */
        td.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        td.MiscFlags      = 0;
        hr = pBackend->pDevice->CreateTexture2D(&td, 0, &pBackendSurface->u.ScreenTarget.pDynamicTexture);
        Assert(SUCCEEDED(hr));
    }

    if (SUCCEEDED(hr))
    {
        /* Staging texture. */
        td.Usage          = D3D11_USAGE_STAGING;
        td.BindFlags      = 0; /* No flags allowed. */
        td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        td.MiscFlags      = 0;
        hr = pBackend->pDevice->CreateTexture2D(&td, 0, &pBackendSurface->u.ScreenTarget.pStagingTexture);
        Assert(SUCCEEDED(hr));
    }

    if (SUCCEEDED(hr))
    {
        /*
         * Success.
         */
        pBackendSurface->enmResType = VMSVGA3D_RESTYPE_SCREEN_TARGET;
        pBackendSurface->enmDxgiFormat = td.Format;
        pSurface->pBackendSurface = pBackendSurface;
        pSurface->idAssociatedContext = SVGA_ID_INVALID;
        pSurface->fDirty = true;
        return VINF_SUCCESS;
    }

    /* Failure. */
    D3D_RELEASE(pBackendSurface->u.ScreenTarget.pStagingTexture);
    D3D_RELEASE(pBackendSurface->u.ScreenTarget.pDynamicTexture);
    D3D_RELEASE(pBackendSurface->u.ScreenTarget.pTexture);
    RTMemFree(pBackendSurface);
    return VERR_NO_MEMORY;
}


static int vmsvga3dBackSurfaceCreateRenderTarget(PVMSVGA3DSTATE p3dState, PVMSVGA3DDXCONTEXT pDXContext, PVMSVGA3DSURFACE pSurface)
{
    DXDEVICE *pDevice = &pDXContext->pBackendDXContext->device;
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    if (pSurface->pBackendSurface != NULL)
    {
        AssertFailed(); /** @todo Should the function not be used like that? */
        vmsvga3dBackSurfaceDestroy(p3dState, pSurface);
    }

    /** @todo Various texture types. */

    PVMSVGA3DBACKENDSURFACE pBackendSurface = (PVMSVGA3DBACKENDSURFACE)RTMemAllocZ(sizeof(VMSVGA3DBACKENDSURFACE));
    AssertPtrReturn(pBackendSurface, VERR_NO_MEMORY);

    D3D11_TEXTURE2D_DESC td;
    RT_ZERO(td);
    td.Width              = pSurface->paMipmapLevels[0].mipmapSize.width;
    td.Height             = pSurface->paMipmapLevels[0].mipmapSize.height;
    Assert(pSurface->cLevels == 1);
    td.MipLevels          = 1;
    td.ArraySize          = 1;
    td.Format             = vmsvgaDXSurfaceFormat2Dxgi(pSurface->format); // DXGI_FORMAT_B8G8R8A8_UNORM;
    AssertReturn(td.Format != DXGI_FORMAT_UNKNOWN, E_FAIL);
    td.SampleDesc.Count   = 1;
    td.SampleDesc.Quality = 0;
    td.Usage              = D3D11_USAGE_DEFAULT;
    td.BindFlags          = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    td.CPUAccessFlags     = 0;
    td.MiscFlags          = 0;

    HRESULT hr = pDevice->pDevice->CreateTexture2D(&td, 0, &pBackendSurface->u.Texture.pTexture);
    Assert(SUCCEEDED(hr));
    if (SUCCEEDED(hr))
    {
        /* Map-able texture. */
        td.Usage          = D3D11_USAGE_DYNAMIC;
        td.BindFlags      = D3D11_BIND_SHADER_RESOURCE; /* Have to specify a supported flag, otherwise E_INVALIDARG will be returned. */
        td.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        td.MiscFlags      = 0;
        hr = pDevice->pDevice->CreateTexture2D(&td, 0, &pBackendSurface->u.Texture.pDynamicTexture);
        Assert(SUCCEEDED(hr));
    }

    if (SUCCEEDED(hr))
    {
        /* Staging texture. */
        td.Usage          = D3D11_USAGE_STAGING;
        td.BindFlags      = 0; /* No flags allowed. */
        td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        td.MiscFlags      = 0;
        hr = pDevice->pDevice->CreateTexture2D(&td, 0, &pBackendSurface->u.Texture.pStagingTexture);
        Assert(SUCCEEDED(hr));
    }

    if (SUCCEEDED(hr))
    {
        /*
         * Success.
         */
        pBackendSurface->enmResType = VMSVGA3D_RESTYPE_TEXTURE;
        pBackendSurface->enmDxgiFormat = td.Format;
        pSurface->pBackendSurface = pBackendSurface;
        pSurface->idAssociatedContext = pDXContext->cid;
        pSurface->fDirty = true;
        return VINF_SUCCESS;
    }

    /* Failure. */
    D3D_RELEASE(pBackendSurface->u.Texture.pStagingTexture);
    D3D_RELEASE(pBackendSurface->u.Texture.pDynamicTexture);
    D3D_RELEASE(pBackendSurface->u.Texture.pTexture);
    RTMemFree(pBackendSurface);
    return VERR_NO_MEMORY;
}


static int vmsvga3dBackSurfaceCreateTexture(PVMSVGA3DSTATE p3dState, PVMSVGA3DDXCONTEXT pDXContext, PVMSVGA3DSURFACE pSurface)
{
    DXDEVICE *pDevice = &pDXContext->pBackendDXContext->device;
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    if (pSurface->pBackendSurface != NULL)
    {
        AssertFailed(); /** @todo Should the function not be used like that? */
        vmsvga3dBackSurfaceDestroy(p3dState, pSurface);
    }

    PVMSVGA3DBACKENDSURFACE pBackendSurface = (PVMSVGA3DBACKENDSURFACE)RTMemAllocZ(sizeof(VMSVGA3DBACKENDSURFACE));
    AssertPtrReturn(pBackendSurface, VERR_NO_MEMORY);

    /** @todo Various texture types. */
    ASMBreakpoint();

    RTMemFree(pBackendSurface);
    return VERR_NOT_IMPLEMENTED;
}


static int vmsvga3dBackSurfaceCreateDepthStencilTexture(PVMSVGA3DSTATE p3dState, PVMSVGA3DDXCONTEXT pDXContext, PVMSVGA3DSURFACE pSurface)
{
    DXDEVICE *pDevice = &pDXContext->pBackendDXContext->device;
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    if (pSurface->pBackendSurface != NULL)
    {
        AssertFailed(); /** @todo Should the function not be used like that? */
        vmsvga3dBackSurfaceDestroy(p3dState, pSurface);
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
    td.Format             = vmsvgaDXSurfaceFormat2Dxgi(pSurface->format);
    AssertReturn(td.Format != DXGI_FORMAT_UNKNOWN, E_FAIL);
    td.SampleDesc.Count   = 1;
    td.SampleDesc.Quality = 0;
    td.Usage              = D3D11_USAGE_DEFAULT;
    td.BindFlags          = D3D11_BIND_DEPTH_STENCIL;
    td.CPUAccessFlags     = 0;
    td.MiscFlags          = 0;

    HRESULT hr = pDevice->pDevice->CreateTexture2D(&td, 0, &pBackendSurface->u.Texture.pTexture);
    Assert(SUCCEEDED(hr));
    if (SUCCEEDED(hr))
    {
        /*
         * Success.
         */
        pBackendSurface->enmResType = VMSVGA3D_RESTYPE_TEXTURE;
        pBackendSurface->enmDxgiFormat = td.Format;
        pSurface->pBackendSurface = pBackendSurface;
        pSurface->idAssociatedContext = pDXContext->cid;
        pSurface->fDirty = true;
        return VINF_SUCCESS;
    }

    /* Failure. */
    D3D_RELEASE(pBackendSurface->u.Texture.pStagingTexture);
    D3D_RELEASE(pBackendSurface->u.Texture.pDynamicTexture);
    D3D_RELEASE(pBackendSurface->u.Texture.pTexture);
    RTMemFree(pBackendSurface);
    return VERR_NO_MEMORY;
}


static int vmsvga3dBackSurfaceCreateBuffer(PVMSVGA3DSTATE p3dState, PVMSVGA3DDXCONTEXT pDXContext, PVMSVGA3DSURFACE pSurface)
{
    DXDEVICE *pDevice = &pDXContext->pBackendDXContext->device;
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    /* Buffers should be created as such. */
    AssertReturn(RT_BOOL(pSurface->surfaceFlags & (  SVGA3D_SURFACE_HINT_INDEXBUFFER
                                                   | SVGA3D_SURFACE_HINT_VERTEXBUFFER
                                                   | SVGA3D_SURFACE_BIND_VERTEX_BUFFER
                                                   | SVGA3D_SURFACE_BIND_INDEX_BUFFER
                                                   /// @todo only for constant buffers| SVGA3D_SURFACE_BIND_CONSTANT_BUFFER
                                                   //| SVGA3D_SURFACE_BIND_STREAM_OUTPUT
                        )), VERR_INVALID_PARAMETER);

    if (pSurface->pBackendSurface != NULL)
    {
        AssertFailed(); /** @todo Should the function not be used like that? */
        vmsvga3dBackSurfaceDestroy(p3dState, pSurface);
    }

    PVMSVGA3DBACKENDSURFACE pBackendSurface = (PVMSVGA3DBACKENDSURFACE)RTMemAllocZ(sizeof(VMSVGA3DBACKENDSURFACE));
    AssertPtrReturn(pBackendSurface, VERR_NO_MEMORY);

    D3D11_BUFFER_DESC bd;
    RT_ZERO(bd);
    bd.ByteWidth = pSurface->paMipmapLevels[0].cbSurface;
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.BindFlags           = D3D11_BIND_VERTEX_BUFFER
                           | D3D11_BIND_INDEX_BUFFER
                           //| D3D11_BIND_CONSTANT_BUFFER
                           //| D3D11_BIND_STREAM_OUTPUT
                           ;
    bd.CPUAccessFlags      = D3D11_CPU_ACCESS_WRITE;
    bd.MiscFlags           = 0;
    bd.StructureByteStride = 0;

    HRESULT hr = pDevice->pDevice->CreateBuffer(&bd, 0, &pBackendSurface->u.Buffer.pBuffer);
    if (SUCCEEDED(hr))
    {
        /*
         * Success.
         */
        pBackendSurface->enmResType = VMSVGA3D_RESTYPE_BUFFER;
        pBackendSurface->enmDxgiFormat = DXGI_FORMAT_UNKNOWN;
        pSurface->pBackendSurface = pBackendSurface;
        pSurface->idAssociatedContext = pDXContext->cid;
        pSurface->fDirty = true;
        return VINF_SUCCESS;
    }

    /* Failure. */
    D3D_RELEASE(pBackendSurface->u.Buffer.pBuffer);
    RTMemFree(pBackendSurface);
    return VERR_NO_MEMORY;
}


static int vmsvga3dBackSurfaceCreateConstantBuffer(PVMSVGA3DSTATE p3dState, PVMSVGA3DDXCONTEXT pDXContext, PVMSVGA3DSURFACE pSurface)
{
    DXDEVICE *pDevice = &pDXContext->pBackendDXContext->device;
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    /* Buffers should be created as such. */
    AssertReturn(RT_BOOL(pSurface->surfaceFlags & ( SVGA3D_SURFACE_BIND_CONSTANT_BUFFER)), VERR_INVALID_PARAMETER);

    if (pSurface->pBackendSurface != NULL)
    {
        AssertFailed(); /** @todo Should the function not be used like that? */
        vmsvga3dBackSurfaceDestroy(p3dState, pSurface);
    }

    PVMSVGA3DBACKENDSURFACE pBackendSurface = (PVMSVGA3DBACKENDSURFACE)RTMemAllocZ(sizeof(VMSVGA3DBACKENDSURFACE));
    AssertPtrReturn(pBackendSurface, VERR_NO_MEMORY);

    D3D11_BUFFER_DESC bd;
    RT_ZERO(bd);
    bd.ByteWidth = pSurface->paMipmapLevels[0].cbSurface;
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.BindFlags           = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags      = D3D11_CPU_ACCESS_WRITE;
    bd.MiscFlags           = 0;
    bd.StructureByteStride = 0;

    HRESULT hr = pDevice->pDevice->CreateBuffer(&bd, 0, &pBackendSurface->u.Buffer.pBuffer);
    if (SUCCEEDED(hr))
    {
        /*
         * Success.
         */
        pBackendSurface->enmResType = VMSVGA3D_RESTYPE_BUFFER;
        pBackendSurface->enmDxgiFormat = DXGI_FORMAT_UNKNOWN;
        pSurface->pBackendSurface = pBackendSurface;
        pSurface->idAssociatedContext = pDXContext->cid;
        pSurface->fDirty = true;
        return VINF_SUCCESS;
    }

    /* Failure. */
    D3D_RELEASE(pBackendSurface->u.Buffer.pBuffer);
    RTMemFree(pBackendSurface);
    return VERR_NO_MEMORY;
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

    rc = RTLdrLoadSystem("D3DCompiler_47", /* fNoUnload = */ true, &pBackend->hD3DCompiler);
    AssertRC(rc);
    if (RT_SUCCESS(rc))
    {
        rc = RTLdrGetSymbol(pBackend->hD3DCompiler, "D3DDisassemble", (void **)&pBackend->pfnD3DDisassemble);
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
        // AssertFailedReturn(VERR_INVALID_PARAMETER); /** @todo The caller must directly use the surface memory. */

    PVMSVGA3DDXCONTEXT pDXContext;
    vmsvga3dDXContextFromCid(pState, pSurface->idAssociatedContext, &pDXContext);

    DXDEVICE *pDevice = NULL;
    if (pDXContext)
    {
        pDevice = &pDXContext->pBackendDXContext->device;
        AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);
    }

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
    else if (pBackendSurface->enmResType == VMSVGA3D_RESTYPE_TEXTURE)
    {
        AssertReturn(pDevice && pDevice->pDevice, VERR_INTERNAL_ERROR);

        ID3D11Texture2D *pMappedTexture;
        if (enmMapType == VMSVGA3D_SURFACE_MAP_READ)
        {
            pMappedTexture = pBackendSurface->u.ScreenTarget.pStagingTexture;

            /* Copy the texture content to the staging texture. */
            pDevice->pImmediateContext->CopyResource(pBackendSurface->u.ScreenTarget.pStagingTexture, pBackendSurface->u.ScreenTarget.pTexture);
        }
        else
            pMappedTexture = pBackendSurface->u.ScreenTarget.pDynamicTexture;

        UINT const Subresource = 0; /* Screen target surfaces have only one subresource. */
        HRESULT hr = pDevice->pImmediateContext->Map(pMappedTexture, Subresource,
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
    else if (pBackendSurface->enmResType == VMSVGA3D_RESTYPE_BUFFER)
    {
        UINT const Subresource = 0; /* Buffers have only one subresource. */
        HRESULT hr = pDevice->pImmediateContext->Map(pSurface->pBackendSurface->u.Buffer.pBuffer, Subresource,
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

    PVMSVGA3DDXCONTEXT pDXContext;
    vmsvga3dDXContextFromCid(pState, pSurface->idAssociatedContext, &pDXContext);

    DXDEVICE *pDevice = NULL;
    if (pDXContext)
    {
        pDevice = &pDXContext->pBackendDXContext->device;
        AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);
    }

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
    else if (pBackendSurface->enmResType == VMSVGA3D_RESTYPE_TEXTURE)
    {
        AssertReturn(pDevice && pDevice->pDevice, VERR_INTERNAL_ERROR);

        ID3D11Texture2D *pMappedTexture;
        if (pMap->enmMapType == VMSVGA3D_SURFACE_MAP_READ)
            pMappedTexture = pBackendSurface->u.ScreenTarget.pStagingTexture;
        else
            pMappedTexture = pBackendSurface->u.ScreenTarget.pDynamicTexture;

        UINT const Subresource = 0; /* Screen target surfaces have only one subresource. */
        pDevice->pImmediateContext->Unmap(pMappedTexture, Subresource);

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
            pDevice->pImmediateContext->CopySubresourceRegion(pDstResource, DstSubresource, DstX, DstY, DstZ,
                                                              pSrcResource, SrcSubresource, &SrcBox);
        }
    }
    else if (pBackendSurface->enmResType == VMSVGA3D_RESTYPE_BUFFER)
    {
        UINT const Subresource = 0; /* Buffers have only one subresource. */
        pDevice->pImmediateContext->Unmap(pBackendSurface->u.Buffer.pBuffer, Subresource);
    }
    else
    {
        AssertFailed();
        rc = VERR_NOT_IMPLEMENTED;
    }

    return rc;
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

    if (idx3dCaps > SVGA3D_DEVCAP_MAX)
    {
        LogRelMax(16, ("VMSVGA: unsupported SVGA3D_DEVCAP %d\n", idx3dCaps));
        return VERR_NOT_SUPPORTED;
    }

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
    case SVGA3D_DEVCAP_DEAD10: /* SVGA3D_DEVCAP_SURFACEFMT_AYUV */
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

    case SVGA3D_DEVCAP_DEAD4: /* SVGA3D_DEVCAP_MULTISAMPLE_NONMASKABLESAMPLES */
    case SVGA3D_DEVCAP_DEAD5: /* SVGA3D_DEVCAP_MULTISAMPLE_MASKABLESAMPLES */
        *pu32Val = (1 << (2-1)) | (1 << (4-1)) | (1 << (8-1)); /* 2x, 4x, 8x */
        break;

    case SVGA3D_DEVCAP_DEAD7: /* SVGA3D_DEVCAP_ALPHATOCOVERAGE */
        /* Obsolete */
        break;

    case SVGA3D_DEVCAP_DEAD6: /* SVGA3D_DEVCAP_SUPERSAMPLE */
        /* Obsolete */
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

    case SVGA3D_DEVCAP_DEAD8: /* SVGA3D_DEVCAP_VIDEO_DECODE */
        /* Obsolete */
        break;

    case SVGA3D_DEVCAP_DEAD9: /* SVGA3D_DEVCAP_VIDEO_PROCESS */
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

    case SVGA3D_DEVCAP_DEAD3: /* Old SVGA3D_DEVCAP_LOGICOPS */
        /* Deprecated. */
        AssertCompile(SVGA3D_DEVCAP_DEAD3 == 92); /* Newer SVGA headers redefine this. */
        break;

    case SVGA3D_DEVCAP_TS_COLOR_KEY:
        *pu32Val = 0; /* DX11 does not seem to support this directly. */
        break;

    case SVGA3D_DEVCAP_DEAD2:
        break;

    case SVGA3D_DEVCAP_DXCONTEXT:
        *pu32Val = 1;
        break;

    case SVGA3D_DEVCAP_DEAD11: /* SVGA3D_DEVCAP_MAX_TEXTURE_ARRAY_SIZE */
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
    case SVGA3D_DEVCAP_DXFMT_FORMAT_DEAD2: /* SVGA3D_DEVCAP_DXFMT_AYUV */
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
    case SVGA3D_DEVCAP_DXFMT_R32_FLOAT_X8X24:
    case SVGA3D_DEVCAP_DXFMT_X32_G8X24_UINT:
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
    case SVGA3D_DEVCAP_DXFMT_R24_UNORM_X8:
    case SVGA3D_DEVCAP_DXFMT_X24_G8_UINT:
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
    case SVGA3D_DEVCAP_DXFMT_BC6H_TYPELESS:
    case SVGA3D_DEVCAP_DXFMT_BC6H_UF16:
    case SVGA3D_DEVCAP_DXFMT_BC6H_SF16:
    case SVGA3D_DEVCAP_DXFMT_BC7_TYPELESS:
    case SVGA3D_DEVCAP_DXFMT_BC7_UNORM:
    case SVGA3D_DEVCAP_DXFMT_BC7_UNORM_SRGB:
    {
        SVGA3dSurfaceFormat const enmFormat = vmsvgaDXDevCapDxfmt2Format(idx3dCaps);
        rc = vmsvgaDXCheckFormatSupport(pState, enmFormat, pu32Val);
        break;
    }

    case SVGA3D_DEVCAP_SM41:
        *pu32Val = 0; /* boolean */
        break;

    case SVGA3D_DEVCAP_MULTISAMPLE_2X:
        *pu32Val = 0; /* boolean */
        break;

    case SVGA3D_DEVCAP_MULTISAMPLE_4X:
        *pu32Val = 0; /* boolean */
        break;

    case SVGA3D_DEVCAP_MS_FULL_QUALITY:
        *pu32Val = 0; /* boolean */
        break;

    case SVGA3D_DEVCAP_LOGICOPS:
        AssertCompile(SVGA3D_DEVCAP_LOGICOPS == 248);
        *pu32Val = 0; /* boolean */
        break;

    case SVGA3D_DEVCAP_LOGIC_BLENDOPS:
        *pu32Val = 0; /* boolean */
        break;

    case SVGA3D_DEVCAP_RESERVED_1:
        break;

    case SVGA3D_DEVCAP_RESERVED_2:
        break;

    case SVGA3D_DEVCAP_SM5:
        *pu32Val = 0; /* boolean */
        break;

    case SVGA3D_DEVCAP_MULTISAMPLE_8X:
        *pu32Val = 0; /* boolean */
        break;

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
    if (!pBackendSurface)
        return;
    pSurface->pBackendSurface = NULL;

    if (pBackendSurface->enmResType == VMSVGA3D_RESTYPE_SCREEN_TARGET)
    {
        D3D_RELEASE(pBackendSurface->u.ScreenTarget.pStagingTexture);
        D3D_RELEASE(pBackendSurface->u.ScreenTarget.pDynamicTexture);
        D3D_RELEASE(pBackendSurface->u.ScreenTarget.pTexture);
    }
    else if (pBackendSurface->enmResType == VMSVGA3D_RESTYPE_TEXTURE)
    {
        D3D_RELEASE(pBackendSurface->u.Texture.pStagingTexture);
        D3D_RELEASE(pBackendSurface->u.Texture.pDynamicTexture);
        D3D_RELEASE(pBackendSurface->u.Texture.pTexture);
    }
    else if (pBackendSurface->enmResType == VMSVGA3D_RESTYPE_BUFFER)
    {
        D3D_RELEASE(pBackendSurface->u.Buffer.pBuffer);
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


/*
 * DX callbacks.
 */

static DECLCALLBACK(int) vmsvga3dBackDXDefineContext(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    /* Allocate a backend specific context structure. */
    PVMSVGA3DBACKENDDXCONTEXT pBackendDXContext = (PVMSVGA3DBACKENDDXCONTEXT)RTMemAllocZ(sizeof(VMSVGA3DBACKENDDXCONTEXT));
    AssertPtrReturn(pBackendDXContext, VERR_NO_MEMORY);
    pDXContext->pBackendDXContext = pBackendDXContext;

    int rc = dxDeviceCreate(pBackend, &pBackendDXContext->device);
    return rc;
}


static DECLCALLBACK(int) vmsvga3dBackDXDestroyContext(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    if (pDXContext->pBackendDXContext)
    {
        dxDeviceDestroy(pBackend, &pDXContext->pBackendDXContext->device);

        RTMemFree(pDXContext->pBackendDXContext);
        pDXContext->pBackendDXContext = NULL;
    }
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackDXBindContext(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend, pDXContext);
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackDXReadbackContext(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXInvalidateContext(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXSetSingleConstantBuffer(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, uint32_t slot, SVGA3dShaderType type, SVGA3dSurfaceId sid, uint32_t offsetInBytes, uint32_t sizeInBytes)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend);

    DXDEVICE *pDevice = &pDXContext->pBackendDXContext->device;
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    PVMSVGA3DSURFACE pSurface;
    int rc = vmsvga3dSurfaceFromSid(pThisCC->svga.p3dState, sid, &pSurface);
    AssertRCReturn(rc, rc);

    uint32_t const cbSurface = pSurface->paMipmapLevels[0].cbSurface;
    ASSERT_GUEST_RETURN(   offsetInBytes < cbSurface
                        && sizeInBytes <= cbSurface - offsetInBytes, VERR_INVALID_PARAMETER);

    if (pSurface->pBackendSurface == NULL)
    {
        /* Create the resource. */
        rc = vmsvga3dBackSurfaceCreateConstantBuffer(pThisCC->svga.p3dState, pDXContext, pSurface);
        AssertRCReturn(rc, rc);
    }

    if (pSurface->fDirty)
    {
        /* Get mobid for the surface and read from the MOB. */
        SVGA3dSurfaceImageId imageId;
        imageId.sid = sid;
        imageId.face = 0;
        imageId.mipmap = 0;

        SVGA3dPoint ptSrc;
        ptSrc.x = offsetInBytes / pSurface->cbBlock;
        ptSrc.y = 0;
        ptSrc.z = 0;

        SVGA3dBox boxDst;
        boxDst.x = 0;
        boxDst.y = 0;
        boxDst.z = 0;
        boxDst.w = sizeInBytes / pSurface->cbBlock;
        boxDst.h = 1;
        boxDst.d = 1;

        rc = vmsvgaR3UpdateGBSurfaceEx(pThisCC, &imageId, &boxDst, &ptSrc);
        AssertRCReturn(rc, rc);
    }

    dxConstantBufferSet(pDevice, slot, type, pSurface->pBackendSurface->u.Buffer.pBuffer);

    uint32_t const idxShaderState = type - SVGA3D_SHADERTYPE_MIN;
    SVGA3dConstantBufferBinding *pCBB = &pDXContext->svgaDXContext.shaderState[idxShaderState].constantBuffers[slot];
    pCBB->sid           = sid;
    pCBB->offsetInBytes = offsetInBytes;
    pCBB->sizeInBytes   = sizeInBytes;
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackDXSetShaderResources(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, uint32_t startView, SVGA3dShaderType type, uint32_t cShaderResourceViewId, SVGA3dShaderResourceViewId const *paShaderResourceViewId)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend);

    DXDEVICE *pDevice = &pDXContext->pBackendDXContext->device;
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    uint32_t const idxShaderState = type - SVGA3D_SHADERTYPE_MIN;
    ID3D11ShaderResourceView *papShaderResourceView[SVGA3D_DX_MAX_SRVIEWS];
    for (uint32_t i = 0; i < cShaderResourceViewId; ++i)
    {
        SVGA3dShaderResourceViewId shaderResourceViewId = paShaderResourceViewId[i];
        if (shaderResourceViewId != SVGA3D_INVALID_ID)
        {
            ASSERT_GUEST_RETURN(shaderResourceViewId < pDXContext->pBackendDXContext->cShaderResourceView, VERR_INVALID_PARAMETER);
            papShaderResourceView[i] = pDXContext->pBackendDXContext->papShaderResourceView[shaderResourceViewId];
        }
        else
            papShaderResourceView[i] = NULL;

        pDXContext->svgaDXContext.shaderState[idxShaderState].shaderResources[startView + i] = shaderResourceViewId;
    }

    dxShaderResourceViewSet(pDevice, type, startView, cShaderResourceViewId, papShaderResourceView);
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackDXSetShader(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, PVMSVGA3DSHADER pShader)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend);

    DXDEVICE *pDevice = &pDXContext->pBackendDXContext->device;
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    DXSHADER *pDXShader = &pDXContext->pBackendDXContext->paShader[pShader->id];
    uint32_t const idxShaderState = pDXShader->enmShaderType - SVGA3D_SHADERTYPE_MIN;
    pDXContext->svgaDXContext.shaderState[idxShaderState].shaderId = pShader->id;

    dxShaderSet(pDevice, pShader, pDXShader);
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackDXSetSamplers(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, uint32_t startSampler, SVGA3dShaderType type, uint32_t cSamplerId, SVGA3dSamplerId const *paSamplerId)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend);

    DXDEVICE *pDevice = &pDXContext->pBackendDXContext->device;
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    uint32_t const idxShaderState = type - SVGA3D_SHADERTYPE_MIN;
    ID3D11SamplerState *papSamplerState[SVGA3D_DX_MAX_SAMPLERS];
    for (uint32_t i = 0; i < cSamplerId; ++i)
    {
        SVGA3dSamplerId samplerId = paSamplerId[i];
        if (samplerId != SVGA3D_INVALID_ID)
        {
            ASSERT_GUEST_RETURN(samplerId < pDXContext->pBackendDXContext->cSamplerState, VERR_INVALID_PARAMETER);
            papSamplerState[i] = pDXContext->pBackendDXContext->papSamplerState[samplerId];
        }
        else
            papSamplerState[i] = NULL;

        pDXContext->svgaDXContext.shaderState[idxShaderState].samplers[startSampler + i] = samplerId;
    }

    dxSamplerSet(pDevice, type, startSampler, cSamplerId, papSamplerState);
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackDXDraw(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, uint32_t vertexCount, uint32_t startVertexLocation)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend);

    DXDEVICE *pDevice = &pDXContext->pBackendDXContext->device;
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    pDevice->pImmediateContext->Draw(vertexCount, startVertexLocation);
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackDXDrawIndexed(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, uint32_t indexCount, uint32_t startIndexLocation, int32_t baseVertexLocation)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend);

    DXDEVICE *pDevice = &pDXContext->pBackendDXContext->device;
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    pDevice->pImmediateContext->DrawIndexed(indexCount, startIndexLocation, baseVertexLocation);
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackDXDrawInstanced(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXDrawIndexedInstanced(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXDrawAuto(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXSetInputLayout(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, SVGA3dElementLayoutId elementLayoutId)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend);

    DXDEVICE *pDevice = &pDXContext->pBackendDXContext->device;
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    pDXContext->svgaDXContext.inputAssembly.layoutId = elementLayoutId;

    DXELEMENTLAYOUT *pDXElementLayout = &pDXContext->pBackendDXContext->paElementLayout[elementLayoutId];
    if (!pDXElementLayout->pElementLayout)
    {
        uint32_t const idxShaderState = SVGA3D_SHADERTYPE_VS - SVGA3D_SHADERTYPE_MIN;
        uint32_t const shid = pDXContext->svgaDXContext.shaderState[idxShaderState].shaderId;
        AssertReturnStmt(shid < pDXContext->pBackendDXContext->cShader,
                         LogRelMax(16, ("VMSVGA: DX shader is not set in DXSetInputLayout: shid = 0x%x\n", shid)),
                         VERR_INVALID_STATE);
        DXSHADER *pDXShader = &pDXContext->pBackendDXContext->paShader[shid];
        AssertReturnStmt(pDXShader->pvDXBC,
                         LogRelMax(16, ("VMSVGA: DX shader bytecode is not available in DXSetInputLayout: shid = %u\n", shid)),
                         VERR_INVALID_STATE);
        HRESULT hr = pDevice->pDevice->CreateInputLayout(pDXElementLayout->aElementDesc,
                                                         pDXElementLayout->cElementDesc,
                                                         pDXShader->pvDXBC,
                                                         pDXShader->cbDXBC,
                                                         &pDXElementLayout->pElementLayout);
        AssertReturn(SUCCEEDED(hr), VERR_NO_MEMORY);
    }

    pDevice->pImmediateContext->IASetInputLayout(pDXElementLayout->pElementLayout);
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackDXSetVertexBuffers(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, uint32_t startBuffer, uint32_t cVertexBuffer, SVGA3dVertexBuffer const *paVertexBuffer)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend);

    DXDEVICE *pDevice = &pDXContext->pBackendDXContext->device;
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    /* For each paVertexBuffer[i]:
     *   If the vertex buffer object does not exist then create it.
     *   If the surface has been updated by the guest then update the buffer object.
     * Use IASetVertexBuffers to set the buffers.
     */

    ID3D11Buffer *paResources[SVGA3D_DX_MAX_VERTEXBUFFERS];
    UINT paStride[SVGA3D_DX_MAX_VERTEXBUFFERS];
    UINT paOffset[SVGA3D_DX_MAX_VERTEXBUFFERS];

    for (uint32_t i = 0; i < cVertexBuffer; ++i)
    {
        uint32_t const idxVertexBuffer = startBuffer + i;

        /* Get corresponding resource. Create the buffer if does not yet exist. */
        if (paVertexBuffer[i].sid != SVGA_ID_INVALID)
        {
            PVMSVGA3DSURFACE pSurface;
            int rc = vmsvga3dSurfaceFromSid(pThisCC->svga.p3dState, paVertexBuffer[i].sid, &pSurface);
            AssertRCReturn(rc, rc);

            if (pSurface->pBackendSurface == NULL)
            {
                /* Create the resource. */
                rc = vmsvga3dBackSurfaceCreateBuffer(pThisCC->svga.p3dState, pDXContext, pSurface);
                AssertRCReturn(rc, rc);
            }

            if (pSurface->fDirty)
            {
                /* Get mobid for the surface and read from the MOB. */
                SVGA3dSurfaceImageId imageId;
                imageId.sid = paVertexBuffer[i].sid;
                imageId.face = 0;
                imageId.mipmap = 0;

                SVGA3dBox box;
                box.x = 0;
                box.y = 0;
                box.z = 0;
                box.w = pSurface->paMipmapLevels[0].mipmapSize.width;
                box.h = 1;
                box.d = 1;

                rc = vmsvgaR3UpdateGBSurface(pThisCC, &imageId, &box);
                AssertRCReturn(rc, rc);
            }

            paResources[idxVertexBuffer] = pSurface->pBackendSurface->u.Buffer.pBuffer;
            paStride[idxVertexBuffer] = paVertexBuffer[i].stride;
            paOffset[idxVertexBuffer] = paVertexBuffer[i].offset;
        }
        else
        {
            paResources[idxVertexBuffer] = NULL;
            paStride[idxVertexBuffer] = 0;
            paOffset[idxVertexBuffer] = 0;
        }

        pDXContext->svgaDXContext.inputAssembly.vertexBuffers[idxVertexBuffer].bufferId = paVertexBuffer[i].sid;
        pDXContext->svgaDXContext.inputAssembly.vertexBuffers[idxVertexBuffer].stride = paVertexBuffer[i].stride;
        pDXContext->svgaDXContext.inputAssembly.vertexBuffers[idxVertexBuffer].offset = paVertexBuffer[i].offset;
    }

    pDevice->pImmediateContext->IASetVertexBuffers(startBuffer, cVertexBuffer, paResources, paStride, paOffset);

    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackDXSetIndexBuffer(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, SVGA3dSurfaceId sid, SVGA3dSurfaceFormat format, uint32_t offset)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend);

    DXDEVICE *pDevice = &pDXContext->pBackendDXContext->device;
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    /* Get corresponding resource. Create the buffer if does not yet exist. */
    ID3D11Buffer *pResource;
    DXGI_FORMAT enmDxgiFormat;

    if (sid != SVGA_ID_INVALID)
    {
        PVMSVGA3DSURFACE pSurface;
        int rc = vmsvga3dSurfaceFromSid(pThisCC->svga.p3dState, sid, &pSurface);
        AssertRCReturn(rc, rc);

        if (pSurface->pBackendSurface == NULL)
        {
            /* Create the resource. */
            rc = vmsvga3dBackSurfaceCreateBuffer(pThisCC->svga.p3dState, pDXContext, pSurface);
            AssertRCReturn(rc, rc);
        }

        if (pSurface->fDirty)
        {
            /* Get mobid for the surface and read from the MOB. */
            SVGA3dSurfaceImageId imageId;
            imageId.sid = sid;
            imageId.face = 0;
            imageId.mipmap = 0;

            SVGA3dBox box;
            box.x = 0;
            box.y = 0;
            box.z = 0;
            box.w = pSurface->paMipmapLevels[0].mipmapSize.width;
            box.h = 1;
            box.d = 1;

            rc = vmsvgaR3UpdateGBSurface(pThisCC, &imageId, &box);
            AssertRCReturn(rc, rc);
        }

        pResource = pSurface->pBackendSurface->u.Buffer.pBuffer;
        enmDxgiFormat = vmsvgaDXSurfaceFormat2Dxgi(format);
        AssertReturn(enmDxgiFormat == DXGI_FORMAT_R16_UINT || enmDxgiFormat == DXGI_FORMAT_R32_UINT, VERR_INVALID_PARAMETER);
    }
    else
    {
        pResource = NULL;
        enmDxgiFormat = DXGI_FORMAT_UNKNOWN;
    }

    pDevice->pImmediateContext->IASetIndexBuffer(pResource, enmDxgiFormat, offset);

    pDXContext->svgaDXContext.inputAssembly.indexBufferSid = sid;
    pDXContext->svgaDXContext.inputAssembly.indexBufferOffset = offset;
    pDXContext->svgaDXContext.inputAssembly.indexBufferFormat = format;

    return VINF_SUCCESS;
}

static D3D11_PRIMITIVE_TOPOLOGY dxTopology(SVGA3dPrimitiveType primitiveType)
{
    static D3D11_PRIMITIVE_TOPOLOGY const aD3D11PrimitiveTopology[SVGA3D_PRIMITIVE_MAX] =
    {
        D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED,
        D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
        D3D11_PRIMITIVE_TOPOLOGY_POINTLIST,
        D3D11_PRIMITIVE_TOPOLOGY_LINELIST,
        D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP,
        D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,
        D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP, /* SVGA3D_PRIMITIVE_TRIANGLEFAN: No FAN in D3D11 */
        D3D11_PRIMITIVE_TOPOLOGY_LINELIST_ADJ,
        D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ,
        D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ,
        D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ,
        D3D11_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST,
        D3D11_PRIMITIVE_TOPOLOGY_2_CONTROL_POINT_PATCHLIST,
        D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST,
        D3D11_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST,
        D3D11_PRIMITIVE_TOPOLOGY_5_CONTROL_POINT_PATCHLIST,
        D3D11_PRIMITIVE_TOPOLOGY_6_CONTROL_POINT_PATCHLIST,
        D3D11_PRIMITIVE_TOPOLOGY_7_CONTROL_POINT_PATCHLIST,
        D3D11_PRIMITIVE_TOPOLOGY_8_CONTROL_POINT_PATCHLIST,
        D3D11_PRIMITIVE_TOPOLOGY_9_CONTROL_POINT_PATCHLIST,
        D3D11_PRIMITIVE_TOPOLOGY_10_CONTROL_POINT_PATCHLIST,
        D3D11_PRIMITIVE_TOPOLOGY_11_CONTROL_POINT_PATCHLIST,
        D3D11_PRIMITIVE_TOPOLOGY_12_CONTROL_POINT_PATCHLIST,
        D3D11_PRIMITIVE_TOPOLOGY_13_CONTROL_POINT_PATCHLIST,
        D3D11_PRIMITIVE_TOPOLOGY_14_CONTROL_POINT_PATCHLIST,
        D3D11_PRIMITIVE_TOPOLOGY_15_CONTROL_POINT_PATCHLIST,
        D3D11_PRIMITIVE_TOPOLOGY_16_CONTROL_POINT_PATCHLIST,
        D3D11_PRIMITIVE_TOPOLOGY_17_CONTROL_POINT_PATCHLIST,
        D3D11_PRIMITIVE_TOPOLOGY_18_CONTROL_POINT_PATCHLIST,
        D3D11_PRIMITIVE_TOPOLOGY_19_CONTROL_POINT_PATCHLIST,
        D3D11_PRIMITIVE_TOPOLOGY_20_CONTROL_POINT_PATCHLIST,
        D3D11_PRIMITIVE_TOPOLOGY_21_CONTROL_POINT_PATCHLIST,
        D3D11_PRIMITIVE_TOPOLOGY_22_CONTROL_POINT_PATCHLIST,
        D3D11_PRIMITIVE_TOPOLOGY_23_CONTROL_POINT_PATCHLIST,
        D3D11_PRIMITIVE_TOPOLOGY_24_CONTROL_POINT_PATCHLIST,
        D3D11_PRIMITIVE_TOPOLOGY_25_CONTROL_POINT_PATCHLIST,
        D3D11_PRIMITIVE_TOPOLOGY_26_CONTROL_POINT_PATCHLIST,
        D3D11_PRIMITIVE_TOPOLOGY_27_CONTROL_POINT_PATCHLIST,
        D3D11_PRIMITIVE_TOPOLOGY_28_CONTROL_POINT_PATCHLIST,
        D3D11_PRIMITIVE_TOPOLOGY_29_CONTROL_POINT_PATCHLIST,
        D3D11_PRIMITIVE_TOPOLOGY_30_CONTROL_POINT_PATCHLIST,
        D3D11_PRIMITIVE_TOPOLOGY_31_CONTROL_POINT_PATCHLIST,
        D3D11_PRIMITIVE_TOPOLOGY_32_CONTROL_POINT_PATCHLIST,
    };
    return aD3D11PrimitiveTopology[primitiveType];
}

static DECLCALLBACK(int) vmsvga3dBackDXSetTopology(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, SVGA3dPrimitiveType topology)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend);

    DXDEVICE *pDevice = &pDXContext->pBackendDXContext->device;
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    D3D11_PRIMITIVE_TOPOLOGY enmTopology = dxTopology(topology);
    pDevice->pImmediateContext->IASetPrimitiveTopology(enmTopology);
    pDXContext->svgaDXContext.inputAssembly.topology = topology;
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackDXSetRenderTargets(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, SVGA3dDepthStencilViewId depthStencilViewId, uint32_t cRenderTargetViewId, SVGA3dRenderTargetViewId const *paRenderTargetViewId)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend);

    DXDEVICE *pDevice = &pDXContext->pBackendDXContext->device;
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    ID3D11RenderTargetView *papRenderTargetViews[SVGA3D_MAX_RENDER_TARGETS];
    for (uint32_t i = 0; i < cRenderTargetViewId; ++i)
    {
        SVGA3dRenderTargetViewId renderTargetViewId = paRenderTargetViewId[i];
        if (renderTargetViewId != SVGA3D_INVALID_ID)
        {
            ASSERT_GUEST_RETURN(renderTargetViewId < pDXContext->pBackendDXContext->cRenderTargetView, VERR_INVALID_PARAMETER);
            papRenderTargetViews[i] = pDXContext->pBackendDXContext->papRenderTargetView[renderTargetViewId];
        }
        else
            papRenderTargetViews[i] = NULL;
    }

    ID3D11DepthStencilView *pDepthStencilView;
    if (depthStencilViewId != SVGA_ID_INVALID)
        pDepthStencilView = pDXContext->pBackendDXContext->papDepthStencilView[depthStencilViewId];
    else
        pDepthStencilView = NULL;

    pDevice->pImmediateContext->OMSetRenderTargets(cRenderTargetViewId, papRenderTargetViews, pDepthStencilView);
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackDXSetBlendState(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, SVGA3dBlendStateId blendId, float const blendFactor[4], uint32_t sampleMask)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend);

    DXDEVICE *pDevice = &pDXContext->pBackendDXContext->device;
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    ID3D11BlendState *pBlendState = pDXContext->pBackendDXContext->papBlendState[blendId];
    pDevice->pImmediateContext->OMSetBlendState(pBlendState, blendFactor, sampleMask);

    pDXContext->svgaDXContext.renderState.blendStateId = blendId;
    memcpy(pDXContext->svgaDXContext.renderState.blendFactor, blendFactor, sizeof(blendFactor));
    pDXContext->svgaDXContext.renderState.sampleMask = sampleMask;

    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackDXSetDepthStencilState(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, SVGA3dDepthStencilStateId depthStencilId, uint32_t stencilRef)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend);

    DXDEVICE *pDevice = &pDXContext->pBackendDXContext->device;
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    ID3D11DepthStencilState *pDepthStencilState = pDXContext->pBackendDXContext->papDepthStencilState[depthStencilId];
    pDevice->pImmediateContext->OMSetDepthStencilState(pDepthStencilState, stencilRef);

    pDXContext->svgaDXContext.renderState.depthStencilStateId = depthStencilId;
    pDXContext->svgaDXContext.renderState.stencilRef = stencilRef;

    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackDXSetRasterizerState(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, SVGA3dRasterizerStateId rasterizerId)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    DXDEVICE *pDevice = &pDXContext->pBackendDXContext->device;
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    RT_NOREF(pBackend);

    pDevice->pImmediateContext->RSSetState(pDXContext->pBackendDXContext->papRasterizerState[rasterizerId]);
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackDXDefineQuery(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXDestroyQuery(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXBindQuery(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXSetQueryOffset(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXBeginQuery(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXEndQuery(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXReadbackQuery(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXSetPredication(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXSetSOTargets(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXSetViewports(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, uint32_t cViewport, SVGA3dViewport const *paViewport)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    DXDEVICE *pDevice = &pDXContext->pBackendDXContext->device;
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    RT_NOREF(pBackend);

    /* D3D11_VIEWPORT is identical to SVGA3dViewport. */
    D3D11_VIEWPORT *pViewports = (D3D11_VIEWPORT *)paViewport;

    pDevice->pImmediateContext->RSSetViewports(cViewport, pViewports);
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackDXSetScissorRects(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, uint32_t cRect, SVGASignedRect const *paRect)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend);

    DXDEVICE *pDevice = &pDXContext->pBackendDXContext->device;
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    /* D3D11_RECT is identical to SVGASignedRect. */
    D3D11_RECT *pRects = (D3D11_RECT *)paRect;

    pDevice->pImmediateContext->RSSetScissorRects(cRect, pRects);
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackDXClearRenderTargetView(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, SVGA3dRenderTargetViewId renderTargetViewId, SVGA3dRGBAFloat const *pRGBA)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend);

    DXDEVICE *pDevice = &pDXContext->pBackendDXContext->device;
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    ID3D11RenderTargetView *pRenderTargetView = pDXContext->pBackendDXContext->papRenderTargetView[renderTargetViewId];
    AssertReturn(pRenderTargetView, VERR_INVALID_STATE);
    pDevice->pImmediateContext->ClearRenderTargetView(pRenderTargetView, pRGBA->value);
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackDXClearDepthStencilView(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, uint32_t flags, SVGA3dDepthStencilViewId depthStencilViewId, float depth, uint8_t stencil)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend);

    DXDEVICE *pDevice = &pDXContext->pBackendDXContext->device;
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    ID3D11DepthStencilView *pDepthStencilView = pDXContext->pBackendDXContext->papDepthStencilView[depthStencilViewId];
    AssertReturn(pDepthStencilView, VERR_INVALID_STATE);
    pDevice->pImmediateContext->ClearDepthStencilView(pDepthStencilView, flags, depth, stencil);
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackDXPredCopyRegion(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, SVGA3dSurfaceId dstSid, uint32_t dstSubResource, SVGA3dSurfaceId srcSid, uint32_t srcSubResource, SVGA3dCopyBox const *pBox)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend);

    DXDEVICE *pDevice = &pDXContext->pBackendDXContext->device;
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    PVMSVGA3DSURFACE pSrcSurface;
    int rc = vmsvga3dSurfaceFromSid(pThisCC->svga.p3dState, srcSid, &pSrcSurface);
    AssertRCReturn(rc, rc);

    PVMSVGA3DSURFACE pDstSurface;
    rc = vmsvga3dSurfaceFromSid(pThisCC->svga.p3dState, dstSid, &pDstSurface);
    AssertRCReturn(rc, rc);

    if (pSrcSurface->pBackendSurface == NULL)
    {
        /* Create the resource. */
        rc = vmsvga3dBackSurfaceCreateTexture(pThisCC->svga.p3dState, pDXContext, pSrcSurface);
        AssertRCReturn(rc, rc);
    }

    if (pDstSurface->pBackendSurface == NULL)
    {
        /* Create the resource. */
        rc = vmsvga3dBackSurfaceCreateTexture(pThisCC->svga.p3dState, pDXContext, pDstSurface);
        AssertRCReturn(rc, rc);
    }

    ID3D11Resource *pDstResource = pDstSurface->pBackendSurface->u.Texture.pTexture;
    UINT DstSubresource = dstSubResource;
    UINT DstX = pBox->x;
    UINT DstY = pBox->y;
    UINT DstZ = pBox->z;

    ID3D11Resource *pSrcResource = pSrcSurface->pBackendSurface->u.Texture.pTexture;
    UINT SrcSubresource = srcSubResource;
    D3D11_BOX SrcBox;
    SrcBox.left   = pBox->x;
    SrcBox.top    = pBox->y;
    SrcBox.front  = pBox->z;
    SrcBox.right  = pBox->x + pBox->w;
    SrcBox.bottom = pBox->y + pBox->h;
    SrcBox.back   = pBox->z + pBox->d;
    pDevice->pImmediateContext->CopySubresourceRegion(pDstResource, DstSubresource, DstX, DstY, DstZ,
                                                      pSrcResource, SrcSubresource, &SrcBox);
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackDXPredCopy(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXPresentBlt(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXGenMips(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, SVGA3dShaderResourceViewId shaderResourceViewId)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend);

    DXDEVICE *pDevice = &pDXContext->pBackendDXContext->device;
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    ID3D11ShaderResourceView *pShaderResourceView = pDXContext->pBackendDXContext->papShaderResourceView[shaderResourceViewId];
    AssertReturn(pShaderResourceView, VERR_INVALID_STATE);
    pDevice->pImmediateContext->GenerateMips(pShaderResourceView);
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackDXUpdateSubResource(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXReadbackSubResource(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXInvalidateSubResource(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXDefineShaderResourceView(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, SVGA3dShaderResourceViewId shaderResourceViewId, SVGACOTableDXSRViewEntry const *pEntry)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend);

    DXDEVICE *pDevice = &pDXContext->pBackendDXContext->device;
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    /* Get corresponding resource for pEntry->sid. Create the surface if does not yet exist. */
    PVMSVGA3DSURFACE pSurface;
    int rc = vmsvga3dSurfaceFromSid(pThisCC->svga.p3dState, pEntry->sid, &pSurface);
    AssertRCReturn(rc, rc);

    if (pSurface->pBackendSurface == NULL)
    {
        /* Create the actual texture. */
        rc = vmsvga3dBackSurfaceCreateTexture(pThisCC->svga.p3dState, pDXContext, pSurface);
        AssertRCReturn(rc, rc);
    }

    HRESULT hr = dxShaderResourceViewCreate(pDevice, pEntry, pSurface->pBackendSurface, &pDXContext->pBackendDXContext->papShaderResourceView[shaderResourceViewId]);
    if (SUCCEEDED(hr))
        return VINF_SUCCESS;
    return VERR_INVALID_STATE;
}


static DECLCALLBACK(int) vmsvga3dBackDXDestroyShaderResourceView(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, SVGA3dShaderResourceViewId shaderResourceViewId)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend);

    D3D_RELEASE(pDXContext->pBackendDXContext->papShaderResourceView[shaderResourceViewId]);
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackDXDefineRenderTargetView(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, SVGA3dRenderTargetViewId renderTargetViewId, SVGACOTableDXRTViewEntry const *pEntry)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend);

    DXDEVICE *pDevice = &pDXContext->pBackendDXContext->device;
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    /* Get corresponding resource for pEntry->sid. Create the surface if does not yet exist. */
    PVMSVGA3DSURFACE pSurface;
    int rc = vmsvga3dSurfaceFromSid(pThisCC->svga.p3dState, pEntry->sid, &pSurface);
    AssertRCReturn(rc, rc);

    if (pSurface->pBackendSurface == NULL)
    {
        /* Create the actual texture. */
        rc = vmsvga3dBackSurfaceCreateRenderTarget(pThisCC->svga.p3dState, pDXContext, pSurface);
        AssertRCReturn(rc, rc);
    }

    HRESULT hr = dxRenderTargetViewCreate(pDevice, pEntry, pSurface->pBackendSurface, &pDXContext->pBackendDXContext->papRenderTargetView[renderTargetViewId]);
    if (SUCCEEDED(hr))
        return VINF_SUCCESS;
    return VERR_INVALID_STATE;
}


static DECLCALLBACK(int) vmsvga3dBackDXDestroyRenderTargetView(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, SVGA3dRenderTargetViewId renderTargetViewId)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend);

    D3D_RELEASE(pDXContext->pBackendDXContext->papRenderTargetView[renderTargetViewId]);
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackDXDefineDepthStencilView(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, SVGA3dDepthStencilViewId depthStencilViewId, SVGACOTableDXDSViewEntry const *pEntry)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend);

    DXDEVICE *pDevice = &pDXContext->pBackendDXContext->device;
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    /* Get corresponding resource for pEntry->sid. Create the surface if does not yet exist. */
    PVMSVGA3DSURFACE pSurface;
    int rc = vmsvga3dSurfaceFromSid(pThisCC->svga.p3dState, pEntry->sid, &pSurface);
    AssertRCReturn(rc, rc);

    if (pSurface->pBackendSurface == NULL)
    {
        /* Create the actual texture. */
        rc = vmsvga3dBackSurfaceCreateDepthStencilTexture(pThisCC->svga.p3dState, pDXContext, pSurface);
        AssertRCReturn(rc, rc);
    }

    HRESULT hr = dxDepthStencilViewCreate(pDevice, pEntry, pSurface->pBackendSurface, &pDXContext->pBackendDXContext->papDepthStencilView[depthStencilViewId]);
    if (SUCCEEDED(hr))
        return VINF_SUCCESS;
    return VERR_INVALID_STATE;
}


static DECLCALLBACK(int) vmsvga3dBackDXDestroyDepthStencilView(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, SVGA3dDepthStencilViewId depthStencilViewId)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend);

    D3D_RELEASE(pDXContext->pBackendDXContext->papDepthStencilView[depthStencilViewId]);
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackDXDefineElementLayout(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, SVGA3dElementLayoutId elementLayoutId, SVGACOTableDXElementLayoutEntry const *pEntry)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    DXDEVICE *pDevice = &pDXContext->pBackendDXContext->device;
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    RT_NOREF(pBackend, elementLayoutId, pEntry);

    /* Not much can be done here because ID3D11Device::CreateInputLayout requires
     * a pShaderBytecodeWithInputSignature which is not known at this moment.
     * InputLayout object will be created in SVGA_3D_CMD_DX_SET_INPUT_LAYOUT.
     */

    DXELEMENTLAYOUT *pDXElementLayout = &pDXContext->pBackendDXContext->paElementLayout[pEntry->elid];
    D3D_RELEASE(pDXElementLayout->pElementLayout);

    /* Semantic name is not interpreted by D3D, therefore arbitrary names can be used
     * if they are consistent between the element layout and shader input signature.
     * "In general, data passed between pipeline stages is completely generic and is not uniquely
     * interpreted by the system; arbitrary semantics are allowed ..."
     *
     * However D3D runtime insists that "SemanticName string ("POSITIO1") cannot end with a number."
     *
     * System-Value semantics ("SV_*") between shaders require proper names of course.
     * But they are irrelevant for input attributes.
     */
    pDXElementLayout->cElementDesc = pEntry->numDescs;
    for (uint32_t i = 0; i < pEntry->numDescs; ++i)
    {
        D3D11_INPUT_ELEMENT_DESC *pDst = &pDXElementLayout->aElementDesc[i];
        SVGA3dInputElementDesc const *pSrc = &pEntry->descs[i];
        pDst->SemanticName         = "ATTRIB";
        pDst->SemanticIndex        = i; /// @todo 'pSrc->inputRegister' is unused, maybe it should somehow.
        pDst->Format               = vmsvgaDXSurfaceFormat2Dxgi(pSrc->format);
        AssertReturn(pDst->Format != DXGI_FORMAT_UNKNOWN, VERR_NOT_IMPLEMENTED);
        pDst->InputSlot            = pSrc->inputSlot;
        pDst->AlignedByteOffset    = pSrc->alignedByteOffset;
        pDst->InputSlotClass       = (D3D11_INPUT_CLASSIFICATION)pSrc->inputSlotClass;
        pDst->InstanceDataStepRate = pSrc->instanceDataStepRate;
    }

    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackDXDestroyElementLayout(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXDefineBlendState(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext,
                                                        SVGA3dBlendStateId blendId, SVGACOTableDXBlendStateEntry const *pEntry)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    DXDEVICE *pDevice = &pDXContext->pBackendDXContext->device;
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    RT_NOREF(pBackend);

    HRESULT hr = dxBlendStateCreate(pDevice, pEntry, &pDXContext->pBackendDXContext->papBlendState[blendId]);
    if (SUCCEEDED(hr))
        return VINF_SUCCESS;
    return VERR_INVALID_STATE;
}


static DECLCALLBACK(int) vmsvga3dBackDXDestroyBlendState(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXDefineDepthStencilState(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, SVGA3dDepthStencilStateId depthStencilId, SVGACOTableDXDepthStencilEntry *pEntry)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    DXDEVICE *pDevice = &pDXContext->pBackendDXContext->device;
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    RT_NOREF(pBackend);

    HRESULT hr = dxDepthStencilStateCreate(pDevice, pEntry, &pDXContext->pBackendDXContext->papDepthStencilState[depthStencilId]);
    if (SUCCEEDED(hr))
        return VINF_SUCCESS;
    return VERR_INVALID_STATE;
}


static DECLCALLBACK(int) vmsvga3dBackDXDestroyDepthStencilState(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXDefineRasterizerState(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, SVGA3dRasterizerStateId rasterizerId, SVGACOTableDXRasterizerStateEntry const *pEntry)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    DXDEVICE *pDevice = &pDXContext->pBackendDXContext->device;
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    RT_NOREF(pBackend);

    HRESULT hr = dxRasterizerStateCreate(pDevice, pEntry, &pDXContext->pBackendDXContext->papRasterizerState[rasterizerId]);
    if (SUCCEEDED(hr))
        return VINF_SUCCESS;
    return VERR_INVALID_STATE;
}


static DECLCALLBACK(int) vmsvga3dBackDXDestroyRasterizerState(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXDefineSamplerState(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, SVGA3dSamplerId samplerId, SVGACOTableDXSamplerEntry const *pEntry)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    DXDEVICE *pDevice = &pDXContext->pBackendDXContext->device;
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    RT_NOREF(pBackend);

    HRESULT hr = dxSamplerStateCreate(pDevice, pEntry, &pDXContext->pBackendDXContext->papSamplerState[samplerId]);
    if (SUCCEEDED(hr))
        return VINF_SUCCESS;
    return VERR_INVALID_STATE;
}


static DECLCALLBACK(int) vmsvga3dBackDXDestroySamplerState(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXDefineShader(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, PVMSVGA3DSHADER pShader)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend);

    DXSHADER *pDXShader = &pDXContext->pBackendDXContext->paShader[pShader->id];
    D3D_RELEASE(pDXShader->pShader);
    pDXShader->enmShaderType = pShader->type;
    pShader->u.pvBackendShader = pDXShader;
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackDXDestroyShader(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXBindShader(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, PVMSVGA3DSHADER pShader, void const *pvShaderBytecode)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    DXDEVICE *pDevice = &pDXContext->pBackendDXContext->device;
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    RT_NOREF(pBackend);

    int rc = VINF_SUCCESS;

    DXSHADER *pDXShader = &pDXContext->pBackendDXContext->paShader[pShader->id];
    if (pDXShader->pvDXBC)
    {
        RTMemFree(pDXShader->pvDXBC);
        pDXShader->pvDXBC = NULL;
        pDXShader->cbDXBC = 0;
    }

    if (pvShaderBytecode)
    {
#ifdef LOG_ENABLED
        Log(("Shader: cid=%u shid=%u type=%d:\n", pDXContext->cid, pShader->id, pDXShader->enmShaderType));
        uint8_t *pu8 = (uint8_t *)pvShaderBytecode;
        for (uint32_t i = 0; i < pShader->cbData; ++i)
        {
            if ((i % 16) == 0)
            {
                if (i > 0)
                    Log((",\n"));

                Log(("    %#04x", pu8[i]));
            }
            else
            {
                Log((", %#04x", pu8[i]));
            }
        }
        Log(("\n"));
#endif

        rc = DXShaderCreateDXBC(&pShader->shaderInfo, pvShaderBytecode, pShader->cbData, &pDXShader->pvDXBC, &pDXShader->cbDXBC);
        if (RT_SUCCESS(rc))
        {
#ifdef LOG_ENABLED
            if (pBackend->pfnD3DDisassemble && LogIs6Enabled())
            {
                ID3D10Blob *pBlob = 0;
                HRESULT hr2 = pBackend->pfnD3DDisassemble(pDXShader->pvDXBC, pDXShader->cbDXBC, 0, NULL, &pBlob);
                if (SUCCEEDED(hr2) && pBlob && pBlob->GetBufferSize())
                {
                    Log6(("Shader: cid=%u shid=%u type=%d:\n%s\n",
                          pDXContext->cid, pShader->id, pDXShader->enmShaderType, pBlob->GetBufferPointer()));
                }
                else
                    AssertFailed();
                D3D_RELEASE(pBlob);
            }
#endif

            HRESULT hr = dxShaderCreate(pDevice, pShader, pDXShader);
            if (FAILED(hr))
                rc = VERR_INVALID_STATE;
        }
        else
            rc = VERR_NO_MEMORY;
    }

    return rc;
}


static DECLCALLBACK(int) vmsvga3dBackDXDefineStreamOutput(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXDestroyStreamOutput(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXSetStreamOutput(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}

static DECLCALLBACK(int) vmsvga3dBackDXSetCOTable(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, SVGACOTableType type, uint32_t cEntries)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    VMSVGA3DBACKENDDXCONTEXT *pBackendDXContext = pDXContext->pBackendDXContext;

    RT_NOREF(pBackend, pDXContext, type);

    int rc = VINF_SUCCESS;

    /* Allocate paBlendState for example. */
    switch (type)
    {
        case SVGA_COTABLE_RTVIEW:
            if (pBackendDXContext->papRenderTargetView)
            {
                DX_RELEASE_ARRAY(pBackendDXContext->cRenderTargetView, pBackendDXContext->papRenderTargetView);
                RTMemFree(pBackendDXContext->papRenderTargetView);
                pBackendDXContext->papRenderTargetView = NULL;
                pBackendDXContext->cRenderTargetView = 0;
            }

            if (cEntries)
            {
                pBackendDXContext->papRenderTargetView = (ID3D11RenderTargetView **)RTMemAllocZ(cEntries * sizeof(pBackendDXContext->papRenderTargetView[0]));
                AssertBreakStmt(pBackendDXContext->papRenderTargetView, rc = VERR_NO_MEMORY);
                pBackendDXContext->cRenderTargetView = cEntries;
            }
            break;
        case SVGA_COTABLE_DSVIEW:
            if (pBackendDXContext->papDepthStencilView)
            {
                DX_RELEASE_ARRAY(pBackendDXContext->cDepthStencilView, pBackendDXContext->papDepthStencilView);
                RTMemFree(pBackendDXContext->papDepthStencilView);
                pBackendDXContext->papDepthStencilView = NULL;
                pBackendDXContext->cDepthStencilView = 0;
            }

            if (cEntries)
            {
                pBackendDXContext->papDepthStencilView = (ID3D11DepthStencilView **)RTMemAllocZ(cEntries * sizeof(pBackendDXContext->papDepthStencilView[0]));
                AssertBreakStmt(pBackendDXContext->papDepthStencilView, rc = VERR_NO_MEMORY);
                pBackendDXContext->cDepthStencilView = cEntries;
            }
            break;
        case SVGA_COTABLE_SRVIEW:
            if (pBackendDXContext->papShaderResourceView)
            {
                DX_RELEASE_ARRAY(pBackendDXContext->cShaderResourceView, pBackendDXContext->papShaderResourceView);
                RTMemFree(pBackendDXContext->papShaderResourceView);
                pBackendDXContext->papShaderResourceView = NULL;
                pBackendDXContext->cShaderResourceView = 0;
            }

            if (cEntries)
            {
                pBackendDXContext->papShaderResourceView = (ID3D11ShaderResourceView **)RTMemAllocZ(cEntries * sizeof(pBackendDXContext->papShaderResourceView[0]));
                AssertBreakStmt(pBackendDXContext->papShaderResourceView, rc = VERR_NO_MEMORY);
                pBackendDXContext->cShaderResourceView = cEntries;
            }
            break;
        case SVGA_COTABLE_ELEMENTLAYOUT:
            if (pBackendDXContext->paElementLayout)
            {
                for (uint32_t i = 0; i < pBackendDXContext->cElementLayout; ++i)
                    D3D_RELEASE(pBackendDXContext->paElementLayout[i].pElementLayout);
                RTMemFree(pBackendDXContext->paElementLayout);
                pBackendDXContext->paElementLayout = NULL;
                pBackendDXContext->cElementLayout = 0;
            }

            if (cEntries)
            {
                pBackendDXContext->paElementLayout = (DXELEMENTLAYOUT *)RTMemAllocZ(cEntries * sizeof(pBackendDXContext->paElementLayout[0]));
                AssertBreakStmt(pBackendDXContext->paElementLayout, rc = VERR_NO_MEMORY);
                pBackendDXContext->cElementLayout = cEntries;
            }
            break;
        case SVGA_COTABLE_BLENDSTATE:
            if (pBackendDXContext->papBlendState)
            {
                DX_RELEASE_ARRAY(pBackendDXContext->cBlendState, pBackendDXContext->papBlendState);
                RTMemFree(pBackendDXContext->papBlendState);
                pBackendDXContext->papBlendState = NULL;
                pBackendDXContext->cBlendState = 0;
            }

            if (cEntries)
            {
                pBackendDXContext->papBlendState = (ID3D11BlendState **)RTMemAllocZ(cEntries * sizeof(pBackendDXContext->papBlendState[0]));
                AssertBreakStmt(pBackendDXContext->papBlendState, rc = VERR_NO_MEMORY);
                pBackendDXContext->cBlendState = cEntries;
            }
            break;
        case SVGA_COTABLE_DEPTHSTENCIL:
            if (pBackendDXContext->papDepthStencilState)
            {
                DX_RELEASE_ARRAY(pBackendDXContext->cDepthStencilState, pBackendDXContext->papDepthStencilState);
                RTMemFree(pBackendDXContext->papDepthStencilState);
                pBackendDXContext->papDepthStencilState = NULL;
                pBackendDXContext->cDepthStencilState = 0;
            }

            if (cEntries)
            {
                pBackendDXContext->papDepthStencilState = (ID3D11DepthStencilState **)RTMemAllocZ(cEntries * sizeof(pBackendDXContext->papDepthStencilState[0]));
                AssertBreakStmt(pBackendDXContext->papDepthStencilState, rc = VERR_NO_MEMORY);
                pBackendDXContext->cDepthStencilState = cEntries;
            }
            break;
        case SVGA_COTABLE_RASTERIZERSTATE:
            if (pBackendDXContext->papRasterizerState)
            {
                DX_RELEASE_ARRAY(pBackendDXContext->cRasterizerState, pBackendDXContext->papRasterizerState);
                RTMemFree(pBackendDXContext->papRasterizerState);
                pBackendDXContext->papRasterizerState = NULL;
                pBackendDXContext->cRasterizerState = 0;
            }

            if (cEntries)
            {
                pBackendDXContext->papRasterizerState = (ID3D11RasterizerState **)RTMemAllocZ(cEntries * sizeof(pBackendDXContext->papRasterizerState[0]));
                AssertBreakStmt(pBackendDXContext->papRasterizerState, rc = VERR_NO_MEMORY);
                pBackendDXContext->cRasterizerState = cEntries;
            }
            break;
        case SVGA_COTABLE_SAMPLER:
            if (pBackendDXContext->papSamplerState)
            {
                DX_RELEASE_ARRAY(pBackendDXContext->cSamplerState, pBackendDXContext->papSamplerState);
                RTMemFree(pBackendDXContext->papSamplerState);
                pBackendDXContext->papSamplerState = NULL;
                pBackendDXContext->cSamplerState = 0;
            }

            if (cEntries)
            {
                pBackendDXContext->papSamplerState = (ID3D11SamplerState **)RTMemAllocZ(cEntries * sizeof(pBackendDXContext->papSamplerState[0]));
                AssertBreakStmt(pBackendDXContext->papSamplerState, rc = VERR_NO_MEMORY);
                pBackendDXContext->cSamplerState = cEntries;
            }
            break;
        case SVGA_COTABLE_STREAMOUTPUT:
            //AssertFailed(); /** @todo Implement */
            break;
        case SVGA_COTABLE_DXQUERY:
            if (pBackendDXContext->papQuery)
            {
                DX_RELEASE_ARRAY(pBackendDXContext->cQuery, pBackendDXContext->papQuery);
                RTMemFree(pBackendDXContext->papQuery);
                pBackendDXContext->papQuery = NULL;
                pBackendDXContext->cQuery = 0;
            }

            if (cEntries)
            {
                pBackendDXContext->papQuery = (ID3D11Query **)RTMemAllocZ(cEntries * sizeof(pBackendDXContext->papQuery[0]));
                AssertBreakStmt(pBackendDXContext->papQuery, rc = VERR_NO_MEMORY);
                pBackendDXContext->cQuery = cEntries;
            }
            break;
        case SVGA_COTABLE_DXSHADER:
            if (pBackendDXContext->paShader)
            {
                for (uint32_t i = 0; i < pBackendDXContext->cShader; ++i)
                    D3D_RELEASE(pBackendDXContext->paShader[i].pShader);
                RTMemFree(pBackendDXContext->paShader);
                pBackendDXContext->paShader = NULL;
                pBackendDXContext->cShader = 0;
            }

            if (cEntries)
            {
                pBackendDXContext->paShader = (DXSHADER *)RTMemAllocZ(cEntries * sizeof(pBackendDXContext->paShader[0]));
                AssertBreakStmt(pBackendDXContext->paShader, rc = VERR_NO_MEMORY);
                pBackendDXContext->cShader = cEntries;
            }
            break;
        case SVGA_COTABLE_UAVIEW:
            AssertFailed(); /** @todo Implement */
            break;
        case SVGA_COTABLE_MAX: break; /* Compiler warning */
    }
    return rc;
}


static DECLCALLBACK(int) vmsvga3dBackDXReadbackCOTable(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXBufferCopy(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXTransferFromBuffer(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXSurfaceCopyAndReadback(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXMoveQuery(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXBindAllQuery(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXReadbackAllQuery(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXPredTransferFromBuffer(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXMobFence64(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXBindAllShader(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXHint(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXBufferUpdate(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXSetVSConstantBufferOffset(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXSetPSConstantBufferOffset(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXSetGSConstantBufferOffset(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXSetHSConstantBufferOffset(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXSetDSConstantBufferOffset(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXSetCSConstantBufferOffset(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXCondBindAllShader(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackScreenCopy(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackGrowOTable(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXGrowCOTable(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackIntraSurfaceCopy(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDefineGBSurface_v3(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXResolveCopy(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXPredResolveCopy(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXPredConvertRegion(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXPredConvert(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackWholeSurfaceCopy(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXDefineUAView(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXDestroyUAView(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXClearUAViewUint(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXClearUAViewFloat(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXCopyStructureCount(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXSetUAViews(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXDrawIndexedInstancedIndirect(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXDrawInstancedIndirect(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXDispatch(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXDispatchIndirect(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackWriteZeroSurface(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackHintZeroSurface(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXTransferToBuffer(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXSetStructureCount(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackLogicOpsBitBlt(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackLogicOpsTransBlt(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackLogicOpsStretchBlt(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackLogicOpsColorFill(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackLogicOpsAlphaBlend(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackLogicOpsClearTypeBlend(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDefineGBSurface_v4(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXSetCSUAViews(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXSetMinLOD(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXDefineStreamOutputWithMob(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXSetShaderIface(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXBindStreamOutput(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackSurfaceStretchBltNonMSToMS(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXBindShaderIface(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


int vmsvga3dQueryInterface(PVGASTATECC pThisCC, char const *pszInterfaceName, void *pvInterfaceFuncs, size_t cbInterfaceFuncs)
{
    PVMSVGA3DSTATE p3dState = pThisCC->svga.p3dState;
    AssertReturn(p3dState, VERR_NOT_SUPPORTED);

    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
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
                p->pfnDXDefineContext             = vmsvga3dBackDXDefineContext;
                p->pfnDXDestroyContext            = vmsvga3dBackDXDestroyContext;
                p->pfnDXBindContext               = vmsvga3dBackDXBindContext;
                p->pfnDXReadbackContext           = vmsvga3dBackDXReadbackContext;
                p->pfnDXInvalidateContext         = vmsvga3dBackDXInvalidateContext;
                p->pfnDXSetSingleConstantBuffer   = vmsvga3dBackDXSetSingleConstantBuffer;
                p->pfnDXSetShaderResources        = vmsvga3dBackDXSetShaderResources;
                p->pfnDXSetShader                 = vmsvga3dBackDXSetShader;
                p->pfnDXSetSamplers               = vmsvga3dBackDXSetSamplers;
                p->pfnDXDraw                      = vmsvga3dBackDXDraw;
                p->pfnDXDrawIndexed               = vmsvga3dBackDXDrawIndexed;
                p->pfnDXDrawInstanced             = vmsvga3dBackDXDrawInstanced;
                p->pfnDXDrawIndexedInstanced      = vmsvga3dBackDXDrawIndexedInstanced;
                p->pfnDXDrawAuto                  = vmsvga3dBackDXDrawAuto;
                p->pfnDXSetInputLayout            = vmsvga3dBackDXSetInputLayout;
                p->pfnDXSetVertexBuffers          = vmsvga3dBackDXSetVertexBuffers;
                p->pfnDXSetIndexBuffer            = vmsvga3dBackDXSetIndexBuffer;
                p->pfnDXSetTopology               = vmsvga3dBackDXSetTopology;
                p->pfnDXSetRenderTargets          = vmsvga3dBackDXSetRenderTargets;
                p->pfnDXSetBlendState             = vmsvga3dBackDXSetBlendState;
                p->pfnDXSetDepthStencilState      = vmsvga3dBackDXSetDepthStencilState;
                p->pfnDXSetRasterizerState        = vmsvga3dBackDXSetRasterizerState;
                p->pfnDXDefineQuery               = vmsvga3dBackDXDefineQuery;
                p->pfnDXDestroyQuery              = vmsvga3dBackDXDestroyQuery;
                p->pfnDXBindQuery                 = vmsvga3dBackDXBindQuery;
                p->pfnDXSetQueryOffset            = vmsvga3dBackDXSetQueryOffset;
                p->pfnDXBeginQuery                = vmsvga3dBackDXBeginQuery;
                p->pfnDXEndQuery                  = vmsvga3dBackDXEndQuery;
                p->pfnDXReadbackQuery             = vmsvga3dBackDXReadbackQuery;
                p->pfnDXSetPredication            = vmsvga3dBackDXSetPredication;
                p->pfnDXSetSOTargets              = vmsvga3dBackDXSetSOTargets;
                p->pfnDXSetViewports              = vmsvga3dBackDXSetViewports;
                p->pfnDXSetScissorRects           = vmsvga3dBackDXSetScissorRects;
                p->pfnDXClearRenderTargetView     = vmsvga3dBackDXClearRenderTargetView;
                p->pfnDXClearDepthStencilView     = vmsvga3dBackDXClearDepthStencilView;
                p->pfnDXPredCopyRegion            = vmsvga3dBackDXPredCopyRegion;
                p->pfnDXPredCopy                  = vmsvga3dBackDXPredCopy;
                p->pfnDXPresentBlt                = vmsvga3dBackDXPresentBlt;
                p->pfnDXGenMips                   = vmsvga3dBackDXGenMips;
                p->pfnDXUpdateSubResource         = vmsvga3dBackDXUpdateSubResource;
                p->pfnDXReadbackSubResource       = vmsvga3dBackDXReadbackSubResource;
                p->pfnDXInvalidateSubResource     = vmsvga3dBackDXInvalidateSubResource;
                p->pfnDXDefineShaderResourceView  = vmsvga3dBackDXDefineShaderResourceView;
                p->pfnDXDestroyShaderResourceView = vmsvga3dBackDXDestroyShaderResourceView;
                p->pfnDXDefineRenderTargetView    = vmsvga3dBackDXDefineRenderTargetView;
                p->pfnDXDestroyRenderTargetView   = vmsvga3dBackDXDestroyRenderTargetView;
                p->pfnDXDefineDepthStencilView    = vmsvga3dBackDXDefineDepthStencilView;
                p->pfnDXDestroyDepthStencilView   = vmsvga3dBackDXDestroyDepthStencilView;
                p->pfnDXDefineElementLayout       = vmsvga3dBackDXDefineElementLayout;
                p->pfnDXDestroyElementLayout      = vmsvga3dBackDXDestroyElementLayout;
                p->pfnDXDefineBlendState          = vmsvga3dBackDXDefineBlendState;
                p->pfnDXDestroyBlendState         = vmsvga3dBackDXDestroyBlendState;
                p->pfnDXDefineDepthStencilState   = vmsvga3dBackDXDefineDepthStencilState;
                p->pfnDXDestroyDepthStencilState  = vmsvga3dBackDXDestroyDepthStencilState;
                p->pfnDXDefineRasterizerState     = vmsvga3dBackDXDefineRasterizerState;
                p->pfnDXDestroyRasterizerState    = vmsvga3dBackDXDestroyRasterizerState;
                p->pfnDXDefineSamplerState        = vmsvga3dBackDXDefineSamplerState;
                p->pfnDXDestroySamplerState       = vmsvga3dBackDXDestroySamplerState;
                p->pfnDXDefineShader              = vmsvga3dBackDXDefineShader;
                p->pfnDXDestroyShader             = vmsvga3dBackDXDestroyShader;
                p->pfnDXBindShader                = vmsvga3dBackDXBindShader;
                p->pfnDXDefineStreamOutput        = vmsvga3dBackDXDefineStreamOutput;
                p->pfnDXDestroyStreamOutput       = vmsvga3dBackDXDestroyStreamOutput;
                p->pfnDXSetStreamOutput           = vmsvga3dBackDXSetStreamOutput;
                p->pfnDXSetCOTable                = vmsvga3dBackDXSetCOTable;
                p->pfnDXReadbackCOTable           = vmsvga3dBackDXReadbackCOTable;
                p->pfnDXBufferCopy                = vmsvga3dBackDXBufferCopy;
                p->pfnDXTransferFromBuffer        = vmsvga3dBackDXTransferFromBuffer;
                p->pfnDXSurfaceCopyAndReadback    = vmsvga3dBackDXSurfaceCopyAndReadback;
                p->pfnDXMoveQuery                 = vmsvga3dBackDXMoveQuery;
                p->pfnDXBindAllQuery              = vmsvga3dBackDXBindAllQuery;
                p->pfnDXReadbackAllQuery          = vmsvga3dBackDXReadbackAllQuery;
                p->pfnDXPredTransferFromBuffer    = vmsvga3dBackDXPredTransferFromBuffer;
                p->pfnDXMobFence64                = vmsvga3dBackDXMobFence64;
                p->pfnDXBindAllShader             = vmsvga3dBackDXBindAllShader;
                p->pfnDXHint                      = vmsvga3dBackDXHint;
                p->pfnDXBufferUpdate              = vmsvga3dBackDXBufferUpdate;
                p->pfnDXSetVSConstantBufferOffset = vmsvga3dBackDXSetVSConstantBufferOffset;
                p->pfnDXSetPSConstantBufferOffset = vmsvga3dBackDXSetPSConstantBufferOffset;
                p->pfnDXSetGSConstantBufferOffset = vmsvga3dBackDXSetGSConstantBufferOffset;
                p->pfnDXSetHSConstantBufferOffset = vmsvga3dBackDXSetHSConstantBufferOffset;
                p->pfnDXSetDSConstantBufferOffset = vmsvga3dBackDXSetDSConstantBufferOffset;
                p->pfnDXSetCSConstantBufferOffset = vmsvga3dBackDXSetCSConstantBufferOffset;
                p->pfnDXCondBindAllShader         = vmsvga3dBackDXCondBindAllShader;
                p->pfnScreenCopy                  = vmsvga3dBackScreenCopy;
                p->pfnGrowOTable                  = vmsvga3dBackGrowOTable;
                p->pfnDXGrowCOTable               = vmsvga3dBackDXGrowCOTable;
                p->pfnIntraSurfaceCopy            = vmsvga3dBackIntraSurfaceCopy;
                p->pfnDefineGBSurface_v3          = vmsvga3dBackDefineGBSurface_v3;
                p->pfnDXResolveCopy               = vmsvga3dBackDXResolveCopy;
                p->pfnDXPredResolveCopy           = vmsvga3dBackDXPredResolveCopy;
                p->pfnDXPredConvertRegion         = vmsvga3dBackDXPredConvertRegion;
                p->pfnDXPredConvert               = vmsvga3dBackDXPredConvert;
                p->pfnWholeSurfaceCopy            = vmsvga3dBackWholeSurfaceCopy;
                p->pfnDXDefineUAView              = vmsvga3dBackDXDefineUAView;
                p->pfnDXDestroyUAView             = vmsvga3dBackDXDestroyUAView;
                p->pfnDXClearUAViewUint           = vmsvga3dBackDXClearUAViewUint;
                p->pfnDXClearUAViewFloat          = vmsvga3dBackDXClearUAViewFloat;
                p->pfnDXCopyStructureCount        = vmsvga3dBackDXCopyStructureCount;
                p->pfnDXSetUAViews                = vmsvga3dBackDXSetUAViews;
                p->pfnDXDrawIndexedInstancedIndirect = vmsvga3dBackDXDrawIndexedInstancedIndirect;
                p->pfnDXDrawInstancedIndirect     = vmsvga3dBackDXDrawInstancedIndirect;
                p->pfnDXDispatch                  = vmsvga3dBackDXDispatch;
                p->pfnDXDispatchIndirect          = vmsvga3dBackDXDispatchIndirect;
                p->pfnWriteZeroSurface            = vmsvga3dBackWriteZeroSurface;
                p->pfnHintZeroSurface             = vmsvga3dBackHintZeroSurface;
                p->pfnDXTransferToBuffer          = vmsvga3dBackDXTransferToBuffer;
                p->pfnDXSetStructureCount         = vmsvga3dBackDXSetStructureCount;
                p->pfnLogicOpsBitBlt              = vmsvga3dBackLogicOpsBitBlt;
                p->pfnLogicOpsTransBlt            = vmsvga3dBackLogicOpsTransBlt;
                p->pfnLogicOpsStretchBlt          = vmsvga3dBackLogicOpsStretchBlt;
                p->pfnLogicOpsColorFill           = vmsvga3dBackLogicOpsColorFill;
                p->pfnLogicOpsAlphaBlend          = vmsvga3dBackLogicOpsAlphaBlend;
                p->pfnLogicOpsClearTypeBlend      = vmsvga3dBackLogicOpsClearTypeBlend;
                p->pfnDefineGBSurface_v4          = vmsvga3dBackDefineGBSurface_v4;
                p->pfnDXSetCSUAViews              = vmsvga3dBackDXSetCSUAViews;
                p->pfnDXSetMinLOD                 = vmsvga3dBackDXSetMinLOD;
                p->pfnDXDefineStreamOutputWithMob = vmsvga3dBackDXDefineStreamOutputWithMob;
                p->pfnDXSetShaderIface            = vmsvga3dBackDXSetShaderIface;
                p->pfnDXBindStreamOutput          = vmsvga3dBackDXBindStreamOutput;
                p->pfnSurfaceStretchBltNonMSToMS  = vmsvga3dBackSurfaceStretchBltNonMSToMS;
                p->pfnDXBindShaderIface           = vmsvga3dBackDXBindShaderIface;
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
