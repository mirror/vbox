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


DXGI_FORMAT vmsvgaDXSurfaceFormat2Dxgi(SVGA3dSurfaceFormat format)
{
    switch (format)
    {
        case SVGA3D_X8R8G8B8: return DXGI_FORMAT_B8G8R8A8_UNORM;
        default: break;
    }
    return DXGI_FORMAT_UNKNOWN;
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
        vmsvga3dReset(pThisCC);

        RTMemFree(pState->pBackend);
        pState->pBackend = NULL;
    }

    return VINF_SUCCESS;
}


int vmsvga3dReset(PVGASTATECC pThisCC)
{
    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    PVMSVGA3DBACKEND pBackend = pState->pBackend;
    if (pBackend)
    {
        D3D_RELEASE(pBackend->pDevice);
        D3D_RELEASE(pBackend->pImmediateContext);
        D3D_RELEASE(pBackend->pDxgiFactory);
    }

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


int vmsvga3dSurfaceMap(PVGASTATECC pThisCC, SVGA3dSurfaceImageId const *pImage, SVGA3dBox const *pBox,
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


int vmsvga3dSurfaceUnmap(PVGASTATECC pThisCC, SVGA3dSurfaceImageId const *pImage, VMSVGA3D_MAPPED_SURFACE *pMap, bool fWritten)
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
    td.Format             = DXGI_FORMAT_B8G8R8A8_UNORM;
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


int vmsvga3dScreenTargetBind(PVGASTATECC pThisCC, VMSVGASCREENOBJECT *pScreen, uint32_t sid)
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


int vmsvga3dScreenTargetUpdate(PVGASTATECC pThisCC, VMSVGASCREENOBJECT *pScreen, SVGA3dRect const *pRect)
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


int vmsvga3dQueryCaps(PVGASTATECC pThisCC, uint32_t idx3dCaps, uint32_t *pu32Val)
{
    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    int rc = VINF_SUCCESS;

    switch (idx3dCaps)
    {
    case SVGA3D_DEVCAP_3D:
        *pu32Val = 1;
        break;

    case SVGA3D_DEVCAP_MAX_TEXTURE_WIDTH:
        *pu32Val = 8192;
        break;

    case SVGA3D_DEVCAP_MAX_TEXTURE_HEIGHT:
        *pu32Val = 8192;
        break;

    case SVGA3D_DEVCAP_DX:
        *pu32Val = 1;
        break;

    default:
        *pu32Val = 0;
        rc = VERR_NOT_SUPPORTED;
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
