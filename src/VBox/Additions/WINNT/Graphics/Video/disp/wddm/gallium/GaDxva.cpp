/* $Id$ */
/** @file
 * VirtualBox WDDM DXVA for the Gallium based driver.
 */

/*
 * Copyright (C) 2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "VBoxGallium.h"
//#include "../../../common/wddm/VBoxMPIf.h"
#include "../VBoxDispD3DCmn.h"
#include "../VBoxDispD3D.h"


#define D3D_RELEASE(ptr) do { \
    if (ptr)                  \
    {                         \
        (ptr)->Release();     \
        (ptr) = 0;            \
    }                         \
} while (0)

struct Vertex
{
    float x, y; /* The vertex position. */
    float u, v; /* Texture coordinates. */
};

/*
 * Draw a quad in order to convert the input resource to the output render target.
 * The pixel shader will provide required conversion.
 */
typedef struct VBOXWDDMVIDEOPROCESSDEVICE
{
    /* Creation parameters. */
    PVBOXWDDMDISP_DEVICE   pDevice;
    GUID                   VideoProcGuid;
    DXVADDI_VIDEODESC      VideoDesc;
    D3DDDIFORMAT           RenderTargetFormat;
    UINT                   MaxSubStreams;

    /* The current render target, i.e. the Blt destination. */
    PVBOXWDDMDISP_RESOURCE pRenderTarget;
    UINT SubResourceIndex;
    IDirect3DTexture9 *pRTTexture;

    /* Private objects for video processing. */
    IDirect3DTexture9           *pTexture;    /* Intermediate texture. */
    IDirect3DVertexBuffer9      *pVB;         /* Vertex buffer which describes the quad we render. */
    IDirect3DVertexDeclaration9 *pVertexDecl; /* Vertex declaration for the quad vertices. */
    IDirect3DVertexShader9      *pVS;         /* Vertex shader. */
    IDirect3DPixelShader9       *pPS;         /* Pixel shader. */
} VBOXWDDMVIDEOPROCESSDEVICE;

static const GUID gaDeviceGuids[] =
{
    DXVADDI_VideoProcProgressiveDevice,
    DXVADDI_VideoProcBobDevice
};

static const D3DDDIFORMAT gaInputFormats[] =
{
    D3DDDIFMT_YUY2
};

static const D3DDDIFORMAT gaOutputFormats[] =
{
    D3DDDIFMT_A8R8G8B8,
    D3DDDIFMT_X8R8G8B8
};

static int vboxDxvaFindDeviceGuid(GUID const *pGuid)
{
    for (int i = 0; i < RT_ELEMENTS(gaDeviceGuids); ++i)
    {
        if (memcmp(pGuid, &gaDeviceGuids[i], sizeof(GUID)) == 0)
            return i;
    }
    return -1;
}

static int vboxDxvaFindInputFormat(D3DDDIFORMAT enmFormat)
{
    for (int i = 0; i < RT_ELEMENTS(gaInputFormats); ++i)
    {
        if (enmFormat == gaInputFormats[0])
            return i;
    }
    return -1;
}


static HRESULT vboxDxvaInit(VBOXWDDMVIDEOPROCESSDEVICE *pVideoProcessDevice /*,
                            DWORD const *paVS, DWORD const *paPS*/)
{
    HRESULT hr;

    IDirect3DDevice9 *pDevice9 = pVideoProcessDevice->pDevice->pDevice9If;
    AssertPtrReturn(pDevice9, E_INVALIDARG);

#if 0
    static D3DVERTEXELEMENT9 const aVertexElements[] =
    {
        {0,  0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {0,  8, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
        D3DDECL_END()
    };

    HRESULT hr = pDevice9->CreateVertexDeclaration(aVertexElements, &pVideoProcessDevice->pVertexDecl);
    AssertReturn(hr == D3D_OK, hr);

    hr = pDevice9->CreateVertexBuffer(6 * sizeof(Vertex), /* 2 triangles. */
                                      D3DUSAGE_WRITEONLY,
                                      0, /* FVF */
                                      D3DPOOL_DEFAULT,
                                      &pVideoProcessDevice->pVB,
                                      0);
    AssertReturn(hr == D3D_OK, hr);

    hr = pDevice9->CreateVertexShader(paVS, &pVideoProcessDevice->pVS);
    AssertReturn(hr == D3D_OK, hr);

    hr = pDevice9->CreatePixelShader(paPS, &pVideoProcessDevice->pPS);
    AssertReturn(hr == D3D_OK, hr);
#endif

    hr = pDevice9->CreateTexture(pVideoProcessDevice->VideoDesc.SampleWidth,
                                         pVideoProcessDevice->VideoDesc.SampleHeight,
                                         0, /* Levels */
                                         0, /* D3DUSAGE_ */
                                         D3DFMT_YUY2, /** @todo Maybe a stretch is enough because host already does the conversion? */
                                         D3DPOOL_DEFAULT,
                                         &pVideoProcessDevice->pTexture,
                                         NULL);
    AssertReturn(hr == D3D_OK, hr);

    return S_OK;
}

static HRESULT vboxDxvaBltRect(VBOXWDDMVIDEOPROCESSDEVICE *pVideoProcessDevice,
                               IDirect3DTexture9 *pSrcTexture,
                               UINT SrcSubResourceIndex,
                               RECT const *pSrcRect,
                               RECT const *pDstRect)
{
    HRESULT hr;

    IDirect3DDevice9 *pDevice9 = pVideoProcessDevice->pDevice->pDevice9If;
    AssertPtrReturn(pDevice9, E_INVALIDARG);

    hr = pDevice9->UpdateTexture(pSrcTexture, pVideoProcessDevice->pTexture);
    AssertReturn(hr == D3D_OK, hr);

    IDirect3DSurface9 *pSrcSurface;
    hr = pVideoProcessDevice->pTexture->GetSurfaceLevel(SrcSubResourceIndex, &pSrcSurface);
    AssertReturn(hr == D3D_OK, hr);

    IDirect3DSurface9 *pDstSurface;
    hr = pVideoProcessDevice->pRTTexture->GetSurfaceLevel(pVideoProcessDevice->SubResourceIndex, &pDstSurface);
    AssertReturnStmt(hr == D3D_OK, D3D_RELEASE(pSrcSurface), hr);

    hr = pDevice9->StretchRect(pSrcSurface, pSrcRect, pDstSurface, pDstRect, D3DTEXF_NONE);

    D3D_RELEASE(pSrcSurface);
    D3D_RELEASE(pDstSurface);

    AssertReturn(hr == D3D_OK, hr);

    return S_OK;
}

/*
 *
 * Public API.
 *
 */
HRESULT VBoxDxvaGetDeviceGuidCount(UINT *pcGuids)
{
    *pcGuids = RT_ELEMENTS(gaDeviceGuids);
    return S_OK;
}

HRESULT VBoxDxvaGetDeviceGuids(GUID *paGuids, UINT cbGuids)
{
    if (cbGuids >= sizeof(gaDeviceGuids))
    {
        memcpy(paGuids, &gaDeviceGuids[0], sizeof(gaDeviceGuids));
        return S_OK;
    }

    AssertFailed();
    return E_INVALIDARG;
}

HRESULT VBoxDxvaGetOutputFormatCount(UINT *pcFormats, DXVADDI_VIDEOPROCESSORINPUT const *pVPI, bool fSubstream)
{
    RT_NOREF(fSubstream);

    UINT cFormats = 0;
    if (pVPI)
    {
        LOGREL(("%dx%d", pVPI->VideoDesc.SampleWidth, pVPI->VideoDesc.SampleHeight));

        if (vboxDxvaFindDeviceGuid(pVPI->pVideoProcGuid) >= 0)
        {
            if (vboxDxvaFindInputFormat(pVPI->VideoDesc.Format) >= 0)
            {
                cFormats = RT_ELEMENTS(gaOutputFormats);
            }
        }
    }

    *pcFormats = cFormats;
    return S_OK;
}

HRESULT VBoxDxvaGetOutputFormats(D3DDDIFORMAT *paFormats, UINT cbFormats, DXVADDI_VIDEOPROCESSORINPUT const *pVPI, bool fSubstream)
{
    RT_NOREF(fSubstream);

    if (pVPI)
    {
        LOGREL(("%dx%d", pVPI->VideoDesc.SampleWidth, pVPI->VideoDesc.SampleHeight));

        if (vboxDxvaFindDeviceGuid(pVPI->pVideoProcGuid) >= 0)
        {
            if (vboxDxvaFindInputFormat(pVPI->VideoDesc.Format) >= 0)
            {
                if (cbFormats >= sizeof(gaOutputFormats))
                {
                    memcpy(paFormats, gaOutputFormats, sizeof(gaOutputFormats));
                    return S_OK;
                }
            }
        }
    }

    AssertFailed();
    return E_INVALIDARG;
}

HRESULT VBoxDxvaGetCaps(DXVADDI_VIDEOPROCESSORCAPS *pVideoProcessorCaps,
                        DXVADDI_VIDEOPROCESSORINPUT const *pVPI)
{
    RT_ZERO(*pVideoProcessorCaps);

    if (pVPI)
    {
        LOGREL(("%dx%d", pVPI->VideoDesc.SampleWidth, pVPI->VideoDesc.SampleHeight));

        if (vboxDxvaFindDeviceGuid(pVPI->pVideoProcGuid) >= 0)
        {
            if (vboxDxvaFindInputFormat(pVPI->VideoDesc.Format) >= 0)
            {
                pVideoProcessorCaps->InputPool                = D3DDDIPOOL_SYSTEMMEM;
                pVideoProcessorCaps->NumForwardRefSamples     = 0; /// @todo 1 for deinterlacing?
                pVideoProcessorCaps->NumBackwardRefSamples    = 0;
                pVideoProcessorCaps->OutputFormat             = D3DDDIFMT_X8R8G8B8;
                pVideoProcessorCaps->DeinterlaceTechnology    = DXVADDI_DEINTERLACETECH_UNKNOWN;
                pVideoProcessorCaps->ProcAmpControlCaps       = DXVADDI_PROCAMP_NONE; /// @todo Maybe some?
                pVideoProcessorCaps->VideoProcessorOperations = DXVADDI_VIDEOPROCESS_YUV2RGB
                                                              | DXVADDI_VIDEOPROCESS_STRETCHX
                                                              | DXVADDI_VIDEOPROCESS_STRETCHY
                                                              | DXVADDI_VIDEOPROCESS_YUV2RGBEXTENDED
                                                              | DXVADDI_VIDEOPROCESS_CONSTRICTION
                                                              | DXVADDI_VIDEOPROCESS_LINEARSCALING
                                                              | DXVADDI_VIDEOPROCESS_GAMMACOMPENSATED;
                pVideoProcessorCaps->NoiseFilterTechnology    = DXVADDI_NOISEFILTERTECH_UNSUPPORTED;
                pVideoProcessorCaps->DetailFilterTechnology   = DXVADDI_DETAILFILTERTECH_UNSUPPORTED;
                return S_OK;
            }
        }
    }

    AssertFailed();
    return E_INVALIDARG;
}

HRESULT VBoxDxvaCreateVideoProcessDevice(PVBOXWDDMDISP_DEVICE pDevice, D3DDDIARG_CREATEVIDEOPROCESSDEVICE *pData)
{
    /*
     * Do minimum here. Devices are created and destroyed without being used.
     */
    VBOXWDDMVIDEOPROCESSDEVICE *pVideoProcessDevice =
        (VBOXWDDMVIDEOPROCESSDEVICE *)RTMemAllocZ(sizeof(VBOXWDDMVIDEOPROCESSDEVICE));
    if (pVideoProcessDevice)
    {
        pVideoProcessDevice->pDevice            = pDevice;
        pVideoProcessDevice->VideoProcGuid      = *pData->pVideoProcGuid;
        pVideoProcessDevice->VideoDesc          = pData->VideoDesc;
        pVideoProcessDevice->RenderTargetFormat = pData->RenderTargetFormat;
        pVideoProcessDevice->MaxSubStreams      = pData->MaxSubStreams;

        pData->hVideoProcess = pVideoProcessDevice;
        return S_OK;
    }

    return E_OUTOFMEMORY;
}

HRESULT VBoxDxvaDestroyVideoProcessDevice(PVBOXWDDMDISP_DEVICE pDevice, HANDLE hVideoProcessor)
{
    RT_NOREF(pDevice);

    VBOXWDDMVIDEOPROCESSDEVICE *pVideoProcessDevice = (VBOXWDDMVIDEOPROCESSDEVICE *)hVideoProcessor;
    if (pVideoProcessDevice)
    {
        D3D_RELEASE(pVideoProcessDevice->pVertexDecl);
        D3D_RELEASE(pVideoProcessDevice->pVB);
        D3D_RELEASE(pVideoProcessDevice->pVS);
        D3D_RELEASE(pVideoProcessDevice->pPS);

        RTMemFree(pVideoProcessDevice);
    }
    return S_OK;
}

HRESULT VBoxDxvaVideoProcessBeginFrame(PVBOXWDDMDISP_DEVICE pDevice, HANDLE hVideoProcessor)
{
    VBOXWDDMVIDEOPROCESSDEVICE *pVideoProcessDevice = (VBOXWDDMVIDEOPROCESSDEVICE *)hVideoProcessor;
    AssertReturn(pDevice == pVideoProcessDevice->pDevice, E_INVALIDARG);

    HRESULT hr = S_OK;
    if (!pVideoProcessDevice->pTexture)
    {
        hr = vboxDxvaInit(pVideoProcessDevice);
    }

    return hr;
}

HRESULT VBoxDxvaVideoProcessEndFrame(PVBOXWDDMDISP_DEVICE pDevice, D3DDDIARG_VIDEOPROCESSENDFRAME *pData)
{
    VBOXWDDMVIDEOPROCESSDEVICE *pVideoProcessDevice = (VBOXWDDMVIDEOPROCESSDEVICE *)pData->hVideoProcess;
    AssertReturn(pDevice == pVideoProcessDevice->pDevice, E_INVALIDARG);
    return S_OK;
}

HRESULT VBoxDxvaSetVideoProcessRenderTarget(PVBOXWDDMDISP_DEVICE pDevice,
                                            const D3DDDIARG_SETVIDEOPROCESSRENDERTARGET *pData)
{
    VBOXWDDMVIDEOPROCESSDEVICE *pVideoProcessDevice = (VBOXWDDMVIDEOPROCESSDEVICE *)pData->hVideoProcess;
    AssertReturn(pDevice == pVideoProcessDevice->pDevice, E_INVALIDARG);

    PVBOXWDDMDISP_RESOURCE pRc = (PVBOXWDDMDISP_RESOURCE)pData->hRenderTarget;
    AssertReturn(pRc->cAllocations > pData->SubResourceIndex, E_INVALIDARG);

    pVideoProcessDevice->pRenderTarget = pRc;
    pVideoProcessDevice->SubResourceIndex = pData->SubResourceIndex;

    VBOXWDDMDISP_ALLOCATION *pAllocation = &pRc->aAllocations[pData->SubResourceIndex];
    AssertPtrReturn(pAllocation->pD3DIf, E_INVALIDARG);
    AssertReturn(pAllocation->enmD3DIfType == VBOXDISP_D3DIFTYPE_TEXTURE, E_INVALIDARG);

    pVideoProcessDevice->pRTTexture = (IDirect3DTexture9 *)pAllocation->pD3DIf;

    LOGREL_EXACT(("VideoProcess RT %dx%d sid=%u\n",
        pRc->aAllocations[0].SurfDesc.width, pRc->aAllocations[0].SurfDesc.height, pAllocation->hostID));

    return S_OK;
}

HRESULT VBoxDxvaVideoProcessBlt(PVBOXWDDMDISP_DEVICE pDevice, const D3DDDIARG_VIDEOPROCESSBLT *pData)
{
    HRESULT  hr;

    VBOXWDDMVIDEOPROCESSDEVICE *pVideoProcessDevice = (VBOXWDDMVIDEOPROCESSDEVICE *)pData->hVideoProcess;
    AssertReturn(pDevice == pVideoProcessDevice->pDevice, E_INVALIDARG);

    PVBOXWDDMDISP_RESOURCE pSrcRc = (PVBOXWDDMDISP_RESOURCE)pData->pSrcSurfaces[0].SrcResource;

    PVBOXWDDMDISP_RESOURCE pRc = (PVBOXWDDMDISP_RESOURCE)pData->pSrcSurfaces[0].SrcResource;
    AssertReturn(pRc->cAllocations > pData->pSrcSurfaces[0].SrcSubResourceIndex, E_INVALIDARG);

    VBOXWDDMDISP_ALLOCATION *pAllocation = &pRc->aAllocations[pData->pSrcSurfaces[0].SrcSubResourceIndex];
    AssertPtrReturn(pAllocation->pD3DIf, E_INVALIDARG);
    AssertReturn(pAllocation->enmD3DIfType == VBOXDISP_D3DIFTYPE_TEXTURE, E_INVALIDARG);

    IDirect3DTexture9 *pSrcTexture = (IDirect3DTexture9 *)pAllocation->pD3DIf;

    LOGREL_EXACT(("VideoProcess Blt sid = %u %d,%d %dx%d (%dx%d) -> %d,%d %dx%d (%d,%d %dx%d, %dx%d)\n",
        pAllocation->hostID,
        pData->pSrcSurfaces[0].SrcRect.left, pData->pSrcSurfaces[0].SrcRect.top,
        pData->pSrcSurfaces[0].SrcRect.right - pData->pSrcSurfaces[0].SrcRect.left,
        pData->pSrcSurfaces[0].SrcRect.bottom - pData->pSrcSurfaces[0].SrcRect.top,

        pSrcRc->aAllocations[0].SurfDesc.width,
        pSrcRc->aAllocations[0].SurfDesc.height,

        pData->pSrcSurfaces[0].DstRect.left, pData->pSrcSurfaces[0].DstRect.top,
        pData->pSrcSurfaces[0].DstRect.right - pData->pSrcSurfaces[0].DstRect.left,
        pData->pSrcSurfaces[0].DstRect.bottom - pData->pSrcSurfaces[0].DstRect.top,

        pData->TargetRect.left, pData->TargetRect.top,
        pData->TargetRect.right - pData->TargetRect.left, pData->TargetRect.bottom - pData->TargetRect.top,

        pVideoProcessDevice->pRenderTarget->aAllocations[0].SurfDesc.width,
        pVideoProcessDevice->pRenderTarget->aAllocations[0].SurfDesc.height));

    hr = vboxDxvaBltRect(pVideoProcessDevice, pSrcTexture, pData->pSrcSurfaces[0].SrcSubResourceIndex,
                         &pData->pSrcSurfaces[0].SrcRect,
                         &pData->pSrcSurfaces[0].DstRect);

    return S_OK;
}
