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

static FORMATOP gVBoxFormatOps[] = {
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
};

#define VBOX_FORMATOP_COUNT() (sizeof(gVBoxFormatOps)/sizeof(gVBoxFormatOps[0]))

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

#endif

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
            AssertBreakpoint();
            Assert(pData->DataSize >= sizeof (DDRAW_CAPS));
            if (pData->DataSize >= sizeof (DDRAW_CAPS))
            {
                memset(pData->pData, 0, sizeof (DDRAW_CAPS));
#ifdef VBOX_WITH_VIDEOHWACCEL
                if (vboxVhwaHasCKeying(pAdapter))
                {
                    DDRAW_CAPS *pCaps = (DDRAW_CAPS*)pData->pData;
                    pCaps->Caps |= DDRAW_CAPS_COLORKEY;
                }
#endif
            }
            else
                hr = E_INVALIDARG;
            break;
        }
        case D3DDDICAPS_DDRAW_MODE_SPECIFIC:
        {
            AssertBreakpoint();
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
            *((uint32_t*)pData->pData) = VBOX_FORMATOP_COUNT();
            break;
        case D3DDDICAPS_GETFORMATDATA:
            Assert(pData->DataSize >= VBOX_FORMATOP_COUNT() * sizeof(FORMATOP));
            memcpy(pData->pData, gVBoxFormatOps, VBOX_FORMATOP_COUNT()*sizeof(FORMATOP));
            break;
        case D3DDDICAPS_GETD3DQUERYCOUNT:
            *((uint32_t*)pData->pData) = 0;
            break;
        case D3DDDICAPS_GETD3D3CAPS:
            Assert(pData->DataSize >= sizeof (D3DHAL_GLOBALDRIVERDATA));
            if (pData->DataSize >= sizeof (D3DHAL_GLOBALDRIVERDATA))
                memset (pData->pData, 0, sizeof (D3DHAL_GLOBALDRIVERDATA));
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
            if (pData->DataSize >= sizeof (D3DCAPS9))
            {
                if (pAdapter->pD3D9If)
                {
                    hr = pAdapter->pD3D9If->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, (D3DCAPS9*)pData->pData);
                    Assert(hr == S_OK);
                    if (hr == S_OK)
                        break;

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
        case D3DDDICAPS_GETD3DQUERYDATA:
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
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevUnlock(HANDLE hDevice, CONST D3DDDIARG_UNLOCK* pData)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
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
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
}
static HRESULT APIENTRY vboxWddmDDevDestroyResource(HANDLE hDevice, HANDLE hResource)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
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
    RTMemFree(hDevice);
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    RTMemFree(hDevice);
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
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
static HRESULT APIENTRY vboxWddmDDevOpenResource(HANDLE hDevice, D3DDDIARG_OPENRESOURCE* pData)
{
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    AssertBreakpoint();
    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return E_FAIL;
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
    vboxVDbgPrint(("==> "__FUNCTION__", hAdapter(0x%p)\n", hAdapter));

//    AssertBreakpoint();

    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)RTMemAllocZ(sizeof (VBOXWDDMDISP_DEVICE));
    if (!pDevice)
    {
        vboxVDbgPrintR((__FUNCTION__": RTMemAllocZ returned NULL\n"));
        return E_OUTOFMEMORY;
    }

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
    if (pCreateData->AllocationListSize)
    {
        vboxVDbgPrintR((__FUNCTION__": Not implemented: AllocationListSize(%d)\n", pCreateData->AllocationListSize));
        //pCreateData->pAllocationList = ??
        return E_FAIL;
    }

    Assert(!pCreateData->PatchLocationListSize);
    if (pCreateData->PatchLocationListSize)
    {
        vboxVDbgPrintR((__FUNCTION__": Not implemented: PatchLocationListSize(%d)\n", pCreateData->PatchLocationListSize));
        //pCreateData->pPatchLocationList = ??
        return E_FAIL;
    }

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

    vboxVDbgPrint(("<== "__FUNCTION__", hAdapter(0x%p)\n", hAdapter));

    return S_OK;
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

#ifdef VBOX_WITH_VIDEOHWACCEL
        pAdapter->cHeads = Query.cInfos;
        for (uint32_t i = 0; i < pAdapter->cHeads; ++i)
        {
            pAdapter->aHeads[i].Vhwa.Settings = Query.aInfos[i];
        }
#endif

        pOpenData->hAdapter = pAdapter;
        pOpenData->pAdapterFuncs->pfnGetCaps = vboxWddmDispGetCaps;
        pOpenData->pAdapterFuncs->pfnCreateDevice = vboxWddmDispCreateDevice;
        pOpenData->pAdapterFuncs->pfnCloseAdapter = vboxWddmDispCloseAdapter;
        pOpenData->DriverVersion = D3D_UMD_INTERFACE_VERSION;

        /* try enable the 3D */
        hr = VBoxDispD3DOpen(&pAdapter->D3D);
        Assert(hr == S_OK);
        if (hr == S_OK)
        {
            hr = pAdapter->D3D.pfnDirect3DCreate9Ex(D3D_SDK_VERSION, &pAdapter->pD3D9If);
            Assert(hr == S_OK);
            if (hr == S_OK)
            {
                vboxVDbgPrint(("<== "__FUNCTION__", SUCCESS 3D Enabled, pAdapter (0x%p)\n", pAdapter));
                return S_OK;
            }
            else
                vboxVDbgPrintR((__FUNCTION__": pfnDirect3DCreate9Ex failed, hr (%d)\n", hr));
        }
        else
            vboxVDbgPrintR((__FUNCTION__": VBoxDispD3DOpen failed, hr (%d)\n", hr));

        vboxVDbgPrint(("<== "__FUNCTION__", SUCCESS 3D DISABLED, pAdapter (0x%p)\n", pAdapter));
        return S_OK;
//        RTMemFree(pAdapter);
    }
    else
    {
        vboxVDbgPrintR((__FUNCTION__": RTMemAllocZ returned NULL\n"));
        hr = E_OUTOFMEMORY;
    }

    vboxVDbgPrint(("<== "__FUNCTION__", FAILURE, hr (%d)\n", hr));

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
