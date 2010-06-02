/** @file
 *
 * VBoxVideo Display D3D User mode dll
 *
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
#include <windows.h>
#include <d3d9types.h>
//#include <d3dtypes.h>
#include <D3dumddi.h>
#include <d3dhal.h>


#include <iprt/initterm.h>
#include <iprt/log.h>
#include <iprt/mem.h>

#include <VBox/Log.h>

#include <VBox/VBoxGuestLib.h>

#include "VBoxDispD3D.h"
#include "VBoxDispD3DCmn.h"

#ifdef VBOXWDDMDISP_DEBUG
# include <stdio.h>
#endif

static FORMATOP gVBoxFormatOps3D[] = {
    {D3DDDIFMT_A8R8G8B8,
        FORMATOP_TEXTURE|FORMATOP_VOLUMETEXTURE|FORMATOP_CUBETEXTURE|FORMATOP_OFFSCREEN_RENDERTARGET|
        FORMATOP_SAME_FORMAT_RENDERTARGET|
        FORMATOP_SAME_FORMAT_UP_TO_ALPHA_RENDERTARGET|
        FORMATOP_CONVERT_TO_ARGB|FORMATOP_OFFSCREENPLAIN|FORMATOP_SRGBREAD|
        FORMATOP_MEMBEROFGROUP_ARGB|
        FORMATOP_SRGBWRITE|FORMATOP_VERTEXTEXTURE, 0, 0, 0},

    {D3DDDIFMT_X8R8G8B8,
        FORMATOP_TEXTURE|FORMATOP_VOLUMETEXTURE|FORMATOP_CUBETEXTURE|FORMATOP_OFFSCREEN_RENDERTARGET|
        FORMATOP_SAME_FORMAT_RENDERTARGET|
        FORMATOP_DISPLAYMODE|FORMATOP_3DACCELERATION|
        FORMATOP_CONVERT_TO_ARGB|FORMATOP_OFFSCREENPLAIN|FORMATOP_SRGBREAD|
        FORMATOP_MEMBEROFGROUP_ARGB|
        FORMATOP_SRGBWRITE|FORMATOP_VERTEXTEXTURE, 0, 0, 0},

    {D3DDDIFMT_A2R10G10B10,
        FORMATOP_TEXTURE|FORMATOP_VOLUMETEXTURE|FORMATOP_CUBETEXTURE|FORMATOP_OFFSCREEN_RENDERTARGET|
        FORMATOP_SAME_FORMAT_RENDERTARGET|
        0|
        FORMATOP_CONVERT_TO_ARGB|FORMATOP_OFFSCREENPLAIN|
        FORMATOP_MEMBEROFGROUP_ARGB|
        FORMATOP_VERTEXTEXTURE, 0, 0, 0},

    {D3DDDIFMT_X1R5G5B5,
        FORMATOP_TEXTURE|FORMATOP_VOLUMETEXTURE|FORMATOP_CUBETEXTURE|FORMATOP_OFFSCREEN_RENDERTARGET|
        FORMATOP_SAME_FORMAT_RENDERTARGET|
        0|
        FORMATOP_CONVERT_TO_ARGB|FORMATOP_OFFSCREENPLAIN|
        FORMATOP_MEMBEROFGROUP_ARGB|
        FORMATOP_VERTEXTEXTURE, 0, 0, 0},

    {D3DDDIFMT_A1R5G5B5,
        FORMATOP_TEXTURE|FORMATOP_VOLUMETEXTURE|FORMATOP_CUBETEXTURE|FORMATOP_OFFSCREEN_RENDERTARGET|
        FORMATOP_SAME_FORMAT_RENDERTARGET|
        FORMATOP_SAME_FORMAT_UP_TO_ALPHA_RENDERTARGET|
        FORMATOP_CONVERT_TO_ARGB|FORMATOP_OFFSCREENPLAIN|
        FORMATOP_MEMBEROFGROUP_ARGB|
        FORMATOP_VERTEXTEXTURE, 0, 0, 0},

    {D3DDDIFMT_A4R4G4B4,
        FORMATOP_TEXTURE|FORMATOP_VOLUMETEXTURE|FORMATOP_CUBETEXTURE|FORMATOP_OFFSCREEN_RENDERTARGET|
        FORMATOP_SAME_FORMAT_RENDERTARGET|
        FORMATOP_SAME_FORMAT_UP_TO_ALPHA_RENDERTARGET|
        FORMATOP_OFFSCREENPLAIN|
        0|
        FORMATOP_VERTEXTEXTURE, 0, 0, 0},

    {D3DDDIFMT_R5G6B5,
        FORMATOP_TEXTURE|FORMATOP_VOLUMETEXTURE|FORMATOP_CUBETEXTURE|FORMATOP_OFFSCREEN_RENDERTARGET|
        FORMATOP_SAME_FORMAT_RENDERTARGET|
        FORMATOP_DISPLAYMODE|FORMATOP_3DACCELERATION|
        FORMATOP_CONVERT_TO_ARGB|FORMATOP_OFFSCREENPLAIN|
        FORMATOP_MEMBEROFGROUP_ARGB|
        FORMATOP_VERTEXTEXTURE, 0, 0, 0},

    {D3DDDIFMT_L16,
        FORMATOP_TEXTURE|FORMATOP_VOLUMETEXTURE|FORMATOP_CUBETEXTURE|
        0|
        0|
        FORMATOP_OFFSCREENPLAIN|
        0|
        FORMATOP_VERTEXTEXTURE, 0, 0, 0},

    {D3DDDIFMT_A8L8,
        FORMATOP_TEXTURE|FORMATOP_VOLUMETEXTURE|FORMATOP_CUBETEXTURE|
        0|
        0|
        FORMATOP_OFFSCREENPLAIN|
        0|
        FORMATOP_VERTEXTEXTURE, 0, 0, 0},

    {D3DDDIFMT_A8,
        FORMATOP_TEXTURE|FORMATOP_VOLUMETEXTURE|FORMATOP_CUBETEXTURE|
        0|
        0|
        FORMATOP_OFFSCREENPLAIN|
        0|
        FORMATOP_VERTEXTEXTURE, 0, 0, 0},

    {D3DDDIFMT_L8,
        FORMATOP_TEXTURE|FORMATOP_VOLUMETEXTURE|FORMATOP_CUBETEXTURE|
        0|
        0|
        FORMATOP_OFFSCREENPLAIN|
        0|
        FORMATOP_VERTEXTEXTURE, 0, 0, 0},

    {D3DDDIFMT_D16,   FORMATOP_TEXTURE|FORMATOP_ZSTENCIL|FORMATOP_ZSTENCIL_WITH_ARBITRARY_COLOR_DEPTH, 0, 0, 0},
    {D3DDDIFMT_D24S8, FORMATOP_TEXTURE|FORMATOP_ZSTENCIL|FORMATOP_ZSTENCIL_WITH_ARBITRARY_COLOR_DEPTH, 0, 0, 0},
    {D3DDDIFMT_D24X8, FORMATOP_TEXTURE|FORMATOP_ZSTENCIL|FORMATOP_ZSTENCIL_WITH_ARBITRARY_COLOR_DEPTH, 0, 0, 0},
    {D3DDDIFMT_D16_LOCKABLE, FORMATOP_ZSTENCIL|FORMATOP_ZSTENCIL_WITH_ARBITRARY_COLOR_DEPTH, 0, 0, 0},
    {D3DDDIFMT_X8D24, FORMATOP_TEXTURE|FORMATOP_ZSTENCIL|FORMATOP_ZSTENCIL_WITH_ARBITRARY_COLOR_DEPTH, 0, 0, 0},
    {D3DDDIFMT_D32F_LOCKABLE, FORMATOP_TEXTURE|FORMATOP_ZSTENCIL|FORMATOP_ZSTENCIL_WITH_ARBITRARY_COLOR_DEPTH, 0, 0, 0},
    {D3DDDIFMT_S8D24, FORMATOP_TEXTURE|FORMATOP_ZSTENCIL|FORMATOP_ZSTENCIL_WITH_ARBITRARY_COLOR_DEPTH, 0, 0, 0},

    {D3DDDIFMT_DXT1,
        FORMATOP_TEXTURE|FORMATOP_CUBETEXTURE|
        0|
        0|
        FORMATOP_OFFSCREENPLAIN|FORMATOP_SRGBREAD|
        0|
        FORMATOP_VERTEXTEXTURE, 0, 0, 0},

    {D3DDDIFMT_DXT2,
        FORMATOP_TEXTURE|FORMATOP_CUBETEXTURE|
        0|
        0|
        FORMATOP_OFFSCREENPLAIN|FORMATOP_SRGBREAD|
        0|
        FORMATOP_VERTEXTEXTURE, 0, 0, 0},

    {D3DDDIFMT_DXT3,
        FORMATOP_TEXTURE|FORMATOP_CUBETEXTURE|
        0|
        0|
        FORMATOP_OFFSCREENPLAIN|FORMATOP_SRGBREAD|
        0|
        FORMATOP_VERTEXTEXTURE, 0, 0, 0},

    {D3DDDIFMT_DXT4,
        FORMATOP_TEXTURE|FORMATOP_CUBETEXTURE|
        0|
        0|
        FORMATOP_OFFSCREENPLAIN|FORMATOP_SRGBREAD|
        0|
        FORMATOP_VERTEXTEXTURE, 0, 0, 0},

    {D3DDDIFMT_DXT5,
        FORMATOP_TEXTURE|FORMATOP_CUBETEXTURE|
        0|
        0|
        FORMATOP_OFFSCREENPLAIN|FORMATOP_SRGBREAD|
        0|
        FORMATOP_VERTEXTEXTURE, 0, 0, 0},

    {D3DDDIFMT_X8L8V8U8,
        FORMATOP_TEXTURE|FORMATOP_VOLUMETEXTURE|FORMATOP_CUBETEXTURE|
        0|
        0|
        0|
        FORMATOP_BUMPMAP|
        FORMATOP_VERTEXTEXTURE, 0, 0, 0},

    {D3DDDIFMT_A2W10V10U10,
        FORMATOP_TEXTURE|FORMATOP_VOLUMETEXTURE|FORMATOP_CUBETEXTURE|
        0|
        0|
        0|
        FORMATOP_BUMPMAP|
        FORMATOP_VERTEXTEXTURE, 0, 0, 0},

    {D3DDDIFMT_V8U8,
        FORMATOP_TEXTURE|FORMATOP_VOLUMETEXTURE|FORMATOP_CUBETEXTURE|
        0|
        0|
        0|
        FORMATOP_BUMPMAP|
        FORMATOP_VERTEXTEXTURE, 0, 0, 0},

    {D3DDDIFMT_Q8W8V8U8,
        FORMATOP_TEXTURE|FORMATOP_VOLUMETEXTURE|FORMATOP_CUBETEXTURE|FORMATOP_OFFSCREEN_RENDERTARGET|
        0|
        0|
        FORMATOP_OFFSCREENPLAIN|
        FORMATOP_BUMPMAP|
        FORMATOP_VERTEXTEXTURE, 0, 0, 0},

    {D3DDDIFMT_CxV8U8, FORMATOP_NOFILTER|FORMATOP_NOALPHABLEND|FORMATOP_NOTEXCOORDWRAPNORMIP, 0, 0, 0},

    {D3DDDIFMT_R16F,
        FORMATOP_TEXTURE|FORMATOP_VOLUMETEXTURE|FORMATOP_CUBETEXTURE|FORMATOP_OFFSCREEN_RENDERTARGET|
        FORMATOP_SAME_FORMAT_RENDERTARGET|
        0|
        FORMATOP_OFFSCREENPLAIN|
        0|
        FORMATOP_VERTEXTEXTURE, 0, 0, 0},

    {D3DDDIFMT_R32F,
        FORMATOP_TEXTURE|FORMATOP_VOLUMETEXTURE|FORMATOP_CUBETEXTURE|FORMATOP_OFFSCREEN_RENDERTARGET|
        FORMATOP_SAME_FORMAT_RENDERTARGET|
        0|
        FORMATOP_OFFSCREENPLAIN|
        0|
        FORMATOP_VERTEXTEXTURE, 0, 0, 0},

    {D3DDDIFMT_G16R16F,
        FORMATOP_TEXTURE|FORMATOP_VOLUMETEXTURE|FORMATOP_CUBETEXTURE|FORMATOP_OFFSCREEN_RENDERTARGET|
        FORMATOP_SAME_FORMAT_RENDERTARGET|
        0|
        FORMATOP_OFFSCREENPLAIN|
        0|
        FORMATOP_VERTEXTEXTURE, 0, 0, 0},

    {D3DDDIFMT_G32R32F,
        FORMATOP_TEXTURE|FORMATOP_VOLUMETEXTURE|FORMATOP_CUBETEXTURE|FORMATOP_OFFSCREEN_RENDERTARGET|
        FORMATOP_SAME_FORMAT_RENDERTARGET|
        0|
        FORMATOP_OFFSCREENPLAIN|
        0|
        FORMATOP_VERTEXTEXTURE, 0, 0, 0},

    {D3DDDIFMT_A16B16G16R16F,
        FORMATOP_TEXTURE|FORMATOP_VOLUMETEXTURE|FORMATOP_CUBETEXTURE|FORMATOP_OFFSCREEN_RENDERTARGET|
        FORMATOP_SAME_FORMAT_RENDERTARGET|
        0|
        FORMATOP_OFFSCREENPLAIN|
        0|
        FORMATOP_VERTEXTEXTURE, 0, 0, 0},

    {D3DDDIFMT_A32B32G32R32F,
        FORMATOP_TEXTURE|FORMATOP_VOLUMETEXTURE|FORMATOP_CUBETEXTURE|FORMATOP_OFFSCREEN_RENDERTARGET|
        FORMATOP_SAME_FORMAT_RENDERTARGET|
        0|
        FORMATOP_OFFSCREENPLAIN|
        0|
        FORMATOP_VERTEXTEXTURE, 0, 0, 0},

    {D3DDDIFMT_G16R16,
        FORMATOP_TEXTURE|FORMATOP_VOLUMETEXTURE|FORMATOP_CUBETEXTURE|FORMATOP_OFFSCREEN_RENDERTARGET|
        FORMATOP_SAME_FORMAT_RENDERTARGET|
        0|
        FORMATOP_OFFSCREENPLAIN|
        0|
        FORMATOP_VERTEXTEXTURE, 0, 0, 0},

    {D3DDDIFMT_A16B16G16R16,
        FORMATOP_TEXTURE|FORMATOP_VOLUMETEXTURE|FORMATOP_CUBETEXTURE|FORMATOP_OFFSCREEN_RENDERTARGET|
        FORMATOP_SAME_FORMAT_RENDERTARGET|
        0|
        FORMATOP_OFFSCREENPLAIN|
        0|
        FORMATOP_VERTEXTEXTURE, 0, 0, 0},

    {D3DDDIFMT_V16U16,
        FORMATOP_TEXTURE|FORMATOP_VOLUMETEXTURE|FORMATOP_CUBETEXTURE|
        0|
        0|
        0|
        FORMATOP_BUMPMAP|
        FORMATOP_VERTEXTEXTURE, 0, 0, 0},

    {D3DDDIFMT_P8, FORMATOP_DISPLAYMODE|FORMATOP_3DACCELERATION|FORMATOP_OFFSCREENPLAIN, 0, 0, 0},

    {D3DDDIFMT_UYVY,
        0|
        0|
        0|
        FORMATOP_CONVERT_TO_ARGB|FORMATOP_OFFSCREENPLAIN|
        FORMATOP_NOFILTER|
        FORMATOP_NOALPHABLEND|
        FORMATOP_NOTEXCOORDWRAPNORMIP, 0, 0, 0},

    {D3DDDIFMT_YUY2,
        0|
        0|
        0|
        FORMATOP_CONVERT_TO_ARGB|FORMATOP_OFFSCREENPLAIN|
        FORMATOP_NOFILTER|
        FORMATOP_NOALPHABLEND|
        FORMATOP_NOTEXCOORDWRAPNORMIP, 0, 0, 0},

    {D3DDDIFMT_Q16W16V16U16,
        FORMATOP_TEXTURE|FORMATOP_VOLUMETEXTURE|FORMATOP_CUBETEXTURE|FORMATOP_OFFSCREEN_RENDERTARGET|
        FORMATOP_SAME_FORMAT_RENDERTARGET|
        0|
        FORMATOP_OFFSCREENPLAIN|
        FORMATOP_BUMPMAP|FORMATOP_DMAP|
        FORMATOP_VERTEXTEXTURE, 0, 0, 0},

    {D3DDDIFMT_X8B8G8R8,
        FORMATOP_TEXTURE|FORMATOP_VOLUMETEXTURE|FORMATOP_CUBETEXTURE|FORMATOP_OFFSCREEN_RENDERTARGET|
        FORMATOP_SAME_FORMAT_RENDERTARGET|
        FORMATOP_SAME_FORMAT_UP_TO_ALPHA_RENDERTARGET|
        FORMATOP_CONVERT_TO_ARGB|FORMATOP_OFFSCREENPLAIN|FORMATOP_SRGBREAD|
        FORMATOP_DMAP|FORMATOP_MEMBEROFGROUP_ARGB|
        FORMATOP_SRGBWRITE|FORMATOP_AUTOGENMIPMAP|FORMATOP_VERTEXTEXTURE|
        FORMATOP_OVERLAY, 0, 0, 0},

    {D3DDDIFMT_BINARYBUFFER, FORMATOP_OFFSCREENPLAIN, 0, 0, 0},

    {D3DDDIFMT_A4L4,
        FORMATOP_TEXTURE|FORMATOP_VOLUMETEXTURE|FORMATOP_CUBETEXTURE|
        0|
        0|
        FORMATOP_CONVERT_TO_ARGB|FORMATOP_OFFSCREENPLAIN|
        FORMATOP_DMAP|
        FORMATOP_VERTEXTEXTURE, 0, 0, 0},

    {D3DDDIFMT_A2B10G10R10,
        FORMATOP_TEXTURE|FORMATOP_VOLUMETEXTURE|FORMATOP_CUBETEXTURE|FORMATOP_OFFSCREEN_RENDERTARGET|
        FORMATOP_SAME_FORMAT_RENDERTARGET|
        0|
        FORMATOP_CONVERT_TO_ARGB|FORMATOP_OFFSCREENPLAIN|
        FORMATOP_DMAP|FORMATOP_MEMBEROFGROUP_ARGB|
        FORMATOP_AUTOGENMIPMAP|FORMATOP_VERTEXTEXTURE, 0, 0, 0},
};

static FORMATOP gVBoxFormatOpsBase[] = {
    {D3DDDIFMT_X8R8G8B8, FORMATOP_DISPLAYMODE, 0, 0, 0},

    {D3DDDIFMT_R8G8B8, FORMATOP_DISPLAYMODE, 0, 0, 0},

    {D3DDDIFMT_R5G6B5, FORMATOP_DISPLAYMODE, 0, 0, 0},

    {D3DDDIFMT_P8, FORMATOP_DISPLAYMODE, 0, 0, 0},
};

static DDSURFACEDESC gVBoxSurfDescsBase[] = {
        {
            sizeof (DDSURFACEDESC), /*    DWORD   dwSize;  */
            DDSD_CAPS | DDSD_PIXELFORMAT,    /* DWORD dwFlags;    */
            0,    /* DWORD dwHeight;   */
            0,    /* DWORD dwWidth;    */
            {
                0, /* Union             */
                   /*   LONG lPitch; */
                   /*   DWORD dwLinearSize; */
            },
            0,  /*    DWORD dwBackBufferCount; */
            {
                0, /* Union */
                   /*  DWORD dwMipMapCount; */
                   /*    DWORD dwZBufferBitDepth; */
                   /*   DWORD dwRefreshRate; */
            },
            0, /*    DWORD dwAlphaBitDepth; */
            0, /*   DWORD dwReserved; */
            NULL, /*   LPVOID lpSurface; */
            {
                0, /* DWORD dwColorSpaceLowValue; */
                0, /* DWORD dwColorSpaceHighValue; */
            }, /* DDCOLORKEY  ddckCKDestOverlay; */
            {
                0, /* DWORD dwColorSpaceLowValue; */
                0, /* DWORD dwColorSpaceHighValue; */
            }, /* DDCOLORKEY  ddckCKDestBlt; */
            {
                0, /* DWORD dwColorSpaceLowValue; */
                0, /* DWORD dwColorSpaceHighValue; */
            }, /* DDCOLORKEY  ddckCKSrcOverlay; */
            {
                0, /* DWORD dwColorSpaceLowValue; */
                0, /* DWORD dwColorSpaceHighValue; */
            }, /* DDCOLORKEY ddckCKSrcBlt; */
            {
                sizeof (DDPIXELFORMAT), /* DWORD dwSize; */
                DDPF_RGB, /* DWORD dwFlags; */
                0, /* DWORD dwFourCC; */
                {
                    32, /* union */
                       /* DWORD dwRGBBitCount; */
                       /* DWORD dwYUVBitCount; */
                       /* DWORD dwZBufferBitDepth; */
                       /* DWORD dwAlphaBitDepth; */
                       /* DWORD dwLuminanceBitCount; */
                       /* DWORD dwBumpBitCount; */
                },
                {
                    0xff0000, /* union */
                       /* DWORD dwRBitMask; */
                       /* DWORD dwYBitMask; */
                        /* DWORD dwStencilBitDepth; */
                        /* DWORD dwLuminanceBitMask; */
                        /* DWORD dwBumpDuBitMask; */
                },
                {
                    0xff00,
                        /* DWORD dwGBitMask; */
                        /* DWORD dwUBitMask; */
                        /* DWORD dwZBitMask; */
                        /* DWORD dwBumpDvBitMask; */
                },
                {
                    0xff,
                        /* DWORD dwBBitMask; */
                        /* DWORD dwVBitMask; */
                        /* DWORD dwStencilBitMask; */
                        /* DWORD dwBumpLuminanceBitMask; */
                },
                {
                    0,
                        /* DWORD dwRGBAlphaBitMask; */
                        /* DWORD dwYUVAlphaBitMask; */
                        /* DWORD dwLuminanceAlphaBitMask; */
                        /* DWORD dwRGBZBitMask; */
                        /* DWORD dwYUVZBitMask; */
                },
            }, /* DDPIXELFORMAT ddpfPixelFormat; */
            {
                DDSCAPS_BACKBUFFER
                | DDSCAPS_COMPLEX
                | DDSCAPS_FLIP
                | DDSCAPS_FRONTBUFFER
                | DDSCAPS_LOCALVIDMEM
                | DDSCAPS_PRIMARYSURFACE
                | DDSCAPS_VIDEOMEMORY
                | DDSCAPS_VISIBLE   /* DWORD dwCaps; */
            } /* DDSCAPS ddsCaps; */
        },
        {
            sizeof (DDSURFACEDESC), /*    DWORD   dwSize;  */
            DDSD_CAPS | DDSD_PIXELFORMAT,    /* DWORD dwFlags;    */
            0,    /* DWORD dwHeight;   */
            0,    /* DWORD dwWidth;    */
            {
                0, /* Union             */
                   /*   LONG lPitch; */
                   /*   DWORD dwLinearSize; */
            },
            0,  /*    DWORD dwBackBufferCount; */
            {
                0, /* Union */
                   /*  DWORD dwMipMapCount; */
                   /*    DWORD dwZBufferBitDepth; */
                   /*   DWORD dwRefreshRate; */
            },
            0, /*    DWORD dwAlphaBitDepth; */
            0, /*   DWORD dwReserved; */
            NULL, /*   LPVOID lpSurface; */
            {
                0, /* DWORD dwColorSpaceLowValue; */
                0, /* DWORD dwColorSpaceHighValue; */
            }, /* DDCOLORKEY  ddckCKDestOverlay; */
            {
                0, /* DWORD dwColorSpaceLowValue; */
                0, /* DWORD dwColorSpaceHighValue; */
            }, /* DDCOLORKEY  ddckCKDestBlt; */
            {
                0, /* DWORD dwColorSpaceLowValue; */
                0, /* DWORD dwColorSpaceHighValue; */
            }, /* DDCOLORKEY  ddckCKSrcOverlay; */
            {
                0, /* DWORD dwColorSpaceLowValue; */
                0, /* DWORD dwColorSpaceHighValue; */
            }, /* DDCOLORKEY ddckCKSrcBlt; */
            {
                sizeof (DDPIXELFORMAT), /* DWORD dwSize; */
                DDPF_RGB, /* DWORD dwFlags; */
                0, /* DWORD dwFourCC; */
                {
                    24, /* union */
                       /* DWORD dwRGBBitCount; */
                       /* DWORD dwYUVBitCount; */
                       /* DWORD dwZBufferBitDepth; */
                       /* DWORD dwAlphaBitDepth; */
                       /* DWORD dwLuminanceBitCount; */
                       /* DWORD dwBumpBitCount; */
                },
                {
                    0xff0000, /* union */
                       /* DWORD dwRBitMask; */
                       /* DWORD dwYBitMask; */
                        /* DWORD dwStencilBitDepth; */
                        /* DWORD dwLuminanceBitMask; */
                        /* DWORD dwBumpDuBitMask; */
                },
                {
                    0xff00,
                        /* DWORD dwGBitMask; */
                        /* DWORD dwUBitMask; */
                        /* DWORD dwZBitMask; */
                        /* DWORD dwBumpDvBitMask; */
                },
                {
                    0xff,
                        /* DWORD dwBBitMask; */
                        /* DWORD dwVBitMask; */
                        /* DWORD dwStencilBitMask; */
                        /* DWORD dwBumpLuminanceBitMask; */
                },
                {
                    0,
                        /* DWORD dwRGBAlphaBitMask; */
                        /* DWORD dwYUVAlphaBitMask; */
                        /* DWORD dwLuminanceAlphaBitMask; */
                        /* DWORD dwRGBZBitMask; */
                        /* DWORD dwYUVZBitMask; */
                },
            }, /* DDPIXELFORMAT ddpfPixelFormat; */
            {
                DDSCAPS_BACKBUFFER
                | DDSCAPS_COMPLEX
                | DDSCAPS_FLIP
                | DDSCAPS_FRONTBUFFER
                | DDSCAPS_LOCALVIDMEM
                | DDSCAPS_PRIMARYSURFACE
                | DDSCAPS_VIDEOMEMORY
                | DDSCAPS_VISIBLE  /* DWORD dwCaps; */
            } /* DDSCAPS ddsCaps; */
        },
        {
            sizeof (DDSURFACEDESC), /*    DWORD   dwSize;  */
            DDSD_CAPS | DDSD_PIXELFORMAT,    /* DWORD dwFlags;    */
            0,    /* DWORD dwHeight;   */
            0,    /* DWORD dwWidth;    */
            {
                0, /* Union             */
                   /*   LONG lPitch; */
                   /*   DWORD dwLinearSize; */
            },
            0,  /*    DWORD dwBackBufferCount; */
            {
                0, /* Union */
                   /*  DWORD dwMipMapCount; */
                   /*    DWORD dwZBufferBitDepth; */
                   /*   DWORD dwRefreshRate; */
            },
            0, /*    DWORD dwAlphaBitDepth; */
            0, /*   DWORD dwReserved; */
            NULL, /*   LPVOID lpSurface; */
            {
                0, /* DWORD dwColorSpaceLowValue; */
                0, /* DWORD dwColorSpaceHighValue; */
            }, /* DDCOLORKEY  ddckCKDestOverlay; */
            {
                0, /* DWORD dwColorSpaceLowValue; */
                0, /* DWORD dwColorSpaceHighValue; */
            }, /* DDCOLORKEY  ddckCKDestBlt; */
            {
                0, /* DWORD dwColorSpaceLowValue; */
                0, /* DWORD dwColorSpaceHighValue; */
            }, /* DDCOLORKEY  ddckCKSrcOverlay; */
            {
                0, /* DWORD dwColorSpaceLowValue; */
                0, /* DWORD dwColorSpaceHighValue; */
            }, /* DDCOLORKEY ddckCKSrcBlt; */
            {
                sizeof (DDPIXELFORMAT), /* DWORD dwSize; */
                DDPF_RGB, /* DWORD dwFlags; */
                0, /* DWORD dwFourCC; */
                {
                    16, /* union */
                       /* DWORD dwRGBBitCount; */
                       /* DWORD dwYUVBitCount; */
                       /* DWORD dwZBufferBitDepth; */
                       /* DWORD dwAlphaBitDepth; */
                       /* DWORD dwLuminanceBitCount; */
                       /* DWORD dwBumpBitCount; */
                },
                {
                    0xf800, /* union */
                       /* DWORD dwRBitMask; */
                       /* DWORD dwYBitMask; */
                        /* DWORD dwStencilBitDepth; */
                        /* DWORD dwLuminanceBitMask; */
                        /* DWORD dwBumpDuBitMask; */
                },
                {
                    0x7e0,
                        /* DWORD dwGBitMask; */
                        /* DWORD dwUBitMask; */
                        /* DWORD dwZBitMask; */
                        /* DWORD dwBumpDvBitMask; */
                },
                {
                    0x1f,
                        /* DWORD dwBBitMask; */
                        /* DWORD dwVBitMask; */
                        /* DWORD dwStencilBitMask; */
                        /* DWORD dwBumpLuminanceBitMask; */
                },
                {
                    0,
                        /* DWORD dwRGBAlphaBitMask; */
                        /* DWORD dwYUVAlphaBitMask; */
                        /* DWORD dwLuminanceAlphaBitMask; */
                        /* DWORD dwRGBZBitMask; */
                        /* DWORD dwYUVZBitMask; */
                },
            }, /* DDPIXELFORMAT ddpfPixelFormat; */
            {
                DDSCAPS_BACKBUFFER
                | DDSCAPS_COMPLEX
                | DDSCAPS_FLIP
                | DDSCAPS_FRONTBUFFER
                | DDSCAPS_LOCALVIDMEM
                | DDSCAPS_PRIMARYSURFACE
                | DDSCAPS_VIDEOMEMORY
                | DDSCAPS_VISIBLE /* DWORD dwCaps; */
            } /* DDSCAPS ddsCaps; */
        },
};

#ifdef VBOX_WITH_VIDEOHWACCEL

static bool vboxVhwaIsEnabled(PVBOXWDDMDISP_ADAPTER pAdapter)
{
    for (uint32_t i = 0; i < pAdapter->cHeads; ++i)
    {
        if (pAdapter->aHeads[i].Vhwa.Settings.fFlags & VBOXVHWA_F_ENABLED)
            return true;
    }
    return false;
}

static bool vboxVhwaHasCKeying(PVBOXWDDMDISP_ADAPTER pAdapter)
{
    for (uint32_t i = 0; i < pAdapter->cHeads; ++i)
    {
        VBOXVHWA_INFO* pSettings = &pAdapter->aHeads[i].Vhwa.Settings;
        if ((pSettings->fFlags & VBOXVHWA_F_ENABLED)
                && ((pSettings->fFlags & VBOXVHWA_F_CKEY_DST)
                        || (pSettings->fFlags & VBOXVHWA_F_CKEY_SRC))
               )
            return true;
    }
    return false;
}

static void vboxVhwaPopulateOverlayFourccSurfDesc(DDSURFACEDESC *pDesc, uint32_t fourcc)
{
    memset(pDesc, 0, sizeof (DDSURFACEDESC));

    pDesc->dwSize = sizeof (DDSURFACEDESC);
    pDesc->dwFlags = DDSD_CAPS | DDSD_PIXELFORMAT;
    pDesc->ddpfPixelFormat.dwSize = sizeof (DDPIXELFORMAT);
    pDesc->ddpfPixelFormat.dwFlags = DDPF_FOURCC;
    pDesc->ddpfPixelFormat.dwFourCC = fourcc;
    pDesc->ddsCaps.dwCaps = DDSCAPS_BACKBUFFER
            | DDSCAPS_COMPLEX
            | DDSCAPS_FLIP
            | DDSCAPS_FRONTBUFFER
            | DDSCAPS_LOCALVIDMEM
            | DDSCAPS_OVERLAY
            | DDSCAPS_VIDEOMEMORY
            | DDSCAPS_VISIBLE;
}

#endif

static bool vboxPixFormatMatch(DDPIXELFORMAT *pFormat1, DDPIXELFORMAT *pFormat2)
{
    return !memcmp(pFormat1, pFormat2, sizeof (DDPIXELFORMAT));
}

int vboxSurfDescMerge(DDSURFACEDESC *paDescs, uint32_t *pcDescs, uint32_t cMaxDescs, DDSURFACEDESC *pDesc)
{
    uint32_t cDescs = *pcDescs;

    Assert(cMaxDescs >= cDescs);
    Assert(pDesc->dwFlags == (DDSD_CAPS | DDSD_PIXELFORMAT));
    if (pDesc->dwFlags != (DDSD_CAPS | DDSD_PIXELFORMAT))
        return VERR_INVALID_PARAMETER;

    for (uint32_t i = 0; i < cDescs; ++i)
    {
        DDSURFACEDESC *pCur = &paDescs[i];
        if (vboxPixFormatMatch(&pCur->ddpfPixelFormat, &pDesc->ddpfPixelFormat))
        {
            if (pDesc->dwFlags & DDSD_CAPS)
            {
                pCur->dwFlags |= DDSD_CAPS;
                pCur->ddsCaps.dwCaps |= pDesc->ddsCaps.dwCaps;
            }
            return VINF_SUCCESS;
        }
    }

    if (cMaxDescs > cDescs)
    {
        paDescs[cDescs] = *pDesc;
        ++cDescs;
        *pcDescs = cDescs;
        return VINF_SUCCESS;
    }
    return VERR_BUFFER_OVERFLOW;
}

int vboxFormatOpsMerge(FORMATOP *paOps, uint32_t *pcOps, uint32_t cMaxOps, FORMATOP *pOp)
{
    uint32_t cOps = *pcOps;

    Assert(cMaxOps >= cOps);

    for (uint32_t i = 0; i < cOps; ++i)
    {
        FORMATOP *pCur = &paOps[i];
        if (pCur->Format == pOp->Format)
        {
            pCur->Operations |= pOp->Operations;
            Assert(pCur->FlipMsTypes == pOp->FlipMsTypes);
            Assert(pCur->BltMsTypes == pOp->BltMsTypes);
            Assert(pCur->PrivateFormatBitCount == pOp->PrivateFormatBitCount);
            return VINF_SUCCESS;
        }
    }

    if (cMaxOps > cOps)
    {
        paOps[cOps] = *pOp;
        ++cOps;
        *pcOps = cOps;
        return VINF_SUCCESS;
    }
    return VERR_BUFFER_OVERFLOW;
}

int vboxCapsInit(PVBOXWDDMDISP_ADAPTER pAdapter)
{
    pAdapter->cFormstOps = 0;
    pAdapter->paFormstOps = NULL;
    pAdapter->cSurfDescs = 0;
    pAdapter->paSurfDescs = NULL;

    if (pAdapter->uIfVersion > 7)
    {
        if (pAdapter->pD3D9If)
        {
            pAdapter->paFormstOps = (FORMATOP*)RTMemAllocZ(sizeof (gVBoxFormatOps3D));
            Assert(pAdapter->paFormstOps);
            if (pAdapter->paFormstOps)
            {
                memcpy (pAdapter->paFormstOps , gVBoxFormatOps3D, sizeof (gVBoxFormatOps3D));
                pAdapter->cFormstOps = RT_ELEMENTS(gVBoxFormatOps3D);
            }
            else
                return VERR_OUT_OF_RESOURCES;

            /* @todo: do we need surface caps here ? */
        }
    }
#ifdef VBOX_WITH_VIDEOHWACCEL
    else
    {
        /* just calc the max number of formats */
        uint32_t cFormats = RT_ELEMENTS(gVBoxFormatOpsBase);
        uint32_t cSurfDescs = RT_ELEMENTS(gVBoxSurfDescsBase);
        uint32_t cOverlayFormats = 0;
        for (uint32_t i = 0; i < pAdapter->cHeads; ++i)
        {
            VBOXDISPVHWA_INFO *pVhwa = &pAdapter->aHeads[i].Vhwa;
            if (pVhwa->Settings.fFlags & VBOXVHWA_F_ENABLED)
            {
                cOverlayFormats += pVhwa->Settings.cFormats;
            }
        }

        cFormats += cOverlayFormats;
        cSurfDescs += cOverlayFormats;

        uint32_t cbFormatOps = cFormats * sizeof (FORMATOP);
        cbFormatOps = (cbFormatOps + 7) & ~3;
        /* ensure the surf descs are 8 byte alligned */
        uint32_t offSurfDescs = (cbFormatOps + 7) & ~3;
        uint32_t cbSurfDescs = cSurfDescs * sizeof (DDSURFACEDESC);
        uint32_t cbBuf = offSurfDescs + cbSurfDescs;
        uint8_t* pvBuf = (uint8_t*)RTMemAllocZ(cbBuf);
        Assert(pvBuf);
        if (pvBuf)
        {
            pAdapter->paFormstOps = (FORMATOP*)pvBuf;
            memcpy (pAdapter->paFormstOps , gVBoxFormatOpsBase, sizeof (gVBoxFormatOpsBase));
            pAdapter->cFormstOps = RT_ELEMENTS(gVBoxFormatOpsBase);

            FORMATOP fo = {D3DDDIFMT_UNKNOWN, 0, 0, 0, 0};
            for (uint32_t i = 0; i < pAdapter->cHeads; ++i)
            {
                VBOXDISPVHWA_INFO *pVhwa = &pAdapter->aHeads[i].Vhwa;
                if (pVhwa->Settings.fFlags & VBOXVHWA_F_ENABLED)
                {
                    for (uint32_t j = 0; j < pVhwa->Settings.cFormats; ++j)
                    {
                        fo.Format = pVhwa->Settings.aFormats[j];
                        fo.Operations = FORMATOP_OVERLAY;
                        int rc = vboxFormatOpsMerge(pAdapter->paFormstOps, &pAdapter->cFormstOps, cFormats, &fo);
                        AssertRC(rc);
                    }
                }
            }

            pAdapter->paSurfDescs = (DDSURFACEDESC*)(pvBuf + offSurfDescs);
            memcpy (pAdapter->paSurfDescs , gVBoxSurfDescsBase, sizeof (gVBoxSurfDescsBase));
            pAdapter->cSurfDescs = RT_ELEMENTS(gVBoxSurfDescsBase);

            DDSURFACEDESC sd;
            for (uint32_t i = 0; i < pAdapter->cHeads; ++i)
            {
                VBOXDISPVHWA_INFO *pVhwa = &pAdapter->aHeads[i].Vhwa;
                if (pVhwa->Settings.fFlags & VBOXVHWA_F_ENABLED)
                {
                    for (uint32_t j = 0; j < pVhwa->Settings.cFormats; ++j)
                    {
                        uint32_t fourcc = vboxWddmFormatToFourcc(pVhwa->Settings.aFormats[j]);
                        if (fourcc)
                        {
                            vboxVhwaPopulateOverlayFourccSurfDesc(&sd, fourcc);
                            int rc = vboxSurfDescMerge(pAdapter->paSurfDescs, &pAdapter->cSurfDescs, cSurfDescs, &sd);
                            AssertRC(rc);
                        }
                    }
                }
            }
        }
        else
            return VERR_OUT_OF_RESOURCES;
    }
#endif

    return VINF_SUCCESS;
}

void vboxCapsFree(PVBOXWDDMDISP_ADAPTER pAdapter)
{
    if (pAdapter->paFormstOps)
        RTMemFree(pAdapter->paFormstOps);
}

static PVBOXWDDMDISP_RESOURCE vboxResourceAlloc(UINT cAllocs)
{
    PVBOXWDDMDISP_RESOURCE pRc = (PVBOXWDDMDISP_RESOURCE)RTMemAllocZ(RT_OFFSETOF(VBOXWDDMDISP_RESOURCE, aAllocations[cAllocs]));
    Assert(pRc);
    if (pRc)
    {
        pRc->cAllocations = cAllocs;
        return pRc;
    }
    return NULL;
}

static void vboxResourceFree(PVBOXWDDMDISP_RESOURCE pRc)
{
    RTMemFree(pRc);
}

/**
 * DLL entry point.
 */
BOOL WINAPI DllMain(HINSTANCE hInstance,
                    DWORD     dwReason,
                    LPVOID    lpReserved)
{
    switch (dwReason)
    {
        case DLL_PROCESS_ATTACH:
        {
            RTR3Init();

            vboxVDbgPrint(("VBoxDispD3D: DLL loaded.\n"));

//                VbglR3Init();
            break;
        }

        case DLL_PROCESS_DETACH:
        {
            vboxVDbgPrint(("VBoxDispD3D: DLL unloaded.\n"));
//                VbglR3Term();
            /// @todo RTR3Term();
            break;
        }

        default:
            break;
    }
    return TRUE;
}

static HRESULT APIENTRY vboxWddmDispGetCaps (HANDLE hAdapter, CONST D3DDDIARG_GETCAPS* pData)
{
    vboxVDbgPrint(("==> "__FUNCTION__", hAdapter(0x%p), caps type(%d)\n", hAdapter, pData->Type));

    HRESULT hr = S_OK;
    PVBOXWDDMDISP_ADAPTER pAdapter = (PVBOXWDDMDISP_ADAPTER)hAdapter;

    switch (pData->Type)
    {
        case D3DDDICAPS_DDRAW:
        {
            Assert(pData->DataSize >= sizeof (DDRAW_CAPS));
            if (pData->DataSize >= sizeof (DDRAW_CAPS))
            {
                memset(pData->pData, 0, sizeof (DDRAW_CAPS));
#ifdef VBOX_WITH_VIDEOHWACCEL
                if (vboxVhwaHasCKeying(pAdapter))
                {
                    DDRAW_CAPS *pCaps = (DDRAW_CAPS*)pData->pData;
                    pCaps->Caps |= DDRAW_CAPS_COLORKEY
                            | DDRAW_CAPS2_DYNAMICTEXTURES
                            | DDRAW_CAPS2_FLIPNOVSYNC
                            ;
                }
#endif
            }
            else
                hr = E_INVALIDARG;
            break;
        }
        case D3DDDICAPS_DDRAW_MODE_SPECIFIC:
        {
            Assert(pData->DataSize >= sizeof (DDRAW_MODE_SPECIFIC_CAPS));
            if (pData->DataSize >= sizeof (DDRAW_MODE_SPECIFIC_CAPS))
            {
                DDRAW_MODE_SPECIFIC_CAPS * pCaps = (DDRAW_MODE_SPECIFIC_CAPS*)pData->pData;
                memset(&pCaps->Caps /* do not cleanup the first "Head" field,
                                    zero starting with the one following "Head", i.e. Caps */,
                        0, sizeof (DDRAW_MODE_SPECIFIC_CAPS) - RT_OFFSETOF(DDRAW_MODE_SPECIFIC_CAPS, Caps));
#ifdef VBOX_WITH_VIDEOHWACCEL
                VBOXVHWA_INFO *pSettings = &pAdapter->aHeads[pCaps->Head].Vhwa.Settings;
                if (pSettings->fFlags & VBOXVHWA_F_ENABLED)
                {
                    pCaps->Caps |= MODE_CAPS_OVERLAY | MODE_CAPS_OVERLAYSTRETCH;

                    if (pSettings->fFlags & VBOXVHWA_F_CKEY_DST)
                    {
                        pCaps->CKeyCaps |= MODE_CKEYCAPS_DESTOVERLAY
                                | MODE_CKEYCAPS_DESTOVERLAYYUV /* ?? */
                                ;
                    }

                    if (pSettings->fFlags & VBOXVHWA_F_CKEY_SRC)
                    {
                        pCaps->CKeyCaps |= MODE_CKEYCAPS_SRCOVERLAY
                                | MODE_CKEYCAPS_SRCOVERLAYCLRSPACE /* ?? */
                                | MODE_CKEYCAPS_SRCOVERLAYCLRSPACEYUV /* ?? */
                                | MODE_CKEYCAPS_SRCOVERLAYYUV /* ?? */
                                ;
                    }

                    pCaps->FxCaps = MODE_FXCAPS_OVERLAYSHRINKX
                            | MODE_FXCAPS_OVERLAYSHRINKY
                            | MODE_FXCAPS_OVERLAYSTRETCHX
                            | MODE_FXCAPS_OVERLAYSTRETCHY;


                    pCaps->MaxVisibleOverlays = pSettings->cOverlaysSupported;
                    pCaps->MinOverlayStretch = 1;
                    pCaps->MaxOverlayStretch = 32000;
                }
#endif
            }
            else
                hr = E_INVALIDARG;
            break;
        }
        case D3DDDICAPS_GETFORMATCOUNT:
            *((uint32_t*)pData->pData) = pAdapter->cFormstOps;
            break;
        case D3DDDICAPS_GETFORMATDATA:
            Assert(pData->DataSize >= pAdapter->cFormstOps * sizeof (FORMATOP));
            memcpy(pData->pData, pAdapter->paFormstOps, pAdapter->cFormstOps * sizeof (FORMATOP));
            break;
        case D3DDDICAPS_GETD3DQUERYCOUNT:
            *((uint32_t*)pData->pData) = 0;//VBOX_QUERYTYPE_COUNT();
            break;
        case D3DDDICAPS_GETD3DQUERYDATA:
            AssertBreakpoint();
            memset(pData->pData, 0, pData->DataSize);
//            Assert(pData->DataSize >= VBOX_QUERYTYPE_COUNT() * sizeof (D3DDDIQUERYTYPE));
//            memcpy(pData->pData, gVBoxQueryTypes, VBOX_QUERYTYPE_COUNT() * sizeof (D3DDDIQUERYTYPE));
            break;
        case D3DDDICAPS_GETD3D3CAPS:
            Assert(pData->DataSize >= sizeof (D3DHAL_GLOBALDRIVERDATA));
            if (pData->DataSize >= sizeof (D3DHAL_GLOBALDRIVERDATA))
            {
                D3DHAL_GLOBALDRIVERDATA *pCaps = (D3DHAL_GLOBALDRIVERDATA *)pData->pData;
                memset (pCaps, 0, sizeof (D3DHAL_GLOBALDRIVERDATA));
                pCaps->dwSize = sizeof (D3DHAL_GLOBALDRIVERDATA);
                pCaps->hwCaps.dwSize = sizeof (D3DDEVICEDESC_V1);
                pCaps->hwCaps.dwFlags = D3DDD_COLORMODEL
                        | D3DDD_DEVCAPS
                        | D3DDD_DEVICERENDERBITDEPTH;

                pCaps->hwCaps.dcmColorModel = D3DCOLOR_RGB;
                pCaps->hwCaps.dwDevCaps = D3DDEVCAPS_CANRENDERAFTERFLIP
                        | D3DDEVCAPS_DRAWPRIMTLVERTEX
                        | D3DDEVCAPS_EXECUTESYSTEMMEMORY
                        | D3DDEVCAPS_FLOATTLVERTEX
                        | D3DDEVCAPS_HWRASTERIZATION
                        | D3DDEVCAPS_HWTRANSFORMANDLIGHT
                        | D3DDEVCAPS_TLVERTEXSYSTEMMEMORY
                        | D3DDEVCAPS_TEXTUREVIDEOMEMORY;
                pCaps->hwCaps.dtcTransformCaps.dwSize = sizeof (D3DTRANSFORMCAPS);
                pCaps->hwCaps.dtcTransformCaps.dwCaps = 0;
                pCaps->hwCaps.bClipping = FALSE;
                pCaps->hwCaps.dlcLightingCaps.dwSize = sizeof (D3DLIGHTINGCAPS);
                pCaps->hwCaps.dlcLightingCaps.dwCaps = 0;
                pCaps->hwCaps.dlcLightingCaps.dwLightingModel = 0;
                pCaps->hwCaps.dlcLightingCaps.dwNumLights = 0;
                pCaps->hwCaps.dpcLineCaps.dwSize = sizeof (D3DPRIMCAPS);
                pCaps->hwCaps.dpcLineCaps.dwMiscCaps = 0;
                pCaps->hwCaps.dpcLineCaps.dwRasterCaps = 0;
                pCaps->hwCaps.dpcLineCaps.dwZCmpCaps = 0;
                pCaps->hwCaps.dpcLineCaps.dwSrcBlendCaps = 0;
                pCaps->hwCaps.dpcLineCaps.dwDestBlendCaps = 0;
                pCaps->hwCaps.dpcLineCaps.dwAlphaCmpCaps = 0;
                pCaps->hwCaps.dpcLineCaps.dwShadeCaps = 0;
                pCaps->hwCaps.dpcLineCaps.dwTextureCaps = 0;
                pCaps->hwCaps.dpcLineCaps.dwTextureFilterCaps = 0;
                pCaps->hwCaps.dpcLineCaps.dwTextureBlendCaps = 0;
                pCaps->hwCaps.dpcLineCaps.dwTextureAddressCaps = 0;
                pCaps->hwCaps.dpcLineCaps.dwStippleWidth = 0;
                pCaps->hwCaps.dpcLineCaps.dwStippleHeight = 0;

                pCaps->hwCaps.dpcTriCaps.dwSize = sizeof (D3DPRIMCAPS);
                pCaps->hwCaps.dpcTriCaps.dwMiscCaps = 0;
                pCaps->hwCaps.dpcTriCaps.dwRasterCaps = 0;
                pCaps->hwCaps.dpcTriCaps.dwZCmpCaps = 0;
                pCaps->hwCaps.dpcTriCaps.dwSrcBlendCaps = 0;
                pCaps->hwCaps.dpcTriCaps.dwDestBlendCaps = 0;
                pCaps->hwCaps.dpcTriCaps.dwAlphaCmpCaps = 0;
                pCaps->hwCaps.dpcTriCaps.dwShadeCaps = 0;
                pCaps->hwCaps.dpcTriCaps.dwTextureCaps = 0;
                pCaps->hwCaps.dpcTriCaps.dwTextureFilterCaps = 0;
                pCaps->hwCaps.dpcTriCaps.dwTextureBlendCaps = 0;
                pCaps->hwCaps.dpcTriCaps.dwTextureAddressCaps = 0;
                pCaps->hwCaps.dpcTriCaps.dwStippleWidth = 0;
                pCaps->hwCaps.dpcTriCaps.dwStippleHeight = 0;
                pCaps->hwCaps.dwDeviceRenderBitDepth = 0;
                pCaps->hwCaps.dwDeviceZBufferBitDepth = DDBD_8 | DDBD_16 | DDBD_24 | DDBD_32;
                pCaps->hwCaps.dwMaxBufferSize = 0;
                pCaps->hwCaps.dwMaxVertexCount = 0;


                pCaps->dwNumVertices = 0;
                pCaps->dwNumClipVertices = 0;
                pCaps->dwNumTextureFormats = pAdapter->cSurfDescs;
                pCaps->lpTextureFormats = pAdapter->paSurfDescs;
            }
            else
                hr = E_INVALIDARG;
            break;
        case D3DDDICAPS_GETD3D7CAPS:
            Assert(pData->DataSize >= sizeof (D3DHAL_D3DEXTENDEDCAPS));
            if (pData->DataSize >= sizeof (D3DHAL_D3DEXTENDEDCAPS))
                memset(pData->pData, 0, sizeof (D3DHAL_D3DEXTENDEDCAPS));
            else
                hr = E_INVALIDARG;
            break;
        case D3DDDICAPS_GETD3D9CAPS:
        {
            Assert(pData->DataSize >= sizeof (D3DCAPS9));
//            AssertBreakpoint();
            if (pData->DataSize >= sizeof (D3DCAPS9))
            {
                if (pAdapter->pD3D9If)
                {
                    D3DCAPS9* pCaps = (D3DCAPS9*)pData->pData;
                    hr = pAdapter->pD3D9If->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, pCaps);
                    Assert(hr == S_OK);
                    if (hr == S_OK)
                    {
                        pCaps->Caps2 |= D3DCAPS2_CANSHARERESOURCE | 0x00080000 /*D3DCAPS2_CANRENDERWINDOWED*/;
                        pCaps->DevCaps |= D3DDEVCAPS_FLOATTLVERTEX /* <- must be set according to the docs */
                                /*| D3DDEVCAPS_HWVERTEXBUFFER | D3DDEVCAPS_HWINDEXBUFFER |  D3DDEVCAPS_SUBVOLUMELOCK */;
                        pCaps->PrimitiveMiscCaps |= D3DPMISCCAPS_INDEPENDENTWRITEMASKS
                                | D3DPMISCCAPS_FOGINFVF
                                | D3DPMISCCAPS_SEPARATEALPHABLEND | D3DPMISCCAPS_MRTINDEPENDENTBITDEPTHS;
                        pCaps->RasterCaps |= D3DPRASTERCAPS_SUBPIXEL | D3DPRASTERCAPS_STIPPLE | D3DPRASTERCAPS_ZBIAS | D3DPRASTERCAPS_COLORPERSPECTIVE /* keep */;
                        pCaps->TextureCaps |= D3DPTEXTURECAPS_TRANSPARENCY | D3DPTEXTURECAPS_TEXREPEATNOTSCALEDBYSIZE;
                        pCaps->TextureAddressCaps |= D3DPTADDRESSCAPS_MIRRORONCE;
                        pCaps->VolumeTextureAddressCaps |= D3DPTADDRESSCAPS_MIRRORONCE;
                        pCaps->GuardBandLeft = -8192.;
                        pCaps->GuardBandTop = -8192.;
                        pCaps->GuardBandRight = 8192.;
                        pCaps->GuardBandBottom = 8192.;
                        pCaps->StencilCaps |= D3DSTENCILCAPS_TWOSIDED;
                        pCaps->DeclTypes |= D3DDTCAPS_FLOAT16_2 | D3DDTCAPS_FLOAT16_4;
                        pCaps->VS20Caps.DynamicFlowControlDepth = 24;
                        pCaps->VS20Caps.NumTemps = D3DVS20_MAX_NUMTEMPS;
                        pCaps->PS20Caps.DynamicFlowControlDepth = 24;
                        pCaps->PS20Caps.NumTemps = D3DVS20_MAX_NUMTEMPS;
                        pCaps->VertexTextureFilterCaps |= D3DPTFILTERCAPS_MINFPOINT | D3DPTFILTERCAPS_MAGFPOINT;
#if 1 /* workaround for wine not returning InstructionSlots correctly for  shaders v3.0 */
                        if ((pCaps->VertexShaderVersion & 0xff00) == 0x0300)
                        {
                            pCaps->MaxVertexShader30InstructionSlots = RT_MIN(32768, pCaps->MaxVertexShader30InstructionSlots);
                            pCaps->MaxPixelShader30InstructionSlots = RT_MIN(32768, pCaps->MaxPixelShader30InstructionSlots);
                        }
#endif
#ifdef DEBUG
                        if ((pCaps->VertexShaderVersion & 0xff00) == 0x0300)
                        {
                            Assert(pCaps->MaxVertexShader30InstructionSlots >= 512);
                            Assert(pCaps->MaxVertexShader30InstructionSlots <= 32768);
                            Assert(pCaps->MaxPixelShader30InstructionSlots >= 512);
                            Assert(pCaps->MaxPixelShader30InstructionSlots <= 32768);
                        }
                        else if ((pCaps->VertexShaderVersion & 0xff00) == 0x0200)
                        {
                            Assert(pCaps->MaxVertexShader30InstructionSlots == 0);
                            Assert(pCaps->MaxPixelShader30InstructionSlots == 0);
                        }
                        else
                        {
                            AssertBreakpoint();
                        }
#endif
                        break;
                    }

                    vboxVDbgPrintR((__FUNCTION__": GetDeviceCaps hr(%d)\n", hr));
                    /* let's fall back to the 3D disabled case */
                    hr = S_OK;
                }

                memset(pData->pData, 0, sizeof (D3DCAPS9));
            }
            else
                hr = E_INVALIDARG;
            break;
        }
        case D3DDDICAPS_GETGAMMARAMPCAPS:
            *((uint32_t*)pData->pData) = 0;
            break;
        case D3DDDICAPS_GETMULTISAMPLEQUALITYLEVELS:
        case D3DDDICAPS_GETD3D5CAPS:
        case D3DDDICAPS_GETD3D6CAPS:
        case D3DDDICAPS_GETD3D8CAPS:
        case D3DDDICAPS_GETDECODEGUIDCOUNT:
        case D3DDDICAPS_GETDECODEGUIDS:
        case D3DDDICAPS_GETDECODERTFORMATCOUNT:
        case D3DDDICAPS_GETDECODERTFORMATS:
        case D3DDDICAPS_GETDECODECOMPRESSEDBUFFERINFOCOUNT:
        case D3DDDICAPS_GETDECODECOMPRESSEDBUFFERINFO:
        case D3DDDICAPS_GETDECODECONFIGURATIONCOUNT:
        case D3DDDICAPS_GETDECODECONFIGURATIONS:
        case D3DDDICAPS_GETVIDEOPROCESSORDEVICEGUIDCOUNT:
        case D3DDDICAPS_GETVIDEOPROCESSORDEVICEGUIDS:
        case D3DDDICAPS_GETVIDEOPROCESSORRTFORMATCOUNT:
        case D3DDDICAPS_GETVIDEOPROCESSORRTFORMATS:
        case D3DDDICAPS_GETVIDEOPROCESSORRTSUBSTREAMFORMATCOUNT:
        case D3DDDICAPS_GETVIDEOPROCESSORRTSUBSTREAMFORMATS:
        case D3DDDICAPS_GETVIDEOPROCESSORCAPS:
        case D3DDDICAPS_GETPROCAMPRANGE:
        case D3DDDICAPS_FILTERPROPERTYRANGE:
        case D3DDDICAPS_GETEXTENSIONGUIDCOUNT:
        case D3DDDICAPS_GETEXTENSIONGUIDS:
        case D3DDDICAPS_GETEXTENSIONCAPS:
            vboxVDbgPrint((__FUNCTION__": unimplemented caps type(%d)\n", pData->Type));
            AssertBreakpoint();
            if (pData->pData && pData->DataSize)
                memset(pData->pData, 0, pData->DataSize);
            break;
        default:
            vboxVDbgPrint((__FUNCTION__": unknown caps type(%d)\n", pData->Type));
            AssertBreakpoint();
    }

    vboxVDbgPrint(("<== "__FUNCTION__", hAdapter(0x%p), caps type(%d)\n", hAdapter, pData->Type));

    return S_OK;
}

static HRESULT APIENTRY vboxWddmDDevSetRenderState(HANDLE hDevice, CONST D3DDDIARG_RENDERSTATE* pData)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}

static HRESULT APIENTRY vboxWddmDDevUpdateWInfo(HANDLE hDevice, CONST D3DDDIARG_WINFO* pData)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}

static HRESULT APIENTRY vboxWddmDDevValidateDevice(HANDLE hDevice, D3DDDIARG_VALIDATETEXTURESTAGESTATE* pData)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}

static HRESULT APIENTRY vboxWddmDDevSetTextureStageState(HANDLE hDevice, CONST D3DDDIARG_TEXTURESTAGESTATE* pData)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}

static HRESULT APIENTRY vboxWddmDDevSetTexture(HANDLE hDevice, UINT Stage, HANDLE hTexture)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}

static HRESULT APIENTRY vboxWddmDDevSetPixelShader(HANDLE hDevice, HANDLE hShaderHandle)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}

static HRESULT APIENTRY vboxWddmDDevSetPixelShaderConst(HANDLE hDevice, CONST D3DDDIARG_SETPIXELSHADERCONST* pData, CONST FLOAT* pRegisters)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}

static HRESULT APIENTRY vboxWddmDDevSetStreamSourceUm(HANDLE hDevice, CONST D3DDDIARG_SETSTREAMSOURCEUM* pData, CONST VOID* pUMBuffer )
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}

static HRESULT APIENTRY vboxWddmDDevSetIndices(HANDLE hDevice, CONST D3DDDIARG_SETINDICES* pData)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}

static HRESULT APIENTRY vboxWddmDDevSetIndicesUm(HANDLE hDevice, UINT IndexSize, CONST VOID* pUMBuffer)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}

static HRESULT APIENTRY vboxWddmDDevDrawPrimitive(HANDLE hDevice, CONST D3DDDIARG_DRAWPRIMITIVE* pData, CONST UINT* pFlagBuffer)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}

static HRESULT APIENTRY vboxWddmDDevDrawIndexedPrimitive(HANDLE hDevice, CONST D3DDDIARG_DRAWINDEXEDPRIMITIVE* pData)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}

static HRESULT APIENTRY vboxWddmDDevDrawRectPatch(HANDLE hDevice, CONST D3DDDIARG_DRAWRECTPATCH* pData, CONST D3DDDIRECTPATCH_INFO* pInfo, CONST FLOAT* pPatch)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}

static HRESULT APIENTRY vboxWddmDDevDrawTriPatch(HANDLE hDevice, CONST D3DDDIARG_DRAWTRIPATCH* pData, CONST D3DDDITRIPATCH_INFO* pInfo, CONST FLOAT* pPatch)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}

static HRESULT APIENTRY vboxWddmDDevDrawPrimitive2(HANDLE hDevice, CONST D3DDDIARG_DRAWPRIMITIVE2* pData)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}

static HRESULT APIENTRY vboxWddmDDevDrawIndexedPrimitive2(HANDLE hDevice, CONST D3DDDIARG_DRAWINDEXEDPRIMITIVE2* pData, UINT dwIndicesSize, CONST VOID* pIndexBuffer, CONST UINT* pFlagBuffer)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}

static HRESULT APIENTRY vboxWddmDDevVolBlt(HANDLE hDevice, CONST D3DDDIARG_VOLUMEBLT* pData)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}

static HRESULT APIENTRY vboxWddmDDevBufBlt(HANDLE hDevice, CONST D3DDDIARG_BUFFERBLT* pData)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}

static HRESULT APIENTRY vboxWddmDDevTexBlt(HANDLE hDevice, CONST D3DDDIARG_TEXBLT* pData)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}

static HRESULT APIENTRY vboxWddmDDevStateSet(HANDLE hDevice, D3DDDIARG_STATESET* pData)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevSetPriority(HANDLE hDevice, CONST D3DDDIARG_SETPRIORITY* pData)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevClear(HANDLE hDevice, CONST D3DDDIARG_CLEAR* pData, UINT NumRect, CONST RECT* pRect)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevUpdatePalette(HANDLE hDevice, CONST D3DDDIARG_UPDATEPALETTE* pData, CONST PALETTEENTRY* pPaletteData)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}

static HRESULT APIENTRY vboxWddmDDevSetPalette(HANDLE hDevice, CONST D3DDDIARG_SETPALETTE* pData)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}

static HRESULT APIENTRY vboxWddmDDevSetVertexShaderConst(HANDLE hDevice, CONST D3DDDIARG_SETVERTEXSHADERCONST* pData , CONST VOID* pRegisters)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevMultiplyTransform(HANDLE hDevice, CONST D3DDDIARG_MULTIPLYTRANSFORM* pData)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevSetTransform(HANDLE hDevice, CONST D3DDDIARG_SETTRANSFORM* pData)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevSetViewport(HANDLE hDevice, CONST D3DDDIARG_VIEWPORTINFO* pData)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevSetZRange(HANDLE hDevice, CONST D3DDDIARG_ZRANGE* pData)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevSetMaterial(HANDLE hDevice, CONST D3DDDIARG_SETMATERIAL* pData)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevSetLight(HANDLE hDevice, CONST D3DDDIARG_SETLIGHT* pData, CONST D3DDDI_LIGHT* pLightProperties)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevCreateLight(HANDLE hDevice, CONST D3DDDIARG_CREATELIGHT* pData)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevDestroyLight(HANDLE hDevice, CONST D3DDDIARG_DESTROYLIGHT* pData)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevSetClipPlane(HANDLE hDevice, CONST D3DDDIARG_SETCLIPPLANE* pData)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}

static HRESULT APIENTRY vboxWddmDDevGetInfo(HANDLE hDevice, UINT DevInfoID, VOID* pDevInfoStruct, UINT DevInfoSize)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_NOTIMPL;
}

static HRESULT APIENTRY vboxWddmDDevLock(HANDLE hDevice, D3DDDIARG_LOCK* pData)
{
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    PVBOXWDDMDISP_RESOURCE pRc = (PVBOXWDDMDISP_RESOURCE)pData->hResource;
    Assert(pData->SubResourceIndex < pRc->cAllocations);
    if (pData->SubResourceIndex >= pRc->cAllocations)
        return E_INVALIDARG;

    PVBOXWDDMDISP_ALLOCATION pAlloc = &pRc->aAllocations[pData->SubResourceIndex];
    D3DDDICB_LOCK LockData;
    LockData.hAllocation = pAlloc->hAllocation;
    LockData.PrivateDriverData = 0;
    LockData.NumPages = 0;
    LockData.pPages = NULL;
    LockData.pData = NULL; /* out */
    LockData.Flags.Value = 0;
    LockData.Flags.Discard = pData->Flags.Discard;
    LockData.Flags.DonotWait = pData->Flags.DoNotWait;


    HRESULT hr = pDevice->RtCallbacks.pfnLockCb(hDevice, &LockData);
    Assert(hr == S_OK || (hr == D3DERR_WASSTILLDRAWING && pData->Flags.DoNotWait));
    if (hr == S_OK)
    {
        uintptr_t offset;
        if (pData->Flags.AreaValid)
        {
            offset = pAlloc->SurfDesc.pitch * pData->Area.top +
                    ((pAlloc->SurfDesc.bpp * pData->Area.left) >> 3);
        }
        else if (pData->Flags.RangeValid)
        {
            offset = pData->Range.Offset;
        }
        else if (pData->Flags.BoxValid)
        {
            vboxVDbgPrintF((__FUNCTION__": Implement Box area"));
            AssertBreakpoint();
        }
        else
        {
            AssertBreakpoint();
        }

        if (pData->Flags.Discard)
        {
            /* check if the surface was renamed */
            if (LockData.hAllocation)
                pAlloc->hAllocation = LockData.hAllocation;
        }

        pData->pSurfData = ((uint8_t*)LockData.pData) + offset;
        pData->Pitch = pAlloc->SurfDesc.pitch;
        pData->SlicePitch = pAlloc->SurfDesc.slicePitch;
    }

    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return hr;
}
static HRESULT APIENTRY vboxWddmDDevUnlock(HANDLE hDevice, CONST D3DDDIARG_UNLOCK* pData)
{
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    PVBOXWDDMDISP_RESOURCE pRc = (PVBOXWDDMDISP_RESOURCE)pData->hResource;
    HRESULT hr = S_OK;

    Assert(pData->SubResourceIndex < pRc->cAllocations);
    if (pData->SubResourceIndex >= pRc->cAllocations)
        return E_INVALIDARG;

    struct
    {
        D3DDDICB_UNLOCK Unlock;
        D3DKMT_HANDLE hAllocation;
    } UnlockData;

    UnlockData.Unlock.NumAllocations = 1;
    UnlockData.Unlock.phAllocations = &UnlockData.hAllocation;
    UnlockData.hAllocation = pRc->aAllocations[pData->SubResourceIndex].hAllocation;

    hr = pDevice->RtCallbacks.pfnUnlockCb(hDevice, &UnlockData.Unlock);
    Assert(hr == S_OK);

    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return hr;
}
static HRESULT APIENTRY vboxWddmDDevLockAsync(HANDLE hDevice, D3DDDIARG_LOCKASYNC* pData)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevUnlockAsync(HANDLE hDevice, CONST D3DDDIARG_UNLOCKASYNC* pData)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevRename(HANDLE hDevice, CONST D3DDDIARG_RENAME* pData)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}

static HRESULT APIENTRY vboxWddmDDevCreateResource(HANDLE hDevice, D3DDDIARG_CREATERESOURCE* pResource)
{
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    HRESULT hr = S_OK;
    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    Assert(pDevice);
    Assert(pResource);

    /* allocate buffer for D3DDDICB_ALLOCATE + D3DDDI_ALLOCATIONINFO * numAllocs + PVBOXWDDM_RCINFO with aAllocInfos[numAllocs] */
    uint32_t cbBuf = sizeof (D3DDDICB_ALLOCATE);
    uint32_t offDdiAllocInfos = (cbBuf + 7) & ~3;
    uint32_t cbDdiAllocInfos = sizeof (D3DDDI_ALLOCATIONINFO) * pResource->SurfCount;
    cbBuf = offDdiAllocInfos + cbDdiAllocInfos;
    uint32_t offRcInfo = (cbBuf + 7) & ~3;
    uint32_t cbRcInfo = sizeof (VBOXWDDM_RCINFO);
    cbBuf = offRcInfo + cbRcInfo;
    uint32_t offAllocInfos = (cbBuf + 7) & ~3;
    uint32_t cbAllocInfos = sizeof (VBOXWDDM_ALLOCINFO) * pResource->SurfCount;
    cbBuf = offAllocInfos + cbAllocInfos;
    uint8_t *pvBuf = (uint8_t*)RTMemAllocZ(cbBuf);
    if (pvBuf)
    {
        D3DDDICB_ALLOCATE *pAllocate = (D3DDDICB_ALLOCATE*)pvBuf;
        D3DDDI_ALLOCATIONINFO* pDdiAllocInfos = (D3DDDI_ALLOCATIONINFO*)(pvBuf + offDdiAllocInfos);
        PVBOXWDDM_RCINFO pRcInfo = (PVBOXWDDM_RCINFO)(pvBuf + offRcInfo);
        PVBOXWDDM_ALLOCINFO pAllocInfos = (PVBOXWDDM_ALLOCINFO)(pvBuf + offAllocInfos);
        pAllocate->pPrivateDriverData = pRcInfo;
        pAllocate->PrivateDriverDataSize = cbRcInfo;
        pAllocate->hResource = pResource->hResource;
        pAllocate->hKMResource = NULL;
        pAllocate->NumAllocations = pResource->SurfCount;
        pAllocate->pAllocationInfo = pDdiAllocInfos;

        pRcInfo->fFlags = VBOXWDDM_RESOURCE_F_TYPE_GENERIC;
        pRcInfo->RcDesc.fFlags = pResource->Flags;
        pRcInfo->RcDesc.enmFormat = pResource->Format;
        pRcInfo->RcDesc.enmPool = pResource->Pool;
        pRcInfo->RcDesc.enmMultisampleType = pResource->MultisampleType;
        pRcInfo->RcDesc.MultisampleQuality = pResource->MultisampleQuality;
        pRcInfo->RcDesc.MipLevels = pResource->MipLevels;
        pRcInfo->RcDesc.Fvf = pResource->Fvf;
        pRcInfo->RcDesc.VidPnSourceId = pResource->VidPnSourceId;
        pRcInfo->RcDesc.RefreshRate = pResource->RefreshRate;
        pRcInfo->RcDesc.enmRotation = pResource->Rotation;
        pRcInfo->cAllocInfos = pResource->SurfCount;

        for (UINT i = 0; i < pResource->SurfCount; ++i)
        {
            PVBOXWDDM_ALLOCINFO pAllocInfo = &pAllocInfos[i];
            D3DDDI_ALLOCATIONINFO* pDdiAllocInfo = &pDdiAllocInfos[i];
            CONST D3DDDI_SURFACEINFO* pSurf = &pResource->pSurfList[i];
            pDdiAllocInfo->hAllocation = NULL;
            pDdiAllocInfo->pSystemMem = pSurf->pSysMem;
            Assert((!!(pSurf->pSysMem)) == (pResource->Pool == D3DDDIPOOL_SYSTEMMEM));
            pDdiAllocInfo->pPrivateDriverData = pAllocInfo;
            pDdiAllocInfo->PrivateDriverDataSize = sizeof (VBOXWDDM_ALLOCINFO);
            pDdiAllocInfo->VidPnSourceId = pResource->VidPnSourceId;
            pDdiAllocInfo->Flags.Value = 0;
            if (pResource->Flags.Primary)
                pDdiAllocInfo->Flags.Primary = 1;

            pAllocInfo->enmType = VBOXWDDM_ALLOC_TYPE_UMD_RC_GENERIC;
            pAllocInfo->SurfDesc.width = pSurf->Width;
            pAllocInfo->SurfDesc.height = pSurf->Height;
            pAllocInfo->SurfDesc.format = pResource->Format;
            pAllocInfo->SurfDesc.bpp = vboxWddmCalcBitsPerPixel(pResource->Format);

            if (pSurf->SysMemPitch)
            {
                pAllocInfo->SurfDesc.pitch = pSurf->SysMemPitch;
#ifdef DEBUG
                UINT tst = vboxWddmCalcPitch(pSurf->Width, pAllocInfo->SurfDesc.bpp);
                Assert(tst == pSurf->SysMemPitch);
#endif
            }
            else
                pAllocInfo->SurfDesc.pitch = vboxWddmCalcPitch(pSurf->Width, pAllocInfo->SurfDesc.bpp);

            pAllocInfo->SurfDesc.cbSize = pAllocInfo->SurfDesc.pitch * pAllocInfo->SurfDesc.height;
            pAllocInfo->SurfDesc.depth = pSurf->Depth;
            pAllocInfo->SurfDesc.slicePitch = pSurf->SysMemSlicePitch;
            pAllocInfo->SurfDesc.VidPnSourceId = pResource->VidPnSourceId;
            pAllocInfo->SurfDesc.RefreshRate = pResource->RefreshRate;
        }

        hr = pDevice->RtCallbacks.pfnAllocateCb(pDevice->hDevice, pAllocate);
        Assert(hr == S_OK);
        if (hr == S_OK)
        {
            Assert(pAllocate->hKMResource);
            PVBOXWDDMDISP_RESOURCE pRc = vboxResourceAlloc(pResource->SurfCount);
            if (pRc)
            {
                pRc->hResource = pResource->hResource;
                pRc->hKMResource = pAllocate->hKMResource;
                pRc->pDevice = pDevice;
                pRc->fFlags = pRcInfo->fFlags;
                pRc->RcDesc = pRcInfo->RcDesc;
                pRc->cAllocations = pRcInfo->cAllocInfos;
                for (UINT i = 0; i < pRcInfo->cAllocInfos; ++i)
                {
                    PVBOXWDDMDISP_ALLOCATION pAllocation = &pRc->aAllocations[i];
                    D3DDDI_ALLOCATIONINFO* pDdiAllocInfo = &pDdiAllocInfos[i];
                    PVBOXWDDM_ALLOCINFO pAllocInfo = &pAllocInfos[i];
                    CONST D3DDDI_SURFACEINFO* pSurf = &pResource->pSurfList[i];
                    pAllocation->hAllocation = pDdiAllocInfo->hAllocation;
                    pAllocation->enmType = VBOXWDDM_ALLOC_TYPE_UMD_RC_GENERIC;
                    pAllocation->pvMem = pSurf->pSysMem;
                    pAllocation->SurfDesc = pAllocInfo->SurfDesc;
                }

                pResource->hResource = pRc;
//                vboxResourceFree(pRc);
            }
            else
            {
                hr = E_OUTOFMEMORY;
            }
        }

        RTMemFree(pvBuf);
    }
    else
    {
        hr = E_OUTOFMEMORY;
    }

    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return hr;
}

static HRESULT APIENTRY vboxWddmDDevDestroyResource(HANDLE hDevice, HANDLE hResource)
{
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    HRESULT hr = S_OK;

    Assert(pDevice);
    Assert(hResource);
    PVBOXWDDMDISP_RESOURCE pRc = (PVBOXWDDMDISP_RESOURCE)hResource;
    if (!(pRc->fFlags & VBOXWDDM_RESOURCE_F_OPENNED))
    {
        D3DDDICB_DEALLOCATE Dealloc;
        Dealloc.hResource = pRc->hResource;
        Assert(pRc->hResource);
        /* according to the docs the below two are ignored in case we set the hResource */
        Dealloc.NumAllocations = 0;
        Dealloc.HandleList = NULL;
        hr = pDevice->RtCallbacks.pfnDeallocateCb(pDevice->hDevice, &Dealloc);
        Assert(hr == S_OK);
    }

    vboxResourceFree(pRc);
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return hr;
}
static HRESULT APIENTRY vboxWddmDDevSetDisplayMode(HANDLE hDevice, CONST D3DDDIARG_SETDISPLAYMODE* pData)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevPresent(HANDLE hDevice, CONST D3DDDIARG_PRESENT* pData)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevFlush(HANDLE hDevice)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevCreateVertexShaderDecl(HANDLE hDevice, D3DDDIARG_CREATEVERTEXSHADERDECL* pData, CONST D3DDDIVERTEXELEMENT* pVertexElements)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevSetVertexShaderDecl(HANDLE hDevice, HANDLE hShaderHandle)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevDeleteVertexShaderDecl(HANDLE hDevice, HANDLE hShaderHandle)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevCreateVertexShaderFunc(HANDLE hDevice, D3DDDIARG_CREATEVERTEXSHADERFUNC* pData, CONST UINT* pCode)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevSetVertexShaderFunc(HANDLE hDevice, HANDLE hShaderHandle)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevDeleteVertexShaderFunc(HANDLE hDevice, HANDLE hShaderHandle)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevSetVertexShaderConstI(HANDLE hDevice, CONST D3DDDIARG_SETVERTEXSHADERCONSTI* pData, CONST INT* pRegisters)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevSetVertexShaderConstB(HANDLE hDevice, CONST D3DDDIARG_SETVERTEXSHADERCONSTB* pData, CONST BOOL* pRegisters)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevSetScissorRect(HANDLE hDevice, CONST RECT* pRect)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevSetStreamSource(HANDLE hDevice, CONST D3DDDIARG_SETSTREAMSOURCE* pData)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevSetStreamSourceFreq(HANDLE hDevice, CONST D3DDDIARG_SETSTREAMSOURCEFREQ* pData)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevSetConvolutionKernelMono(HANDLE hDevice, CONST D3DDDIARG_SETCONVOLUTIONKERNELMONO* pData)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevComposeRects(HANDLE hDevice, CONST D3DDDIARG_COMPOSERECTS* pData)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevBlt(HANDLE hDevice, CONST D3DDDIARG_BLT* pData)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevColorFill(HANDLE hDevice, CONST D3DDDIARG_COLORFILL* pData)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevDepthFill(HANDLE hDevice, CONST D3DDDIARG_DEPTHFILL* pData)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevCreateQuery(HANDLE hDevice, D3DDDIARG_CREATEQUERY* pData)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevDestroyQuery(HANDLE hDevice, HANDLE hQuery)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevIssueQuery(HANDLE hDevice, CONST D3DDDIARG_ISSUEQUERY* pData)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevGetQueryData(HANDLE hDevice, CONST D3DDDIARG_GETQUERYDATA* pData)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevSetRenderTarget(HANDLE hDevice, CONST D3DDDIARG_SETRENDERTARGET* pData)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevSetDepthStencil(HANDLE hDevice, CONST D3DDDIARG_SETDEPTHSTENCIL* pData)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevGenerateMipSubLevels(HANDLE hDevice, CONST D3DDDIARG_GENERATEMIPSUBLEVELS* pData)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevSetPixelShaderConstI(HANDLE hDevice, CONST D3DDDIARG_SETPIXELSHADERCONSTI* pData, CONST INT* pRegisters)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevSetPixelShaderConstB(HANDLE hDevice, CONST D3DDDIARG_SETPIXELSHADERCONSTB* pData, CONST BOOL* pRegisters)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevCreatePixelShader(HANDLE hDevice, D3DDDIARG_CREATEPIXELSHADER* pData, CONST UINT* pCode)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevDeletePixelShader(HANDLE hDevice, HANDLE hShaderHandle)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevCreateDecodeDevice(HANDLE hDevice, D3DDDIARG_CREATEDECODEDEVICE* pData)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevDestroyDecodeDevice(HANDLE hDevice, HANDLE hDecodeDevice)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevSetDecodeRenderTarget(HANDLE hDevice, CONST D3DDDIARG_SETDECODERENDERTARGET* pData)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevDecodeBeginFrame(HANDLE hDevice, D3DDDIARG_DECODEBEGINFRAME* pData)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevDecodeEndFrame(HANDLE hDevice, D3DDDIARG_DECODEENDFRAME* pData)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevDecodeExecute(HANDLE hDevice, CONST D3DDDIARG_DECODEEXECUTE* pData)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevDecodeExtensionExecute(HANDLE hDevice, CONST D3DDDIARG_DECODEEXTENSIONEXECUTE* pData)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevCreateVideoProcessDevice(HANDLE hDevice, D3DDDIARG_CREATEVIDEOPROCESSDEVICE* pData)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevDestroyVideoProcessDevice(HANDLE hDevice, HANDLE hVideoProcessor)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevVideoProcessBeginFrame(HANDLE hDevice, HANDLE hVideoProcess)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevVideoProcessEndFrame(HANDLE hDevice, D3DDDIARG_VIDEOPROCESSENDFRAME* pData)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevSetVideoProcessRenderTarget(HANDLE hDevice, CONST D3DDDIARG_SETVIDEOPROCESSRENDERTARGET* pData)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevVideoProcessBlt(HANDLE hDevice, CONST D3DDDIARG_VIDEOPROCESSBLT* pData)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevCreateExtensionDevice(HANDLE hDevice, D3DDDIARG_CREATEEXTENSIONDEVICE* pData)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevDestroyExtensionDevice(HANDLE hDevice, HANDLE hExtension)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevExtensionExecute(HANDLE hDevice, CONST D3DDDIARG_EXTENSIONEXECUTE* pData)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevDestroyDevice(IN HANDLE hDevice)
{
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));

    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    if (pDevice->DefaultContext.ContextInfo.hContext)
    {
        D3DDDICB_DESTROYCONTEXT DestroyContext;
        DestroyContext.hContext = pDevice->DefaultContext.ContextInfo.hContext;
        HRESULT tmpHr = pDevice->RtCallbacks.pfnDestroyContextCb(pDevice->hDevice, &DestroyContext);
        Assert(tmpHr == S_OK);
    }
    RTMemFree(hDevice);
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return S_OK;
}
static HRESULT APIENTRY vboxWddmDDevCreateOverlay(HANDLE hDevice, D3DDDIARG_CREATEOVERLAY* pData)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevUpdateOverlay(HANDLE hDevice, CONST D3DDDIARG_UPDATEOVERLAY* pData)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevFlipOverlay(HANDLE hDevice, CONST D3DDDIARG_FLIPOVERLAY* pData)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevGetOverlayColorControls(HANDLE hDevice, D3DDDIARG_GETOVERLAYCOLORCONTROLS* pData)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevSetOverlayColorControls(HANDLE hDevice, CONST D3DDDIARG_SETOVERLAYCOLORCONTROLS* pData)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevDestroyOverlay(HANDLE hDevice, CONST D3DDDIARG_DESTROYOVERLAY* pData)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevQueryResourceResidency(HANDLE hDevice, CONST D3DDDIARG_QUERYRESOURCERESIDENCY* pData)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}

static HRESULT vboxAllocationInit(PVBOXWDDMDISP_ALLOCATION pAlloc, D3DDDI_OPENALLOCATIONINFO *pInfo)
{
    HRESULT hr = S_OK;
    pAlloc->hAllocation = pInfo->hAllocation;
    Assert(pInfo->PrivateDriverDataSize == sizeof (VBOXWDDM_ALLOCINFO));
    Assert(pInfo->pPrivateDriverData);
    if (pInfo->PrivateDriverDataSize >= sizeof (VBOXWDDM_ALLOCINFO))
    {
        PVBOXWDDM_ALLOCINFO pAllocInfo = (PVBOXWDDM_ALLOCINFO)pInfo->pPrivateDriverData;
        pAlloc->enmType = pAllocInfo->enmType;
        Assert(pAllocInfo->enmType == VBOXWDDM_ALLOC_TYPE_STD_SHAREDPRIMARYSURFACE
                || VBOXWDDM_ALLOC_TYPE_STD_SHADOWSURFACE
                || VBOXWDDM_ALLOC_TYPE_STD_STAGINGSURFACE);
        pAlloc->pvMem = NULL;
        pAlloc->SurfDesc = pAllocInfo->SurfDesc;
    }
    else
    {
        vboxVDbgPrintR((__FUNCTION__": ERROR: PrivateDriverDataSize(%d) < (%d)\n", pInfo->PrivateDriverDataSize, sizeof (VBOXWDDM_ALLOCINFO)));
        hr = E_INVALIDARG;
    }
    return hr;
}

static HRESULT APIENTRY vboxWddmDDevOpenResource(HANDLE hDevice, D3DDDIARG_OPENRESOURCE* pData)
{
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    HRESULT hr = S_OK;

    Assert(pDevice);
    Assert(pData->NumAllocations);
    PVBOXWDDMDISP_RESOURCE pRc = vboxResourceAlloc(pData->NumAllocations);
    Assert(pRc);
    if (pRc)
    {
        pRc->hResource = pData->hResource;
        pRc->hKMResource = pData->hKMResource;
        pRc->pDevice = pDevice;
        pRc->RcDesc.enmRotation = pData->Rotation;
        pRc->fFlags = VBOXWDDM_RESOURCE_F_OPENNED;
        if (!pData->pPrivateDriverData || !pData->PrivateDriverDataSize)
        {
            /* this is a "standard" allocation resource */

            /* both should be actually zero */
            Assert(!pData->pPrivateDriverData && !pData->PrivateDriverDataSize);
            pRc->RcDesc.enmPool = D3DDDIPOOL_LOCALVIDMEM;
            pRc->RcDesc.enmMultisampleType = D3DDDIMULTISAMPLE_NONE;
            pRc->RcDesc.MultisampleQuality = 0;
            pRc->RcDesc.MipLevels = 0;
            pRc->RcDesc.Fvf;
            pRc->RcDesc.fFlags.Value = 0;

            Assert(pData->NumAllocations);
            D3DDDI_OPENALLOCATIONINFO* pDdiAllocInfo = &pData->pOpenAllocationInfo[0];
            Assert(pDdiAllocInfo->pPrivateDriverData);
            Assert(pDdiAllocInfo->PrivateDriverDataSize >= sizeof (VBOXWDDM_ALLOCINFO));
            if (pDdiAllocInfo->pPrivateDriverData && pDdiAllocInfo->PrivateDriverDataSize >= sizeof (VBOXWDDM_ALLOCINFO))
            {
                PVBOXWDDM_ALLOCINFO pAllocInfo = (PVBOXWDDM_ALLOCINFO)pDdiAllocInfo->pPrivateDriverData;
                switch(pAllocInfo->enmType)
                {
                    case VBOXWDDM_ALLOC_TYPE_STD_SHAREDPRIMARYSURFACE:
                        pRc->RcDesc.fFlags.Primary = 1;
                    case VBOXWDDM_ALLOC_TYPE_STD_SHADOWSURFACE:
                    case VBOXWDDM_ALLOC_TYPE_STD_STAGINGSURFACE:
                        pRc->RcDesc.enmFormat = pAllocInfo->SurfDesc.format;
                        pRc->RcDesc.VidPnSourceId = pAllocInfo->SurfDesc.VidPnSourceId;
                        pRc->RcDesc.RefreshRate = pAllocInfo->SurfDesc.RefreshRate;
                        break;
                    default:
                        AssertBreakpoint();
                        hr = E_INVALIDARG;
                }
            }
            else
                hr = E_INVALIDARG;
        }
        else
        {
            /* this is a "generic" resource whose creation is initiaded by the UMD */
            Assert(pData->PrivateDriverDataSize == sizeof (VBOXWDDM_RCINFO));
            if (pData->PrivateDriverDataSize >= sizeof (VBOXWDDM_RCINFO))
            {
                VBOXWDDM_RCINFO *pRcInfo = (VBOXWDDM_RCINFO*)pData->pPrivateDriverData;
                Assert(pRcInfo->fFlags == VBOXWDDM_RESOURCE_F_TYPE_GENERIC);
                Assert(pRcInfo->cAllocInfos == pData->NumAllocations);
                pRc->fFlags = pRcInfo->fFlags | VBOXWDDM_RESOURCE_F_OPENNED;
                pRc->RcDesc = pRcInfo->RcDesc;
                pRc->cAllocations = pData->NumAllocations;
            }
            else
                hr = E_INVALIDARG;
        }

        if (hr == S_OK)
        {
            for (UINT i = 0; i < pData->NumAllocations; ++i)
            {
                hr = vboxAllocationInit(&pRc->aAllocations[i], &pData->pOpenAllocationInfo[i]);
                Assert(hr == S_OK);
                if (hr != S_OK)
                    break;
            }
        }

        if (hr == S_OK)
            pData->hResource = pRc;
        else
            vboxResourceFree(pRc);
    }
    else
    {
        vboxVDbgPrintR((__FUNCTION__": vboxResourceAlloc failed for hDevice(0x%p), NumAllocations(%d)\n", hDevice, pData->NumAllocations));
        hr = E_OUTOFMEMORY;
    }

    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return hr;
}
static HRESULT APIENTRY vboxWddmDDevGetCaptureAllocationHandle(HANDLE hDevice, D3DDDIARG_GETCAPTUREALLOCATIONHANDLE* pData)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}

static HRESULT APIENTRY vboxWddmDDevCaptureToSysMem(HANDLE hDevice, CONST D3DDDIARG_CAPTURETOSYSMEM* pData)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}

static HRESULT APIENTRY vboxWddmDispCreateDevice (IN HANDLE hAdapter, IN D3DDDIARG_CREATEDEVICE* pCreateData)
{
    HRESULT hr = S_OK;
    vboxVDbgPrint(("==> "__FUNCTION__", hAdapter(0x%p), Interface(%d), Version(%d)\n", hAdapter, pCreateData->Interface, pCreateData->Version));

//    AssertBreakpoint();

    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)RTMemAllocZ(sizeof (VBOXWDDMDISP_DEVICE));
    if (pDevice)
    {
        PVBOXWDDMDISP_ADAPTER pAdapter = (PVBOXWDDMDISP_ADAPTER)hAdapter;

        pDevice->hDevice = pCreateData->hDevice;
        pDevice->pAdapter = pAdapter;
        pDevice->u32IfVersion = pCreateData->Interface;
        pDevice->uRtVersion = pCreateData->Version;
        pDevice->RtCallbacks = *pCreateData->pCallbacks;
        pDevice->pvCmdBuffer = pCreateData->pCommandBuffer;
        pDevice->cbCmdBuffer = pCreateData->CommandBufferSize;
        pDevice->fFlags = pCreateData->Flags;

        Assert(!pCreateData->AllocationListSize);
        Assert(!pCreateData->PatchLocationListSize);

        pCreateData->hDevice = pDevice;

        pCreateData->pDeviceFuncs->pfnSetRenderState = vboxWddmDDevSetRenderState;
        pCreateData->pDeviceFuncs->pfnUpdateWInfo = vboxWddmDDevUpdateWInfo;
        pCreateData->pDeviceFuncs->pfnValidateDevice = vboxWddmDDevValidateDevice;
        pCreateData->pDeviceFuncs->pfnSetTextureStageState = vboxWddmDDevSetTextureStageState;
        pCreateData->pDeviceFuncs->pfnSetTexture = vboxWddmDDevSetTexture;
        pCreateData->pDeviceFuncs->pfnSetPixelShader = vboxWddmDDevSetPixelShader;
        pCreateData->pDeviceFuncs->pfnSetPixelShaderConst = vboxWddmDDevSetPixelShaderConst;
        pCreateData->pDeviceFuncs->pfnSetStreamSourceUm = vboxWddmDDevSetStreamSourceUm;
        pCreateData->pDeviceFuncs->pfnSetIndices = vboxWddmDDevSetIndices;
        pCreateData->pDeviceFuncs->pfnSetIndicesUm = vboxWddmDDevSetIndicesUm;
        pCreateData->pDeviceFuncs->pfnDrawPrimitive = vboxWddmDDevDrawPrimitive;
        pCreateData->pDeviceFuncs->pfnDrawIndexedPrimitive = vboxWddmDDevDrawIndexedPrimitive;
        pCreateData->pDeviceFuncs->pfnDrawRectPatch = vboxWddmDDevDrawRectPatch;
        pCreateData->pDeviceFuncs->pfnDrawTriPatch = vboxWddmDDevDrawTriPatch;
        pCreateData->pDeviceFuncs->pfnDrawPrimitive2 = vboxWddmDDevDrawPrimitive2;
        pCreateData->pDeviceFuncs->pfnDrawIndexedPrimitive2 = vboxWddmDDevDrawIndexedPrimitive2;
        pCreateData->pDeviceFuncs->pfnVolBlt = vboxWddmDDevVolBlt;
        pCreateData->pDeviceFuncs->pfnBufBlt = vboxWddmDDevBufBlt;
        pCreateData->pDeviceFuncs->pfnTexBlt = vboxWddmDDevTexBlt;
        pCreateData->pDeviceFuncs->pfnStateSet = vboxWddmDDevStateSet;
        pCreateData->pDeviceFuncs->pfnSetPriority = vboxWddmDDevSetPriority;
        pCreateData->pDeviceFuncs->pfnClear = vboxWddmDDevClear;
        pCreateData->pDeviceFuncs->pfnUpdatePalette = vboxWddmDDevUpdatePalette;
        pCreateData->pDeviceFuncs->pfnSetPalette = vboxWddmDDevSetPalette;
        pCreateData->pDeviceFuncs->pfnSetVertexShaderConst = vboxWddmDDevSetVertexShaderConst;
        pCreateData->pDeviceFuncs->pfnMultiplyTransform = vboxWddmDDevMultiplyTransform;
        pCreateData->pDeviceFuncs->pfnSetTransform = vboxWddmDDevSetTransform;
        pCreateData->pDeviceFuncs->pfnSetViewport = vboxWddmDDevSetViewport;
        pCreateData->pDeviceFuncs->pfnSetZRange = vboxWddmDDevSetZRange;
        pCreateData->pDeviceFuncs->pfnSetMaterial = vboxWddmDDevSetMaterial;
        pCreateData->pDeviceFuncs->pfnSetLight = vboxWddmDDevSetLight;
        pCreateData->pDeviceFuncs->pfnCreateLight = vboxWddmDDevCreateLight;
        pCreateData->pDeviceFuncs->pfnDestroyLight = vboxWddmDDevDestroyLight;
        pCreateData->pDeviceFuncs->pfnSetClipPlane = vboxWddmDDevSetClipPlane;
        pCreateData->pDeviceFuncs->pfnGetInfo = vboxWddmDDevGetInfo;
        pCreateData->pDeviceFuncs->pfnLock = vboxWddmDDevLock;
        pCreateData->pDeviceFuncs->pfnUnlock = vboxWddmDDevUnlock;
        pCreateData->pDeviceFuncs->pfnCreateResource = vboxWddmDDevCreateResource;
        pCreateData->pDeviceFuncs->pfnDestroyResource = vboxWddmDDevDestroyResource;
        pCreateData->pDeviceFuncs->pfnSetDisplayMode = vboxWddmDDevSetDisplayMode;
        pCreateData->pDeviceFuncs->pfnPresent = vboxWddmDDevPresent;
        pCreateData->pDeviceFuncs->pfnFlush = vboxWddmDDevFlush;
        pCreateData->pDeviceFuncs->pfnCreateVertexShaderFunc = vboxWddmDDevCreateVertexShaderFunc;
        pCreateData->pDeviceFuncs->pfnDeleteVertexShaderFunc = vboxWddmDDevDeleteVertexShaderFunc;
        pCreateData->pDeviceFuncs->pfnSetVertexShaderFunc = vboxWddmDDevSetVertexShaderFunc;
        pCreateData->pDeviceFuncs->pfnCreateVertexShaderDecl = vboxWddmDDevCreateVertexShaderDecl;
        pCreateData->pDeviceFuncs->pfnDeleteVertexShaderDecl = vboxWddmDDevDeleteVertexShaderDecl;
        pCreateData->pDeviceFuncs->pfnSetVertexShaderDecl = vboxWddmDDevSetVertexShaderDecl;
        pCreateData->pDeviceFuncs->pfnSetVertexShaderConstI = vboxWddmDDevSetVertexShaderConstI;
        pCreateData->pDeviceFuncs->pfnSetVertexShaderConstB = vboxWddmDDevSetVertexShaderConstB;
        pCreateData->pDeviceFuncs->pfnSetScissorRect = vboxWddmDDevSetScissorRect;
        pCreateData->pDeviceFuncs->pfnSetStreamSource = vboxWddmDDevSetStreamSource;
        pCreateData->pDeviceFuncs->pfnSetStreamSourceFreq = vboxWddmDDevSetStreamSourceFreq;
        pCreateData->pDeviceFuncs->pfnSetConvolutionKernelMono = vboxWddmDDevSetConvolutionKernelMono;
        pCreateData->pDeviceFuncs->pfnComposeRects = vboxWddmDDevComposeRects;
        pCreateData->pDeviceFuncs->pfnBlt = vboxWddmDDevBlt;
        pCreateData->pDeviceFuncs->pfnColorFill = vboxWddmDDevColorFill;
        pCreateData->pDeviceFuncs->pfnDepthFill = vboxWddmDDevDepthFill;
        pCreateData->pDeviceFuncs->pfnCreateQuery = vboxWddmDDevCreateQuery;
        pCreateData->pDeviceFuncs->pfnDestroyQuery = vboxWddmDDevDestroyQuery;
        pCreateData->pDeviceFuncs->pfnIssueQuery = vboxWddmDDevIssueQuery;
        pCreateData->pDeviceFuncs->pfnGetQueryData = vboxWddmDDevGetQueryData;
        pCreateData->pDeviceFuncs->pfnSetRenderTarget = vboxWddmDDevSetRenderTarget;
        pCreateData->pDeviceFuncs->pfnSetDepthStencil = vboxWddmDDevSetDepthStencil;
        pCreateData->pDeviceFuncs->pfnGenerateMipSubLevels = vboxWddmDDevGenerateMipSubLevels;
        pCreateData->pDeviceFuncs->pfnSetPixelShaderConstI = vboxWddmDDevSetPixelShaderConstI;
        pCreateData->pDeviceFuncs->pfnSetPixelShaderConstB = vboxWddmDDevSetPixelShaderConstB;
        pCreateData->pDeviceFuncs->pfnCreatePixelShader = vboxWddmDDevCreatePixelShader;
        pCreateData->pDeviceFuncs->pfnDeletePixelShader = vboxWddmDDevDeletePixelShader;
        pCreateData->pDeviceFuncs->pfnCreateDecodeDevice = vboxWddmDDevCreateDecodeDevice;
        pCreateData->pDeviceFuncs->pfnDestroyDecodeDevice = vboxWddmDDevDestroyDecodeDevice;
        pCreateData->pDeviceFuncs->pfnSetDecodeRenderTarget = vboxWddmDDevSetDecodeRenderTarget;
        pCreateData->pDeviceFuncs->pfnDecodeBeginFrame = vboxWddmDDevDecodeBeginFrame;
        pCreateData->pDeviceFuncs->pfnDecodeEndFrame = vboxWddmDDevDecodeEndFrame;
        pCreateData->pDeviceFuncs->pfnDecodeExecute = vboxWddmDDevDecodeExecute;
        pCreateData->pDeviceFuncs->pfnDecodeExtensionExecute = vboxWddmDDevDecodeExtensionExecute;
        pCreateData->pDeviceFuncs->pfnCreateVideoProcessDevice = vboxWddmDDevCreateVideoProcessDevice;
        pCreateData->pDeviceFuncs->pfnDestroyVideoProcessDevice = vboxWddmDDevDestroyVideoProcessDevice;
        pCreateData->pDeviceFuncs->pfnVideoProcessBeginFrame = vboxWddmDDevVideoProcessBeginFrame;
        pCreateData->pDeviceFuncs->pfnVideoProcessEndFrame = vboxWddmDDevVideoProcessEndFrame;
        pCreateData->pDeviceFuncs->pfnSetVideoProcessRenderTarget = vboxWddmDDevSetVideoProcessRenderTarget;
        pCreateData->pDeviceFuncs->pfnVideoProcessBlt = vboxWddmDDevVideoProcessBlt;
        pCreateData->pDeviceFuncs->pfnCreateExtensionDevice = vboxWddmDDevCreateExtensionDevice;
        pCreateData->pDeviceFuncs->pfnDestroyExtensionDevice = vboxWddmDDevDestroyExtensionDevice;
        pCreateData->pDeviceFuncs->pfnExtensionExecute = vboxWddmDDevExtensionExecute;
        pCreateData->pDeviceFuncs->pfnCreateOverlay = vboxWddmDDevCreateOverlay;
        pCreateData->pDeviceFuncs->pfnUpdateOverlay = vboxWddmDDevUpdateOverlay;
        pCreateData->pDeviceFuncs->pfnFlipOverlay = vboxWddmDDevFlipOverlay;
        pCreateData->pDeviceFuncs->pfnGetOverlayColorControls = vboxWddmDDevGetOverlayColorControls;
        pCreateData->pDeviceFuncs->pfnSetOverlayColorControls = vboxWddmDDevSetOverlayColorControls;
        pCreateData->pDeviceFuncs->pfnDestroyOverlay = vboxWddmDDevDestroyOverlay;
        pCreateData->pDeviceFuncs->pfnDestroyDevice = vboxWddmDDevDestroyDevice;
        pCreateData->pDeviceFuncs->pfnQueryResourceResidency = vboxWddmDDevQueryResourceResidency;
        pCreateData->pDeviceFuncs->pfnOpenResource = vboxWddmDDevOpenResource;
        pCreateData->pDeviceFuncs->pfnGetCaptureAllocationHandle = vboxWddmDDevGetCaptureAllocationHandle;
        pCreateData->pDeviceFuncs->pfnCaptureToSysMem = vboxWddmDDevCaptureToSysMem;
        pCreateData->pDeviceFuncs->pfnLockAsync = NULL; //vboxWddmDDevLockAsync;
        pCreateData->pDeviceFuncs->pfnUnlockAsync = NULL; //vboxWddmDDevUnlockAsync;
        pCreateData->pDeviceFuncs->pfnRename = NULL; //vboxWddmDDevRename;


        do
        {
            Assert(!pCreateData->AllocationListSize
                    && !pCreateData->PatchLocationListSize);
            if (!pCreateData->AllocationListSize
                    && !pCreateData->PatchLocationListSize)
            {
                pDevice->DefaultContext.ContextInfo.NodeOrdinal = 0;
                pDevice->DefaultContext.ContextInfo.EngineAffinity = 0;
                pDevice->DefaultContext.ContextInfo.Flags.Value = 0;
                pDevice->DefaultContext.ContextInfo.pPrivateDriverData = NULL;
                pDevice->DefaultContext.ContextInfo.PrivateDriverDataSize = 0;
                pDevice->DefaultContext.ContextInfo.hContext = 0;
                pDevice->DefaultContext.ContextInfo.pCommandBuffer = NULL;
                pDevice->DefaultContext.ContextInfo.CommandBufferSize = 0;
                pDevice->DefaultContext.ContextInfo.pAllocationList = NULL;
                pDevice->DefaultContext.ContextInfo.AllocationListSize = 0;
                pDevice->DefaultContext.ContextInfo.pPatchLocationList = NULL;
                pDevice->DefaultContext.ContextInfo.PatchLocationListSize = 0;

                hr = pDevice->RtCallbacks.pfnCreateContextCb(pDevice->hDevice, &pDevice->DefaultContext.ContextInfo);
                Assert(hr == S_OK);
                if (hr == S_OK)
                {
                    if (pDevice->u32IfVersion > 7)
                    {
                        /* D3D */
                        if (pAdapter->pD3D9If)
                        {
                            /* */
                            vboxVDbgPrint((__FUNCTION__": TODO: Implement D3D Device Creation\n"));
                            break;
                        }
                        else
                        {
                            /* ballback */
                            vboxVDbgPrint((__FUNCTION__": D3D Device Being Created, but D3D is unavailable\n"));
                            break;
                        }
                    }
                    else
                    {
                        /* DDraw */
                        vboxVDbgPrint((__FUNCTION__": DirectDraw Device Created\n"));
                        break;
                    }

                    D3DDDICB_DESTROYCONTEXT DestroyContext;
                    DestroyContext.hContext = pDevice->DefaultContext.ContextInfo.hContext;

                    HRESULT tmpHr = pDevice->RtCallbacks.pfnDestroyContextCb(pDevice->hDevice, &DestroyContext);
                    Assert(tmpHr == S_OK);
                }
                else
                    vboxVDbgPrintR((__FUNCTION__": pfnCreateContextCb failed, hr(%d)\n", hr));
            }
            else
            {
                vboxVDbgPrintR((__FUNCTION__": Not implemented: PatchLocationListSize(%d), AllocationListSize(%d)\n",
                        pCreateData->PatchLocationListSize, pCreateData->AllocationListSize));
                //pCreateData->pAllocationList = ??
                hr = E_FAIL;
            }

            RTMemFree(pDevice);
        } while (0);
    }
    else
    {
        vboxVDbgPrintR((__FUNCTION__": RTMemAllocZ returned NULL\n"));
        hr = E_OUTOFMEMORY;
    }

    vboxVDbgPrint(("<== "__FUNCTION__", hAdapter(0x%p)\n", hAdapter));

    return hr;
}

static HRESULT APIENTRY vboxWddmDispCloseAdapter (IN HANDLE hAdapter)
{
    vboxVDbgPrint(("==> "__FUNCTION__", hAdapter(0x%p)\n", hAdapter));

//    AssertBreakpoint();

    PVBOXWDDMDISP_ADAPTER pAdapter = (PVBOXWDDMDISP_ADAPTER)hAdapter;
    if (pAdapter->pD3D9If)
    {
        HRESULT hr = pAdapter->pD3D9If->Release();
        Assert(hr == S_OK);
        VBoxDispD3DClose(&pAdapter->D3D);
    }

    vboxCapsFree(pAdapter);

    RTMemFree(pAdapter);

    vboxVDbgPrint(("<== "__FUNCTION__", hAdapter(0x%p)\n", hAdapter));

    return S_OK;
}

HRESULT APIENTRY OpenAdapter (__inout D3DDDIARG_OPENADAPTER*  pOpenData)
{
    vboxVDbgPrint(("==> "__FUNCTION__"\n"));

    VBOXWDDM_QI Query;
    D3DDDICB_QUERYADAPTERINFO DdiQuery;
    DdiQuery.PrivateDriverDataSize = sizeof(Query);
    DdiQuery.pPrivateDriverData = &Query;
    HRESULT hr = pOpenData->pAdapterCallbacks->pfnQueryAdapterInfoCb(pOpenData->hAdapter, &DdiQuery);
    Assert(hr == S_OK);
    if (hr != S_OK)
    {
        vboxVDbgPrintR((__FUNCTION__": pfnQueryAdapterInfoCb failed, hr (%d)\n", hr));
        return E_FAIL;
    }

    /* check the miniport version match display version */
    if (Query.u32Version != VBOXVIDEOIF_VERSION)
    {
        vboxVDbgPrintR((__FUNCTION__": miniport version mismatch, expected (%d), but was (%d)\n",
                VBOXVIDEOIF_VERSION,
                Query.u32Version));
        return E_FAIL;
    }

#ifdef VBOX_WITH_VIDEOHWACCEL
    Assert(Query.cInfos >= 1);
    PVBOXWDDMDISP_ADAPTER pAdapter = (PVBOXWDDMDISP_ADAPTER)RTMemAllocZ(RT_OFFSETOF(VBOXWDDMDISP_ADAPTER, aHeads[Query.cInfos]));
#else
    PVBOXWDDMDISP_ADAPTER pAdapter = (PVBOXWDDMDISP_ADAPTER)RTMemAllocZ(sizeof (VBOXWDDMDISP_ADAPTER));
#endif
    Assert(pAdapter);
    if (pAdapter)
    {
        pAdapter->hAdapter = pOpenData->hAdapter;
        pAdapter->uIfVersion = pOpenData->Interface;
        pAdapter->uRtVersion= pOpenData->Version;
        pAdapter->RtCallbacks = *pOpenData->pAdapterCallbacks;

        pAdapter->cHeads = Query.cInfos;


        pOpenData->hAdapter = pAdapter;
        pOpenData->pAdapterFuncs->pfnGetCaps = vboxWddmDispGetCaps;
        pOpenData->pAdapterFuncs->pfnCreateDevice = vboxWddmDispCreateDevice;
        pOpenData->pAdapterFuncs->pfnCloseAdapter = vboxWddmDispCloseAdapter;
        pOpenData->DriverVersion = D3D_UMD_INTERFACE_VERSION;
        /*
         * here we detect whether we are called by the d3d or ddraw.
         * in the d3d case we init our d3d environment
         * in the ddraw case we init 2D acceleration
         * if interface version is > 7, this is D3D, treat it as so
         * otherwise treat it as ddraw
         * @todo: need a more clean way of doing this */

        if (pAdapter->uIfVersion > 7)
        {
            do
            {
                /* try enable the 3D */
                hr = VBoxDispD3DOpen(&pAdapter->D3D);
                Assert(hr == S_OK);
                if (hr == S_OK)
                {
                    hr = pAdapter->D3D.pfnDirect3DCreate9Ex(D3D_SDK_VERSION, &pAdapter->pD3D9If);
                    Assert(hr == S_OK);
                    if (hr == S_OK)
                    {
                        vboxVDbgPrint((__FUNCTION__": SUCCESS 3D Enabled, pAdapter (0x%p)\n", pAdapter));
                        break;
                    }
                    else
                        vboxVDbgPrintR((__FUNCTION__": pfnDirect3DCreate9Ex failed, hr (%d)\n", hr));
                    VBoxDispD3DClose(&pAdapter->D3D);
                }
                else
                    vboxVDbgPrintR((__FUNCTION__": VBoxDispD3DOpen failed, hr (%d)\n", hr));
            } while (0);
        }
#ifdef VBOX_WITH_VIDEOHWACCEL
        else
        {
            for (uint32_t i = 0; i < pAdapter->cHeads; ++i)
            {
                pAdapter->aHeads[i].Vhwa.Settings = Query.aInfos[i];
            }
        }
#endif

        vboxCapsInit(pAdapter);
        hr = S_OK;
//        RTMemFree(pAdapter);
    }
    else
    {
        vboxVDbgPrintR((__FUNCTION__": RTMemAllocZ returned NULL\n"));
        hr = E_OUTOFMEMORY;
    }

    vboxVDbgPrint(("<== "__FUNCTION__", hr (%d)\n", hr));

    return hr;
}

#ifdef VBOXWDDMDISP_DEBUG
VOID vboxVDbgDoPrint(LPCSTR szString, ...)
{
    char szBuffer[1024] = {0};
    va_list pArgList;
    va_start(pArgList, szString);
    _vsnprintf(szBuffer, sizeof(szBuffer) / sizeof(szBuffer[0]), szString, pArgList);
    va_end(pArgList);

    OutputDebugStringA(szBuffer);
}
#endif
