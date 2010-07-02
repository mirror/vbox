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
#include <iprt/initterm.h>
#include <iprt/log.h>
#include <iprt/mem.h>

#include <VBox/Log.h>

#include <VBox/VBoxGuestLib.h>

#include "VBoxDispD3DCmn.h"
#include "VBoxDispD3D.h"

#ifdef VBOXDISPMP_TEST
HRESULT vboxDispMpTstStart();
HRESULT vboxDispMpTstStop();
#endif

#ifdef VBOXWDDMDISP_DEBUG
# include <stdio.h>
#endif

#define VBOXWDDMDISP_WITH_TMPWORKAROUND 1

#define VBOXWDDMOVERLAY_TEST

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

static D3DDDIQUERYTYPE gVBoxQueryTypes[] = {
        D3DDDIQUERYTYPE_EVENT,
        D3DDDIQUERYTYPE_OCCLUSION
};

#define VBOX_QUERYTYPE_COUNT() RT_ELEMENTS(gVBoxQueryTypes)

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

static void vboxWddmLockUnlockMemSynch(PVBOXWDDMDISP_ALLOCATION pAlloc, D3DLOCKED_RECT *pLockInfo, RECT *pRect, bool bToLockInfo)
{
    Assert(pAlloc->SurfDesc.pitch);
    Assert(pAlloc->pvMem);

    if (!pRect)
    {
        if (pAlloc->SurfDesc.pitch == pLockInfo->Pitch)
        {
            if (bToLockInfo)
                memcpy(pLockInfo->pBits, pAlloc->pvMem, pAlloc->SurfDesc.pitch * pAlloc->SurfDesc.height);
            else
                memcpy(pAlloc->pvMem, pLockInfo->pBits, pAlloc->SurfDesc.pitch * pAlloc->SurfDesc.height);
        }
        else
        {
            uint8_t *pvSrc, *pvDst;
            uint32_t srcPitch, dstPitch;
            if (bToLockInfo)
            {
                pvSrc = (uint8_t *)pAlloc->pvMem;
                pvDst = (uint8_t *)pLockInfo->pBits;
                srcPitch = pAlloc->SurfDesc.pitch;
                dstPitch = pLockInfo->Pitch;
            }
            else
            {
                pvDst = (uint8_t *)pAlloc->pvMem;
                pvSrc = (uint8_t *)pLockInfo->pBits;
                dstPitch = pAlloc->SurfDesc.pitch;
                srcPitch = (uint32_t)pLockInfo->Pitch;
            }

            uint32_t pitch = RT_MIN(srcPitch, dstPitch);
            Assert(pitch);
            for (UINT j = 0; j < pAlloc->SurfDesc.height; ++j)
            {
                memcpy(pvDst, pvSrc, pitch);
                pvSrc += srcPitch;
                pvDst += dstPitch;
            }
        }
    }
    else
    {
        uint8_t *pvSrc, *pvDst;
        uint32_t srcPitch, dstPitch;
        /* @todo: this is not entirely correct */
        uint8_t * pvAllocMemStart = (uint8_t *)pAlloc->pvMem;
        uint32_t cbPP = pAlloc->SurfDesc.pitch/pAlloc->SurfDesc.width;
        pvAllocMemStart += pAlloc->SurfDesc.pitch * pRect->top + pRect->left * cbPP;

        if (bToLockInfo)
        {
            pvSrc = (uint8_t *)pvAllocMemStart;
            pvDst = (uint8_t *)pLockInfo->pBits;
            srcPitch = pAlloc->SurfDesc.pitch;
            dstPitch = pLockInfo->Pitch;
        }
        else
        {
            pvDst = (uint8_t *)pvAllocMemStart;
            pvSrc = (uint8_t *)pLockInfo->pBits;
            dstPitch = pAlloc->SurfDesc.pitch;
            srcPitch = (uint32_t)pLockInfo->Pitch;
        }

        uint32_t cPixCopyLine = pRect->right - pRect->left;

        if (cPixCopyLine == pAlloc->SurfDesc.width && srcPitch == dstPitch)
        {
            memcpy(pvDst, pvSrc, pAlloc->SurfDesc.pitch * (pRect->bottom - pRect->top));
        }
        else
        {
            uint32_t pitch = RT_MIN(srcPitch, dstPitch);
            uint32_t cbCopyLine = cPixCopyLine * cbPP;
            Assert(pitch);
            for (int j = pRect->top; j < pRect->bottom; ++j)
            {
                memcpy(pvDst, pvSrc, cbCopyLine);
                pvSrc += srcPitch;
                pvDst += dstPitch;
            }
        }
    }
}

#if 0
static HRESULT vboxWddmRectBltPerform(uint8_t *pvDstSurf, const uint8_t *pvSrcSurf,
        RECT *pDstRect, RECT *pSrcRect,
        uint32_t DstPitch, uint32_t SrcPitch, uint32_t bpp,
        RECT *pDstCopyRect, RECT *pSrcCopyRect)
{
    uint32_t DstCopyWidth = pDstCopyRect->left - pDstCopyRect->right;
    uint32_t DstCopyHeight = pDstCopyRect->bottom - pDstCopyRect->top;
    uint32_t SrcCopyWidth = pSrcCopyRect->left - pSrcCopyRect->right;
    uint32_t SrcCopyHeight = pSrcCopyRect->bottom - pSrcCopyRect->top;
    uint32_t srcBpp = bpp;
    uint32_t dstBpp = bpp;
    /* we do not support stretching */
    Assert(DstCopyWidth == SrcCopyWidth);
    Assert(DstCopyHeight == SrcCopyWidth);
    if (DstCopyWidth != SrcCopyWidth)
        return E_FAIL;
    if (DstCopyHeight != SrcCopyWidth)
        return E_FAIL;

    uint32_t DstWidth = pDstRect->left - pDstRect->right;
    uint32_t DstHeight = pDstRect->bottom - pDstRect->top;
    uint32_t SrcWidth = pSrcRect->left - pSrcRect->right;
    uint32_t SrcHeight = pSrcRect->bottom - pSrcRect->top;

    if (DstWidth == DstCopyWidth
            && SrcWidth == SrcCopyWidth
            && SrcWidth == DstWidth)
    {
        Assert(!pDstCopyRect->left);
        Assert(!pSrcCopyRect->left);
        uint32_t cbOff = DstPitch * pDstCopyRect->top;
        uint32_t cbSize = DstPitch * DstCopyHeight;
        memcpy(pvDstSurf + cbOff, pvSrcSurf + cbOff, cbSize);
    }
    else
    {
        uint32_t offDstLineStart = pDstCopyRect->left * dstBpp >> 3;
        uint32_t offDstLineEnd = ((pDstCopyRect->left * dstBpp + 7) >> 3) + ((dstBpp * DstCopyWidth + 7) >> 3);
        uint32_t cbDstLine = offDstLineEnd - offDstLineStart;
        uint32_t offDstStart = DstPitch * pDstCopyRect->top + offDstLineStart;
        Assert(cbDstLine <= DstPitch);
        uint32_t cbDstSkip = DstPitch;
        uint8_t * pvDstStart = pvDstSurf + offDstStart;

        uint32_t offSrcLineStart = pSrcCopyRect->left * srcBpp >> 3;
        uint32_t offSrcLineEnd = ((pSrcCopyRect->left * srcBpp + 7) >> 3) + ((srcBpp * SrcCopyWidth + 7) >> 3);
        uint32_t cbSrcLine = offSrcLineEnd - offSrcLineStart;
        uint32_t offSrcStart = SrcPitch * pSrcCopyRect->top + offSrcLineStart;
        Assert(cbSrcLine <= SrcPitch);
        uint32_t cbSrcSkip = SrcPitch;
        const uint8_t * pvSrcStart = pvSrcSurf + offSrcStart;

        Assert(cbDstLine == cbSrcLine);

        for (uint32_t i = 0; ; ++i)
        {
            memcpy (pvDstStart, pvSrcStart, cbDstLine);
            if (i == DstCopyHeight)
                break;
            pvDstStart += cbDstSkip;
            pvSrcStart += cbSrcSkip;
        }
    }
    return S_OK;
}
#endif

static HRESULT vboxWddmRectBltPerform(uint8_t *pvDstSurf, const uint8_t *pvSrcSurf,
        const RECT *pDstRect, const RECT *pSrcRect,
        uint32_t DstPitch, uint32_t SrcPitch, uint32_t bpp)
{
    uint32_t DstWidth = pDstRect->left - pDstRect->right;
    uint32_t DstHeight = pDstRect->bottom - pDstRect->top;
    uint32_t SrcWidth = pSrcRect->left - pSrcRect->right;
    uint32_t SrcHeight = pSrcRect->bottom - pSrcRect->top;
    uint32_t srcBpp = bpp;
    uint32_t dstBpp = bpp;
    /* we do not support stretching */
    Assert(DstWidth == SrcWidth);
    Assert(DstHeight == SrcWidth);
    if (DstWidth != SrcWidth)
        return E_FAIL;
    if (DstHeight != SrcWidth)
        return E_FAIL;

    if (DstPitch == SrcPitch
            && ((DstWidth * bpp)/8) == DstPitch)
    {
        Assert(!pDstRect->left);
        Assert(!pSrcRect->left);
        uint32_t cbOff = DstPitch * pDstRect->top;
        uint32_t cbSize = DstPitch * DstHeight;
        memcpy(pvDstSurf + cbOff, pvSrcSurf + cbOff, cbSize);
    }
    else
    {

        uint32_t cbDstLine = (((DstWidth * dstBpp) + 7) >> 3);
        for (uint32_t i = 0; ; ++i)
        {
            memcpy (pvDstSurf, pvSrcSurf, cbDstLine);
            if (i == DstHeight)
                break;
            pvDstSurf += DstPitch;
            pvSrcSurf += SrcPitch;
        }
    }
    return S_OK;
}

static D3DFORMAT vboxDDI2D3DFormat(D3DDDIFORMAT format)
{
    /* @todo: check they are all equal */
    return (D3DFORMAT)format;
}

D3DMULTISAMPLE_TYPE vboxDDI2D3DMultiSampleType(D3DDDIMULTISAMPLE_TYPE enmType)
{
    /* @todo: check they are all equal */
    return (D3DMULTISAMPLE_TYPE)enmType;
}

D3DPOOL vboxDDI2D3DPool(D3DDDI_POOL enmPool)
{
    /* @todo: check they are all equal */
    switch (enmPool)
    {
    case D3DDDIPOOL_SYSTEMMEM:
        return D3DPOOL_SYSTEMMEM;
    case D3DDDIPOOL_VIDEOMEMORY:
    case D3DDDIPOOL_LOCALVIDMEM:
    case D3DDDIPOOL_NONLOCALVIDMEM:
        /* @todo: what would be propper here? */
        return D3DPOOL_DEFAULT;
    default:
        AssertBreakpoint();
    }
    return D3DPOOL_DEFAULT;
}

D3DRENDERSTATETYPE vboxDDI2D3DRenderStateType(D3DDDIRENDERSTATETYPE enmType)
{
    /* @todo: @fixme: not entirely correct, need to check */
    return (D3DRENDERSTATETYPE)enmType;
}

VBOXWDDMDISP_TSS_LOOKUP vboxDDI2D3DTestureStageStateType(D3DDDITEXTURESTAGESTATETYPE enmType)
{
    static const VBOXWDDMDISP_TSS_LOOKUP lookup[] =
    {
        {FALSE, D3DTSS_FORCE_DWORD},             /*  0, D3DDDITSS_TEXTUREMAP */
        {FALSE, D3DTSS_COLOROP},                 /*  1, D3DDDITSS_COLOROP */
        {FALSE, D3DTSS_COLORARG1},               /*  2, D3DDDITSS_COLORARG1 */
        {FALSE, D3DTSS_COLORARG2},               /*  3, D3DDDITSS_COLORARG2 */
        {FALSE, D3DTSS_ALPHAOP},                 /*  4, D3DDDITSS_ALPHAOP */
        {FALSE, D3DTSS_ALPHAARG1},               /*  5, D3DDDITSS_ALPHAARG1 */
        {FALSE, D3DTSS_ALPHAARG2},               /*  6, D3DDDITSS_ALPHAARG2 */
        {FALSE, D3DTSS_BUMPENVMAT00},            /*  7, D3DDDITSS_BUMPENVMAT00 */
        {FALSE, D3DTSS_BUMPENVMAT01},            /*  8, D3DDDITSS_BUMPENVMAT01 */
        {FALSE, D3DTSS_BUMPENVMAT10},            /*  9, D3DDDITSS_BUMPENVMAT10 */
        {FALSE, D3DTSS_BUMPENVMAT11},            /* 10, D3DDDITSS_BUMPENVMAT11 */
        {FALSE, D3DTSS_TEXCOORDINDEX},           /* 11, D3DDDITSS_TEXCOORDINDEX */
        {FALSE, D3DTSS_FORCE_DWORD},             /* 12, unused */
        {TRUE, D3DSAMP_ADDRESSU},                /* 13, D3DDDITSS_ADDRESSU */
        {TRUE, D3DSAMP_ADDRESSV},                /* 14, D3DDDITSS_ADDRESSV */
        {TRUE, D3DSAMP_BORDERCOLOR},             /* 15, D3DDDITSS_BORDERCOLOR */
        {TRUE, D3DSAMP_MAGFILTER},               /* 16, D3DDDITSS_MAGFILTER */
        {TRUE, D3DSAMP_MINFILTER},               /* 17, D3DDDITSS_MINFILTER */
        {TRUE, D3DSAMP_MIPFILTER},               /* 18, D3DDDITSS_MIPFILTER */
        {TRUE, D3DSAMP_MIPMAPLODBIAS},           /* 19, D3DDDITSS_MIPMAPLODBIAS */
        {TRUE, D3DSAMP_MAXMIPLEVEL},             /* 20, D3DDDITSS_MAXMIPLEVEL */
        {TRUE, D3DSAMP_MAXANISOTROPY},           /* 21, D3DDDITSS_MAXANISOTROPY */
        {FALSE, D3DTSS_BUMPENVLSCALE},           /* 22, D3DDDITSS_BUMPENVLSCALE */
        {FALSE, D3DTSS_BUMPENVLOFFSET},          /* 23, D3DDDITSS_BUMPENVLOFFSET */
        {FALSE, D3DTSS_TEXTURETRANSFORMFLAGS},   /* 24, D3DDDITSS_TEXTURETRANSFORMFLAGS */
        {TRUE, D3DSAMP_ADDRESSW},                /* 25, D3DDDITSS_ADDRESSW */
        {FALSE, D3DTSS_COLORARG0},               /* 26, D3DDDITSS_COLORARG0 */
        {FALSE, D3DTSS_ALPHAARG0},               /* 27, D3DDDITSS_ALPHAARG0 */
        {FALSE, D3DTSS_RESULTARG},               /* 28, D3DDDITSS_RESULTARG */
        {FALSE, D3DTSS_FORCE_DWORD},             /* 29, D3DDDITSS_SRGBTEXTURE */
        {FALSE, D3DTSS_FORCE_DWORD},             /* 30, D3DDDITSS_ELEMENTINDEX */
        {FALSE, D3DTSS_FORCE_DWORD},             /* 31, D3DDDITSS_DMAPOFFSET */
        {FALSE, D3DTSS_CONSTANT},                /* 32, D3DDDITSS_CONSTANT */
        {FALSE, D3DTSS_FORCE_DWORD},             /* 33, D3DDDITSS_DISABLETEXTURECOLORKEY */
        {FALSE, D3DTSS_FORCE_DWORD},             /* 34, D3DDDITSS_TEXTURECOLORKEYVAL */
    };

    return lookup[enmType];
}

DWORD vboxDDI2D3DUsage(D3DDDI_RESOURCEFLAGS fFlags)
{
    DWORD fUsage = 0;
    if (fFlags.Dynamic)
        fUsage |= D3DUSAGE_DYNAMIC;
    if (fFlags.AutogenMipmap)
        fUsage |= D3DUSAGE_AUTOGENMIPMAP;
    if (fFlags.DMap)
        fUsage |= D3DUSAGE_DMAP;
    if (fFlags.WriteOnly)
        fUsage |= D3DUSAGE_WRITEONLY;
    if (fFlags.NPatches)
        fUsage |= D3DUSAGE_NPATCHES;
    if (fFlags.Points)
        fUsage |= D3DUSAGE_POINTS;
    if (fFlags.RenderTarget)
        fUsage |= D3DUSAGE_RENDERTARGET;
    if (fFlags.RtPatches)
        fUsage |= D3DUSAGE_RTPATCHES;
    if (fFlags.TextApi)
        fUsage |= D3DUSAGE_TEXTAPI;
    if (fFlags.WriteOnly)
        fUsage |= D3DUSAGE_WRITEONLY;
    //below are wddm 1.1-specific
//    if (fFlags.RestrictedContent)
//        fUsage |= D3DUSAGE_RESTRICTED_CONTENT;
//    if (fFlags.RestrictSharedAccess)
//        fUsage |= D3DUSAGE_RESTRICT_SHARED_RESOURCE;
    return fUsage;
}

DWORD vboxDDI2D3DLockFlags(D3DDDI_LOCKFLAGS fLockFlags)
{
    DWORD fFlags = 0;
    if (fLockFlags.Discard)
        fFlags |= D3DLOCK_DISCARD;
    if (fLockFlags.NoOverwrite)
        fFlags |= D3DLOCK_NOOVERWRITE;
    if (fLockFlags.ReadOnly)
        fFlags |= D3DLOCK_READONLY;
    if (fLockFlags.DoNotWait)
        fFlags |= D3DLOCK_DONOTWAIT;
    return fFlags;
}

D3DTEXTUREFILTERTYPE vboxDDI2D3DBltFlags(D3DDDI_BLTFLAGS fFlags)
{
    if (fFlags.Point)
    {
        /* no flags other than [Begin|Continue|End]PresentToDwm are set */
        Assert((fFlags.Value & (~(0x00000100 | 0x00000200 | 0x00000400))) == 1);
        return D3DTEXF_POINT;
    }
    if (fFlags.Linear)
    {
        /* no flags other than [Begin|Continue|End]PresentToDwm are set */
        Assert((fFlags.Value & (~(0x00000100 | 0x00000200 | 0x00000400))) == 2);
        return D3DTEXF_LINEAR;
    }
    /* no flags other than [Begin|Continue|End]PresentToDwm are set */
    Assert((fFlags.Value & (~(0x00000100 | 0x00000200 | 0x00000400))) == 0);
    return D3DTEXF_NONE;
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
    BOOL bOk = TRUE;

    switch (dwReason)
    {
        case DLL_PROCESS_ATTACH:
        {
            RTR3Init();

            HRESULT hr = vboxDispCmInit();
            Assert(hr == S_OK);
#ifdef VBOXDISPMP_TEST
            if (hr == S_OK)
            {
                hr = vboxDispMpTstStart();
                Assert(hr == S_OK);
            }
#endif
            if (hr == S_OK)
                vboxVDbgPrint(("VBoxDispD3D: DLL loaded.\n"));
            else
                bOk = FALSE;

//                VbglR3Init();
            break;
        }

        case DLL_PROCESS_DETACH:
        {
            HRESULT hr;
#ifdef VBOXDISPMP_TEST
            hr = vboxDispMpTstStop();
            Assert(hr == S_OK);
            if (hr == S_OK)
#endif
            {
                hr = vboxDispCmTerm();
                Assert(hr == S_OK);
                if (hr == S_OK)
                    vboxVDbgPrint(("VBoxDispD3D: DLL unloaded.\n"));
                else
                    bOk = FALSE;
            }
//                VbglR3Term();
            /// @todo RTR3Term();
            break;
        }

        default:
            break;
    }
    return bOk;
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
            Assert(!VBOXDISPMODE_IS_3D(pAdapter));
            Assert(pData->DataSize == sizeof (DDRAW_CAPS));
            if (pData->DataSize >= sizeof (DDRAW_CAPS))
            {
                memset(pData->pData, 0, sizeof (DDRAW_CAPS));
#ifdef VBOX_WITH_VIDEOHWACCEL
                if (vboxVhwaHasCKeying(pAdapter))
                {
                    DDRAW_CAPS *pCaps = (DDRAW_CAPS*)pData->pData;
                    pCaps->Caps |= DDRAW_CAPS_COLORKEY;
//                    pCaps->Caps2 |= DDRAW_CAPS2_FLIPNOVSYNC;
                }
#endif
            }
            else
                hr = E_INVALIDARG;
            break;
        }
        case D3DDDICAPS_DDRAW_MODE_SPECIFIC:
        {
            Assert(!VBOXDISPMODE_IS_3D(pAdapter));
            Assert(pData->DataSize == sizeof (DDRAW_MODE_SPECIFIC_CAPS));
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
            Assert(pData->DataSize == pAdapter->cFormstOps * sizeof (FORMATOP));
            memcpy(pData->pData, pAdapter->paFormstOps, pAdapter->cFormstOps * sizeof (FORMATOP));
            break;
        case D3DDDICAPS_GETD3DQUERYCOUNT:
#if 0
            *((uint32_t*)pData->pData) = VBOX_QUERYTYPE_COUNT();
#else
            *((uint32_t*)pData->pData) = 0;
#endif
            break;
        case D3DDDICAPS_GETD3DQUERYDATA:
#if 0
            Assert(pData->DataSize == VBOX_QUERYTYPE_COUNT() * sizeof (D3DDDIQUERYTYPE));
            memcpy(pData->pData, gVBoxQueryTypes, VBOX_QUERYTYPE_COUNT() * sizeof (D3DDDIQUERYTYPE));
#else
            AssertBreakpoint();
            memset(pData->pData, 0, pData->DataSize);
#endif
            break;
        case D3DDDICAPS_GETD3D3CAPS:
            Assert(!VBOXDISPMODE_IS_3D(pAdapter));
            Assert(pData->DataSize == sizeof (D3DHAL_GLOBALDRIVERDATA));
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
//                        | D3DDEVCAPS_DRAWPRIMTLVERTEX
                        | D3DDEVCAPS_EXECUTESYSTEMMEMORY
                        | D3DDEVCAPS_EXECUTEVIDEOMEMORY
//                        | D3DDEVCAPS_FLOATTLVERTEX
                        | D3DDEVCAPS_HWRASTERIZATION
//                        | D3DDEVCAPS_HWTRANSFORMANDLIGHT
//                        | D3DDEVCAPS_TLVERTEXSYSTEMMEMORY
//                        | D3DDEVCAPS_TEXTUREVIDEOMEMORY
                        ;
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
                pCaps->hwCaps.dwDeviceRenderBitDepth = DDBD_8 | DDBD_16 | DDBD_24 | DDBD_32;
                pCaps->hwCaps.dwDeviceZBufferBitDepth = 0;
                pCaps->hwCaps.dwMaxBufferSize = 0;
                pCaps->hwCaps.dwMaxVertexCount = 0;


                pCaps->dwNumVertices = 0;
                pCaps->dwNumClipVertices = 0;
                pCaps->dwNumTextureFormats = 0;//pAdapter->cSurfDescs;
                pCaps->lpTextureFormats = NULL;//pAdapter->paSurfDescs;
            }
            else
                hr = E_INVALIDARG;
            break;
        case D3DDDICAPS_GETD3D7CAPS:
            Assert(!VBOXDISPMODE_IS_3D(pAdapter));
            Assert(pData->DataSize == sizeof (D3DHAL_D3DEXTENDEDCAPS));
            if (pData->DataSize >= sizeof (D3DHAL_D3DEXTENDEDCAPS))
            {
                memset(pData->pData, 0, sizeof (D3DHAL_D3DEXTENDEDCAPS));
                D3DHAL_D3DEXTENDEDCAPS *pCaps = (D3DHAL_D3DEXTENDEDCAPS*)pData->pData;
                pCaps->dwSize = sizeof (D3DHAL_D3DEXTENDEDCAPS);
            }
            else
                hr = E_INVALIDARG;
            break;
        case D3DDDICAPS_GETD3D9CAPS:
        {
            Assert(pData->DataSize >= sizeof (D3DCAPS9));
//            AssertBreakpoint();
            if (pData->DataSize >= sizeof (D3DCAPS9))
            {
                Assert(VBOXDISPMODE_IS_3D(pAdapter));
                if (VBOXDISPMODE_IS_3D(pAdapter))
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
        case D3DDDICAPS_GETVIDEOPROCESSORCAPS:
        case D3DDDICAPS_GETEXTENSIONGUIDCOUNT:
        case D3DDDICAPS_GETDECODEGUIDCOUNT:
        case D3DDDICAPS_GETVIDEOPROCESSORDEVICEGUIDCOUNT:
            if (pData->pData && pData->DataSize)
                memset(pData->pData, 0, pData->DataSize);
            break;
        case D3DDDICAPS_GETMULTISAMPLEQUALITYLEVELS:
        case D3DDDICAPS_GETD3D5CAPS:
        case D3DDDICAPS_GETD3D6CAPS:
        case D3DDDICAPS_GETD3D8CAPS:
        case D3DDDICAPS_GETDECODEGUIDS:
        case D3DDDICAPS_GETDECODERTFORMATCOUNT:
        case D3DDDICAPS_GETDECODERTFORMATS:
        case D3DDDICAPS_GETDECODECOMPRESSEDBUFFERINFOCOUNT:
        case D3DDDICAPS_GETDECODECOMPRESSEDBUFFERINFO:
        case D3DDDICAPS_GETDECODECONFIGURATIONCOUNT:
        case D3DDDICAPS_GETDECODECONFIGURATIONS:
        case D3DDDICAPS_GETVIDEOPROCESSORDEVICEGUIDS:
        case D3DDDICAPS_GETVIDEOPROCESSORRTFORMATCOUNT:
        case D3DDDICAPS_GETVIDEOPROCESSORRTFORMATS:
        case D3DDDICAPS_GETVIDEOPROCESSORRTSUBSTREAMFORMATCOUNT:
        case D3DDDICAPS_GETVIDEOPROCESSORRTSUBSTREAMFORMATS:
        case D3DDDICAPS_GETPROCAMPRANGE:
        case D3DDDICAPS_FILTERPROPERTYRANGE:
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
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    Assert(pDevice);
    Assert(pDevice->pDevice9If);
    HRESULT hr = pDevice->pDevice9If->SetRenderState(vboxDDI2D3DRenderStateType(pData->State), pData->Value);
    Assert(hr == S_OK);
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p), hr(0x%x)\n", hDevice, hr));
    return hr;
}

static HRESULT APIENTRY vboxWddmDDevUpdateWInfo(HANDLE hDevice, CONST D3DDDIARG_WINFO* pData)
{
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return S_OK;
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
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    Assert(pDevice);
    Assert(pDevice->pDevice9If);

    VBOXWDDMDISP_TSS_LOOKUP lookup = vboxDDI2D3DTestureStageStateType(pData->State);
    HRESULT hr;

    if (!lookup.bSamplerState)
    {
        hr = pDevice->pDevice9If->SetTextureStageState(pData->Stage, D3DTEXTURESTAGESTATETYPE(lookup.dType), pData->Value);
    } 
    else
    {
        hr = pDevice->pDevice9If->SetSamplerState(pData->Stage, D3DSAMPLERSTATETYPE(lookup.dType), pData->Value);
    }

    Assert(hr == S_OK);
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p), hr(0x%x)\n", hDevice, hr));
    return hr;
}

static HRESULT APIENTRY vboxWddmDDevSetTexture(HANDLE hDevice, UINT Stage, HANDLE hTexture)
{
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    Assert(pDevice);
    Assert(pDevice->pDevice9If);
    PVBOXWDDMDISP_RESOURCE pRc = (PVBOXWDDMDISP_RESOURCE)hTexture;
//    Assert(pRc);
    IDirect3DTexture9 *pD3DIfTex;
    if (pRc)
    {
        Assert(pRc->aAllocations[0].enmD3DIfType == VBOXDISP_D3DIFTYPE_TEXTURE);
        pD3DIfTex = (IDirect3DTexture9*)pRc->aAllocations[0].pD3DIf;
    }
    else
        pD3DIfTex = NULL;

//    Assert(pD3DIfTex);
    HRESULT hr = pDevice->pDevice9If->SetTexture(Stage, pD3DIfTex);
    Assert(hr == S_OK);
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p), hr(0x%x)\n", hDevice, hr));
    return hr;
}

static HRESULT APIENTRY vboxWddmDDevSetPixelShader(HANDLE hDevice, HANDLE hShaderHandle)
{
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    Assert(pDevice);
    Assert(pDevice->pDevice9If);
    IDirect3DPixelShader9 *pShader = (IDirect3DPixelShader9*)hShaderHandle;
    Assert(pShader);
    HRESULT hr = pDevice->pDevice9If->SetPixelShader(pShader);
    Assert(hr == S_OK);
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p), hr(0x%x)\n", hDevice, hr));
    return hr;
}

static HRESULT APIENTRY vboxWddmDDevSetPixelShaderConst(HANDLE hDevice, CONST D3DDDIARG_SETPIXELSHADERCONST* pData, CONST FLOAT* pRegisters)
{
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    Assert(pDevice);
    Assert(pDevice->pDevice9If);
    HRESULT hr = pDevice->pDevice9If->SetPixelShaderConstantF(pData->Register, pRegisters, pData->Count);
    Assert(hr == S_OK);
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p), hr(0x%x)\n", hDevice, hr));
    return hr;
}

static HRESULT APIENTRY vboxWddmDDevSetStreamSourceUm(HANDLE hDevice, CONST D3DDDIARG_SETSTREAMSOURCEUM* pData, CONST VOID* pUMBuffer )
{
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    Assert(pDevice);
    Assert(pDevice->pDevice9If);
    HRESULT hr = S_OK;
//    IDirect3DVertexBuffer9 *pStreamData;
//    UINT cbOffset;
//    UINT cbStride;
//    hr = pDevice->pDevice9If->GetStreamSource(pData->Stream, &pStreamData, &cbOffset, &cbStride);
//    Assert(hr == S_OK);
//    if (hr == S_OK)
//    {
//        if (pStreamData)
//        {
//            AssertBreakpoint();
//            /* @todo: impl! */
//        }
//        else
//        {
            Assert(pData->Stream < RT_ELEMENTS(pDevice->aStreamSourceUm));
            PVBOXWDDMDISP_STREAMSOURCEUM pStrSrcUm = &pDevice->aStreamSourceUm[pData->Stream];
            pStrSrcUm->pvBuffer = pUMBuffer;
            pStrSrcUm->cbStride = pData->Stride;
//        }
//    }
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p), hr(0x%x)\n", hDevice, hr));
    return hr;
}

static HRESULT APIENTRY vboxWddmDDevSetIndices(HANDLE hDevice, CONST D3DDDIARG_SETINDICES* pData)
{
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    Assert(pDevice);
    Assert(pDevice->pDevice9If);
    PVBOXWDDMDISP_RESOURCE pRc = (PVBOXWDDMDISP_RESOURCE)pData->hIndexBuffer;
    PVBOXWDDMDISP_ALLOCATION pAlloc = NULL;
    IDirect3DIndexBuffer9 *pIndexBuffer = NULL;
    if (pRc)
    {
        Assert(pRc->cAllocations == 1);
        pAlloc = &pRc->aAllocations[0];
        Assert(pAlloc->pD3DIf);
        pIndexBuffer = (IDirect3DIndexBuffer9*)pAlloc->pD3DIf;
    }
    HRESULT hr = pDevice->pDevice9If->SetIndices(pIndexBuffer);
    Assert(hr == S_OK);
    if (hr == S_OK)
    {
        pDevice->pIndicesAlloc = pAlloc;
        pDevice->IndiciesInfo.uiStride = pData->Stride;
    }
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p), hr(0x%x)\n", hDevice, hr));
    return hr;
}

static HRESULT APIENTRY vboxWddmDDevSetIndicesUm(HANDLE hDevice, UINT IndexSize, CONST VOID* pUMBuffer)
{
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    Assert(pDevice);
    Assert(pDevice->pDevice9If);
    HRESULT hr = S_OK;
    pDevice->IndiciesUm.pvBuffer = pUMBuffer;
    pDevice->IndiciesUm.cbSize = IndexSize;
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p), hr(0x%x)\n", hDevice, hr));
    return hr;
}

static HRESULT APIENTRY vboxWddmDDevDrawPrimitive(HANDLE hDevice, CONST D3DDDIARG_DRAWPRIMITIVE* pData, CONST UINT* pFlagBuffer)
{
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    Assert(pDevice);
    Assert(pDevice->pDevice9If);
    Assert(!pFlagBuffer);
    HRESULT hr = S_OK;

    if (!pDevice->cStreamSources)
    {
        if (pDevice->aStreamSourceUm[0].pvBuffer)
        {
#ifdef DEBUG
            for (UINT i = 1; i < RT_ELEMENTS(pDevice->aStreamSourceUm); ++i)
            {
                Assert(!pDevice->aStreamSourceUm[i].pvBuffer);
            }
#endif
            hr = pDevice->pDevice9If->DrawPrimitiveUP(pData->PrimitiveType,
                                      pData->PrimitiveCount,
                                      ((uint8_t*)pDevice->aStreamSourceUm[0].pvBuffer) + pData->VStart * pDevice->aStreamSourceUm[0].cbStride,
                                      pDevice->aStreamSourceUm[0].cbStride);
            Assert(hr == S_OK);
        }
        else
        {
            /* todo: impl */
            AssertBreakpoint();
        }
    }
    else
    {
        hr = pDevice->pDevice9If->DrawPrimitive(pData->PrimitiveType,
                                                pData->VStart,
                                                pData->PrimitiveCount);
        Assert(hr == S_OK);
#if 0
        IDirect3DVertexDeclaration9* pDecl;
        hr = pDevice->pDevice9If->GetVertexDeclaration(&pDecl);
        Assert(hr == S_OK);
        if (hr == S_OK)
        {
            Assert(pDecl);
            D3DVERTEXELEMENT9 aDecls9[MAXD3DDECLLENGTH];
            UINT cDecls9 = 0;
            hr = pDecl->GetDeclaration(aDecls9, &cDecls9);
            Assert(hr == S_OK);
            if (hr == S_OK)
            {
                Assert(cDecls9);
                for (UINT i = 0; i < cDecls9 - 1 /* the last one is D3DDECL_END */; ++i)
                {
                    D3DVERTEXELEMENT9 *pDecl9 = &aDecls9[i];
                    Assert(pDecl9->Stream < RT_ELEMENTS(pDevice->aStreamSourceUm) || pDecl9->Stream == 0xff);
                    if (pDecl9->Stream != 0xff)
                    {
                        PVBOXWDDMDISP_STREAMSOURCEUM pStrSrc = &pDevice->aStreamSourceUm[pDecl9->Stream];
                        if (pStrSrc->pvBuffer)
                        {
                            WORD iStream = pDecl9->Stream;
                            D3DVERTEXELEMENT9 *pLastCDecl9 = pDecl9;
                            for (UINT j = i+1; j < cDecls9 - 1 /* the last one is D3DDECL_END */; ++j)
                            {
                                pDecl9 = &aDecls9[j];
                                if (iStream == pDecl9->Stream)
                                {
                                    pDecl9->Stream = 0xff; /* mark as done */
                                    Assert(pDecl9->Offset != pLastCDecl9->Offset);
                                    if (pDecl9->Offset > pLastCDecl9->Offset)
                                        pLastCDecl9 = pDecl9;
                                }
                            }
                            /* vertex size is MAX(all Offset's) + sizeof (data_type with MAX offset) + stride*/
                            UINT cbVertex = pLastCDecl9->Offset + pStrSrc->cbStride;
                            UINT cbType;
                            switch (pLastCDecl9->Type)
                            {
                                case D3DDECLTYPE_FLOAT1:
                                    cbType = sizeof (float);
                                    break;
                                case D3DDECLTYPE_FLOAT2:
                                    cbType = sizeof (float) * 2;
                                    break;
                                case D3DDECLTYPE_FLOAT3:
                                    cbType = sizeof (float) * 3;
                                    break;
                                case D3DDECLTYPE_FLOAT4:
                                    cbType = sizeof (float) * 4;
                                    break;
                                case D3DDECLTYPE_D3DCOLOR:
                                    cbType = 4;
                                    break;
                                case D3DDECLTYPE_UBYTE4:
                                    cbType = 4;
                                    break;
                                case D3DDECLTYPE_SHORT2:
                                    cbType = sizeof (short) * 2;
                                    break;
                                case D3DDECLTYPE_SHORT4:
                                    cbType = sizeof (short) * 4;
                                    break;
                                case D3DDECLTYPE_UBYTE4N:
                                    cbType = 4;
                                    break;
                                case D3DDECLTYPE_SHORT2N:
                                    cbType = sizeof (short) * 2;
                                    break;
                                case D3DDECLTYPE_SHORT4N:
                                    cbType = sizeof (short) * 4;
                                    break;
                                case D3DDECLTYPE_USHORT2N:
                                    cbType = sizeof (short) * 2;
                                    break;
                                case D3DDECLTYPE_USHORT4N:
                                    cbType = sizeof (short) * 4;
                                    break;
                                case D3DDECLTYPE_UDEC3:
                                    cbType = sizeof (signed) * 3;
                                    break;
                                case D3DDECLTYPE_DEC3N:
                                    cbType = sizeof (unsigned) * 3;
                                    break;
                                case D3DDECLTYPE_FLOAT16_2:
                                    cbType = 2 * 2;
                                    break;
                                case D3DDECLTYPE_FLOAT16_4:
                                    cbType = 2 * 4;
                                    break;
                                default:
                                    AssertBreakpoint();
                                    cbType = 1;
                            }
                            cbVertex += cbType;

                            UINT cVertexes;
                            switch (pData->PrimitiveType)
                            {
                                case D3DPT_POINTLIST:
                                    cVertexes = pData->PrimitiveCount;
                                    break;
                                case D3DPT_LINELIST:
                                    cVertexes = pData->PrimitiveCount * 2;
                                    break;
                                case D3DPT_LINESTRIP:
                                    cVertexes = pData->PrimitiveCount + 1;
                                    break;
                                case D3DPT_TRIANGLELIST:
                                    cVertexes = pData->PrimitiveCount * 3;
                                    break;
                                case D3DPT_TRIANGLESTRIP:
                                    cVertexes = pData->PrimitiveCount + 2;
                                    break;
                                case D3DPT_TRIANGLEFAN:
                                    cVertexes = pData->PrimitiveCount + 2;
                                    break;
                                default:
                                    AssertBreakpoint();
                                    cVertexes = pData->PrimitiveCount;
                            }
                            UINT cbVertexes = cVertexes * cbVertex;
                            IDirect3DVertexBuffer9 *pCurVb = NULL, *pVb = NULL;
                            UINT cbOffset;
                            UINT cbStride;
                            hr = pDevice->pDevice9If->GetStreamSource(iStream, &pCurVb, &cbOffset, &cbStride);
                            Assert(hr == S_OK);
                            if (hr == S_OK)
                            {
                                if (pCurVb)
                                {
                                    if (cbStride == pStrSrc->cbStride)
                                    {
                                        /* ensure our data feets in the buffer */
                                        D3DVERTEXBUFFER_DESC Desc;
                                        hr = pCurVb->GetDesc(&Desc);
                                        Assert(hr == S_OK);
                                        if (hr == S_OK)
                                        {
                                            if (Desc.Size >= cbVertexes)
                                                pVb = pCurVb;
                                        }
                                    }
                                }
                            }
                            else
                            {
                                pCurVb = NULL;
                            }

                            if (!pVb)
                            {
                                hr = pDevice->pDevice9If->CreateVertexBuffer(cbVertexes,
                                        0, /* DWORD Usage */
                                        0, /* DWORD FVF */
                                        D3DPOOL_DEFAULT, /* D3DPOOL Pool */
                                        &pVb,
                                        NULL /*HANDLE* pSharedHandle*/);
                                Assert(hr == S_OK);
                                if (hr == S_OK)
                                {
                                    hr = pDevice->pDevice9If->SetStreamSource(iStream, pVb, 0, pStrSrc->cbStride);
                                    Assert(hr == S_OK);
                                    if (hr == S_OK)
                                    {
                                        if (pCurVb)
                                            pCurVb->Release();
                                    }
                                    else
                                    {
                                        pVb->Release();
                                        pVb = NULL;
                                    }
                                }
                            }

                            if (pVb)
                            {
                                Assert(hr == S_OK);
                                VOID *pvData;
                                hr = pVb->Lock(0, /* UINT OffsetToLock */
                                        cbVertexes,
                                        &pvData,
                                        D3DLOCK_DISCARD);
                                Assert(hr == S_OK);
                                if (hr == S_OK)
                                {
                                    memcpy (pvData, ((uint8_t*)pStrSrc->pvBuffer) + pData->VStart * cbVertex, cbVertexes);
                                    HRESULT tmpHr = pVb->Unlock();
                                    Assert(tmpHr == S_OK);
                                }
                            }
                        }
                    }
                }
            }
            if (hr == S_OK)
            {
                hr = pDevice->pDevice9If->DrawPrimitive(pData->PrimitiveType,
                        0 /* <- since we use our owne StreamSource buffer which has data at the very beginning*/,
                        pData->PrimitiveCount);
                Assert(hr == S_OK);
            }
        }
#endif
    }
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p), hr(0x%x)\n", hDevice, hr));
    return hr;
}

static HRESULT APIENTRY vboxWddmDDevDrawIndexedPrimitive(HANDLE hDevice, CONST D3DDDIARG_DRAWINDEXEDPRIMITIVE* pData)
{
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    Assert(pDevice);
    Assert(pDevice->pDevice9If);
    HRESULT hr = pDevice->pDevice9If->DrawIndexedPrimitive(
            pData->PrimitiveType,
            pData->BaseVertexIndex,
            pData->MinIndex,
            pData->NumVertices,
            pData->StartIndex,
            pData->PrimitiveCount);
    Assert(hr == S_OK);

    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p), hr(0x%x)\n", hDevice, hr));
    return hr;
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
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    Assert(pDevice);
    Assert(pDevice->pDevice9If);
    HRESULT hr;

#if 0
    int stream;
    for (stream=0; stream<VBOXWDDMDISP_MAX_VERTEX_STREAMS; ++stream)
    {
        if (pDevice->aStreamSource[stream] && pDevice->aStreamSource[stream]->LockInfo.cLocks)
        {
            VBOXWDDMDISP_LOCKINFO *pLock = &pDevice->aStreamSource[stream]->LockInfo;
            if (pLock->fFlags.MightDrawFromLocked && (pLock->fFlags.Discard || pLock->fFlags.NoOverwrite))
            {
                IDirect3DVertexBuffer9 *pD3D9VBuf = (IDirect3DVertexBuffer9*)pDevice->aStreamSource[stream]->pD3DIf;
                Assert(pLock->fFlags.RangeValid);
                pD3D9VBuf->Lock(pLock->Range.Offset, pLock->Range.Size,
                                &pLock->LockedRect.pBits,
                                vboxDDI2D3DLockFlags(pLock->fFlags));
                RECT r;
                r.top = 0;
                r.left = pLock->Range.Offset;
                r.bottom = 1;
                r.right = pLock->Range.Offset + pLock->Range.Size;

                vboxWddmLockUnlockMemSynch(pDevice->aStreamSource[stream], &pLock->LockedRect, &r, true /*bool bToLockInfo*/);

                pD3D9VBuf->Unlock();
            }
        }
    }

    hr = pDevice->pDevice9If->DrawPrimitive(pData->PrimitiveType, pData->FirstVertexOffset, pData->PrimitiveCount);
#else
    int stream;
    for (stream=0; stream<VBOXWDDMDISP_MAX_VERTEX_STREAMS; ++stream)
    {
        if (pDevice->aStreamSource[stream])
        {
            Assert(stream==0); /*only stream 0 should be accessed here*/
            Assert(pDevice->StreamSourceInfo[stream].uiStride!=0);
            VBOXWDDMDISP_LOCKINFO *pLock = &pDevice->aStreamSource[stream]->LockInfo;

            if (pDevice->aStreamSource[stream]->LockInfo.cLocks)
            {
                Assert(pLock->fFlags.MightDrawFromLocked && (pLock->fFlags.Discard || pLock->fFlags.NoOverwrite));
                hr = pDevice->pDevice9If->DrawPrimitiveUP(pData->PrimitiveType, pData->PrimitiveCount,
                        (void*)((uintptr_t)pDevice->aStreamSource[stream]->pvMem+pDevice->StreamSourceInfo[stream].uiOffset+pData->FirstVertexOffset),
                         pDevice->StreamSourceInfo[stream].uiStride);
                Assert(hr == S_OK);
                hr = pDevice->pDevice9If->SetStreamSource(stream, (IDirect3DVertexBuffer9*)pDevice->aStreamSource[stream]->pD3DIf, pDevice->StreamSourceInfo[stream].uiOffset, pDevice->StreamSourceInfo[stream].uiStride);
            }
            else
            {
                hr = pDevice->pDevice9If->DrawPrimitive(pData->PrimitiveType, pData->FirstVertexOffset/pDevice->StreamSourceInfo[stream].uiStride, pData->PrimitiveCount);
            }
        }
    }
#endif

    Assert(hr == S_OK);
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p), hr(0x%x)\n", hDevice, hr));
    return hr;
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
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    Assert(pDevice);
    Assert(pDevice->pDevice9If);
    PVBOXWDDMDISP_RESOURCE pDstRc = (PVBOXWDDMDISP_RESOURCE)pData->hDstResource;
    PVBOXWDDMDISP_RESOURCE pSrcRc = (PVBOXWDDMDISP_RESOURCE)pData->hSrcResource;
    IDirect3DTexture9 *pD3DIfSrcTex = (IDirect3DTexture9*)pSrcRc->aAllocations[0].pD3DIf;
    IDirect3DTexture9 *pD3DIfDstTex = (IDirect3DTexture9*)pDstRc->aAllocations[0].pD3DIf;
    Assert(pD3DIfSrcTex);
    Assert(pD3DIfDstTex);
    HRESULT hr = S_OK;

    if (pSrcRc->aAllocations[0].SurfDesc.width == pDstRc->aAllocations[0].SurfDesc.width
            && pSrcRc->aAllocations[0].SurfDesc.height == pDstRc->aAllocations[0].SurfDesc.height
            && pSrcRc->RcDesc.enmFormat == pDstRc->RcDesc.enmFormat)
    {
        /* first check if we can do IDirect3DDevice9::UpdateTexture */
        if (pData->DstPoint.x == 0 && pData->DstPoint.y == 0
                && pData->SrcRect.left == 0 && pData->SrcRect.top == 0
                && pData->SrcRect.right - pData->SrcRect.left == pSrcRc->aAllocations[0].SurfDesc.width
                && pData->SrcRect.bottom - pData->SrcRect.top == pSrcRc->aAllocations[0].SurfDesc.height)
        {
            hr = pDevice->pDevice9If->UpdateTexture(pD3DIfSrcTex, pD3DIfDstTex);
            Assert(hr == S_OK);
        }
        else
        {
            AssertBreakpoint();
            /* @todo: impl */
        }
    }
    else
    {
        AssertBreakpoint();
        /* @todo: impl */
    }

    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p), hr(0x%x)\n", hDevice, hr));
    return hr;
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
AssertCompile(sizeof (RECT) == sizeof (D3DRECT));
AssertCompile(RT_SIZEOFMEMB(RECT, left) == RT_SIZEOFMEMB(D3DRECT, x1));
AssertCompile(RT_SIZEOFMEMB(RECT, right) == RT_SIZEOFMEMB(D3DRECT, x2));
AssertCompile(RT_SIZEOFMEMB(RECT, top) == RT_SIZEOFMEMB(D3DRECT, y1));
AssertCompile(RT_SIZEOFMEMB(RECT, bottom) == RT_SIZEOFMEMB(D3DRECT, y2));
AssertCompile(RT_OFFSETOF(RECT, left) == RT_OFFSETOF(D3DRECT, x1));
AssertCompile(RT_OFFSETOF(RECT, right) == RT_OFFSETOF(D3DRECT, x2));
AssertCompile(RT_OFFSETOF(RECT, top) == RT_OFFSETOF(D3DRECT, y1));
AssertCompile(RT_OFFSETOF(RECT, bottom) == RT_OFFSETOF(D3DRECT, y2));

static HRESULT APIENTRY vboxWddmDDevClear(HANDLE hDevice, CONST D3DDDIARG_CLEAR* pData, UINT NumRect, CONST RECT* pRect)
{
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    Assert(pDevice);
    Assert(pDevice->pDevice9If);
    HRESULT hr = pDevice->pDevice9If->Clear(NumRect, (D3DRECT*)pRect /* see AssertCompile above */,
            pData->Flags,
            pData->FillColor,
            pData->FillDepth,
            pData->FillStencil);
    Assert(hr == S_OK);
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p), hr(0x%x)\n", hDevice, hr));
    return hr;
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
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    Assert(pDevice);
    Assert(pDevice->pDevice9If);
    HRESULT hr = pDevice->pDevice9If->SetVertexShaderConstantF(
            pData->Register,
            (CONST float*)pRegisters,
            pData->Count);
    Assert(hr == S_OK);
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p), hr(0x%x)\n", hDevice, hr));
    return hr;
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
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    Assert(pDevice);
    Assert(pDevice->pDevice9If);
    pDevice->ViewPort.X = pData->X;
    pDevice->ViewPort.Y = pData->Y;
    pDevice->ViewPort.Width = pData->Width;
    pDevice->ViewPort.Height = pData->Height;
    HRESULT hr = pDevice->pDevice9If->SetViewport(&pDevice->ViewPort);
    Assert(hr == S_OK);
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p), hr(0x%x)\n", hDevice, hr));
    return hr;
}
static HRESULT APIENTRY vboxWddmDDevSetZRange(HANDLE hDevice, CONST D3DDDIARG_ZRANGE* pData)
{
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    Assert(pDevice);
    Assert(pDevice->pDevice9If);
    pDevice->ViewPort.MinZ = pData->MinZ;
    pDevice->ViewPort.MaxZ = pData->MaxZ;
    HRESULT hr = pDevice->pDevice9If->SetViewport(&pDevice->ViewPort);
    Assert(hr == S_OK);
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p), hr(0x%x)\n", hDevice, hr));
    return hr;
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
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    Assert(pDevice);
    Assert(pDevice->pDevice9If);
    HRESULT hr = pDevice->pDevice9If->SetClipPlane(pData->Index, pData->Plane);
    Assert(hr == S_OK);
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p), hr(0x%x)\n", hDevice, hr));
    return hr;
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
    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    PVBOXWDDMDISP_RESOURCE pRc = (PVBOXWDDMDISP_RESOURCE)pData->hResource;
    Assert(pData->SubResourceIndex < pRc->cAllocations);
    if (pData->SubResourceIndex >= pRc->cAllocations)
        return E_INVALIDARG;

    HRESULT hr = S_OK;

    if (VBOXDISPMODE_IS_3D(pDevice->pAdapter))
    {
        if (pRc->aAllocations[0].enmD3DIfType == VBOXDISP_D3DIFTYPE_TEXTURE)
        {
            PVBOXWDDMDISP_ALLOCATION pTexAlloc = &pRc->aAllocations[0];
            Assert(pData->SubResourceIndex < pRc->cAllocations);
            PVBOXWDDMDISP_ALLOCATION pLockAlloc = &pRc->aAllocations[pData->SubResourceIndex];
            IDirect3DTexture9 *pD3DIfTex = (IDirect3DTexture9*)pTexAlloc->pD3DIf;
            Assert(pD3DIfTex);
            RECT *pRect = NULL;
            bool bNeedResynch = false;
            Assert(!pData->Flags.RangeValid);
            Assert(!pData->Flags.BoxValid);
            if (pData->Flags.AreaValid)
            {
                pRect = &pData->Area;
            }

            /* else - we lock the entire texture, pRect == NULL */

//            Assert(!pLockAlloc->LockInfo.cLocks);
            if (!pLockAlloc->LockInfo.cLocks)
            {
                hr = pD3DIfTex->LockRect(pData->SubResourceIndex,
                        &pLockAlloc->LockInfo.LockedRect,
                        pRect,
                        vboxDDI2D3DLockFlags(pData->Flags));
                Assert(hr == S_OK);
                if (hr == S_OK)
                {

//                    Assert(pLockAlloc->LockInfo.fFlags.Value == 0);
                    pLockAlloc->LockInfo.fFlags = pData->Flags;
                    if (pRect)
                    {
                        pLockAlloc->LockInfo.Area = *pRect;
                        Assert(pLockAlloc->LockInfo.fFlags.AreaValid == 1);
//                        pLockAlloc->LockInfo.fFlags.AreaValid = 1;
                    }
                    else
                    {
                        Assert(pLockAlloc->LockInfo.fFlags.AreaValid == 0);
//                        pLockAlloc->LockInfo.fFlags.AreaValid = 0;
                    }

                    bNeedResynch = !pData->Flags.Discard;
                }
            }
            else
            {
//                Assert(pLockAlloc->LockInfo.fFlags.Value == pData->Flags.Value);
//                if (pLockAlloc->LockInfo.fFlags.Value != pData->Flags.Value)
//                {
//                }
                Assert(pLockAlloc->LockInfo.fFlags.AreaValid == pData->Flags.AreaValid);
                if (pLockAlloc->LockInfo.fFlags.AreaValid && pData->Flags.AreaValid)
                {
                    Assert(pLockAlloc->LockInfo.Area.left == pData->Area.left);
                    Assert(pLockAlloc->LockInfo.Area.top == pData->Area.top);
                    Assert(pLockAlloc->LockInfo.Area.right == pData->Area.right);
                    Assert(pLockAlloc->LockInfo.Area.bottom == pData->Area.bottom);
                }
                Assert(pLockAlloc->LockInfo.LockedRect.pBits);

                bNeedResynch = pLockAlloc->LockInfo.fFlags.Discard && !pData->Flags.Discard;

                Assert(!bNeedResynch);

                if (/*(pLockAlloc->LockInfo.fFlags.Discard && !pData->Flags.Discard)
                        || */
                        (pLockAlloc->LockInfo.fFlags.ReadOnly && !pData->Flags.ReadOnly))
                {
                    hr = pD3DIfTex->UnlockRect(pData->SubResourceIndex);
                    Assert(hr == S_OK);
                    if (hr == S_OK)
                    {
                        hr = pD3DIfTex->LockRect(pData->SubResourceIndex,
                                &pLockAlloc->LockInfo.LockedRect,
                                pRect,
                                vboxDDI2D3DLockFlags(pData->Flags));
                        Assert(hr == S_OK);
                        pLockAlloc->LockInfo.fFlags.ReadOnly = 0;
                    }
                }
            }

            if (hr == S_OK)
            {
                ++pLockAlloc->LockInfo.cLocks;

                if (!pData->Flags.NotifyOnly)
                {
                    pData->pSurfData = pLockAlloc->LockInfo.LockedRect.pBits;
                    pData->Pitch = pLockAlloc->LockInfo.LockedRect.Pitch;
                    pData->SlicePitch = 0;
                    Assert(pLockAlloc->SurfDesc.slicePitch == 0);
                    Assert(!pLockAlloc->pvMem);
                }
                else
                {
                    Assert(pLockAlloc->pvMem);
                    Assert(pRc->RcDesc.enmPool == D3DDDIPOOL_SYSTEMMEM);

                    if (bNeedResynch)
                        vboxWddmLockUnlockMemSynch(pLockAlloc, &pLockAlloc->LockInfo.LockedRect, pRect, false /*bool bToLockInfo*/);
                }
            }
        }
        else if (pRc->aAllocations[0].enmD3DIfType == VBOXDISP_D3DIFTYPE_VERTEXBUFFER)
        {
            Assert(pData->SubResourceIndex < pRc->cAllocations);
            PVBOXWDDMDISP_ALLOCATION pAlloc = &pRc->aAllocations[pData->SubResourceIndex];
            IDirect3DVertexBuffer9 *pD3D9VBuf = (IDirect3DVertexBuffer9*)pAlloc->pD3DIf;
            BOOL bLocked = false;
            Assert(pD3D9VBuf);
            Assert(!pData->Flags.AreaValid);
            Assert(!pData->Flags.BoxValid);
            D3DDDIRANGE *pRange = NULL;
            if (pData->Flags.RangeValid)
            {
                pRange = &pData->Range;
            }

            /* else - we lock the entire vertex buffer, pRect == NULL */

            Assert(!pAlloc->LockInfo.cLocks);
            if (!pAlloc->LockInfo.cLocks)
            {
                if (!pData->Flags.MightDrawFromLocked || (!pData->Flags.Discard && !pData->Flags.NoOverwrite))
                {
                    hr = pD3D9VBuf->Lock(pRange ? pRange->Offset : 0,
                                          pRange ? pRange->Size : 0,
                                          &pAlloc->LockInfo.LockedRect.pBits,
                                          vboxDDI2D3DLockFlags(pData->Flags));
                    bLocked = true;
                }

                Assert(hr == S_OK);
                if (hr == S_OK)
                {
                    pAlloc->LockInfo.LockedRect.Pitch = pAlloc->SurfDesc.width;
//                    Assert(pLockAlloc->LockInfo.fFlags.Value == 0);
                    pAlloc->LockInfo.fFlags = pData->Flags;
                    if (pRange)
                    {
                        pAlloc->LockInfo.Range = *pRange;
                        Assert(pAlloc->LockInfo.fFlags.RangeValid == 1);
//                        pAlloc->LockInfo.fFlags.RangeValid = 1;
                    }
                    else
                    {
                        Assert(pAlloc->LockInfo.fFlags.RangeValid == 0);
//                        pAlloc->LockInfo.fFlags.RangeValid = 0;
                    }
                }
            }
            else
            {
//                Assert(pAlloc->LockInfo.fFlags.Value == pData->Flags.Value);
//                if (pAlloc->LockInfo.fFlags.Value != pData->Flags.Value)
//                {
//                }
                Assert(pAlloc->LockInfo.fFlags.RangeValid == pData->Flags.RangeValid);
                if (pAlloc->LockInfo.fFlags.RangeValid && pData->Flags.RangeValid)
                {
                    Assert(pAlloc->LockInfo.Range.Offset == pData->Range.Offset);
                    Assert(pAlloc->LockInfo.Range.Size == pData->Range.Size);
                }
                Assert(pAlloc->LockInfo.LockedRect.pBits);
            }

            if (hr == S_OK)
            {
                ++pAlloc->LockInfo.cLocks;

                if (!pData->Flags.NotifyOnly)
                {
                    pData->pSurfData = pAlloc->LockInfo.LockedRect.pBits;
                    pData->Pitch = pAlloc->LockInfo.LockedRect.Pitch;
                    pData->SlicePitch = 0;
                    Assert(pAlloc->SurfDesc.slicePitch == 0);
                    Assert(!pAlloc->pvMem);
                }
                else
                {
                    Assert(pAlloc->pvMem);
                    Assert(pRc->RcDesc.enmPool == D3DDDIPOOL_SYSTEMMEM);
                    if (bLocked && !pData->Flags.Discard)
                    {
                        RECT r, *pr;
                        if (pRange)
                        {
                            r.top = 0;
                            r.left = pRange->Offset;
                            r.bottom = 1;
                            r.right = pRange->Offset + pRange->Size;
                            pr = &r;
                        }
                        else
                            pr = NULL;
                        vboxWddmLockUnlockMemSynch(pAlloc, &pAlloc->LockInfo.LockedRect, pr, false /*bool bToLockInfo*/);
                    }
                }
            }
        }
        else if (pRc->aAllocations[0].enmD3DIfType == VBOXDISP_D3DIFTYPE_INDEXBUFFER)
        {
            Assert(pData->SubResourceIndex < pRc->cAllocations);
            PVBOXWDDMDISP_ALLOCATION pAlloc = &pRc->aAllocations[pData->SubResourceIndex];
            IDirect3DIndexBuffer9 *pD3D9IBuf = (IDirect3DIndexBuffer9*)pAlloc->pD3DIf;
            BOOL bLocked = false;
            Assert(pD3D9IBuf);
            Assert(!pData->Flags.AreaValid);
            Assert(!pData->Flags.BoxValid);
            D3DDDIRANGE *pRange = NULL;
            if (pData->Flags.RangeValid)
            {
                pRange = &pData->Range;
            }

            /* else - we lock the entire vertex buffer, pRect == NULL */

            Assert(!pAlloc->LockInfo.cLocks);
            if (!pAlloc->LockInfo.cLocks)
            {
                if (!pData->Flags.MightDrawFromLocked || (!pData->Flags.Discard && !pData->Flags.NoOverwrite))
                {
                    hr = pD3D9IBuf->Lock(pRange ? pRange->Offset : 0,
                                          pRange ? pRange->Size : 0,
                                          &pAlloc->LockInfo.LockedRect.pBits,
                                          vboxDDI2D3DLockFlags(pData->Flags));
                    bLocked = true;
                }

                Assert(hr == S_OK);
                if (hr == S_OK)
                {
                    pAlloc->LockInfo.LockedRect.Pitch = pAlloc->SurfDesc.width;
//                    Assert(pLockAlloc->LockInfo.fFlags.Value == 0);
                    pAlloc->LockInfo.fFlags = pData->Flags;
                    if (pRange)
                    {
                        pAlloc->LockInfo.Range = *pRange;
                        Assert(pAlloc->LockInfo.fFlags.RangeValid == 1);
//                        pAlloc->LockInfo.fFlags.RangeValid = 1;
                    }
                    else
                    {
                        Assert(pAlloc->LockInfo.fFlags.RangeValid == 0);
//                        pAlloc->LockInfo.fFlags.RangeValid = 0;
                    }
                }
            }
            else
            {
//                Assert(pAlloc->LockInfo.fFlags.Value == pData->Flags.Value);
//                if (pAlloc->LockInfo.fFlags.Value != pData->Flags.Value)
//                {
//                }
                Assert(pAlloc->LockInfo.fFlags.RangeValid == pData->Flags.RangeValid);
                if (pAlloc->LockInfo.fFlags.RangeValid && pData->Flags.RangeValid)
                {
                    Assert(pAlloc->LockInfo.Range.Offset == pData->Range.Offset);
                    Assert(pAlloc->LockInfo.Range.Size == pData->Range.Size);
                }
                Assert(pAlloc->LockInfo.LockedRect.pBits);
            }

            if (hr == S_OK)
            {
                ++pAlloc->LockInfo.cLocks;

                if (!pData->Flags.NotifyOnly)
                {
                    pData->pSurfData = pAlloc->LockInfo.LockedRect.pBits;
                    pData->Pitch = pAlloc->LockInfo.LockedRect.Pitch;
                    pData->SlicePitch = 0;
                    Assert(pAlloc->SurfDesc.slicePitch == 0);
                    Assert(!pAlloc->pvMem);
                }
                else
                {
                    Assert(pAlloc->pvMem);
                    Assert(pRc->RcDesc.enmPool == D3DDDIPOOL_SYSTEMMEM);
                    if (bLocked && !pData->Flags.Discard)
                    {
                        RECT r, *pr;
                        if (pRange)
                        {
                            r.top = 0;
                            r.left = pRange->Offset;
                            r.bottom = 1;
                            r.right = pRange->Offset + pRange->Size;
                            pr = &r;
                        }
                        else
                            pr = NULL;
                        vboxWddmLockUnlockMemSynch(pAlloc, &pAlloc->LockInfo.LockedRect, pr, false /*bool bToLockInfo*/);
                    }
                }
            }
        }
        else
        {
            AssertBreakpoint();
        }
    }
    else
    {
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


        hr = pDevice->RtCallbacks.pfnLockCb(pDevice->hDevice, &LockData);
        Assert(hr == S_OK || (hr == D3DERR_WASSTILLDRAWING && pData->Flags.DoNotWait));
        if (hr == S_OK)
        {
            Assert(!pAlloc->LockInfo.cLocks);

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
                offset = 0;
            }

            if (!pData->Flags.ReadOnly)
            {
                if (pData->Flags.AreaValid)
                    vboxWddmDirtyRegionAddRect(&pAlloc->DirtyRegion, &pData->Area);
                else
                {
                    Assert(!pData->Flags.RangeValid);
                    Assert(!pData->Flags.BoxValid);
                    vboxWddmDirtyRegionAddRect(&pAlloc->DirtyRegion, NULL); /* <- NULL means the entire surface */
                }
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

            Assert(hr == S_OK);
            ++pAlloc->LockInfo.cLocks;
        }
    }

    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p), hr(%d)\n", hDevice, hr));
    return hr;
}
static HRESULT APIENTRY vboxWddmDDevUnlock(HANDLE hDevice, CONST D3DDDIARG_UNLOCK* pData)
{
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    PVBOXWDDMDISP_RESOURCE pRc = (PVBOXWDDMDISP_RESOURCE)pData->hResource;
    HRESULT hr = S_OK;

    Assert(pData->SubResourceIndex < pRc->cAllocations);
    if (pData->SubResourceIndex >= pRc->cAllocations)
        return E_INVALIDARG;

    if (VBOXDISPMODE_IS_3D(pDevice->pAdapter))
    {
        if (pRc->aAllocations[0].enmD3DIfType == VBOXDISP_D3DIFTYPE_TEXTURE)
        {
            Assert(pData->SubResourceIndex < pRc->cAllocations);
            PVBOXWDDMDISP_ALLOCATION pLockAlloc = &pRc->aAllocations[pData->SubResourceIndex];

            --pLockAlloc->LockInfo.cLocks;
            Assert(pLockAlloc->LockInfo.cLocks < UINT32_MAX);
//            pLockAlloc->LockInfo.cLocks = 0;
            if (!pLockAlloc->LockInfo.cLocks)
            {
                PVBOXWDDMDISP_ALLOCATION pTexAlloc = &pRc->aAllocations[0];
//                Assert(!pLockAlloc->LockInfo.cLocks);
                IDirect3DTexture9 *pD3DIfTex = (IDirect3DTexture9*)pTexAlloc->pD3DIf;
                Assert(pD3DIfTex);
                /* this is a sysmem texture, update  */
                if (pLockAlloc->pvMem && !pLockAlloc->LockInfo.fFlags.ReadOnly)
                {
                    vboxWddmLockUnlockMemSynch(pLockAlloc, &pLockAlloc->LockInfo.LockedRect,
                            pLockAlloc->LockInfo.fFlags.AreaValid ? &pLockAlloc->LockInfo.Area : NULL,
                            true /*bool bToLockInfo*/);
                }
                hr = pD3DIfTex->UnlockRect(pData->SubResourceIndex);
                Assert(hr == S_OK);
            }
            else
            {
                Assert(pLockAlloc->LockInfo.cLocks < UINT32_MAX);
            }
        }
        else if (pRc->aAllocations[0].enmD3DIfType == VBOXDISP_D3DIFTYPE_VERTEXBUFFER)
        {
            Assert(pData->SubResourceIndex < pRc->cAllocations);
            PVBOXWDDMDISP_ALLOCATION pAlloc = &pRc->aAllocations[pData->SubResourceIndex];

            --pAlloc->LockInfo.cLocks;
            Assert(pAlloc->LockInfo.cLocks < UINT32_MAX);
            if (!pAlloc->LockInfo.cLocks 
                && (!pAlloc->LockInfo.fFlags.MightDrawFromLocked
                    || (!pAlloc->LockInfo.fFlags.Discard && !pAlloc->LockInfo.fFlags.NoOverwrite)))
            {
//                Assert(!pAlloc->LockInfo.cLocks);
                IDirect3DVertexBuffer9 *pD3D9VBuf = (IDirect3DVertexBuffer9*)pAlloc->pD3DIf;
                Assert(pD3D9VBuf);
                /* this is a sysmem texture, update  */
                if (pAlloc->pvMem && !pAlloc->LockInfo.fFlags.ReadOnly)
                {
                    RECT r, *pr;
                    if (pAlloc->LockInfo.fFlags.RangeValid)
                    {
                        r.top = 0;
                        r.left = pAlloc->LockInfo.Range.Offset;
                        r.bottom = 1;
                        r.right = pAlloc->LockInfo.Range.Offset + pAlloc->LockInfo.Range.Size;
                        pr = &r;
                    }
                    else
                        pr = NULL;
                    vboxWddmLockUnlockMemSynch(pAlloc, &pAlloc->LockInfo.LockedRect,
                            pr,
                            true /*bool bToLockInfo*/);
                }
                hr = pD3D9VBuf->Unlock();
                Assert(hr == S_OK);
            }
            else
            {
                Assert(pAlloc->LockInfo.cLocks < UINT32_MAX);
            }
        }
        else if (pRc->aAllocations[0].enmD3DIfType == VBOXDISP_D3DIFTYPE_INDEXBUFFER)
        {
            Assert(pData->SubResourceIndex < pRc->cAllocations);
            PVBOXWDDMDISP_ALLOCATION pAlloc = &pRc->aAllocations[pData->SubResourceIndex];

            --pAlloc->LockInfo.cLocks;
            Assert(pAlloc->LockInfo.cLocks < UINT32_MAX);
            if (!pAlloc->LockInfo.cLocks
                && (!pAlloc->LockInfo.fFlags.MightDrawFromLocked
                    || (!pAlloc->LockInfo.fFlags.Discard && !pAlloc->LockInfo.fFlags.NoOverwrite)))
            {
//                Assert(!pAlloc->LockInfo.cLocks);
                IDirect3DIndexBuffer9 *pD3D9IBuf = (IDirect3DIndexBuffer9*)pAlloc->pD3DIf;
                Assert(pD3D9IBuf);
                /* this is a sysmem texture, update  */
                if (pAlloc->pvMem && !pAlloc->LockInfo.fFlags.ReadOnly)
                {
                    RECT r, *pr;
                    if (pAlloc->LockInfo.fFlags.RangeValid)
                    {
                        r.top = 0;
                        r.left = pAlloc->LockInfo.Range.Offset;
                        r.bottom = 1;
                        r.right = pAlloc->LockInfo.Range.Offset + pAlloc->LockInfo.Range.Size;
                        pr = &r;
                    }
                    else
                        pr = NULL;
                    vboxWddmLockUnlockMemSynch(pAlloc, &pAlloc->LockInfo.LockedRect,
                            pr,
                            true /*bool bToLockInfo*/);
                }
                hr = pD3D9IBuf->Unlock();
                Assert(hr == S_OK);
            }
            else
            {
                Assert(pAlloc->LockInfo.cLocks < UINT32_MAX);
            }
        }
        else
        {
            AssertBreakpoint();
        }
    }
    else
    {
        struct
        {
            D3DDDICB_UNLOCK Unlock;
            D3DKMT_HANDLE hAllocation;
        } UnlockData;

        PVBOXWDDMDISP_ALLOCATION pAlloc = &pRc->aAllocations[pData->SubResourceIndex];

        UnlockData.Unlock.NumAllocations = 1;
        UnlockData.Unlock.phAllocations = &UnlockData.hAllocation;
        UnlockData.hAllocation = pAlloc->hAllocation;

        hr = pDevice->RtCallbacks.pfnUnlockCb(pDevice->hDevice, &UnlockData.Unlock);
        Assert(hr == S_OK);
        if (hr == S_OK)
        {
            Assert(pAlloc->LockInfo.cLocks);
            --pAlloc->LockInfo.cLocks;
            Assert(pAlloc->LockInfo.cLocks < UINT32_MAX);
        }
    }

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

static void vboxWddmRequestAllocFree(D3DDDICB_ALLOCATE* pAlloc)
{
    RTMemFree(pAlloc);
}

static D3DDDICB_ALLOCATE* vboxWddmRequestAllocAlloc(D3DDDIARG_CREATERESOURCE* pResource)
{
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
    Assert(pvBuf);
    if (pvBuf)
    {
        D3DDDICB_ALLOCATE* pAlloc = (D3DDDICB_ALLOCATE*)pvBuf;
        pAlloc->NumAllocations = pResource->SurfCount;
        pAlloc->pAllocationInfo = (D3DDDI_ALLOCATIONINFO*)(pvBuf + offDdiAllocInfos);
        PVBOXWDDM_RCINFO pRcInfo = (PVBOXWDDM_RCINFO)(pvBuf + offRcInfo);
        pAlloc->PrivateDriverDataSize = cbRcInfo;
        pAlloc->pPrivateDriverData = pRcInfo;
        pAlloc->hResource = pResource->hResource;
        PVBOXWDDM_ALLOCINFO pAllocInfos = (PVBOXWDDM_ALLOCINFO)(pvBuf + offAllocInfos);
        for (UINT i = 0; i < pResource->SurfCount; ++i)
        {
            D3DDDI_ALLOCATIONINFO* pDdiAllocInfo = &pAlloc->pAllocationInfo[i];
            PVBOXWDDM_ALLOCINFO pAllocInfo = &pAllocInfos[i];
            pDdiAllocInfo->pPrivateDriverData = pAllocInfo;
            pDdiAllocInfo->PrivateDriverDataSize = sizeof (VBOXWDDM_ALLOCINFO);
        }
        return pAlloc;
    }
    return NULL;
}
static HRESULT APIENTRY vboxWddmDDevCreateResource(HANDLE hDevice, D3DDDIARG_CREATERESOURCE* pResource)
{
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    HRESULT hr = S_OK;
    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    Assert(pDevice);
    Assert(pResource);
    PVBOXWDDMDISP_ADAPTER pAdapter = pDevice->pAdapter;

    PVBOXWDDMDISP_RESOURCE pRc = vboxResourceAlloc(pResource->SurfCount);
    Assert(pRc);
    if (pRc)
    {
        bool bIssueCreateResource = false;

        pRc->hResource = pResource->hResource;
        pRc->hKMResource = NULL;
        pRc->pDevice = pDevice;
        pRc->fFlags = VBOXWDDM_RESOURCE_F_TYPE_GENERIC;
        pRc->RcDesc.fFlags = pResource->Flags;
        pRc->RcDesc.enmFormat = pResource->Format;
        pRc->RcDesc.enmPool = pResource->Pool;
        pRc->RcDesc.enmMultisampleType = pResource->MultisampleType;
        pRc->RcDesc.MultisampleQuality = pResource->MultisampleQuality;
        pRc->RcDesc.MipLevels = pResource->MipLevels;
        pRc->RcDesc.Fvf = pResource->Fvf;
        pRc->RcDesc.VidPnSourceId = pResource->VidPnSourceId;
        pRc->RcDesc.RefreshRate = pResource->RefreshRate;
        pRc->RcDesc.enmRotation = pResource->Rotation;
        pRc->cAllocations = pResource->SurfCount;
        for (UINT i = 0; i < pResource->SurfCount; ++i)
        {
            PVBOXWDDMDISP_ALLOCATION pAllocation = &pRc->aAllocations[i];
            CONST D3DDDI_SURFACEINFO* pSurf = &pResource->pSurfList[i];
            pAllocation->hAllocation = NULL;
            pAllocation->enmType = VBOXWDDM_ALLOC_TYPE_UMD_RC_GENERIC;
            pAllocation->pvMem = (void*)pSurf->pSysMem;
            pAllocation->SurfDesc.pitch = pSurf->SysMemPitch;
            pAllocation->SurfDesc.slicePitch = pSurf->SysMemSlicePitch;
            pAllocation->SurfDesc.depth = pSurf->Depth;
            pAllocation->SurfDesc.width = pSurf->Width;
            pAllocation->SurfDesc.height = pSurf->Height;
            pAllocation->SurfDesc.format = pResource->Format;
        }

        if (VBOXDISPMODE_IS_3D(pAdapter))
        {
            if (pResource->Flags.SharedResource)
            {
                AssertBreakpoint(); /* <-- need to test that */
                bIssueCreateResource = true;
            }

            if (pResource->Flags.ZBuffer)
            {
                Assert(pDevice->pDevice9If);
                for (UINT i = 0; i < pResource->SurfCount; ++i)
                {
                    PVBOXWDDMDISP_ALLOCATION pAllocation = &pRc->aAllocations[i];
                    IDirect3DSurface9 *pD3D9Surf;
                    hr = pDevice->pDevice9If->CreateDepthStencilSurface(pAllocation->SurfDesc.width,
                            pAllocation->SurfDesc.height,
                            vboxDDI2D3DFormat(pResource->Format),
                            vboxDDI2D3DMultiSampleType(pResource->MultisampleType),
                            pResource->MultisampleQuality,
                            TRUE /* @todo: BOOL Discard */,
                            &pD3D9Surf,
                            NULL /*HANDLE* pSharedHandle*/);
                    Assert(hr == S_OK);
                    if (hr == S_OK)
                    {
                        Assert(pD3D9Surf);
                        pAllocation->enmD3DIfType = VBOXDISP_D3DIFTYPE_SURFACE;
                        pAllocation->pD3DIf = pD3D9Surf;
                        if (pResource->Pool == D3DDDIPOOL_SYSTEMMEM)
                        {
                            Assert(pAllocation->pvMem);
                            D3DLOCKED_RECT lockInfo;
                            hr = pD3D9Surf->LockRect(&lockInfo, NULL, D3DLOCK_DISCARD);
                            Assert(hr == S_OK);
                            if (hr == S_OK)
                            {
                                vboxWddmLockUnlockMemSynch(pAllocation, &lockInfo, NULL, true /*bool bToLockInfo*/);
                                HRESULT tmpHr = pD3D9Surf->UnlockRect();
                                Assert(tmpHr == S_OK);
                            }
                        }
                        else
                        {
                            Assert(!pAllocation->pvMem);
                        }
                    }
                    else
                    {
                        for (UINT j = 0; j < i; ++j)
                        {
                            pRc->aAllocations[j].pD3DIf->Release();
                        }
                        break;
                    }
                }
            }
            else if (pResource->Flags.VertexBuffer)
            {
                Assert(pDevice->pDevice9If);
                for (UINT i = 0; i < pResource->SurfCount; ++i)
                {
                    PVBOXWDDMDISP_ALLOCATION pAllocation = &pRc->aAllocations[i];
                    IDirect3DVertexBuffer9  *pD3D9VBuf;
                    hr = pDevice->pDevice9If->CreateVertexBuffer(pAllocation->SurfDesc.width,
                            vboxDDI2D3DUsage(pResource->Flags),
                            pResource->Fvf,
                            vboxDDI2D3DPool(pResource->Pool),
                            &pD3D9VBuf,
                            NULL /*HANDLE* pSharedHandle*/);
                    Assert(hr == S_OK);
                    if (hr == S_OK)
                    {
                        Assert(pD3D9VBuf);
                        pAllocation->enmD3DIfType = VBOXDISP_D3DIFTYPE_VERTEXBUFFER;
                        pAllocation->pD3DIf = pD3D9VBuf;
                        if (pResource->Pool == D3DDDIPOOL_SYSTEMMEM)
                        {
                            Assert(pAllocation->pvMem);
                            D3DLOCKED_RECT lockInfo;
                            hr = pD3D9VBuf->Lock(0, pAllocation->SurfDesc.width, &lockInfo.pBits, D3DLOCK_DISCARD);
                            Assert(hr == S_OK);
                            if (hr == S_OK)
                            {
                                lockInfo.Pitch = pAllocation->SurfDesc.pitch;
                                vboxWddmLockUnlockMemSynch(pAllocation, &lockInfo, NULL, true /*bool bToLockInfo*/);
                                HRESULT tmpHr = pD3D9VBuf->Unlock();
                                Assert(tmpHr == S_OK);
                            }
                        }
                        else
                        {
                            Assert(!pAllocation->pvMem);
                        }
                    }
                    else
                    {
                        for (UINT j = 0; j < i; ++j)
                        {
                            pRc->aAllocations[j].pD3DIf->Release();
                        }
                        break;
                    }
                }
            }
            else if (pResource->Flags.IndexBuffer)
            {
                Assert(pDevice->pDevice9If);
                for (UINT i = 0; i < pResource->SurfCount; ++i)
                {
                    PVBOXWDDMDISP_ALLOCATION pAllocation = &pRc->aAllocations[i];
                    CONST D3DDDI_SURFACEINFO* pSurf = &pResource->pSurfList[i];
                    IDirect3DIndexBuffer9  *pD3D9IBuf;
                    hr = pDevice->pDevice9If->CreateIndexBuffer(pSurf->Width,
                            vboxDDI2D3DUsage(pResource->Flags),
                            vboxDDI2D3DFormat(pResource->Format),
                            vboxDDI2D3DPool(pResource->Pool),
                            &pD3D9IBuf,
                            NULL /*HANDLE* pSharedHandle*/
                          );
                    Assert(hr == S_OK);
                    if (hr == S_OK)
                    {
                        Assert(pD3D9IBuf);
                        pAllocation->enmD3DIfType = VBOXDISP_D3DIFTYPE_INDEXBUFFER;
                        pAllocation->pD3DIf = pD3D9IBuf;
                        if (pResource->Pool == D3DDDIPOOL_SYSTEMMEM)
                        {
                            Assert(pAllocation->pvMem);
                            D3DLOCKED_RECT lockInfo;
                            hr = pD3D9IBuf->Lock(0, pAllocation->SurfDesc.width, &lockInfo.pBits, D3DLOCK_DISCARD);
                            Assert(hr == S_OK);
                            if (hr == S_OK)
                            {
                                lockInfo.Pitch = pAllocation->SurfDesc.pitch;
                                vboxWddmLockUnlockMemSynch(pAllocation, &lockInfo, NULL, true /*bool bToLockInfo*/);
                                HRESULT tmpHr = pD3D9IBuf->Unlock();
                                Assert(tmpHr == S_OK);
                            }
                        }
                        else
                        {
                            Assert(!pAllocation->pvMem);
                        }
                    }
                    else
                    {
                        for (UINT j = 0; j < i; ++j)
                        {
                            pRc->aAllocations[j].pD3DIf->Release();
                        }
                        break;
                    }
                }
            }
            else if (pResource->Flags.Texture || pResource->Flags.Value == 0)
            {
                Assert(pDevice->pDevice9If);
#ifdef DEBUG
                {
                    uint32_t tstW = pResource->pSurfList[0].Width;
                    uint32_t tstH = pResource->pSurfList[0].Height;
                    for (UINT i = 1; i < pResource->SurfCount; ++i)
                    {
                        tstW /= 2;
                        tstH /= 2;
                        CONST D3DDDI_SURFACEINFO* pSurf = &pResource->pSurfList[i];
                        Assert(pSurf->Width == tstW);
                        Assert(pSurf->Height == tstH);
                    }
                }
#endif

                if (pResource->Flags.RenderTarget)
                    bIssueCreateResource = true;

                PVBOXWDDMDISP_ALLOCATION pAllocation = &pRc->aAllocations[0];
                CONST D3DDDI_SURFACEINFO* pSurf = &pResource->pSurfList[0];
                IDirect3DTexture9 *pD3DIfTex;
                hr = pDevice->pDevice9If->CreateTexture(pSurf->Width,
                                            pSurf->Height,
                                            pResource->SurfCount,
                                            vboxDDI2D3DUsage(pResource->Flags),
                                            vboxDDI2D3DFormat(pResource->Format),
                                            vboxDDI2D3DPool(pResource->Pool),
                                            &pD3DIfTex,
                                            NULL /* HANDLE* pSharedHandle */
                                            );
                Assert(hr == S_OK);
                if (hr == S_OK)
                {
                    Assert(pD3DIfTex);
                    pAllocation->enmD3DIfType = VBOXDISP_D3DIFTYPE_TEXTURE;
                    pAllocation->pD3DIf = pD3DIfTex;
                    if (pResource->Pool == D3DDDIPOOL_SYSTEMMEM)
                    {
                        for (UINT i = 0; i < pResource->SurfCount; ++i)
                        {
                            D3DLOCKED_RECT lockInfo;
                            PVBOXWDDMDISP_ALLOCATION pAllocation = &pRc->aAllocations[i];
                            Assert(pAllocation->pvMem);
                            hr = pD3DIfTex->LockRect(i, &lockInfo, NULL, D3DLOCK_DISCARD);
                            Assert(hr == S_OK);
                            if (hr == S_OK)
                            {
                                vboxWddmLockUnlockMemSynch(pAllocation, &lockInfo, NULL, true /*bool bToLockInfo*/);
                                HRESULT tmpHr = pD3DIfTex->UnlockRect(i);
                                Assert(tmpHr == S_OK);
                            }
                            else
                            {
                                pD3DIfTex->Release();
                                break;
                            }
                        }
                    }
                }
#ifdef DEBUG
                else
                {
                    for (UINT i = 0; i < pResource->SurfCount; ++i)
                    {
                        PVBOXWDDMDISP_ALLOCATION pAllocation = &pRc->aAllocations[i];
                        Assert(!pAllocation->pvMem);
                    }
                }
#endif
            }
            else if (pResource->Flags.RenderTarget)
            {
                HWND hWnd = NULL;
                bool bDevCreated = false;
                Assert(pResource->SurfCount);
                if (!pDevice->pDevice9If)
                {
                    Assert(!pDevice->hWnd);
                    hr = VBoxDispWndCreate(pAdapter, pResource->pSurfList[0].Width, pResource->pSurfList[0].Height, &hWnd);
                    Assert(hr == S_OK);
                    if (hr == S_OK)
                    {
                        DWORD fFlags = D3DCREATE_HARDWARE_VERTEXPROCESSING;
                        if (pDevice->fFlags.AllowMultithreading)
                            fFlags |= D3DCREATE_MULTITHREADED;

                        IDirect3DDevice9 *pDevice9If = NULL;
                        D3DPRESENT_PARAMETERS params;
                        memset(&params, 0, sizeof (params));
                        //            params.BackBufferWidth = 0;
                        //            params.BackBufferHeight = 0;
                        params.BackBufferFormat = vboxDDI2D3DFormat(pResource->Format);
                        Assert(pResource->SurfCount);
                        params.BackBufferCount = pResource->SurfCount - 1;
                        params.MultiSampleType = vboxDDI2D3DMultiSampleType(pResource->MultisampleType);
                        if (pResource->Flags.DiscardRenderTarget)
                            params.SwapEffect = D3DSWAPEFFECT_DISCARD;
                        params.hDeviceWindow = hWnd;
                                    /* @todo: it seems there should be a way to detect this correctly since
                                     * our vboxWddmDDevSetDisplayMode will be called in case we are using full-screen */
                        params.Windowed = TRUE;
                        //            params.EnableAutoDepthStencil = FALSE;
                        //            params.AutoDepthStencilFormat = D3DFMT_UNKNOWN;
                        //            params.Flags;
                        //            params.FullScreen_RefreshRateInHz;
                        //            params.FullScreen_PresentationInterval;
                        hr = pAdapter->pD3D9If->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, fFlags, &params, &pDevice9If);
                        Assert(hr == S_OK);
                        if (hr == S_OK)
                        {
                            pDevice->pDevice9If = pDevice9If;
                            pDevice->hWnd = hWnd;
                            bDevCreated = true;
                        }
                    }
                }
                else
                {
                    Assert(pDevice->hWnd);
                }

                if (hr == S_OK)
                {
                    Assert(pDevice->pDevice9If);
                    bIssueCreateResource = true;
                    for (UINT i = 0; i < pResource->SurfCount; ++i)
                    {
                        PVBOXWDDMDISP_ALLOCATION pAllocation = &pRc->aAllocations[i];

                        IDirect3DSurface9* pD3D9Surf;
                        hr = pDevice->pDevice9If->CreateRenderTarget(pAllocation->SurfDesc.width,
                                pAllocation->SurfDesc.height,
                                vboxDDI2D3DFormat(pResource->Format),
                                vboxDDI2D3DMultiSampleType(pResource->MultisampleType),
                                pResource->MultisampleQuality,
                                !pResource->Flags.NotLockable /* BOOL Lockable */,
                                &pD3D9Surf,
                                NULL /* HANDLE* pSharedHandle */
                                );
                        Assert(hr == S_OK);
                        if (hr == S_OK)
                        {
                            Assert(pD3D9Surf);
                            pAllocation->enmD3DIfType = VBOXDISP_D3DIFTYPE_SURFACE;
                            pAllocation->pD3DIf = pD3D9Surf;
                            if (pResource->Pool == D3DDDIPOOL_SYSTEMMEM)
                            {
                                Assert(pAllocation->pvMem);
                                D3DLOCKED_RECT lockInfo;
                                hr = pD3D9Surf->LockRect(&lockInfo, NULL, D3DLOCK_DISCARD);
                                Assert(hr == S_OK);
                                if (hr == S_OK)
                                {
                                    vboxWddmLockUnlockMemSynch(pAllocation, &lockInfo, NULL, true /*bool bToLockInfo*/);
                                    HRESULT tmpHr = pD3D9Surf->UnlockRect();
                                    Assert(tmpHr == S_OK);
                                }
                            }
                            else
                            {
                                Assert(!pAllocation->pvMem);
                            }
                        }
                        else
                        {
                            for (UINT j = 0; j < i; ++j)
                            {
                                pRc->aAllocations[j].pD3DIf->Release();
                            }
                            break;
                        }
                    }
                }

                if (hr != S_OK)
                {
                    if (bDevCreated)
                    {
                        VBoxDispWndDestroy(pAdapter, hWnd);
                        pDevice->pDevice9If->Release();
                    }
                }
            }
            else
            {
                Assert(pDevice->pDevice9If);
                Assert(0);
            }
        }
        else
            bIssueCreateResource = true;


        if (hr == S_OK && bIssueCreateResource)
        {
            D3DDDICB_ALLOCATE *pDdiAllocate = vboxWddmRequestAllocAlloc(pResource);
            Assert(pDdiAllocate);
            if (pDdiAllocate)
            {
                Assert(pDdiAllocate->pPrivateDriverData);
                Assert(pDdiAllocate->PrivateDriverDataSize == sizeof (VBOXWDDM_RCINFO));
                PVBOXWDDM_RCINFO pRcInfo = (PVBOXWDDM_RCINFO)pDdiAllocate->pPrivateDriverData;
                pRcInfo->fFlags = VBOXWDDM_RESOURCE_F_TYPE_GENERIC;
                pRcInfo->RcDesc = pRc->RcDesc;
                pRcInfo->cAllocInfos = pResource->SurfCount;

                for (UINT i = 0; i < pResource->SurfCount; ++i)
                {
                    D3DDDI_ALLOCATIONINFO *pDdiAllocI = &pDdiAllocate->pAllocationInfo[i];
                    Assert(pDdiAllocI->pPrivateDriverData);
                    Assert(pDdiAllocI->PrivateDriverDataSize == sizeof (VBOXWDDM_ALLOCINFO));
                    PVBOXWDDM_ALLOCINFO pAllocInfo = (PVBOXWDDM_ALLOCINFO)pDdiAllocI->pPrivateDriverData;
                    CONST D3DDDI_SURFACEINFO* pSurf = &pResource->pSurfList[i];
                    pDdiAllocI->hAllocation = NULL;
                    pDdiAllocI->pSystemMem = pSurf->pSysMem;
                    Assert((!!(pSurf->pSysMem)) == (pResource->Pool == D3DDDIPOOL_SYSTEMMEM));
                    pDdiAllocI->VidPnSourceId = pResource->VidPnSourceId;
                    pDdiAllocI->Flags.Value = 0;
                    if (pResource->Flags.Primary)
                    {
                        Assert(pResource->Flags.RenderTarget);
                        pDdiAllocI->Flags.Primary = 1;
                    }

                    pAllocInfo->enmType = VBOXWDDM_ALLOC_TYPE_UMD_RC_GENERIC;
                    pAllocInfo->fFlags = pResource->Flags;
                    pAllocInfo->SurfDesc.width = pSurf->Width;
                    pAllocInfo->SurfDesc.height = pSurf->Height;
                    pAllocInfo->SurfDesc.format = pResource->Format;
                    if (!vboxWddmFormatToFourcc(pResource->Format))
                        pAllocInfo->SurfDesc.bpp = vboxWddmCalcBitsPerPixel(pResource->Format);
                    else
                        pAllocInfo->SurfDesc.bpp = 0;

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

                hr = pDevice->RtCallbacks.pfnAllocateCb(pDevice->hDevice, pDdiAllocate);
                Assert(hr == S_OK);
                Assert(pDdiAllocate->hKMResource);
                if (hr == S_OK)
                {
                    pRc->hKMResource = pDdiAllocate->hKMResource;

                    for (UINT i = 0; i < pResource->SurfCount; ++i)
                    {
                        PVBOXWDDMDISP_ALLOCATION pAllocation = &pRc->aAllocations[i];
                        D3DDDI_ALLOCATIONINFO *pDdiAllocI = &pDdiAllocate->pAllocationInfo[i];
                        PVBOXWDDM_ALLOCINFO pAllocInfo = (PVBOXWDDM_ALLOCINFO)pDdiAllocI->pPrivateDriverData;
                        CONST D3DDDI_SURFACEINFO* pSurf = &pResource->pSurfList[i];
                        pAllocation->hAllocation = pDdiAllocI->hAllocation;
                        pAllocation->enmType = VBOXWDDM_ALLOC_TYPE_UMD_RC_GENERIC;
                        pAllocation->pvMem = (void*)pSurf->pSysMem;
                        pAllocation->SurfDesc = pAllocInfo->SurfDesc;
                    }
                }

                vboxWddmRequestAllocFree(pDdiAllocate);
            }
            else
            {
                hr = E_OUTOFMEMORY;
            }

            if (hr == S_OK)
                pResource->hResource = pRc;
            else
                vboxResourceFree(pRc);
        }

        if (hr == S_OK)
            pResource->hResource = pRc;
        else
            vboxResourceFree(pRc);
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
    PVBOXWDDMDISP_ADAPTER pAdapter = pDevice->pAdapter;
    PVBOXWDDMDISP_RESOURCE pRc = (PVBOXWDDMDISP_RESOURCE)hResource;

    HRESULT hr = S_OK;

    Assert(pDevice);
    Assert(hResource);

    if (VBOXDISPMODE_IS_3D(pAdapter))
    {
        if (pRc->RcDesc.fFlags.RenderTarget)
        {
            Assert(pDevice->hWnd);
            Assert(pDevice->pDevice9If);
        }

        for (UINT i = 0; i < pRc->cAllocations; ++i)
        {
            PVBOXWDDMDISP_ALLOCATION pAlloc = &pRc->aAllocations[i];
            if (pAlloc->pD3DIf)
                pAlloc->pD3DIf->Release();
        }
    }

    Assert(pRc->hResource);
    Assert(pRc->hKMResource || VBOXDISPMODE_IS_3D(pAdapter));
    if (pRc->hKMResource)
    {
        if (!(pRc->fFlags & VBOXWDDM_RESOURCE_F_OPENNED))
        {
            D3DDDICB_DEALLOCATE Dealloc;
            Dealloc.hResource = pRc->hResource;
            /* according to the docs the below two are ignored in case we set the hResource */
            Dealloc.NumAllocations = 0;
            Dealloc.HandleList = NULL;
            hr = pDevice->RtCallbacks.pfnDeallocateCb(pDevice->hDevice, &Dealloc);
            Assert(hr == S_OK);
//            for (UINT j = 0; j < pRc->cAllocations; ++j)
//            {
//                D3DDDICB_DEALLOCATE Dealloc;
//                Dealloc.hResource = NULL;
//                Dealloc.NumAllocations = 1;
//                Dealloc.HandleList = &pRc->aAllocations[j].hAllocation;
//                HRESULT tmpHr = pDevice->RtCallbacks.pfnDeallocateCb(pDevice->hDevice, &Dealloc);
//                Assert(tmpHr = S_OK);
//            }
        }
    }

    vboxResourceFree(pRc);
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return hr;
}
static HRESULT APIENTRY vboxWddmDDevSetDisplayMode(HANDLE hDevice, CONST D3DDDIARG_SETDISPLAYMODE* pData)
{
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    HRESULT hr = S_OK;
    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    Assert(pDevice);
    Assert(VBOXDISPMODE_IS_3D(pDevice->pAdapter));
    PVBOXWDDMDISP_RESOURCE pRc = (PVBOXWDDMDISP_RESOURCE)pData->hResource;
    Assert(pRc);
    Assert(pRc->cAllocations > pData->SubResourceIndex);
    PVBOXWDDMDISP_ALLOCATION pAlloc = &pRc->aAllocations[pData->SubResourceIndex];
    Assert(pRc->RcDesc.fFlags.RenderTarget);
    Assert(pAlloc->enmD3DIfType == VBOXDISP_D3DIFTYPE_SURFACE);
    Assert(pAlloc->hAllocation);
    D3DDDICB_SETDISPLAYMODE DdiDm = {0};
    DdiDm.hPrimaryAllocation = pAlloc->hAllocation;
//    DdiDm.PrivateDriverFormatAttribute = 0;
#if 0
    IDirect3DSurface9 *pD3DIfSurf = (IDirect3DSurface9*)pAlloc->pD3DIf;
    hr = pDevice->pDevice9If->SetRenderTarget(0, pD3DIfSurf);
    Assert(hr == S_OK);
    if (hr == S_OK)
#endif
    {
        hr = pDevice->RtCallbacks.pfnSetDisplayModeCb(pDevice->hDevice, &DdiDm);
        Assert(hr == S_OK);
    }

    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return hr;
}
static HRESULT APIENTRY vboxWddmDDevPresent(HANDLE hDevice, CONST D3DDDIARG_PRESENT* pData)
{
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    Assert(pDevice);
    HRESULT hr = S_OK;
    if (VBOXDISPMODE_IS_3D(pDevice->pAdapter))
    {
        Assert(pDevice->pDevice9If);
#if 0
        hr = pDevice->pDevice9If->Present(NULL, /* CONST RECT * pSourceRect */
                NULL, /* CONST RECT * pDestRect */
                NULL, /* HWND hDestWindowOverride */
                NULL /*CONST RGNDATA * pDirtyRegion */
                );
        Assert(hr == S_OK);
#endif
    }
#if 0
    else
#endif
    {
        D3DDDICB_PRESENT DdiPresent = {0};
        if (pData->hSrcResource)
        {
            PVBOXWDDMDISP_RESOURCE pRc = (PVBOXWDDMDISP_RESOURCE)pData->hSrcResource;
            Assert(pRc->hKMResource);
            Assert(pRc->cAllocations > pData->SrcSubResourceIndex);
            PVBOXWDDMDISP_ALLOCATION pAlloc = &pRc->aAllocations[pData->SrcSubResourceIndex];
            Assert(pAlloc->hAllocation);
            DdiPresent.hSrcAllocation = pAlloc->hAllocation;
        }
        if (pData->hDstResource)
        {
            PVBOXWDDMDISP_RESOURCE pRc = (PVBOXWDDMDISP_RESOURCE)pData->hDstResource;
            Assert(pRc->hKMResource);
            Assert(pRc->cAllocations > pData->DstSubResourceIndex);
            PVBOXWDDMDISP_ALLOCATION pAlloc = &pRc->aAllocations[pData->DstSubResourceIndex];
            Assert(pAlloc->hAllocation);
            DdiPresent.hDstAllocation = pAlloc->hAllocation;
        }
        DdiPresent.hContext = pDevice->DefaultContext.ContextInfo.hContext;
//        DdiPresent.BroadcastContextCount;
//        DdiPresent.BroadcastContext[D3DDDI_MAX_BROADCAST_CONTEXT];

        hr = pDevice->RtCallbacks.pfnPresentCb(pDevice->hDevice, &DdiPresent);
        Assert(hr == S_OK);
    }
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p), hr(0x%x)\n", hDevice, hr));
    return hr;
}
static HRESULT APIENTRY vboxWddmDDevFlush(HANDLE hDevice)
{
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    Assert(pDevice);
    HRESULT hr = S_OK;
    if (VBOXDISPMODE_IS_3D(pDevice->pAdapter))
    {
        Assert(pDevice->pDevice9If);
        hr = pDevice->pDevice9If->Present(NULL, /* CONST RECT * pSourceRect */
                NULL, /* CONST RECT * pDestRect */
                NULL, /* HWND hDestWindowOverride */
                NULL /*CONST RGNDATA * pDirtyRegion */
                );
        Assert(hr == S_OK);
    }
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p), hr(0x%x)\n", hDevice, hr));
    return hr;
}

AssertCompile(sizeof (D3DDDIVERTEXELEMENT) == sizeof (D3DVERTEXELEMENT9));
AssertCompile(RT_SIZEOFMEMB(D3DDDIVERTEXELEMENT, Stream) == RT_SIZEOFMEMB(D3DVERTEXELEMENT9, Stream));
AssertCompile(RT_SIZEOFMEMB(D3DDDIVERTEXELEMENT, Offset) == RT_SIZEOFMEMB(D3DVERTEXELEMENT9, Offset));
AssertCompile(RT_SIZEOFMEMB(D3DDDIVERTEXELEMENT, Type) == RT_SIZEOFMEMB(D3DVERTEXELEMENT9, Type));
AssertCompile(RT_SIZEOFMEMB(D3DDDIVERTEXELEMENT, Method) == RT_SIZEOFMEMB(D3DVERTEXELEMENT9, Method));
AssertCompile(RT_SIZEOFMEMB(D3DDDIVERTEXELEMENT, Usage) == RT_SIZEOFMEMB(D3DVERTEXELEMENT9, Usage));
AssertCompile(RT_SIZEOFMEMB(D3DDDIVERTEXELEMENT, UsageIndex) == RT_SIZEOFMEMB(D3DVERTEXELEMENT9, UsageIndex));

AssertCompile(RT_OFFSETOF(D3DDDIVERTEXELEMENT, Stream) == RT_OFFSETOF(D3DVERTEXELEMENT9, Stream));
AssertCompile(RT_OFFSETOF(D3DDDIVERTEXELEMENT, Offset) == RT_OFFSETOF(D3DVERTEXELEMENT9, Offset));
AssertCompile(RT_OFFSETOF(D3DDDIVERTEXELEMENT, Type) == RT_OFFSETOF(D3DVERTEXELEMENT9, Type));
AssertCompile(RT_OFFSETOF(D3DDDIVERTEXELEMENT, Method) == RT_OFFSETOF(D3DVERTEXELEMENT9, Method));
AssertCompile(RT_OFFSETOF(D3DDDIVERTEXELEMENT, Usage) == RT_OFFSETOF(D3DVERTEXELEMENT9, Usage));
AssertCompile(RT_OFFSETOF(D3DDDIVERTEXELEMENT, UsageIndex) == RT_OFFSETOF(D3DVERTEXELEMENT9, UsageIndex));

static HRESULT APIENTRY vboxWddmDDevCreateVertexShaderDecl(HANDLE hDevice, D3DDDIARG_CREATEVERTEXSHADERDECL* pData, CONST D3DDDIVERTEXELEMENT* pVertexElements)
{
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    Assert(pDevice);
    Assert(pDevice->pDevice9If);
    IDirect3DVertexDeclaration9 *pDecl;
    static D3DVERTEXELEMENT9 DeclEnd = D3DDECL_END();
    Assert(!memcmp(&DeclEnd, &pVertexElements[pData->NumVertexElements], sizeof (DeclEnd)));
    HRESULT hr = pDevice->pDevice9If->CreateVertexDeclaration(
            (CONST D3DVERTEXELEMENT9*)pVertexElements,
            &pDecl
          );
    Assert(hr == S_OK);
    if (hr == S_OK)
    {
        Assert(pDecl);
        pData->ShaderHandle = pDecl;
    }
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p), hr(0x%x)\n", hDevice, hr));
    return hr;
}
static HRESULT APIENTRY vboxWddmDDevSetVertexShaderDecl(HANDLE hDevice, HANDLE hShaderHandle)
{
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    Assert(pDevice);
    Assert(pDevice->pDevice9If);
    IDirect3DVertexDeclaration9 *pDecl = (IDirect3DVertexDeclaration9*)hShaderHandle;
    Assert(pDecl);
    HRESULT hr = pDevice->pDevice9If->SetVertexDeclaration(pDecl);
    Assert(hr == S_OK);
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p), hr(0x%x)\n", hDevice, hr));
    return hr;
}
static HRESULT APIENTRY vboxWddmDDevDeleteVertexShaderDecl(HANDLE hDevice, HANDLE hShaderHandle)
{
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    Assert(pDevice);
    Assert(pDevice->pDevice9If);
    IDirect3DVertexDeclaration9 *pDecl = (IDirect3DVertexDeclaration9*)hShaderHandle;
    HRESULT hr = S_OK;
    pDecl->Release();
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p), hr(0x%x)\n", hDevice, hr));
    return hr;
}
static HRESULT APIENTRY vboxWddmDDevCreateVertexShaderFunc(HANDLE hDevice, D3DDDIARG_CREATEVERTEXSHADERFUNC* pData, CONST UINT* pCode)
{
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    Assert(pDevice);
    Assert(pDevice->pDevice9If);
    IDirect3DVertexShader9 *pShader;
    Assert(*((UINT*)((uint8_t*)pCode + pData->Size-4)) == 0x0000FFFF /* end token */);
    HRESULT hr = pDevice->pDevice9If->CreateVertexShader((const DWORD *)pCode, &pShader);
    Assert(hr == S_OK);
    if (hr == S_OK)
    {
        Assert(pShader);
        pData->ShaderHandle = pShader;
    }
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p), hr(0x%x)\n", hDevice, hr));
    return hr;
}
static HRESULT APIENTRY vboxWddmDDevSetVertexShaderFunc(HANDLE hDevice, HANDLE hShaderHandle)
{
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    Assert(pDevice);
    Assert(pDevice->pDevice9If);
    IDirect3DVertexShader9 *pShader = (IDirect3DVertexShader9*)hShaderHandle;
    Assert(pShader);
    HRESULT hr = pDevice->pDevice9If->SetVertexShader(pShader);
    Assert(hr == S_OK);
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p), hr(0x%x)\n", hDevice, hr));
    return hr;
}
static HRESULT APIENTRY vboxWddmDDevDeleteVertexShaderFunc(HANDLE hDevice, HANDLE hShaderHandle)
{
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    Assert(pDevice);
    Assert(pDevice->pDevice9If);
    IDirect3DVertexShader9 *pShader = (IDirect3DVertexShader9*)hShaderHandle;
    HRESULT hr = S_OK;
    pShader->Release();
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p), hr(0x%x)\n", hDevice, hr));
    return hr;
}
static HRESULT APIENTRY vboxWddmDDevSetVertexShaderConstI(HANDLE hDevice, CONST D3DDDIARG_SETVERTEXSHADERCONSTI* pData, CONST INT* pRegisters)
{
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    Assert(pDevice);
    Assert(pDevice->pDevice9If);
    HRESULT hr = pDevice->pDevice9If->SetVertexShaderConstantI(pData->Register, pRegisters, pData->Count);
    Assert(hr == S_OK);
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p), hr(0x%x)\n", hDevice, hr));
    return hr;
}
static HRESULT APIENTRY vboxWddmDDevSetVertexShaderConstB(HANDLE hDevice, CONST D3DDDIARG_SETVERTEXSHADERCONSTB* pData, CONST BOOL* pRegisters)
{
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    Assert(pDevice);
    Assert(pDevice->pDevice9If);
    HRESULT hr = pDevice->pDevice9If->SetVertexShaderConstantB(pData->Register, pRegisters, pData->Count);
    Assert(hr == S_OK);
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p), hr(0x%x)\n", hDevice, hr));
    return hr;
}
static HRESULT APIENTRY vboxWddmDDevSetScissorRect(HANDLE hDevice, CONST RECT* pRect)
{
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    Assert(pDevice);
    Assert(pDevice->pDevice9If);
    HRESULT hr = pDevice->pDevice9If->SetScissorRect(pRect);
    Assert(hr == S_OK);
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p), hr(0x%x)\n", hDevice, hr));
    return hr;
}
static HRESULT APIENTRY vboxWddmDDevSetStreamSource(HANDLE hDevice, CONST D3DDDIARG_SETSTREAMSOURCE* pData)
{
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    Assert(pDevice);
    Assert(pDevice->pDevice9If);
    PVBOXWDDMDISP_RESOURCE pRc = (PVBOXWDDMDISP_RESOURCE)pData->hVertexBuffer;
    PVBOXWDDMDISP_ALLOCATION pAlloc = NULL;
    IDirect3DVertexBuffer9 *pStreamData = NULL;
    if (pRc)
    {
        Assert(pRc->cAllocations == 1);
        pAlloc = &pRc->aAllocations[0];
        Assert(pAlloc->pD3DIf);
        pStreamData = (IDirect3DVertexBuffer9*)pAlloc->pD3DIf;
    }
    HRESULT hr = pDevice->pDevice9If->SetStreamSource(pData->Stream, pStreamData, pData->Offset, pData->Stride);
    Assert(hr == S_OK);
    Assert(pData->Stream<VBOXWDDMDISP_MAX_VERTEX_STREAMS);
    if (hr == S_OK)
    {
        if (pDevice->aStreamSource[pData->Stream] && !pAlloc)
        {
            --pDevice->cStreamSources;
            Assert(pDevice->cStreamSources < UINT32_MAX/2);
        }
        else if (!pDevice->aStreamSource[pData->Stream] && pAlloc)
        {
            ++pDevice->cStreamSources;
            Assert(pDevice->cStreamSources <= RT_ELEMENTS(pDevice->aStreamSource));
        }
        pDevice->aStreamSource[pData->Stream] = pAlloc;
        pDevice->StreamSourceInfo[pData->Stream].uiOffset = pData->Offset;
        pDevice->StreamSourceInfo[pData->Stream].uiStride = pData->Stride;
    }
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p), hr(0x%x)\n", hDevice, hr));
    return hr;
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
static HRESULT vboxWddmLockRect(PVBOXWDDMDISP_RESOURCE pRc, UINT iAlloc,
        D3DLOCKED_RECT * pLockedRect,
        CONST RECT *pRect,
        DWORD fLockFlags)
{
    HRESULT hr = E_FAIL;
    Assert(!pRc->aAllocations[iAlloc].LockInfo.cLocks);
    Assert(pRc->cAllocations > iAlloc);
    switch (pRc->aAllocations[0].enmD3DIfType)
    {
        case VBOXDISP_D3DIFTYPE_SURFACE:
        {
            IDirect3DSurface9 *pD3DIfSurf = (IDirect3DSurface9*)pRc->aAllocations[iAlloc].pD3DIf;
            Assert(pD3DIfSurf);
            hr = pD3DIfSurf->LockRect(pLockedRect, pRect, fLockFlags);
            Assert(hr == S_OK);
            break;
        }
        case VBOXDISP_D3DIFTYPE_TEXTURE:
        {
            IDirect3DTexture9 *pD3DIfTex = (IDirect3DTexture9*)pRc->aAllocations[0].pD3DIf;
            Assert(pD3DIfTex);
            hr = pD3DIfTex->LockRect(iAlloc, pLockedRect, pRect, fLockFlags);
            Assert(hr == S_OK);
            break;
        }
        default:
            AssertBreakpoint();
            break;
    }
    return hr;
}
static HRESULT vboxWddmUnlockRect(PVBOXWDDMDISP_RESOURCE pRc, UINT iAlloc)
{
    HRESULT hr = S_OK;
    Assert(pRc->cAllocations > iAlloc);
    switch (pRc->aAllocations[0].enmD3DIfType)
    {
        case VBOXDISP_D3DIFTYPE_SURFACE:
        {
            IDirect3DSurface9 *pD3DIfSurf = (IDirect3DSurface9*)pRc->aAllocations[iAlloc].pD3DIf;
            Assert(pD3DIfSurf);
            hr = pD3DIfSurf->UnlockRect();
            Assert(hr == S_OK);
            break;
        }
        case VBOXDISP_D3DIFTYPE_TEXTURE:
        {
            IDirect3DTexture9 *pD3DIfTex = (IDirect3DTexture9*)pRc->aAllocations[0].pD3DIf;
            Assert(pD3DIfTex);
            hr = pD3DIfTex->UnlockRect(iAlloc);
            Assert(hr == S_OK);
            break;
        }
        default:
            AssertBreakpoint();
            hr = E_FAIL;
            break;
    }
    return hr;
}

/* on success increments the surface ref counter,
 * i.e. one must call pSurf->Release() once the surface is not needed*/
static HRESULT vboxWddmSurfGet(PVBOXWDDMDISP_RESOURCE pRc, UINT iAlloc, IDirect3DSurface9 **ppSurf)
{
    HRESULT hr = S_OK;
    Assert(pRc->cAllocations > iAlloc);
    switch (pRc->aAllocations[0].enmD3DIfType)
    {
        case VBOXDISP_D3DIFTYPE_SURFACE:
        {
            IDirect3DSurface9 *pD3DIfSurf = (IDirect3DSurface9*)pRc->aAllocations[iAlloc].pD3DIf;
            Assert(pD3DIfSurf);
            pD3DIfSurf->AddRef();
            *ppSurf = pD3DIfSurf;
            break;
        }
        case VBOXDISP_D3DIFTYPE_TEXTURE:
        {
            IDirect3DTexture9 *pD3DIfTex = (IDirect3DTexture9*)pRc->aAllocations[0].pD3DIf;
            IDirect3DSurface9 *pSurfaceLevel;
            Assert(pD3DIfTex);
            hr = pD3DIfTex->GetSurfaceLevel(iAlloc, &pSurfaceLevel);
            Assert(hr == S_OK);
            if (hr == S_OK)
            {
                *ppSurf = pSurfaceLevel;
            }
            break;
        }
        default:
            AssertBreakpoint();
            hr = E_FAIL;
            break;
    }
    return hr;
}

static HRESULT APIENTRY vboxWddmDDevBlt(HANDLE hDevice, CONST D3DDDIARG_BLT* pData)
{
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    Assert(pDevice);
    Assert(pDevice->pDevice9If);
    PVBOXWDDMDISP_RESOURCE pDstRc = (PVBOXWDDMDISP_RESOURCE)pData->hDstResource;
    PVBOXWDDMDISP_RESOURCE pSrcRc = (PVBOXWDDMDISP_RESOURCE)pData->hSrcResource;
    Assert(pDstRc->cAllocations > pData->DstSubResourceIndex);
    Assert(pSrcRc->cAllocations > pData->SrcSubResourceIndex);
    HRESULT hr = S_OK;
    /* try StretchRect */
    IDirect3DSurface9 *pSrcSurfIf = NULL;
    IDirect3DSurface9 *pDstSurfIf = NULL;
    hr = vboxWddmSurfGet(pSrcRc, pData->SrcSubResourceIndex, &pSrcSurfIf);
    Assert(hr == S_OK);
    if (hr == S_OK)
    {
        Assert(pSrcSurfIf);
        hr = vboxWddmSurfGet(pDstRc, pData->DstSubResourceIndex, &pDstSurfIf);
        Assert(hr == S_OK);
        if (hr == S_OK)
        {
            Assert(pDstSurfIf);
            /* we support only Point & Linear, we ignore [Begin|Continue|End]PresentToDwm */
            Assert((pData->Flags.Value & (~(0x00000100 | 0x00000200 | 0x00000400 | 0x00000001  | 0x00000002))) == 0);
            hr = pDevice->pDevice9If->StretchRect(pSrcSurfIf,
                                &pData->SrcRect,
                                pDstSurfIf,
                                &pData->DstRect,
                                vboxDDI2D3DBltFlags(pData->Flags));
            Assert(hr == S_OK);
            pDstSurfIf->Release();
        }
        pSrcSurfIf->Release();
    }

    if (hr != S_OK)
    {
        /* todo: fallback to memcpy or whatever ? */
        AssertBreakpoint();
    }


#if 0
    if ((use pAlloc->enmD3DIfType instead!!! pDstRc->RcDesc.fFlags.Texture || pDstRc->RcDesc.fFlags.Value == 0)
            && (pSrcRc->RcDesc.fFlags.Texture || pSrcRc->RcDesc.fFlags.Value == 0))
    {
        IDirect3DTexture9 *pD3DIfSrcTex = (IDirect3DTexture9*)pSrcAlloc->pD3DIf;
        IDirect3DTexture9 *pD3DIfDstTex = (IDirect3DTexture9*)pDstAlloc->pD3DIf;
        Assert(pD3DIfSrcTex);
        Assert(pD3DIfDstTex);

        if (pSrcRc->RcDesc.enmFormat == pDstRc->RcDesc.enmFormat)
        {
            if (pSrcRc->aAllocations[0].SurfDesc.width == pDstRc->aAllocations[0].SurfDesc.width
                    && pSrcRc->aAllocations[0].SurfDesc.height == pDstRc->aAllocations[0].SurfDesc.height
                    && pData->DstRect.left == 0 && pData->DstRect.top == 0
                    && pData->SrcRect.left == 0 && pData->SrcRect.top == 0
                    && pData->SrcRect.right - pData->SrcRect.left == pSrcRc->aAllocations[0].SurfDesc.width
                    && pData->SrcRect.bottom - pData->SrcRect.top == pSrcRc->aAllocations[0].SurfDesc.height
                    && pData->DstRect.right - pData->DstRect.left == pDstRc->aAllocations[0].SurfDesc.width
                    && pData->DstRect.bottom - pData->DstRect.top == pDstRc->aAllocations[0].SurfDesc.height
                    )
            {
                hr = pDevice->pDevice9If->UpdateTexture(pD3DIfSrcTex, pD3DIfDstTex);
                Assert(hr == S_OK);
            }
            else if (pData->SrcRect.right - pData->SrcRect.left == pData->DstRect.right - pData->DstRect.left
                    && pData->SrcRect.bottom - pData->SrcRect.top == pData->DstRect.bottom - pData->DstRect.top)
            {
                Assert(pDstAlloc->SurfDesc.bpp);
                Assert(pSrcAlloc->SurfDesc.bpp);
                Assert(pSrcAlloc->SurfDesc.bpp == pDstAlloc->SurfDesc.bpp);
                D3DLOCKED_RECT DstRect, SrcRect;
                Assert(!pSrcAlloc->LockInfo.cLocks);
                Assert(!pDstAlloc->LockInfo.cLocks);
                hr = pD3DIfDstTex->LockRect(pData->DstSubResourceIndex, &DstRect, &pData->DstRect, D3DLOCK_DISCARD);
                Assert(hr == S_OK);
                if (hr == S_OK)
                {
                    hr = pD3DIfSrcTex->LockRect(pData->SrcSubResourceIndex, &SrcRect, &pData->SrcRect, D3DLOCK_READONLY);
                    Assert(hr == S_OK);
                    if (hr == S_OK)
                    {
                        hr = vboxWddmRectBltPerform((uint8_t *)DstRect.pBits, (uint8_t *)SrcRect.pBits,
                                &pData->DstRect,  &pData->SrcRect,
                                DstRect.Pitch, SrcRect.Pitch, pDstAlloc->SurfDesc.bpp);
                        Assert(hr == S_OK);

                        pD3DIfSrcTex->UnlockRect(pData->SrcSubResourceIndex);
                    }
                    pD3DIfDstTex->UnlockRect(pData->DstSubResourceIndex);
                }
            }
            else
            {

                AssertBreakpoint();
                /* @todo: impl */
            }
        }
        else
        {
            AssertBreakpoint();
            /* @todo: impl */
        }
    }
    else
    {
        if (pData->SrcRect.right - pData->SrcRect.left == pData->DstRect.right - pData->DstRect.left
                            && pData->SrcRect.bottom - pData->SrcRect.top == pData->DstRect.bottom - pData->DstRect.top)
        {
            Assert(pDstAlloc->SurfDesc.bpp);
            Assert(pSrcAlloc->SurfDesc.bpp);
            Assert(pSrcAlloc->SurfDesc.bpp == pDstAlloc->SurfDesc.bpp);

            D3DLOCKED_RECT DstRect, SrcRect;
            hr = vboxWddmLockRect(pDstAlloc, pData->DstSubResourceIndex, pDstRc->RcDesc.fFlags,
                    &DstRect, &pData->DstRect, D3DLOCK_DISCARD);
            Assert(hr == S_OK);
            if (hr == S_OK)
            {
                hr = vboxWddmLockRect(pSrcAlloc, pData->SrcSubResourceIndex, pSrcRc->RcDesc.fFlags,
                        &SrcRect, &pData->SrcRect, D3DLOCK_READONLY);
                Assert(hr == S_OK);
                if (hr == S_OK)
                {
                    hr = vboxWddmRectBltPerform((uint8_t *)DstRect.pBits, (uint8_t *)SrcRect.pBits,
                            &pData->DstRect,  &pData->SrcRect,
                            DstRect.Pitch, SrcRect.Pitch, pDstAlloc->SurfDesc.bpp);
                    Assert(hr == S_OK);

                    HRESULT tmpHr = vboxWddmUnlockRect(pSrcAlloc, pData->SrcSubResourceIndex, pSrcRc->RcDesc.fFlags);
                    Assert(tmpHr == S_OK);
                }
                HRESULT tmpHr = vboxWddmUnlockRect(pDstAlloc, pData->DstSubResourceIndex, pDstRc->RcDesc.fFlags);
                Assert(tmpHr == S_OK);
            }
        }
        else
        {
            AssertBreakpoint();
            /* @todo: impl */
        }
    }
#endif

    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p), hr(0x%x)\n", hDevice, hr));
    return hr;
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
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    Assert(pDevice);
    Assert(pDevice->pDevice9If);
    PVBOXWDDMDISP_RESOURCE pRc = (PVBOXWDDMDISP_RESOURCE)pData->hRenderTarget;
    Assert(pRc);
    Assert(pData->SubResourceIndex < pRc->cAllocations);
    IDirect3DSurface9 *pD3D9Surf = (IDirect3DSurface9*)pRc->aAllocations[pData->SubResourceIndex].pD3DIf;
    Assert(pD3D9Surf);
    HRESULT hr = pDevice->pDevice9If->SetRenderTarget(pData->RenderTargetIndex, pD3D9Surf);
    Assert(hr == S_OK);
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p), hr(0x%x)\n", hDevice, hr));
    return hr;
}
static HRESULT APIENTRY vboxWddmDDevSetDepthStencil(HANDLE hDevice, CONST D3DDDIARG_SETDEPTHSTENCIL* pData)
{
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    Assert(pDevice);
    Assert(pDevice->pDevice9If);
    PVBOXWDDMDISP_RESOURCE pRc = (PVBOXWDDMDISP_RESOURCE)pData->hZBuffer;
    IDirect3DSurface9 *pD3D9Surf = NULL;
    if (pRc)
    {
        Assert(pRc->cAllocations == 1);
        pD3D9Surf = (IDirect3DSurface9*)pRc->aAllocations[0].pD3DIf;
        Assert(pD3D9Surf);
    }
    HRESULT hr = pDevice->pDevice9If->SetDepthStencilSurface(pD3D9Surf);
    Assert(hr == S_OK);
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p), hr(0x%x)\n", hDevice, hr));
    return hr;
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
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    Assert(pDevice);
    Assert(pDevice->pDevice9If);
    HRESULT hr = pDevice->pDevice9If->SetPixelShaderConstantI(pData->Register, pRegisters, pData->Count);
    Assert(hr == S_OK);
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p), hr(0x%x)\n", hDevice, hr));
    return hr;
}
static HRESULT APIENTRY vboxWddmDDevSetPixelShaderConstB(HANDLE hDevice, CONST D3DDDIARG_SETPIXELSHADERCONSTB* pData, CONST BOOL* pRegisters)
{
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    Assert(pDevice);
    Assert(pDevice->pDevice9If);
    HRESULT hr = pDevice->pDevice9If->SetPixelShaderConstantB(pData->Register, pRegisters, pData->Count);
    Assert(hr == S_OK);
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p), hr(0x%x)\n", hDevice, hr));
    return hr;
}
static HRESULT APIENTRY vboxWddmDDevCreatePixelShader(HANDLE hDevice, D3DDDIARG_CREATEPIXELSHADER* pData, CONST UINT* pCode)
{
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    Assert(pDevice);
    Assert(pDevice->pDevice9If);
    IDirect3DPixelShader9 *pShader;
    Assert(*((UINT*)((uint8_t*)pCode + pData->CodeSize-4)) == 0x0000FFFF /* end token */);
    HRESULT hr = pDevice->pDevice9If->CreatePixelShader((const DWORD *)pCode, &pShader);
    Assert(hr == S_OK);
    if (hr == S_OK)
    {
        Assert(pShader);
        pData->ShaderHandle = pShader;
    }
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p), hr(0x%x)\n", hDevice, hr));
    return hr;
}
static HRESULT APIENTRY vboxWddmDDevDeletePixelShader(HANDLE hDevice, HANDLE hShaderHandle)
{
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    Assert(pDevice);
    Assert(pDevice->pDevice9If);
    IDirect3DPixelShader9 *pShader = (IDirect3DPixelShader9*)hShaderHandle;
    HRESULT hr = S_OK;
    pShader->Release();
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p), hr(0x%x)\n", hDevice, hr));
    return hr;
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
    PVBOXWDDMDISP_ADAPTER pAdapter = pDevice->pAdapter;
    if (pDevice->pDevice9If)
    {
        pDevice->pDevice9If->Release();
        Assert(pDevice->hWnd);
        HRESULT tmpHr = VBoxDispWndDestroy(pAdapter, pDevice->hWnd);
        Assert(tmpHr == S_OK);
    }
    HRESULT hr = vboxDispCmCtxDestroy(pDevice, &pDevice->DefaultContext);
    Assert(hr == S_OK);
    if (hr == S_OK)
        RTMemFree(pDevice);
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return hr;
}

AssertCompile(sizeof (RECT) == sizeof (D3DDDIRECT));
AssertCompile(RT_SIZEOFMEMB(RECT, left) == RT_SIZEOFMEMB(D3DDDIRECT, left));
AssertCompile(RT_SIZEOFMEMB(RECT, right) == RT_SIZEOFMEMB(D3DDDIRECT, right));
AssertCompile(RT_SIZEOFMEMB(RECT, top) == RT_SIZEOFMEMB(D3DDDIRECT, top));
AssertCompile(RT_SIZEOFMEMB(RECT, bottom) == RT_SIZEOFMEMB(D3DDDIRECT, bottom));
AssertCompile(RT_OFFSETOF(RECT, left) == RT_OFFSETOF(D3DDDIRECT, left));
AssertCompile(RT_OFFSETOF(RECT, right) == RT_OFFSETOF(D3DDDIRECT, right));
AssertCompile(RT_OFFSETOF(RECT, top) == RT_OFFSETOF(D3DDDIRECT, top));
AssertCompile(RT_OFFSETOF(RECT, bottom) == RT_OFFSETOF(D3DDDIRECT, bottom));

static HRESULT APIENTRY vboxWddmDDevCreateOverlay(HANDLE hDevice, D3DDDIARG_CREATEOVERLAY* pData)
{
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    PVBOXWDDMDISP_RESOURCE pRc = (PVBOXWDDMDISP_RESOURCE)pData->OverlayInfo.hResource;
    Assert(pRc);
    Assert(pRc->cAllocations > pData->OverlayInfo.SubResourceIndex);
    PVBOXWDDMDISP_ALLOCATION pAlloc = &pRc->aAllocations[pData->OverlayInfo.SubResourceIndex];
    HRESULT hr = S_OK;
    PVBOXWDDMDISP_OVERLAY pOverlay = (PVBOXWDDMDISP_OVERLAY)RTMemAllocZ(sizeof (VBOXWDDMDISP_OVERLAY));
    Assert(pOverlay);
    if (pOverlay)
    {
        VBOXWDDM_OVERLAY_INFO OurInfo;
        OurInfo.OverlayDesc.DstColorKeyLow = pData->OverlayInfo.DstColorKeyLow;
        OurInfo.OverlayDesc.DstColorKeyHigh = pData->OverlayInfo.DstColorKeyHigh;
        OurInfo.OverlayDesc.SrcColorKeyLow = pData->OverlayInfo.SrcColorKeyLow;
        OurInfo.OverlayDesc.SrcColorKeyHigh = pData->OverlayInfo.SrcColorKeyHigh;
        OurInfo.OverlayDesc.fFlags = pData->OverlayInfo.Flags.Value;
        vboxWddmDirtyRegionClear(&OurInfo.DirtyRegion);
        Assert(!pAlloc->LockInfo.cLocks);
        vboxWddmDirtyRegionUnite(&OurInfo.DirtyRegion, &pAlloc->DirtyRegion);
        D3DDDICB_CREATEOVERLAY OverInfo;
        OverInfo.VidPnSourceId = pData->VidPnSourceId;
        OverInfo.OverlayInfo.hAllocation = pAlloc->hAllocation;
        Assert(pAlloc->hAllocation);
        OverInfo.OverlayInfo.DstRect = *(D3DDDIRECT*)((void*)&pData->OverlayInfo.DstRect);
        OverInfo.OverlayInfo.SrcRect = *(D3DDDIRECT*)((void*)&pData->OverlayInfo.SrcRect);
        OverInfo.OverlayInfo.pPrivateDriverData = &OurInfo;
        OverInfo.OverlayInfo.PrivateDriverDataSize = sizeof (OurInfo);
        OverInfo.hKernelOverlay = NULL; /* <-- out */
#ifndef VBOXWDDMOVERLAY_TEST
        hr = pDevice->RtCallbacks.pfnCreateOverlayCb(pDevice->hDevice, &OverInfo);
        Assert(hr == S_OK);
        if (hr == S_OK)
        {
            Assert(OverInfo.hKernelOverlay);
            pOverlay->hOverlay = OverInfo.hKernelOverlay;
            pOverlay->VidPnSourceId = pData->VidPnSourceId;

            Assert(!pAlloc->LockInfo.cLocks);
            if (!pAlloc->LockInfo.cLocks)
            {
                /* we have reported the dirty rect, may clear it if no locks are pending currently */
                vboxWddmDirtyRegionClear(&pAlloc->DirtyRegion);
            }

            pData->hOverlay = pOverlay;
        }
        else
        {
            RTMemFree(pOverlay);
        }
#else
        pData->hOverlay = pOverlay;
#endif
    }
    else
        hr = E_OUTOFMEMORY;

    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return hr;
}
static HRESULT APIENTRY vboxWddmDDevUpdateOverlay(HANDLE hDevice, CONST D3DDDIARG_UPDATEOVERLAY* pData)
{
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    PVBOXWDDMDISP_RESOURCE pRc = (PVBOXWDDMDISP_RESOURCE)pData->OverlayInfo.hResource;
    Assert(pRc);
    Assert(pRc->cAllocations > pData->OverlayInfo.SubResourceIndex);
    PVBOXWDDMDISP_ALLOCATION pAlloc = &pRc->aAllocations[pData->OverlayInfo.SubResourceIndex];
    HRESULT hr = S_OK;
    PVBOXWDDMDISP_OVERLAY pOverlay = (PVBOXWDDMDISP_OVERLAY)pData->hOverlay;
    VBOXWDDM_OVERLAY_INFO OurInfo;
    OurInfo.OverlayDesc.DstColorKeyLow = pData->OverlayInfo.DstColorKeyLow;
    OurInfo.OverlayDesc.DstColorKeyHigh = pData->OverlayInfo.DstColorKeyHigh;
    OurInfo.OverlayDesc.SrcColorKeyLow = pData->OverlayInfo.SrcColorKeyLow;
    OurInfo.OverlayDesc.SrcColorKeyHigh = pData->OverlayInfo.SrcColorKeyHigh;
    OurInfo.OverlayDesc.fFlags = pData->OverlayInfo.Flags.Value;
    vboxWddmDirtyRegionClear(&OurInfo.DirtyRegion);
    Assert(!pAlloc->LockInfo.cLocks);
    vboxWddmDirtyRegionUnite(&OurInfo.DirtyRegion, &pAlloc->DirtyRegion);
    D3DDDICB_UPDATEOVERLAY OverInfo;
    OverInfo.hKernelOverlay = pOverlay->hOverlay;
    OverInfo.OverlayInfo.hAllocation = pAlloc->hAllocation;
    OverInfo.OverlayInfo.DstRect = *(D3DDDIRECT*)((void*)&pData->OverlayInfo.DstRect);
    OverInfo.OverlayInfo.SrcRect = *(D3DDDIRECT*)((void*)&pData->OverlayInfo.SrcRect);
    OverInfo.OverlayInfo.pPrivateDriverData = &OurInfo;
    OverInfo.OverlayInfo.PrivateDriverDataSize = sizeof (OurInfo);
#ifndef VBOXWDDMOVERLAY_TEST
    hr = pDevice->RtCallbacks.pfnUpdateOverlayCb(pDevice->hDevice, &OverInfo);
    Assert(hr == S_OK);
    if (hr == S_OK)
#endif
    {
        Assert(!pAlloc->LockInfo.cLocks);
        if (!pAlloc->LockInfo.cLocks)
        {
            /* we have reported the dirty rect, may clear it if no locks are pending currently */
            vboxWddmDirtyRegionClear(&pAlloc->DirtyRegion);
        }
    }

    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return hr;
}
static HRESULT APIENTRY vboxWddmDDevFlipOverlay(HANDLE hDevice, CONST D3DDDIARG_FLIPOVERLAY* pData)
{
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    PVBOXWDDMDISP_RESOURCE pRc = (PVBOXWDDMDISP_RESOURCE)pData->hSource;
    Assert(pRc);
    Assert(pRc->cAllocations > pData->SourceIndex);
    PVBOXWDDMDISP_ALLOCATION pAlloc = &pRc->aAllocations[pData->SourceIndex];
    HRESULT hr = S_OK;
    PVBOXWDDMDISP_OVERLAY pOverlay = (PVBOXWDDMDISP_OVERLAY)pData->hOverlay;
    VBOXWDDM_OVERLAYFLIP_INFO OurInfo;
    vboxWddmDirtyRegionClear(&OurInfo.DirtyRegion);
    Assert(!pAlloc->LockInfo.cLocks);
    vboxWddmDirtyRegionUnite(&OurInfo.DirtyRegion, &pAlloc->DirtyRegion);
    D3DDDICB_FLIPOVERLAY OverInfo;
    OverInfo.hKernelOverlay = pOverlay->hOverlay;
    OverInfo.hSource = pAlloc->hAllocation;
    OverInfo.pPrivateDriverData = &OurInfo;
    OverInfo.PrivateDriverDataSize = sizeof (OurInfo);
#ifndef VBOXWDDMOVERLAY_TEST
    hr = pDevice->RtCallbacks.pfnFlipOverlayCb(pDevice->hDevice, &OverInfo);
    Assert(hr == S_OK);
    if (hr == S_OK)
#endif
    {
        Assert(!pAlloc->LockInfo.cLocks);
        if (!pAlloc->LockInfo.cLocks)
        {
            /* we have reported the dirty rect, may clear it if no locks are pending currently */
            vboxWddmDirtyRegionClear(&pAlloc->DirtyRegion);
        }
    }

    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return hr;
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
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    PVBOXWDDMDISP_OVERLAY pOverlay = (PVBOXWDDMDISP_OVERLAY)pData->hOverlay;
    D3DDDICB_DESTROYOVERLAY OverInfo;
    OverInfo.hKernelOverlay = pOverlay->hOverlay;
#ifndef VBOXWDDMOVERLAY_TEST
    HRESULT hr = pDevice->RtCallbacks.pfnDestroyOverlayCb(pDevice->hDevice, &OverInfo);
    Assert(hr == S_OK);
    if (hr == S_OK)
#else
    HRESULT hr = S_OK;
#endif
    {
        RTMemFree(pOverlay);
    }

    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return hr;
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
            AssertBreakpoint(); /* <-- need to test that */

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
        /* Set Viewport to some default values */
        pDevice->ViewPort.X = 0;
        pDevice->ViewPort.Y = 0;
        pDevice->ViewPort.Width = 1;
        pDevice->ViewPort.Height = 1;
        pDevice->ViewPort.MinZ = 0.;
        pDevice->ViewPort.MaxZ = 1.;

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
                hr = vboxDispCmCtxCreate(pDevice, &pDevice->DefaultContext);
                Assert(hr == S_OK);
                if (hr == S_OK)
                    break;
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
    if (VBOXDISPMODE_IS_3D(pAdapter))
    {
        HRESULT hr = VBoxDispWorkerDestroy(&pAdapter->WndWorker);
        Assert(hr == S_OK);
        pAdapter->pD3D9If->Release();
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
                        hr = VBoxDispWorkerCreate(&pAdapter->WndWorker);
                        Assert(hr == S_OK);
                        if (hr == S_OK)
                        {
                            vboxVDbgPrint((__FUNCTION__": SUCCESS 3D Enabled, pAdapter (0x%p)\n", pAdapter));
                            break;
                        }
                        pAdapter->pD3D9If->Release();
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
