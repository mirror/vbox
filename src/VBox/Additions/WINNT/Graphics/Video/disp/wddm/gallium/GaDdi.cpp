/* $Id$ */
/** @file
 * WDDM D3DDDI callbacks implemented for the Gallium based driver.
 */

/*
 * Copyright (C) 2017 Oracle Corporation
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


/* Copy surface data from D3DPOOL_DEFAULT to D3DPOOL_SYSTEMMEM */
static HRESULT gaSurfaceCopyD2S(IDirect3DDevice9 *pDevice,
                                D3DDDIFORMAT srcFormat,
                                IDirect3DSurface9 *pSrcSurf,
                                const RECT *pSrcRect,
                                D3DDDIFORMAT dstFormat,
                                IDirect3DSurface9 *pDstSurf,
                                const POINT *pDstPoint)
{
    RT_NOREF(pDevice);

    AssertReturn(pSrcRect->right >= pSrcRect->left, E_NOTIMPL);
    AssertReturn(pSrcRect->bottom >= pSrcRect->top, E_NOTIMPL);
    AssertReturn(dstFormat == srcFormat, E_NOTIMPL);

    RECT dstRect;
    dstRect.left   = pDstPoint->x;
    dstRect.top    = pDstPoint->y;
    dstRect.right  = pDstPoint->x + (pSrcRect->right - pSrcRect->left);
    dstRect.bottom = pDstPoint->y + (pSrcRect->bottom - pSrcRect->top);

    D3DLOCKED_RECT SrcLockedRect;
    HRESULT hr = pSrcSurf->LockRect(&SrcLockedRect, pSrcRect, D3DLOCK_READONLY);
    Assert(hr == S_OK);
    if (SUCCEEDED(hr))
    {
        D3DLOCKED_RECT DstLockedRect;
        hr = pDstSurf->LockRect(&DstLockedRect, &dstRect, D3DLOCK_DISCARD);
        Assert(hr == S_OK);
        if (SUCCEEDED(hr))
        {
            const uint8_t *pu8Src = (uint8_t *)SrcLockedRect.pBits;
            uint8_t *pu8Dst = (uint8_t *)DstLockedRect.pBits;

            const uint32_t cbLine = vboxWddmCalcRowSize(pSrcRect->left, pSrcRect->right, srcFormat);
            const uint32_t cRows = vboxWddmCalcNumRows(pSrcRect->top, pSrcRect->bottom, srcFormat);
            for (uint32_t i = 0; i < cRows; ++i)
            {
                memcpy(pu8Dst, pu8Src, cbLine);
                pu8Src += SrcLockedRect.Pitch;
                pu8Dst += DstLockedRect.Pitch;
            }

            hr = pDstSurf->UnlockRect();
            Assert(hr == S_OK);
        }

        hr = pSrcSurf->UnlockRect();
        Assert(hr == S_OK);
    }

    return hr;
}


HRESULT APIENTRY GaDdiBlt(HANDLE hDevice, const D3DDDIARG_BLT *pData)
{
    VBOXVDBG_BREAK_DDI();

    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    vboxVDbgPrintF(("src %p[%d] %d,%d %d,%d dst %p[%d] %d,%d %d,%d ColorKey 0x%08X Flags 0x%08X\n",
                  pData->hSrcResource, pData->SrcSubResourceIndex,
                  pData->SrcRect.left, pData->SrcRect.top, pData->SrcRect.right, pData->SrcRect.bottom,
                  pData->hDstResource, pData->DstSubResourceIndex,
                  pData->DstRect.left, pData->DstRect.top, pData->DstRect.right, pData->DstRect.bottom,
                  pData->ColorKey, pData->Flags.Value));

    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    IDirect3DDevice9 *pDevice9If = VBOXDISP_D3DEV(pDevice);

    PVBOXWDDMDISP_RESOURCE pDstRc = (PVBOXWDDMDISP_RESOURCE)pData->hDstResource;
    PVBOXWDDMDISP_RESOURCE pSrcRc = (PVBOXWDDMDISP_RESOURCE)pData->hSrcResource;

    AssertReturn(pDstRc->cAllocations > pData->DstSubResourceIndex, E_INVALIDARG);
    AssertReturn(pSrcRc->cAllocations > pData->SrcSubResourceIndex, E_INVALIDARG);

    PVBOXWDDMDISP_ALLOCATION pSrcAlloc = &pSrcRc->aAllocations[pData->SrcSubResourceIndex];
//    PVBOXWDDMDISP_ALLOCATION pDstAlloc = &pDstRc->aAllocations[pData->DstSubResourceIndex];

    IDirect3DSurface9 *pSrcSurfIf = NULL;
    IDirect3DSurface9 *pDstSurfIf = NULL;

    HRESULT hr;
    hr = VBoxD3DIfSurfGet(pDstRc, pData->DstSubResourceIndex, &pDstSurfIf);
    Assert(hr == S_OK);
    if (hr == S_OK)
    {
        Assert(pDstSurfIf);

        hr = VBoxD3DIfSurfGet(pSrcRc, pData->SrcSubResourceIndex, &pSrcSurfIf);
        Assert(hr == S_OK);
        if (hr == S_OK)
        {
            Assert(pSrcSurfIf);

            /*
             * Use appropriate method depending on where the resource is allocated (system memory or default pool).
             * @todo Store the actual pool, where the resorce was created, in the VBOXWDDMDISP_RESOURCE structure.
             */
            const D3DPOOL poolSrc = vboxDDI2D3DPool(pSrcRc->RcDesc.enmPool);
            const D3DPOOL poolDst = vboxDDI2D3DPool(pDstRc->RcDesc.enmPool);

            if (poolSrc == D3DPOOL_SYSTEMMEM)
            {
                if (poolDst == D3DPOOL_SYSTEMMEM)
                {
                    /* D3DPOOL_SYSTEMMEM -> D3DPOOL_SYSTEMMEM
                     *
                     * "If both the source and destination allocations are in system memory,
                     * the driver should synchronize as necessary but should not copy the contents
                     * of the source surface. The Direct3D runtime copies the contents after the call
                     * to pfnRenderCb returns. "
                     *
                     * @todo Later, if necessary. memcpy?
                     */
                    AssertFailed();
                    hr = E_NOTIMPL;
                }
                else
                {
                    /* D3DPOOL_SYSTEMMEM -> D3DPOOL_DEFAULT
                     * UpdateSurface allows copying from memory to surface.
                     * @todo Stretching.
                     */
                    Assert(pData->DstRect.right - pData->DstRect.left == pData->SrcRect.right - pData->SrcRect.left);
                    Assert(pData->DstRect.bottom - pData->DstRect.top == pData->SrcRect.bottom - pData->SrcRect.top);

                    POINT pointDst;
                    pointDst.x = pData->DstRect.left;
                    pointDst.y = pData->DstRect.top;
                    hr = pDevice9If->UpdateSurface(pSrcSurfIf, &pData->SrcRect, pDstSurfIf, &pointDst);
                }
            }
            else
            {
                if (poolDst == D3DPOOL_SYSTEMMEM)
                {
                    /* D3DPOOL_DEFAULT -> D3DPOOL_SYSTEMMEM
                     * @todo GetRenderTargetData, and rectangle copy?
                     * Lock both and memcpy?
                     */
                    Assert(pData->DstRect.right - pData->DstRect.left == pData->SrcRect.right - pData->SrcRect.left);
                    Assert(pData->DstRect.bottom - pData->DstRect.top == pData->SrcRect.bottom - pData->SrcRect.top);

// seems to work                    AssertFailed(); /** @todo test this */

                    POINT pointDst;
                    pointDst.x = pData->DstRect.left;
                    pointDst.y = pData->DstRect.top;
                    hr = gaSurfaceCopyD2S(pDevice9If,
                                          pSrcRc->RcDesc.enmFormat, pSrcSurfIf, &pData->SrcRect,
                                          pDstRc->RcDesc.enmFormat, pDstSurfIf, &pointDst);
                }
                else
                {
                    /* D3DPOOL_DEFAULT -> D3DPOOL_DEFAULT
                     * Can use StretchRect.
                     */
                    const D3DTEXTUREFILTERTYPE Filter = vboxDDI2D3DBltFlags(pData->Flags);

                    /* we support only Point & Linear, we ignore [Begin|Continue|End]PresentToDwm */
                    Assert((pData->Flags.Value & (~(0x00000100 | 0x00000200 | 0x00000400 | 0x00000001  | 0x00000002))) == 0);

                    if (!pSrcRc->RcDesc.fFlags.RenderTarget || pDstRc->RcDesc.fFlags.RenderTarget)
                    {
                        /** @todo It seems that Gallium flips the image vertically if scaling is applied.
                         * In this case the SVGA driver draws a quad using the source as texture
                         * and apparently texture coords are set using the OpenGL coodinate system
                         * with the vertical axis going up for quad vertices, while in D3D the texture
                         * vertical axis goes down.
                         *
                         * The result is that StrechRect produces different results:
                         * - if scaling is required then the image will be flipped;
                         * - if scaling is NOT required then the image will be correct.
                         *
                         * Figure out who is to blame for the vertical flipping.
                         * At the moment NineDevice9_StretchRect includes VBox hack, see "Hack. Flip src Y."
                         */
                        hr = pDevice9If->StretchRect(pSrcSurfIf, &pData->SrcRect, pDstSurfIf, &pData->DstRect, Filter);
                    }
                    else
                    {
                        /* If src is a render target and destination is not, StretchRect will fail.
                         * Instead use a very slow path: GetRenderTargetData + UpdateSurface with a tmp surface.
                         */
                        const UINT Width       = pSrcAlloc->SurfDesc.width;
                        const UINT Height      = pSrcAlloc->SurfDesc.height;
                        const UINT Levels      = 1;
                        const DWORD Usage      = 0;
                        const D3DFORMAT Format = vboxDDI2D3DFormat(pSrcRc->RcDesc.enmFormat);
                        const D3DPOOL Pool     = D3DPOOL_SYSTEMMEM;
                        IDirect3DTexture9 *pTmpTexture;
                        hr = pDevice9If->CreateTexture(Width, Height, Levels, Usage, Format, Pool, &pTmpTexture, NULL);
                        Assert(hr == D3D_OK);
                        if (SUCCEEDED(hr))
                        {
                            IDirect3DSurface9 *pTmpSurface;
                            hr = pTmpTexture->GetSurfaceLevel(0, &pTmpSurface);
                            Assert(hr == D3D_OK);
                            if (SUCCEEDED(hr))
                            {
                                hr = pDevice9If->GetRenderTargetData(pSrcSurfIf, pTmpSurface);
                                Assert(hr == D3D_OK);
                                if (SUCCEEDED(hr))
                                {
                                    Assert(pData->DstRect.right - pData->DstRect.left == pData->SrcRect.right - pData->SrcRect.left);
                                    Assert(pData->DstRect.bottom - pData->DstRect.top == pData->SrcRect.bottom - pData->SrcRect.top);

                                    POINT pointDst;
                                    pointDst.x = pData->DstRect.left;
                                    pointDst.y = pData->DstRect.top;

                                    hr = pDevice9If->UpdateSurface(pTmpSurface, &pData->SrcRect, pDstSurfIf, &pointDst);
                                    Assert(hr == D3D_OK);
                                }

                                pTmpSurface->Release();
                            }

                            pTmpTexture->Release();
                        }
                    }
                }
            }

            pSrcSurfIf->Release();
        }

        pDstSurfIf->Release();
    }

    if (hr != S_OK)
    {
        /** @todo fallback to memcpy or whatever ? */
        Assert(0);
    }

    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p), hr(0x%x)\n", hDevice, hr));
    return hr;
}

HRESULT APIENTRY GaDdiTexBlt(HANDLE hDevice, const D3DDDIARG_TEXBLT *pData)
{
    VBOXVDBG_BREAK_DDI();

    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    vboxVDbgPrintF(("hDst %p, hSrc %p, face %d, dst %d,%d src %d,%d %d,%d\n",
                  pData->hDstResource,
                  pData->hSrcResource,
                  pData->CubeMapFace,
                  pData->DstPoint.x, pData->DstPoint.y,
                  pData->SrcRect.left, pData->SrcRect.top, pData->SrcRect.right, pData->SrcRect.bottom));

    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    IDirect3DDevice9 *pDevice9If = VBOXDISP_D3DEV(pDevice);

    PVBOXWDDMDISP_RESOURCE pDstRc = (PVBOXWDDMDISP_RESOURCE)pData->hDstResource;
    PVBOXWDDMDISP_RESOURCE pSrcRc = (PVBOXWDDMDISP_RESOURCE)pData->hSrcResource;

    Assert(    pDstRc->aAllocations[0].enmD3DIfType == VBOXDISP_D3DIFTYPE_TEXTURE
            || pDstRc->aAllocations[0].enmD3DIfType == VBOXDISP_D3DIFTYPE_CUBE_TEXTURE);
    Assert(    pSrcRc->aAllocations[0].enmD3DIfType == VBOXDISP_D3DIFTYPE_TEXTURE
            || pSrcRc->aAllocations[0].enmD3DIfType == VBOXDISP_D3DIFTYPE_CUBE_TEXTURE);
    Assert(pSrcRc->aAllocations[0].enmD3DIfType == pDstRc->aAllocations[0].enmD3DIfType);
    Assert(pSrcRc->RcDesc.enmPool == D3DDDIPOOL_SYSTEMMEM);
    Assert(pDstRc->RcDesc.enmPool != D3DDDIPOOL_SYSTEMMEM);

    HRESULT hr = S_OK;
/// @todo    VBOXVDBG_CHECK_SMSYNC(pDstRc);
/// @todo    VBOXVDBG_CHECK_SMSYNC(pSrcRc);

    if (   pSrcRc->aAllocations[0].SurfDesc.d3dWidth == pDstRc->aAllocations[0].SurfDesc.d3dWidth
        && pSrcRc->aAllocations[0].SurfDesc.height   == pDstRc->aAllocations[0].SurfDesc.height
        && pSrcRc->RcDesc.enmFormat == pDstRc->RcDesc.enmFormat
        && pData->DstPoint.x   == 0
        && pData->DstPoint.y   == 0
        && pData->SrcRect.left == 0
        && pData->SrcRect.top  == 0
        && pData->SrcRect.right - pData->SrcRect.left == (LONG)pSrcRc->aAllocations[0].SurfDesc.width
        && pData->SrcRect.bottom - pData->SrcRect.top == (LONG)pSrcRc->aAllocations[0].SurfDesc.height)
    {
        IDirect3DBaseTexture9 *pD3DIfSrcTex = (IDirect3DBaseTexture9*)pSrcRc->aAllocations[0].pD3DIf;
        IDirect3DBaseTexture9 *pD3DIfDstTex = (IDirect3DBaseTexture9*)pDstRc->aAllocations[0].pD3DIf;
        Assert(pD3DIfSrcTex);
        Assert(pD3DIfDstTex);
        VBOXVDBG_CHECK_TEXBLT(
                hr = pDevice9If->UpdateTexture(pD3DIfSrcTex, pD3DIfDstTex); Assert(hr == S_OK),
                pSrcRc,
                &pData->SrcRect,
                pDstRc,
                &pData->DstPoint);
    }
    else
    {
        Assert(pDstRc->aAllocations[0].enmD3DIfType != VBOXDISP_D3DIFTYPE_VOLUME_TEXTURE);
        Assert(pSrcRc->aAllocations[0].enmD3DIfType != VBOXDISP_D3DIFTYPE_VOLUME_TEXTURE);
        Assert(pDstRc->RcDesc.MipLevels == 1);
        Assert(pSrcRc->RcDesc.MipLevels == 1);

        /// @todo Miplevels
        IDirect3DSurface9 *pSrcSurfIf = NULL;
        IDirect3DSurface9 *pDstSurfIf = NULL;
        hr = VBoxD3DIfSurfGet(pDstRc, 0, &pDstSurfIf);
        Assert(hr == S_OK);
        if (hr == S_OK)
        {
            hr = VBoxD3DIfSurfGet(pSrcRc, 0, &pSrcSurfIf);
            Assert(hr == S_OK);
            if (hr == S_OK)
            {
                VBOXVDBG_CHECK_TEXBLT(hr = pDevice9If->UpdateSurface(pSrcSurfIf, &pData->SrcRect, pDstSurfIf, &pData->DstPoint);
                                      Assert(hr == S_OK),
                                      pSrcRc, &pData->SrcRect,
                                      pDstRc, &pData->DstPoint);
                pSrcSurfIf->Release();
            }
            pDstSurfIf->Release();
        }
    }

//    vboxWddmDalCheckAddRc(pDevice, pDstRc, TRUE);
//    vboxWddmDalCheckAddRc(pDevice, pSrcRc, FALSE);

    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p), hr(0x%x)\n", hDevice, hr));
    return hr;
}

static void wddmD3DBOXOrder(D3DBOX *pBox)
{
    UINT uTmp;
    if (pBox->Left > pBox->Right)
    {
        uTmp = pBox->Left;
        pBox->Left = pBox->Right;
        pBox->Right = uTmp;
    }
    if (pBox->Top > pBox->Bottom)
    {
        uTmp = pBox->Top;
        pBox->Top = pBox->Bottom;
        pBox->Bottom = uTmp;
    }
    if (pBox->Front > pBox->Back)
    {
        uTmp = pBox->Front;
        pBox->Front = pBox->Back;
        pBox->Back = uTmp;
    }
}

UINT wddmCoordDivBy2(UINT v)
{
    if (v > 0)
    {
        v >>= 1;
        if (v > 0)
            return v;
        return 1;
    }
    return 0;
}

void wddmD3DBoxDivBy2(D3DBOX *pBox)
{
    pBox->Left   = wddmCoordDivBy2(pBox->Left);
    pBox->Top    = wddmCoordDivBy2(pBox->Top);
    pBox->Right  = wddmCoordDivBy2(pBox->Right);
    pBox->Bottom = wddmCoordDivBy2(pBox->Bottom);
    pBox->Front  = wddmCoordDivBy2(pBox->Front);
    pBox->Back   = wddmCoordDivBy2(pBox->Back);
}

HRESULT APIENTRY GaDdiVolBlt(HANDLE hDevice, const D3DDDIARG_VOLUMEBLT *pData)
{
    VBOXVDBG_BREAK_DDI();

    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    vboxVDbgPrintF(("hDst %p, hSrc %p, dst %d,%d,%d src LT %d,%d RB %d,%d FB %d,%d\n",
                  pData->hDstResource,
                  pData->hSrcResource,
                  pData->DstX, pData->DstY, pData->DstZ,
                  pData->SrcBox.Left, pData->SrcBox.Top,
                  pData->SrcBox.Right, pData->SrcBox.Bottom,
                  pData->SrcBox.Front, pData->SrcBox.Back));

    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    IDirect3DDevice9 *pDevice9If = VBOXDISP_D3DEV(pDevice);

    PVBOXWDDMDISP_RESOURCE pDstRc = (PVBOXWDDMDISP_RESOURCE)pData->hDstResource;
    PVBOXWDDMDISP_RESOURCE pSrcRc = (PVBOXWDDMDISP_RESOURCE)pData->hSrcResource;

    Assert(pSrcRc->aAllocations[0].enmD3DIfType == VBOXDISP_D3DIFTYPE_VOLUME_TEXTURE);
    Assert(pDstRc->aAllocations[0].enmD3DIfType == VBOXDISP_D3DIFTYPE_VOLUME_TEXTURE);
    Assert(pSrcRc->cAllocations == pDstRc->cAllocations);
    Assert(pSrcRc->RcDesc.enmPool == D3DDDIPOOL_SYSTEMMEM);
    Assert(pDstRc->RcDesc.enmPool != D3DDDIPOOL_SYSTEMMEM);

    HRESULT hr = S_OK;

    RT_NOREF(pDevice9If);

    INT iWidth  = pData->SrcBox.Right - pData->SrcBox.Left;
    INT iHeight = pData->SrcBox.Bottom - pData->SrcBox.Top;
    INT iDepth  = pData->SrcBox.Back - pData->SrcBox.Front;

    D3DBOX srcBox;
    srcBox.Left   = pData->SrcBox.Left;
    srcBox.Top    = pData->SrcBox.Top;
    srcBox.Right  = pData->SrcBox.Right;
    srcBox.Bottom = pData->SrcBox.Bottom;
    srcBox.Front  = pData->SrcBox.Front;
    srcBox.Back   = pData->SrcBox.Back;
    wddmD3DBOXOrder(&srcBox);

    D3DBOX dstBox;
    dstBox.Left   = pData->DstX;
    dstBox.Top    = pData->DstY;
    dstBox.Right  = pData->DstX + iWidth;
    dstBox.Bottom = pData->DstY + iHeight;
    dstBox.Front  = pData->DstZ;
    dstBox.Back   = pData->DstZ + iDepth;
    wddmD3DBOXOrder(&dstBox);

    UINT Level;
    for (Level = 0; Level < pSrcRc->cAllocations; ++Level)
    {
        if (Level > 0)
        {
            /* Each next level is 2 times smaller. */
            iWidth  = wddmCoordDivBy2(iWidth);
            iHeight = wddmCoordDivBy2(iHeight);
            iDepth  = wddmCoordDivBy2(iDepth);
            wddmD3DBoxDivBy2(&srcBox);
            wddmD3DBoxDivBy2(&dstBox);
        }

        IDirect3DVolumeTexture9 *pSrcVolTex = (IDirect3DVolumeTexture9 *)pSrcRc->aAllocations[0].pD3DIf;
        D3DLOCKED_BOX srcLockedVolume;
        hr = pSrcVolTex->LockBox(Level, &srcLockedVolume, &srcBox, D3DLOCK_READONLY);
        Assert(hr == S_OK);
        if (SUCCEEDED(hr))
        {
            IDirect3DVolumeTexture9 *pDstVolTex = (IDirect3DVolumeTexture9 *)pDstRc->aAllocations[0].pD3DIf;
            D3DLOCKED_BOX dstLockedVolume;
            hr = pDstVolTex->LockBox(Level, &dstLockedVolume, &dstBox, D3DLOCK_DISCARD);
            Assert(hr == S_OK);
            if (SUCCEEDED(hr))
            {
                const UINT cbLine = vboxWddmCalcRowSize(srcBox.Left, srcBox.Right, pSrcRc->RcDesc.enmFormat);
                uint8_t *pu8Dst = (uint8_t *)dstLockedVolume.pBits;
                const uint8_t *pu8Src = (uint8_t *)srcLockedVolume.pBits;
                for (INT d = 0; d < iDepth; ++d)
                {
                    uint8_t *pu8RowDst = pu8Dst;
                    const uint8_t *pu8RowSrc = pu8Src;
                    const UINT cRows = vboxWddmCalcNumRows(0, iHeight, pSrcRc->RcDesc.enmFormat);
                    for (UINT h = 0; h < cRows; ++h)
                    {
                        memcpy(pu8RowDst, pu8RowSrc, cbLine);
                        pu8RowDst += dstLockedVolume.RowPitch;
                        pu8RowSrc += srcLockedVolume.RowPitch;
                    }
                    pu8Dst += dstLockedVolume.SlicePitch;
                    pu8Src += srcLockedVolume.SlicePitch;
                }

                hr = pDstVolTex->UnlockBox(Level);
                Assert(hr == S_OK);
            }
            hr = pSrcVolTex->UnlockBox(Level);
            Assert(hr == S_OK);
        }
    }

    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p), hr(0x%x)\n", hDevice, hr));
    return hr;
}

HRESULT APIENTRY GaDdiFlush(HANDLE hDevice)
{
    VBOXVDBG_BREAK_DDI();

    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));

    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    IDirect3DDevice9 *pDevice9If = pDevice->pDevice9If;

    HRESULT hr = S_OK;
    if (   VBOXDISPMODE_IS_3D(pDevice->pAdapter)
        && pDevice9If) /* Windows 10 can call the Flush when pDevice9If is not yet initialized. */
    {
#if 1
        /* Flush the Gallium pipe. */
        IGaDirect3DDevice9Ex *pGaD3DDevice9Ex = NULL;
        HRESULT hr2 = pDevice9If->QueryInterface(IID_IGaDirect3DDevice9Ex, (void**)&pGaD3DDevice9Ex);
        if (SUCCEEDED(hr2))
        {
            hr2 = pGaD3DDevice9Ex->GaFlush();

            pGaD3DDevice9Ex->Release();
        }
#else
        /** @todo remove. Test code for D3DQUERYTYPE_EVENT, which does the flush too. */
        IDirect3DQuery9 *pQuery;
        hr = pDevice9If->CreateQuery(D3DQUERYTYPE_EVENT, &pQuery);
        if (SUCCEEDED(hr))
        {
            hr = pQuery->Issue(D3DISSUE_END);
            while (pQuery->GetData(NULL, 0, D3DGETDATA_FLUSH) == S_FALSE);
            pQuery->Release();
        }
#endif

        VBOXVDBG_DUMP_FLUSH(pDevice);
    }

    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p), hr(0x%x)\n", hDevice, hr));
    return hr;
}

HRESULT APIENTRY GaDdiPresent(HANDLE hDevice, const D3DDDIARG_PRESENT *pData)
{
    VBOXVDBG_BREAK_DDI();

    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));

    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    IDirect3DDevice9 *pDevice9If = VBOXDISP_D3DEV(pDevice);
    PVBOXWDDMDISP_RESOURCE pSrcRc = NULL;
    PVBOXWDDMDISP_RESOURCE pDstRc = NULL;
    PVBOXWDDMDISP_ALLOCATION pSrcAlloc = NULL;
    PVBOXWDDMDISP_ALLOCATION pDstAlloc = NULL;

    if (pData->hSrcResource)
    {
        pSrcRc = (PVBOXWDDMDISP_RESOURCE)pData->hSrcResource;
        Assert(pSrcRc->cAllocations > pData->SrcSubResourceIndex);
        pSrcAlloc = &pSrcRc->aAllocations[pData->SrcSubResourceIndex];
        Assert(pSrcAlloc->hAllocation);
    }

    if (pData->hDstResource)
    {
        pDstRc = (PVBOXWDDMDISP_RESOURCE)pData->hDstResource;
        Assert(pDstRc->cAllocations > pData->DstSubResourceIndex);
        pDstAlloc = &pDstRc->aAllocations[pData->DstSubResourceIndex];
        Assert(pDstAlloc->hAllocation);
    }

    IGaDirect3DDevice9Ex *pGaD3DDevice9Ex = NULL;
    HRESULT hr = pDevice9If->QueryInterface(IID_IGaDirect3DDevice9Ex, (void**)&pGaD3DDevice9Ex);
    if (SUCCEEDED(hr))
    {
        /* Query DdiPresent.hContext for this device. */
        HANDLE hContext = 0;
        hr = pGaD3DDevice9Ex->GaWDDMContextHandle(&hContext);
        Assert(hr == S_OK);
        if (SUCCEEDED(hr))
        {
            HRESULT hr2 = pGaD3DDevice9Ex->GaFlush();
            Assert(hr2 == S_OK);
            RT_NOREF(hr2);
        }

        pGaD3DDevice9Ex->Release();

        if (SUCCEEDED(hr))
        {
            D3DDDICB_PRESENT DdiPresent;
            RT_ZERO(DdiPresent);
            DdiPresent.hSrcAllocation = pSrcAlloc ? pSrcAlloc->hAllocation : 0;
            DdiPresent.hDstAllocation = pDstAlloc ? pDstAlloc->hAllocation : 0;
            DdiPresent.hContext       = hContext;

            hr = pDevice->RtCallbacks.pfnPresentCb(pDevice->hDevice, &DdiPresent);
            Assert(hr == S_OK);
        }
    }
    else
    {
        AssertFailed();
    }

    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p), hr(0x%x)\n", hDevice, hr));
    return hr;
}

HRESULT APIENTRY GaDdiLock(HANDLE hDevice, D3DDDIARG_LOCK *pData)
{
    VBOXVDBG_BREAK_DDI();

    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p) hResource 0x%p[%d] flags 0x%08X\n",
                    hDevice, pData->hResource, pData->SubResourceIndex, pData->Flags.Value));

    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    PVBOXWDDMDISP_RESOURCE pRc = (PVBOXWDDMDISP_RESOURCE)pData->hResource;
    AssertReturn(pData->SubResourceIndex < pRc->cAllocations, E_INVALIDARG);

    HRESULT hr = S_OK;

    /*
     * Memory buffers for D3DDDIPOOL_SYSTEMMEM resources are allocated by Windows (pAlloc->pvMem).
     * Normally the Gallium D3D backend (Nine state stracker) also has its own memory for
     * corresponding D3D resources.
     * The driver must synchronize these memory buffers:
     *  - copy from the backend to Windows buffer on Lock;
     *  - copy from Windows to the backend buffer on Unlock.
     *
     * However for textures and cube textures we can use Gallium backend feature:
     * the shared handle of a D3DPOOL_SYSTEMMEM is the pointer to the actual memory buffer.
     * So we create texture and cube texture resources for D3DDDIPOOL_SYSTEMMEM with
     * pSharedHandle set to pAlloc->pvMem (the buffer has the same layout as the Gallium one).
     * See GaD3DIfCreateForRc. There is no need to sync in this case.
     *
     * This is how D3DDDIPOOL_SYSTEMMEM resource synchronization is handled:
     *   Index and vertex buffers - copy data on lock/unlock.
     *   Textures - set the shared handle to pAlloc->pvMem. No sync required.
     *   Cube textures - set the shared handle to pAlloc->pvMem, because VBox version of Nine
     *                   implements SYSTEMMEM shared cube textures. No sync required.
     *   Volume textures - GaD3DResourceSynchMem. Possibly have to implement SharedHandle support in Nine.
     *   Surfaces - these should not reallly be in D3DDDIPOOL_SYSTEMMEM, so GaD3DResourceSynchMem.
     *
     * NotifyOnly flag is set for D3DDDIPOOL_SYSTEMMEM locks/unlocks:
     * "... for preallocated system memory surfaces, the runtime ignores the driver-set memory and pitch, ... .
     * The runtime sets the NotifyOnly bit-field flag in the Flags member of the D3DDDIARG_LOCK structure
     * to differentiate Lock calls that lock preallocated system memory surfaces from other Lock calls."
     *
     * It turned out that Windows always passes pData->SubResourceIndex == 0 for
     * NotifyOnly locks when locking a textures, cubemaps and volumes.
     * So the GaD3DResourceSynchMem must sync all subresources in this case.
     */

    PVBOXWDDMDISP_ALLOCATION pAlloc = &pRc->aAllocations[pData->SubResourceIndex];
    const VBOXDISP_D3DIFTYPE enmD3DIfType = pAlloc->enmD3DIfType;
    const DWORD dwD3DLockFlags = vboxDDI2D3DLockFlags(pData->Flags);

    if (VBOXDISPMODE_IS_3D(pDevice->pAdapter))
    {
        if (pData->Flags.NotifyOnly)
        {
            Assert(pAlloc->pvMem);
            Assert(pRc->RcDesc.enmPool == D3DDDIPOOL_SYSTEMMEM);

            if (   enmD3DIfType == VBOXDISP_D3DIFTYPE_SURFACE
                || enmD3DIfType == VBOXDISP_D3DIFTYPE_VOLUME_TEXTURE)
            {
                /* Brute-force. */
                if (pAlloc->LockInfo.cLocks == 0)
                {
                    vboxVDbgPrintF((__FUNCTION__", sync from backend\n"));
                    GaD3DResourceSynchMem(pRc, /* fToBackend */ false);
                }
            }

            //VBOXVDBG_CHECK_SMSYNC(pRc);
        }
        else
        {
            Assert(!pAlloc->pvMem);
            Assert(pRc->RcDesc.enmPool != D3DDDIPOOL_SYSTEMMEM);
        }

        if (   enmD3DIfType == VBOXDISP_D3DIFTYPE_TEXTURE
            || enmD3DIfType == VBOXDISP_D3DIFTYPE_CUBE_TEXTURE
            || enmD3DIfType == VBOXDISP_D3DIFTYPE_SURFACE)
        {
            Assert(pAlloc->pD3DIf);
            IDirect3DTexture9 *pD3DIfTex = (IDirect3DTexture9*)pAlloc->pD3DIf;
            IDirect3DCubeTexture9 *pD3DIfCubeTex = (IDirect3DCubeTexture9*)pAlloc->pD3DIf;
            IDirect3DSurface9 *pD3DIfSurface = (IDirect3DSurface9*)pAlloc->pD3DIf;

            Assert(!pData->Flags.RangeValid);
            Assert(!pData->Flags.BoxValid);

            RECT *pRect = NULL;
            if (pData->Flags.AreaValid)
            {
                pRect = &pData->Area;
            }
            /* else - we lock the entire texture, pRect == NULL */

            BOOL fNeedLock = TRUE;
            if (pAlloc->LockInfo.cLocks)
            {
                /* Can happen.
                 * It is OK to lock buffers again, but Gallium backend does not allow nested locking for anything else.
                 */
                Assert(pAlloc->LockInfo.LockedRect.pBits);

                bool fSameLock =   (pAlloc->LockInfo.fFlags.ReadOnly == pData->Flags.ReadOnly)
                                && (pAlloc->LockInfo.fFlags.AreaValid == pData->Flags.AreaValid);
                if (fSameLock && pAlloc->LockInfo.fFlags.AreaValid)
                {
                    fSameLock =   fSameLock
                               && (pAlloc->LockInfo.Area.left == pData->Area.left)
                               && (pAlloc->LockInfo.Area.top == pData->Area.top)
                               && (pAlloc->LockInfo.Area.right == pData->Area.right)
                               && (pAlloc->LockInfo.Area.bottom == pData->Area.bottom);
                }

                if (!fSameLock)
                {
                    switch (enmD3DIfType)
                    {
                        case VBOXDISP_D3DIFTYPE_TEXTURE:
                            hr = pD3DIfTex->UnlockRect(pData->SubResourceIndex);
                            break;
                        case VBOXDISP_D3DIFTYPE_CUBE_TEXTURE:
                            hr = pD3DIfCubeTex->UnlockRect(VBOXDISP_CUBEMAP_INDEX_TO_FACE(pRc, pData->SubResourceIndex),
                                    VBOXDISP_CUBEMAP_INDEX_TO_LEVEL(pRc, pData->SubResourceIndex));
                            break;
                        case VBOXDISP_D3DIFTYPE_SURFACE:
                            hr = pD3DIfSurface->UnlockRect();
                            break;
                        default:
                            AssertFailed();
                            break;
                    }
                    Assert(hr == S_OK);
                }
                else
                {
                    fNeedLock = FALSE;
                }
            }

            if (fNeedLock && SUCCEEDED(hr))
            {
                pAlloc->LockInfo.fFlags = pData->Flags;
                if (pRect)
                {
                    pAlloc->LockInfo.Area = *pRect;
                    Assert(pAlloc->LockInfo.fFlags.AreaValid == 1);
                }
                else
                {
                    Assert(pAlloc->LockInfo.fFlags.AreaValid == 0);
                }

                switch (enmD3DIfType)
                {
                    case VBOXDISP_D3DIFTYPE_TEXTURE:
                        hr = pD3DIfTex->LockRect(pData->SubResourceIndex,
                                &pAlloc->LockInfo.LockedRect,
                                pRect,
                                dwD3DLockFlags);
                        break;
                    case VBOXDISP_D3DIFTYPE_CUBE_TEXTURE:
                        hr = pD3DIfCubeTex->LockRect(VBOXDISP_CUBEMAP_INDEX_TO_FACE(pRc, pData->SubResourceIndex),
                                VBOXDISP_CUBEMAP_INDEX_TO_LEVEL(pRc, pData->SubResourceIndex),
                                &pAlloc->LockInfo.LockedRect,
                                pRect,
                                dwD3DLockFlags);
                        break;
                    case VBOXDISP_D3DIFTYPE_SURFACE:
                        hr = pD3DIfSurface->LockRect(&pAlloc->LockInfo.LockedRect,
                                pRect,
                                dwD3DLockFlags);
                        break;
                    default:
                        AssertFailed();
                        break;
                }

                if (FAILED(hr))
                {
                    WARN(("LockRect failed, hr %x", hr));
                }
            }

            if (SUCCEEDED(hr))
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

                VBOXVDBG_DUMP_LOCK_ST(pData);

                hr = S_OK;
            }
        }
        else if (enmD3DIfType == VBOXDISP_D3DIFTYPE_VOLUME_TEXTURE)
        {
            Assert(pAlloc->pD3DIf);
            IDirect3DVolumeTexture9 *pD3DIfTex = (IDirect3DVolumeTexture9*)pAlloc->pD3DIf;

            Assert(!pData->Flags.AreaValid);
            Assert(!pData->Flags.RangeValid);

            D3DDDIBOX *pBox = NULL;
            if (pData->Flags.BoxValid)
            {
                pBox = &pData->Box;
            }
            /* else - we lock the entire texture, pBox == NULL */

            BOOL fNeedLock = TRUE;
            if (pAlloc->LockInfo.cLocks)
            {
                Assert(pAlloc->LockInfo.LockedBox.pBits);

                bool fSameLock =   (pAlloc->LockInfo.fFlags.ReadOnly == pData->Flags.ReadOnly)
                                && (pAlloc->LockInfo.fFlags.BoxValid == pData->Flags.BoxValid);
                if (fSameLock && pAlloc->LockInfo.fFlags.BoxValid)
                {
                    fSameLock =   fSameLock
                               && (pAlloc->LockInfo.Box.Left == pData->Box.Left)
                               && (pAlloc->LockInfo.Box.Top == pData->Box.Top)
                               && (pAlloc->LockInfo.Box.Right == pData->Box.Right)
                               && (pAlloc->LockInfo.Box.Bottom == pData->Box.Bottom)
                               && (pAlloc->LockInfo.Box.Front == pData->Box.Front)
                               && (pAlloc->LockInfo.Box.Back == pData->Box.Back);
                }

                if (!fSameLock)
                {
                    hr = pD3DIfTex->UnlockBox(pData->SubResourceIndex);
                    Assert(hr == S_OK);
                }
                else
                {
                    fNeedLock = FALSE;
                }
            }

            if (fNeedLock && SUCCEEDED(hr))
            {
                pAlloc->LockInfo.fFlags = pData->Flags;
                if (pBox)
                {
                    pAlloc->LockInfo.Box = *pBox;
                    Assert(pAlloc->LockInfo.fFlags.BoxValid == 1);
                }
                else
                {
                    Assert(pAlloc->LockInfo.fFlags.BoxValid == 0);
                }

                hr = pD3DIfTex->LockBox(pData->SubResourceIndex,
                                &pAlloc->LockInfo.LockedBox,
                                (D3DBOX *)pBox,
                                dwD3DLockFlags);
                if (FAILED(hr))
                {
                    WARN(("LockRect failed, hr", hr));
                }
            }

            if (SUCCEEDED(hr))
            {
                ++pAlloc->LockInfo.cLocks;

                if (!pData->Flags.NotifyOnly)
                {
                    pData->pSurfData = pAlloc->LockInfo.LockedBox.pBits;
                    pData->Pitch = pAlloc->LockInfo.LockedBox.RowPitch;
                    pData->SlicePitch = pAlloc->LockInfo.LockedBox.SlicePitch;
                    Assert(!pAlloc->pvMem);
                }

                VBOXVDBG_DUMP_LOCK_ST(pData);

                hr = S_OK;
            }
        }
        else if (enmD3DIfType == VBOXDISP_D3DIFTYPE_VERTEXBUFFER)
        {
            Assert(pAlloc->pD3DIf);
            IDirect3DVertexBuffer9 *pD3D9VBuf = (IDirect3DVertexBuffer9*)pAlloc->pD3DIf;

            Assert(!pData->Flags.AreaValid);
            Assert(!pData->Flags.BoxValid);

            D3DDDIRANGE *pRange = NULL;
            if (pData->Flags.RangeValid)
            {
                pRange = &pData->Range;
            }
            /* else - we lock the entire vertex buffer, pRange == NULL */

            bool bLocked = false;
            if (!pAlloc->LockInfo.cLocks)
            {
                if (!pData->Flags.MightDrawFromLocked || (!pData->Flags.Discard && !pData->Flags.NoOverwrite))
                {
                    hr = pD3D9VBuf->Lock(pRange ? pRange->Offset : 0,
                                          pRange ? pRange->Size : 0,
                                          &pAlloc->LockInfo.LockedRect.pBits,
                                          dwD3DLockFlags);
                    bLocked = true;
                }

                Assert(hr == S_OK);
                if (hr == S_OK)
                {
                    Assert(pAlloc->SurfDesc.pitch == pAlloc->SurfDesc.width);
                    pAlloc->LockInfo.LockedRect.Pitch = pAlloc->SurfDesc.pitch;
                    pAlloc->LockInfo.fFlags = pData->Flags;
                    if (pRange)
                    {
                        pAlloc->LockInfo.Range = *pRange;
                        Assert(pAlloc->LockInfo.fFlags.RangeValid == 1);
                    }
                    else
                    {
                        Assert(pAlloc->LockInfo.fFlags.RangeValid == 0);
                    }
                }
            }
            else
            {
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
                        VBoxD3DIfLockUnlockMemSynch(pAlloc, &pAlloc->LockInfo.LockedRect, pr, false /*bool bToLockInfo*/);
                    }
                }

            }
        }
        else if (enmD3DIfType == VBOXDISP_D3DIFTYPE_INDEXBUFFER)
        {
            Assert(pAlloc->pD3DIf);
            IDirect3DIndexBuffer9 *pD3D9IBuf = (IDirect3DIndexBuffer9*)pAlloc->pD3DIf;

            Assert(!pData->Flags.AreaValid);
            Assert(!pData->Flags.BoxValid);

            D3DDDIRANGE *pRange = NULL;
            if (pData->Flags.RangeValid)
            {
                pRange = &pData->Range;
            }
            /* else - we lock the entire vertex buffer, pRect == NULL */

            bool bLocked = false;
            if (!pAlloc->LockInfo.cLocks)
            {
                if (!pData->Flags.MightDrawFromLocked || (!pData->Flags.Discard && !pData->Flags.NoOverwrite))
                {
                    hr = pD3D9IBuf->Lock(pRange ? pRange->Offset : 0,
                                          pRange ? pRange->Size : 0,
                                          &pAlloc->LockInfo.LockedRect.pBits,
                                          dwD3DLockFlags);
                    bLocked = true;
                }

                Assert(hr == S_OK);
                if (hr == S_OK)
                {
                    Assert(pAlloc->SurfDesc.pitch == pAlloc->SurfDesc.width);
                    pAlloc->LockInfo.LockedRect.Pitch = pAlloc->SurfDesc.pitch;
                    pAlloc->LockInfo.fFlags = pData->Flags;
                    if (pRange)
                    {
                        pAlloc->LockInfo.Range = *pRange;
                        Assert(pAlloc->LockInfo.fFlags.RangeValid == 1);
                    }
                    else
                    {
                        Assert(pAlloc->LockInfo.fFlags.RangeValid == 0);
                    }
                }
            }
            else
            {
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
                        VBoxD3DIfLockUnlockMemSynch(pAlloc, &pAlloc->LockInfo.LockedRect, pr, false /*bool bToLockInfo*/);
                    }
                }
            }
        }
        else
        {
            WARN(("not implemented %d", enmD3DIfType));
        }
    }
    else /* if !VBOXDISPMODE_IS_3D(pDevice->pAdapter) */
    {
        if (pAlloc->hAllocation)
        {
            if (pRc->RcDesc.enmPool != D3DDDIPOOL_SYSTEMMEM)
            {
                D3DDDICB_LOCK LockData;
                LockData.hAllocation = pAlloc->hAllocation;
                LockData.PrivateDriverData = 0;
                LockData.NumPages = 0;
                LockData.pPages = NULL;
                LockData.pData = NULL; /* out */
                LockData.Flags.Value = 0;
                LockData.Flags.Discard = pData->Flags.Discard;
                LockData.Flags.DonotWait = pData->Flags.DoNotWait;

                uintptr_t offset;
                if (pData->Flags.AreaValid)
                {
                    offset = vboxWddmCalcOffXYrd(pData->Area.left, pData->Area.top, pAlloc->SurfDesc.pitch,
                                                 pAlloc->SurfDesc.format);
                }
                else if (pData->Flags.RangeValid)
                {
                    offset = pData->Range.Offset;
                }
                else if (pData->Flags.BoxValid)
                {
                    vboxVDbgPrintF((__FUNCTION__": Implement Box area"));
                    Assert(0);
                    offset = 0;
                }
                else
                {
                    offset = 0;
                }

                hr = pDevice->RtCallbacks.pfnLockCb(pDevice->hDevice, &LockData);
                Assert(hr == S_OK || (hr == D3DERR_WASSTILLDRAWING && pData->Flags.DoNotWait));
                if (hr == S_OK)
                {
                    pData->pSurfData = ((uint8_t*)LockData.pData) + offset;
                    pData->Pitch = pAlloc->SurfDesc.pitch;
                    pData->SlicePitch = pAlloc->SurfDesc.slicePitch;

                    if (pData->Flags.Discard)
                    {
                        /* check if the surface was renamed */
                        if (LockData.hAllocation)
                            pAlloc->hAllocation = LockData.hAllocation;
                    }
                }
            }
            /* else - d3d may create sysmem render targets and call our Present callbacks for those
             * to make it work properly we need to create a VRAM surface corresponding to sysmem one
             * and copy stuff to VRAM on lock/unlock
             *
             * so we don't do any locking here, but still track the lock info here
             * and do lock-memcopy-unlock to VRAM surface on sysmem surface unlock
             * */

            if (hr == S_OK)
            {
                Assert(!pAlloc->LockInfo.cLocks);

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

                ++pAlloc->LockInfo.cLocks;
            }
        }
    }

    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p), hr(0x%x)\n", hDevice, hr));
    return hr;
}

HRESULT APIENTRY GaDdiUnlock(HANDLE hDevice, const D3DDDIARG_UNLOCK *pData)
{
    VBOXVDBG_BREAK_DDI();

    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p) hResource 0x%p[%d]\n",
                    hDevice, pData->hResource, pData->SubResourceIndex));

    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    PVBOXWDDMDISP_RESOURCE pRc = (PVBOXWDDMDISP_RESOURCE)pData->hResource;
    AssertReturn(pData->SubResourceIndex < pRc->cAllocations, E_INVALIDARG);

    HRESULT hr = S_OK;

    PVBOXWDDMDISP_ALLOCATION pAlloc = &pRc->aAllocations[pData->SubResourceIndex];
    const VBOXDISP_D3DIFTYPE enmD3DIfType = pAlloc->enmD3DIfType;

    if (VBOXDISPMODE_IS_3D(pDevice->pAdapter))
    {
        if (   enmD3DIfType == VBOXDISP_D3DIFTYPE_TEXTURE
            || enmD3DIfType == VBOXDISP_D3DIFTYPE_CUBE_TEXTURE
            || enmD3DIfType == VBOXDISP_D3DIFTYPE_SURFACE)
        {
            VBOXVDBG_DUMP_UNLOCK_ST(pData);

            --pAlloc->LockInfo.cLocks;
            Assert(pAlloc->LockInfo.cLocks < UINT32_MAX);
            if (!pAlloc->LockInfo.cLocks)
            {
                Assert(pAlloc->pD3DIf);
                switch (enmD3DIfType)
                {
                    case VBOXDISP_D3DIFTYPE_TEXTURE:
                    {
                        IDirect3DTexture9 *pD3DIfTex = (IDirect3DTexture9*)pAlloc->pD3DIf;
                        hr = pD3DIfTex->UnlockRect(pData->SubResourceIndex);
                        break;
                    }
                    case VBOXDISP_D3DIFTYPE_CUBE_TEXTURE:
                    {
                        IDirect3DCubeTexture9 *pD3DIfCubeTex = (IDirect3DCubeTexture9*)pAlloc->pD3DIf;
                        hr = pD3DIfCubeTex->UnlockRect(VBOXDISP_CUBEMAP_INDEX_TO_FACE(pRc, pData->SubResourceIndex),
                                VBOXDISP_CUBEMAP_INDEX_TO_LEVEL(pRc, pData->SubResourceIndex));
                        break;
                    }
                    case VBOXDISP_D3DIFTYPE_SURFACE:
                    {
                        IDirect3DSurface9 *pD3DIfSurf = (IDirect3DSurface9*)pAlloc->pD3DIf;
                        hr = pD3DIfSurf->UnlockRect();
                        break;
                    }
                    default:
                        AssertFailed();
                        break;
                }
                Assert(hr == S_OK);
            }
        }
        else if (enmD3DIfType == VBOXDISP_D3DIFTYPE_VOLUME_TEXTURE)
        {
            VBOXVDBG_DUMP_UNLOCK_ST(pData);

            --pAlloc->LockInfo.cLocks;
            Assert(pAlloc->LockInfo.cLocks < UINT32_MAX);
            if (!pAlloc->LockInfo.cLocks)
            {
                Assert(pAlloc->pD3DIf);
                IDirect3DVolumeTexture9 *pD3DIfTex = (IDirect3DVolumeTexture9*)pAlloc->pD3DIf;
                hr = pD3DIfTex->UnlockBox(pData->SubResourceIndex);
                Assert(hr == S_OK);
            }
        }
        else if (enmD3DIfType == VBOXDISP_D3DIFTYPE_VERTEXBUFFER)
        {
            --pAlloc->LockInfo.cLocks;
            Assert(pAlloc->LockInfo.cLocks < UINT32_MAX);
            if (!pAlloc->LockInfo.cLocks
                && (!pAlloc->LockInfo.fFlags.MightDrawFromLocked
                    || (!pAlloc->LockInfo.fFlags.Discard && !pAlloc->LockInfo.fFlags.NoOverwrite)))
            {
                Assert(pAlloc->pD3DIf);
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
                    VBoxD3DIfLockUnlockMemSynch(pAlloc, &pAlloc->LockInfo.LockedRect,
                            pr,
                            true /*bool bToLockInfo*/);
                }
                IDirect3DVertexBuffer9 *pD3D9VBuf = (IDirect3DVertexBuffer9*)pAlloc->pD3DIf;
                hr = pD3D9VBuf->Unlock();
                Assert(hr == S_OK);
            }
        }
        else if (enmD3DIfType == VBOXDISP_D3DIFTYPE_INDEXBUFFER)
        {
            --pAlloc->LockInfo.cLocks;
            Assert(pAlloc->LockInfo.cLocks < UINT32_MAX);
            if (!pAlloc->LockInfo.cLocks
                && (!pAlloc->LockInfo.fFlags.MightDrawFromLocked
                    || (!pAlloc->LockInfo.fFlags.Discard && !pAlloc->LockInfo.fFlags.NoOverwrite)))
            {
                Assert(pAlloc->pD3DIf);
                IDirect3DIndexBuffer9 *pD3D9IBuf = (IDirect3DIndexBuffer9*)pAlloc->pD3DIf;
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
                    VBoxD3DIfLockUnlockMemSynch(pAlloc, &pAlloc->LockInfo.LockedRect,
                            pr,
                            true /*bool bToLockInfo*/);
                }
                hr = pD3D9IBuf->Unlock();
                Assert(hr == S_OK);
            }
        }
        else
        {
            WARN(("Unlock unsupported %d", pRc->aAllocations[0].enmD3DIfType));
        }

        if (hr == S_OK)
        {
            if (pData->Flags.NotifyOnly)
            {
                Assert(pAlloc->pvMem);
                Assert(pRc->RcDesc.enmPool == D3DDDIPOOL_SYSTEMMEM);

                if (   enmD3DIfType == VBOXDISP_D3DIFTYPE_SURFACE
                    || enmD3DIfType == VBOXDISP_D3DIFTYPE_VOLUME_TEXTURE)
                {
                    /* Brute-force. */
                    if (pAlloc->LockInfo.cLocks == 0)
                    {
                        vboxVDbgPrintF((__FUNCTION__", sync to backend\n"));
                        GaD3DResourceSynchMem(pRc, /* fToBackend */ true);
                    }
                }

                //VBOXVDBG_CHECK_SMSYNC(pRc);
            }
            else
            {
                Assert(!pAlloc->pvMem);
                Assert(pRc->RcDesc.enmPool != D3DDDIPOOL_SYSTEMMEM);
            }
        }
    }
    else
    {
        if (pAlloc->hAllocation)
        {
            BOOL fDoUnlock = FALSE;

            Assert(pAlloc->LockInfo.cLocks);
            --pAlloc->LockInfo.cLocks;
            Assert(pAlloc->LockInfo.cLocks < UINT32_MAX);

            if (pRc->RcDesc.enmPool != D3DDDIPOOL_SYSTEMMEM)
            {
                fDoUnlock = TRUE;
            }
            else
            {
                if (!pAlloc->LockInfo.cLocks)
                {
                    D3DDDICB_LOCK LockData;
                    LockData.hAllocation = pAlloc->hAllocation;
                    LockData.PrivateDriverData = 0;
                    LockData.NumPages = 0;
                    LockData.pPages = NULL;
                    LockData.pData = NULL; /* out */
                    LockData.Flags.Value = 0;

                    hr = pDevice->RtCallbacks.pfnLockCb(pDevice->hDevice, &LockData);
                    if (hr == S_OK)
                    {
                        D3DLOCKED_RECT LRect;
                        LRect.pBits = LockData.pData;
                        LRect.Pitch = pAlloc->SurfDesc.pitch;
                        Assert(pAlloc->DirtyRegion.fFlags & VBOXWDDM_DIRTYREGION_F_VALID);
                        VBoxD3DIfLockUnlockMemSynch(pAlloc, &LRect, &pAlloc->DirtyRegion.Rect, TRUE /* bool bToLockInfo*/);
                        vboxWddmDirtyRegionClear(&pAlloc->DirtyRegion);
                        fDoUnlock = TRUE;
                    }
                    else
                    {
                        WARN(("pfnLockCb failed, hr 0x%x", hr));
                    }
                }
            }

            if (fDoUnlock)
            {
                D3DDDICB_UNLOCK Unlock;

                Unlock.NumAllocations = 1;
                Unlock.phAllocations = &pAlloc->hAllocation;

                hr = pDevice->RtCallbacks.pfnUnlockCb(pDevice->hDevice, &Unlock);
                if(hr != S_OK)
                {
                    WARN(("pfnUnlockCb failed, hr 0x%x", hr));
                }
            }

            if (!SUCCEEDED(hr))
            {
                WARN(("unlock failure!"));
                ++pAlloc->LockInfo.cLocks;
            }
        }
    }

    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return hr;
}

HRESULT APIENTRY GaDdiCreateVertexShaderFunc(HANDLE hDevice, D3DDDIARG_CREATEVERTEXSHADERFUNC *pData, const UINT *pCode)
{
    VBOXVDBG_BREAK_DDI();

    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p) Size %d\n", hDevice, pData->Size));

    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    IDirect3DDevice9 *pDevice9If = VBOXDISP_D3DEV(pDevice);

    AssertReturn(pDevice9If, E_INVALIDARG);
    AssertReturn(pData->Size >= 2 * sizeof(uint32_t), E_INVALIDARG);

#ifdef LOG_ENABLED
    vboxVDbgPrintF(("Vertex shader code:\n"));
    const uint32_t *paTokens = (uint32_t *)pCode;
    const uint32_t cTokens = pData->Size / sizeof(uint32_t);
    for (uint32_t iToken = 0; iToken < cTokens; ++iToken)
    {
        vboxVDbgPrintF(("%08X", paTokens[iToken]));
    }
    vboxVDbgPrintF(("\n"));
#endif

    HRESULT hr = S_OK;
    DWORD *pFunction;
    if (pCode[0] == 0xFFFE0200)
    {
        /* Treat 2.0 shaders as 2.1, because Gallium is strict and rejects 2.0 shaders which use 2.1 instructions. */
        vboxVDbgPrintF(("Bumping version 2.0 to 2.1\n"));

        pFunction = (DWORD *)RTMemAlloc(pData->Size);
        if (pFunction)
        {
            memcpy(pFunction, pCode, pData->Size);
            pFunction[0] |= 1;
        }
        else
        {
            hr = E_OUTOFMEMORY;
        }
    }
    else
    {
        pFunction = (DWORD *)pCode;
    }

    if (hr == S_OK)
    {
        IDirect3DVertexShader9 *pShader;
        hr = pDevice9If->CreateVertexShader(pFunction, &pShader);
        Assert(hr == S_OK);
        if (hr == S_OK)
        {
            Assert(pShader);
            pData->ShaderHandle = pShader;
        }

        if ((uintptr_t)pFunction != (uintptr_t)pCode)
        {
            RTMemFree(pFunction);
        }
    }

    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p), hr(0x%x)\n", hDevice, hr));
    return hr;
}

HRESULT APIENTRY GaDdiCreatePixelShader(HANDLE hDevice, D3DDDIARG_CREATEPIXELSHADER *pData, const UINT *pCode)
{
    VBOXVDBG_BREAK_DDI();

    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p) Size %d\n", hDevice, pData->CodeSize));
    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    IDirect3DDevice9 *pDevice9If = VBOXDISP_D3DEV(pDevice);

    AssertReturn(pDevice9If, E_INVALIDARG);
    AssertReturn(pData->CodeSize >= 2 * sizeof(uint32_t), E_INVALIDARG);

#ifdef LOG_ENABLED
    vboxVDbgPrintF(("Shader code:\n"));
    const uint32_t *paTokens = (uint32_t *)pCode;
    const uint32_t cTokens = pData->CodeSize / sizeof(uint32_t);
    for (uint32_t iToken = 0; iToken < cTokens; ++iToken)
    {
        vboxVDbgPrintF(("%08X", paTokens[iToken]));
    }
    vboxVDbgPrintF(("\n"));
#endif

    HRESULT hr = S_OK;
    DWORD *pFunction;
    if (pCode[0] == 0xFFFF0200)
    {
        /* Treat 2.0 shaders as 2.1, because Gallium is strict and rejects 2.0 shaders which use 2.1 instructions. */
        vboxVDbgPrintF(("Bumping version 2.0 to 2.1\n"));

        pFunction = (DWORD *)RTMemAlloc(pData->CodeSize);
        if (pFunction)
        {
            memcpy(pFunction, pCode, pData->CodeSize);
            pFunction[0] |= 1;
        }
        else
        {
            hr = E_OUTOFMEMORY;
        }
    }
    else
    {
        pFunction = (DWORD *)pCode;
    }

    if (hr == S_OK)
    {
        IDirect3DPixelShader9 *pShader;
        hr = pDevice9If->CreatePixelShader(pFunction, &pShader);
        Assert(hr == S_OK);
        if (hr == S_OK)
        {
            Assert(pShader);
            pData->ShaderHandle = pShader;
        }

        if ((uintptr_t)pFunction != (uintptr_t)pCode)
        {
            RTMemFree(pFunction);
        }
    }

    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p), hr(0x%x)\n", hDevice, hr));
    return hr;
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

HRESULT APIENTRY GaDdiCreateResource(HANDLE hDevice, D3DDDIARG_CREATERESOURCE *pResource)
{
    VBOXVDBG_BREAK_DDI();

    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));

    HRESULT hr = S_OK;
    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    PVBOXWDDMDISP_ADAPTER pAdapter = pDevice->pAdapter;

    vboxVDbgPrintF(("Format %d(0x%x), Shared %d, Pool %d, MsType %d, MsQuality %d, SurfCount %d, MipLevels %d, Fvf 0x%x, VidPnSourceId 0x%x, hResource 0x%x, Flags 0x%x, Rotation %d\n",
                    pResource->Format, pResource->Format, pResource->Flags.SharedResource, pResource->Pool, pResource->MultisampleType, pResource->MultisampleQuality,
                    pResource->SurfCount, pResource->MipLevels, pResource->Fvf, pResource->VidPnSourceId,
                    pResource->hResource, pResource->Flags, pResource->Rotation));

    for (UINT iSurf = 0; iSurf < pResource->SurfCount; ++iSurf)
    {
        vboxVDbgPrintF(("    [%d]: %dx%d @%d SysMem %p pitch %d, slice %d\n",
                        iSurf,
                        pResource->pSurfList[iSurf].Width,
                        pResource->pSurfList[iSurf].Height,
                        pResource->pSurfList[iSurf].Depth,
                        pResource->pSurfList[iSurf].pSysMem,
                        pResource->pSurfList[iSurf].SysMemPitch,
                        pResource->pSurfList[iSurf].SysMemSlicePitch));
    }

    PVBOXWDDMDISP_RESOURCE pRc = (PVBOXWDDMDISP_RESOURCE)RTMemAllocZ(RT_UOFFSETOF_DYN(VBOXWDDMDISP_RESOURCE,
                                                                                      aAllocations[pResource->SurfCount]));
    if (!pRc)
    {
        WARN(("vboxResourceAlloc failed"));
        return E_OUTOFMEMORY;
    }

    bool bIssueCreateResource = false;
    bool bCreateKMResource = false;
    bool bSetHostID = false;

    pRc->hResource                 = pResource->hResource;
    pRc->hKMResource               = NULL;
    pRc->pDevice                   = pDevice;
    pRc->fFlags.Generic            = 1;
    pRc->RcDesc.fFlags             = pResource->Flags;
    pRc->RcDesc.enmFormat          = pResource->Format;
    pRc->RcDesc.enmPool            = pResource->Pool;
    pRc->RcDesc.enmMultisampleType = pResource->MultisampleType;
    pRc->RcDesc.MultisampleQuality = pResource->MultisampleQuality;
    pRc->RcDesc.MipLevels          = pResource->MipLevels;
    pRc->RcDesc.Fvf                = pResource->Fvf;
    pRc->RcDesc.VidPnSourceId      = pResource->VidPnSourceId;
    pRc->RcDesc.RefreshRate        = pResource->RefreshRate;
    pRc->RcDesc.enmRotation        = pResource->Rotation;
    pRc->cAllocations              = pResource->SurfCount;
    for (UINT i = 0; i < pResource->SurfCount; ++i)
    {
        PVBOXWDDMDISP_ALLOCATION pAllocation = &pRc->aAllocations[i];
        const D3DDDI_SURFACEINFO *pSurf = &pResource->pSurfList[i];

        pAllocation->iAlloc      = i;
        pAllocation->pRc         = pRc;
        pAllocation->hAllocation = 0;
        pAllocation->enmType     = VBOXWDDM_ALLOC_TYPE_UMD_RC_GENERIC;
        pAllocation->pvMem       = (void *)pSurf->pSysMem;

        pAllocation->SurfDesc.slicePitch    = pSurf->SysMemSlicePitch;
        pAllocation->SurfDesc.depth         = pSurf->Depth;
        pAllocation->SurfDesc.width         = pSurf->Width;
        pAllocation->SurfDesc.height        = pSurf->Height;
        pAllocation->SurfDesc.format        = pResource->Format;
        pAllocation->SurfDesc.VidPnSourceId = pResource->VidPnSourceId;

        /* No bpp for formats represented by FOURCC code. */
        if (!vboxWddmFormatToFourcc(pResource->Format))
            pAllocation->SurfDesc.bpp = vboxWddmCalcBitsPerPixel(pResource->Format);
        else
            pAllocation->SurfDesc.bpp = 0;

        if (pSurf->SysMemPitch)
            pAllocation->SurfDesc.pitch = pSurf->SysMemPitch;
        else
            pAllocation->SurfDesc.pitch = vboxWddmCalcPitch(pSurf->Width, pResource->Format);

        pAllocation->SurfDesc.cbSize = vboxWddmCalcSize(pAllocation->SurfDesc.pitch,
                                                        pAllocation->SurfDesc.height,
                                                        pAllocation->SurfDesc.format);

        /* Calculate full scanline width, which might be greater than width. Apparently for SYSTEMMEM only. */
        if (pRc->RcDesc.enmPool == D3DDDIPOOL_SYSTEMMEM)
        {
            pAllocation->SurfDesc.d3dWidth = vboxWddmCalcWidthForPitch(pAllocation->SurfDesc.pitch,
                                                                       pAllocation->SurfDesc.format);
            Assert(pAllocation->SurfDesc.d3dWidth >= pAllocation->SurfDesc.width);
        }
        else
        {
            pAllocation->SurfDesc.d3dWidth = pSurf->Width;
        }
    }

    if (VBOXDISPMODE_IS_3D(pAdapter))
    {
        if (pRc->RcDesc.fFlags.SharedResource)
        {
            bIssueCreateResource = true;
            bCreateKMResource = true;
            /* Miniport needs to know id of the surface which is being shared. */
            bSetHostID = true;
        }

        if (pRc->RcDesc.fFlags.RenderTarget || pRc->RcDesc.fFlags.Primary)
        {
            bIssueCreateResource = true;
            bSetHostID = true;
        }

        hr = GaD3DIfCreateForRc(pRc);
        if (FAILED(hr))
        {
            WARN(("D3DIfCreateForRc failed, hr 0x%x", hr));
        }
    }
    else
    {
        bIssueCreateResource = (pResource->Pool != D3DDDIPOOL_SYSTEMMEM) || pResource->Flags.RenderTarget;
        bCreateKMResource = bIssueCreateResource;
    }

    if (SUCCEEDED(hr) && bIssueCreateResource)
    {
        pRc->fFlags.KmResource = bCreateKMResource;

        D3DDDICB_ALLOCATE *pDdiAllocate = vboxWddmRequestAllocAlloc(pResource);
        if (pDdiAllocate)
        {
            IGaDirect3DDevice9Ex *pGaD3DDevice9Ex = NULL;
            if (bSetHostID)
            {
                IDirect3DDevice9 *pDevice9If = VBOXDISP_D3DEV(pDevice);
                hr = pDevice9If->QueryInterface(IID_IGaDirect3DDevice9Ex, (void**)&pGaD3DDevice9Ex);
                if (FAILED(hr))
                {
                    WARN(("QueryInterface(IID_IGaDirect3DDevice9Ex) failed, hr 0x%x", hr));
                }
            }

            Assert(pDdiAllocate->pPrivateDriverData);
            Assert(pDdiAllocate->PrivateDriverDataSize == sizeof(VBOXWDDM_RCINFO));

            PVBOXWDDM_RCINFO pRcInfo = (PVBOXWDDM_RCINFO)pDdiAllocate->pPrivateDriverData;
            pRcInfo->fFlags      = pRc->fFlags;
            pRcInfo->RcDesc      = pRc->RcDesc;
            pRcInfo->cAllocInfos = pResource->SurfCount;

            for (UINT i = 0; i < pResource->SurfCount; ++i)
            {
                PVBOXWDDMDISP_ALLOCATION pAllocation = &pRc->aAllocations[i];
                const D3DDDI_SURFACEINFO* pSurf = &pResource->pSurfList[i];

                Assert(RT_BOOL(pSurf->pSysMem) == (pResource->Pool == D3DDDIPOOL_SYSTEMMEM));

                D3DDDI_ALLOCATIONINFO *pDdiAllocInfo = &pDdiAllocate->pAllocationInfo[i];
                pDdiAllocInfo->hAllocation   = 0;
                pDdiAllocInfo->pSystemMem    = pSurf->pSysMem;
                pDdiAllocInfo->VidPnSourceId = pResource->VidPnSourceId;
                pDdiAllocInfo->Flags.Value   = 0;
                if (pResource->Flags.Primary)
                {
                    Assert(pResource->Flags.RenderTarget);
                    pDdiAllocInfo->Flags.Primary = 1;
                }

                Assert(pDdiAllocInfo->pPrivateDriverData);
                Assert(pDdiAllocInfo->PrivateDriverDataSize == sizeof(VBOXWDDM_ALLOCINFO));

                PVBOXWDDM_ALLOCINFO pWddmAllocInfo = (PVBOXWDDM_ALLOCINFO)pDdiAllocInfo->pPrivateDriverData;
                pWddmAllocInfo->enmType       = VBOXWDDM_ALLOC_TYPE_UMD_RC_GENERIC;
                pWddmAllocInfo->fFlags        = pResource->Flags;
                pWddmAllocInfo->hSharedHandle = (uintptr_t)pAllocation->hSharedHandle;
                pWddmAllocInfo->SurfDesc      = pAllocation->SurfDesc;

                if (bSetHostID)
                {
                    IDirect3DSurface9 *pSurfIf = NULL;
                    hr = VBoxD3DIfSurfGet(pRc, i, &pSurfIf);
                    if (SUCCEEDED(hr))
                    {
                        Assert(pGaD3DDevice9Ex);
                        hr = pGaD3DDevice9Ex->GaSurfaceId(pAllocation->pD3DIf, &pWddmAllocInfo->hostID);
                        if (SUCCEEDED(hr))
                        {
                            Assert(pWddmAllocInfo->hostID);
                        }
                        else
                        {
                            WARN(("pfnVBoxWineExD3DSurf9GetHostId failed, hr 0x%x", hr));
                            break;
                        }
                        pSurfIf->Release();
                    }
                    else
                    {
                        WARN(("VBoxD3DIfSurfGet failed, hr 0x%x", hr));
                        break;
                    }
                }
                else
                    pWddmAllocInfo->hostID = 0;

                pAllocation->hostID = pWddmAllocInfo->hostID;
                if (pResource->Flags.SharedResource)
                {
                    pWddmAllocInfo->hSharedHandle = pWddmAllocInfo->hostID;
                    pAllocation->hSharedHandle = (HANDLE)pWddmAllocInfo->hostID;
                }
            }

            Assert(!pRc->fFlags.Opened);
            Assert(pRc->fFlags.Generic);

            if (SUCCEEDED(hr))
            {
                if (bCreateKMResource)
                {
                    Assert(pRc->fFlags.KmResource);

                    hr = pDevice->RtCallbacks.pfnAllocateCb(pDevice->hDevice, pDdiAllocate);
                    Assert(hr == S_OK);
                    /* For some reason shared resources are created with zero km resource handle on Win7+. */
                    Assert(pDdiAllocate->hKMResource || pResource->Flags.SharedResource);
                }
                else
                {
                    Assert(!pRc->fFlags.KmResource);

                    pDdiAllocate->hResource             = NULL;
                    pDdiAllocate->NumAllocations        = 1;
                    pDdiAllocate->PrivateDriverDataSize = 0;
                    pDdiAllocate->pPrivateDriverData    = NULL;

                    D3DDDI_ALLOCATIONINFO *pDdiAllocIBase = pDdiAllocate->pAllocationInfo;
                    for (UINT i = 0; i < pResource->SurfCount; ++i)
                    {
                        pDdiAllocate->pAllocationInfo = &pDdiAllocIBase[i];
                        hr = pDevice->RtCallbacks.pfnAllocateCb(pDevice->hDevice, pDdiAllocate);
                        Assert(hr == S_OK);
                        Assert(!pDdiAllocate->hKMResource);
                        if (SUCCEEDED(hr))
                        {
                            Assert(pDdiAllocate->pAllocationInfo->hAllocation);
                        }
                        else
                        {
                            for (UINT j = 0; j < i; ++j)
                            {
                                D3DDDI_ALLOCATIONINFO * pCur = &pDdiAllocIBase[i];
                                D3DDDICB_DEALLOCATE Dealloc;
                                Dealloc.hResource = 0;
                                Dealloc.NumAllocations = 1;
                                Dealloc.HandleList = &pCur->hAllocation;
                                HRESULT hr2 = pDevice->RtCallbacks.pfnDeallocateCb(pDevice->hDevice, &Dealloc);
                                Assert(hr2 == S_OK); NOREF(hr2);
                            }
                            break;
                        }
                    }

                    pDdiAllocate->pAllocationInfo = pDdiAllocIBase;
                }

                if (SUCCEEDED(hr))
                {
                    pRc->hKMResource = pDdiAllocate->hKMResource;

                    for (UINT i = 0; i < pResource->SurfCount; ++i)
                    {
                        PVBOXWDDMDISP_ALLOCATION pAllocation = &pRc->aAllocations[i];
                        D3DDDI_ALLOCATIONINFO *pDdiAllocInfo = &pDdiAllocate->pAllocationInfo[i];
                        PVBOXWDDM_ALLOCINFO pWddmAllocInfo = (PVBOXWDDM_ALLOCINFO)pDdiAllocInfo->pPrivateDriverData;
                        const D3DDDI_SURFACEINFO *pSurf = &pResource->pSurfList[i];

                        pAllocation->hAllocation = pDdiAllocInfo->hAllocation;
                        pAllocation->enmType     = VBOXWDDM_ALLOC_TYPE_UMD_RC_GENERIC;
                        pAllocation->pvMem       = (void *)pSurf->pSysMem;
                        pAllocation->SurfDesc    = pWddmAllocInfo->SurfDesc;
                    }
                }
            }

            vboxWddmRequestAllocFree(pDdiAllocate);

            if (pGaD3DDevice9Ex)
                pGaD3DDevice9Ex->Release();
        }
        else
        {
            AssertFailed();
            hr = E_OUTOFMEMORY;
        }
    }

    if (SUCCEEDED(hr))
    {
        pResource->hResource = pRc;
        hr = S_OK;
    }
    else
    {
        if (pRc)
        {
            /** @todo GaDdiDestroyResource(hDevice, pRc); */
            RTMemFree(pRc);
        }
    }

    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p), pRc 0x%p, hr %x\n", hDevice, pRc, hr));
    return hr;
}

/** @todo Will be removed when there will be a full set of GaDdi functions, which will not use this. */
extern BOOLEAN vboxWddmDalCheckNotifyRemove(PVBOXWDDMDISP_DEVICE pDevice, PVBOXWDDMDISP_ALLOCATION pAlloc);

HRESULT APIENTRY GaDdiDestroyResource(HANDLE hDevice, HANDLE hResource)
{
    VBOXVDBG_BREAK_DDI();

    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p) hResource %p\n", hDevice, hResource));

    HRESULT hr = S_OK;
    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    PVBOXWDDMDISP_ADAPTER pAdapter = pDevice->pAdapter;
    PVBOXWDDMDISP_RESOURCE pRc = (PVBOXWDDMDISP_RESOURCE)hResource;

    if (VBOXDISPMODE_IS_3D(pAdapter))
    {
        for (UINT i = 0; i < pRc->cAllocations; ++i)
        {
            PVBOXWDDMDISP_ALLOCATION pAlloc = &pRc->aAllocations[i];
            if (pAlloc->hSharedHandle)
            {
                if (i == 0)
                {
                    /* Tell miniport to remove the sid -> shared sid mapping. */
                    IGaDirect3DDevice9Ex *pGaD3DDevice9Ex = NULL;
                    IDirect3DDevice9 *pDevice9If = VBOXDISP_D3DEV(pDevice);
                    HRESULT hr2 = pDevice9If->QueryInterface(IID_IGaDirect3DDevice9Ex, (void**)&pGaD3DDevice9Ex);
                    if (SUCCEEDED(hr2))
                    {
                        Assert(pGaD3DDevice9Ex);
                        /* Inform the miniport. */
                        VBOXDISPIFESCAPE_GASHAREDSID data;
                        RT_ZERO(data);
                        data.EscapeHdr.escapeCode = VBOXESC_GASHAREDSID;
                        data.u32Sid               = pAlloc->hostID;
                        data.u32SharedSid         = (uint32_t)~0;
                        hr2 = pGaD3DDevice9Ex->EscapeCb(&data, sizeof(data), /* fHardwareAccess= */ false);

                        pGaD3DDevice9Ex->Release();
                    }
                }
            }

            if (pAlloc->pD3DIf)
                pAlloc->pD3DIf->Release();

            vboxWddmDalCheckNotifyRemove(pDevice, pAlloc);
        }
    }

    if (pRc->fFlags.KmResource)
    {
        D3DDDICB_DEALLOCATE ddiDealloc;
        RT_ZERO(ddiDealloc);
        ddiDealloc.hResource = pRc->hResource;
        /* according to the docs the below two are ignored in case we set the hResource */
        // ddiDealloc.NumAllocations = 0;
        // ddiDealloc.HandleList = NULL;
        hr = pDevice->RtCallbacks.pfnDeallocateCb(pDevice->hDevice, &ddiDealloc);
        Assert(hr == S_OK);
    }
    else
    {
        Assert(!(pRc->fFlags.Opened));
        for (UINT j = 0; j < pRc->cAllocations; ++j)
        {
            if (pRc->aAllocations[j].hAllocation)
            {
                D3DDDICB_DEALLOCATE ddiDealloc;
                ddiDealloc.hResource      = NULL;
                ddiDealloc.NumAllocations = 1;
                ddiDealloc.HandleList     = &pRc->aAllocations[j].hAllocation;
                HRESULT hr2 = pDevice->RtCallbacks.pfnDeallocateCb(pDevice->hDevice, &ddiDealloc);
                Assert(hr2 == S_OK); NOREF(hr2);
            }
        }
    }

    RTMemFree(pRc);

    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return hr;
}

HRESULT APIENTRY GaDdiOpenResource(HANDLE hDevice, D3DDDIARG_OPENRESOURCE *pResource)
{
    VBOXVDBG_BREAK_DDI();

    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));

    HRESULT hr = S_OK;
    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;

    Assert(pResource->hKMResource);
    Assert(pResource->NumAllocations);

    PVBOXWDDMDISP_RESOURCE pRc = (PVBOXWDDMDISP_RESOURCE)RTMemAllocZ(RT_UOFFSETOF_DYN(VBOXWDDMDISP_RESOURCE,
                                                                                      aAllocations[pResource->NumAllocations]));
    if (pRc)
    {
        pRc->cAllocations       = pResource->NumAllocations;
        pRc->hResource          = pResource->hResource;
        pRc->hKMResource        = pResource->hKMResource;
        pRc->pDevice            = pDevice;
        pRc->RcDesc.enmRotation = pResource->Rotation;
        // pRc->fFlags.Value       = 0;
        pRc->fFlags.Opened      = 1;
        pRc->fFlags.KmResource  = 1;

        for (UINT i = 0; i < pResource->NumAllocations; ++i)
        {
            PVBOXWDDMDISP_ALLOCATION pAllocation = &pRc->aAllocations[i];
            pAllocation->iAlloc = i;
            pAllocation->pRc    = pRc;

            D3DDDI_OPENALLOCATIONINFO *pOAI = &pResource->pOpenAllocationInfo[i];
            if (pOAI->PrivateDriverDataSize != sizeof (VBOXWDDM_ALLOCINFO))
            {
                AssertFailed();
                hr = E_INVALIDARG;
                break;
            }
            Assert(pOAI->pPrivateDriverData);

            PVBOXWDDM_ALLOCINFO pWddmAllocInfo = (PVBOXWDDM_ALLOCINFO)pOAI->pPrivateDriverData;
            pAllocation->hAllocation   = pOAI->hAllocation;
            pAllocation->enmType       = pWddmAllocInfo->enmType;
            pAllocation->hSharedHandle = (HANDLE)pWddmAllocInfo->hSharedHandle;
            pAllocation->SurfDesc      = pWddmAllocInfo->SurfDesc;
            pAllocation->pvMem         = NULL;

            Assert(!pAllocation->hSharedHandle == (pAllocation->enmType == VBOXWDDM_ALLOC_TYPE_STD_SHAREDPRIMARYSURFACE));
        }

        if (!pResource->pPrivateDriverData || !pResource->PrivateDriverDataSize)
        {
            /* this is a "standard" allocation resource */

            /* both should be actually zero */
            Assert(!pResource->pPrivateDriverData && !pResource->PrivateDriverDataSize);

            pRc->RcDesc.enmPool            = D3DDDIPOOL_LOCALVIDMEM;
            pRc->RcDesc.enmMultisampleType = D3DDDIMULTISAMPLE_NONE;
            // pRc->RcDesc.MultisampleQuality = 0;
            // pRc->RcDesc.MipLevels          = 0;
            // pRc->RcDesc.Fvf                = 0;
            pRc->RcDesc.fFlags.SharedResource = 1;

            if (pResource->NumAllocations != 1)
            {
                WARN(("NumAllocations is expected to be 1, but was %d", pResource->NumAllocations));
            }

            for (UINT i = 0; i < pResource->NumAllocations; ++i)
            {
                PVBOXWDDMDISP_ALLOCATION pAlloc = &pRc->aAllocations[i];
                pAlloc->enmD3DIfType = VBOXDISP_D3DIFTYPE_SURFACE;
                pAlloc->pD3DIf = NULL;
            }

            D3DDDI_OPENALLOCATIONINFO *pOAI = &pResource->pOpenAllocationInfo[0];
            Assert(pOAI->pPrivateDriverData);
            Assert(pOAI->PrivateDriverDataSize >= sizeof(VBOXWDDM_ALLOCINFO));
            if (pOAI->pPrivateDriverData && pOAI->PrivateDriverDataSize >= sizeof(VBOXWDDM_ALLOCINFO))
            {
                PVBOXWDDM_ALLOCINFO pWddmAllocInfo = (PVBOXWDDM_ALLOCINFO)pOAI->pPrivateDriverData;
                switch (pWddmAllocInfo->enmType)
                {
                    case VBOXWDDM_ALLOC_TYPE_STD_SHAREDPRIMARYSURFACE:
                        pRc->RcDesc.fFlags.Primary = 1;
                    case VBOXWDDM_ALLOC_TYPE_STD_SHADOWSURFACE:
                    case VBOXWDDM_ALLOC_TYPE_STD_STAGINGSURFACE:
                        pRc->RcDesc.enmFormat      = pWddmAllocInfo->SurfDesc.format;
                        pRc->RcDesc.VidPnSourceId  = pWddmAllocInfo->SurfDesc.VidPnSourceId;
                        pRc->RcDesc.RefreshRate    = pWddmAllocInfo->SurfDesc.RefreshRate;
                        break;
                    default:
                        AssertFailed();
                        hr = E_INVALIDARG;
                }
            }
            else
                hr = E_INVALIDARG;
        }
        else
        {
            /* this is a "generic" resource whose creation is initiated by the UMD */
            Assert(pResource->PrivateDriverDataSize == sizeof(VBOXWDDM_RCINFO));
            if (pResource->PrivateDriverDataSize == sizeof(VBOXWDDM_RCINFO))
            {
                VBOXWDDM_RCINFO *pRcInfo = (VBOXWDDM_RCINFO *)pResource->pPrivateDriverData;
                Assert(pRcInfo->fFlags.Generic);
                Assert(!pRcInfo->fFlags.Opened);
                Assert(pRcInfo->cAllocInfos == pResource->NumAllocations);

                pRc->fFlags.Value  |= pRcInfo->fFlags.Value;
                pRc->fFlags.Generic = 1;
                pRc->RcDesc         = pRcInfo->RcDesc;
                pRc->cAllocations   = pResource->NumAllocations;
                Assert(pRc->RcDesc.fFlags.SharedResource);

// ASMBreakpoint();
                hr = GaD3DIfCreateForRc(pRc);
                if (SUCCEEDED(hr))
                {
                   /* Get the just created surface id and inform the miniport that the surface id
                    * should be replaced with the original surface id.
                    */
                   IDirect3DDevice9 *pDevice9If = VBOXDISP_D3DEV(pDevice);
                   IGaDirect3DDevice9Ex *pGaD3DDevice9Ex = NULL;
                   HRESULT hr2 = pDevice9If->QueryInterface(IID_IGaDirect3DDevice9Ex, (void**)&pGaD3DDevice9Ex);
                   if (SUCCEEDED(hr2))
                   {
                       Assert(pGaD3DDevice9Ex);
                       PVBOXWDDMDISP_ALLOCATION pAllocation = &pRc->aAllocations[0]; /* First allocation is enough. */
                       uint32_t u32Sid;
                       hr2 = pGaD3DDevice9Ex->GaSurfaceId(pAllocation->pD3DIf, &u32Sid);
                       if (SUCCEEDED(hr2))
                       {
                           /* Inform the miniport. */
                           Assert(pAllocation->hSharedHandle);

                           pAllocation->hostID = u32Sid;

                           VBOXDISPIFESCAPE_GASHAREDSID data;
                           RT_ZERO(data);
                           data.EscapeHdr.escapeCode = VBOXESC_GASHAREDSID;
                           data.u32Sid = u32Sid;
                           data.u32SharedSid = (uint32_t)(uintptr_t)pAllocation->hSharedHandle;
                           hr2 = pGaD3DDevice9Ex->EscapeCb(&data, sizeof(data), /* fHardwareAccess= */ false);
                       }
                       pGaD3DDevice9Ex->Release();
                   }
                }
            }
            else
                hr = E_INVALIDARG;
        }

        if (hr == S_OK)
        {
            pResource->hResource = pRc;
            vboxVDbgPrintF(("<== "__FUNCTION__", pRc(0x%p)\n", pRc));
        }
        else
            RTMemFree(pRc);
    }
    else
    {
        vboxVDbgPrintR((__FUNCTION__": vboxResourceAlloc failed for hDevice(0x%p), NumAllocations(%d)\n", hDevice, pResource->NumAllocations));
        hr = E_OUTOFMEMORY;
    }

    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return hr;
}

/** @todo Will be removed when there will be a full set of GaDdi functions, which will not use this. */
extern VOID vboxWddmDalCheckAddOnDraw(PVBOXWDDMDISP_DEVICE pDevice);

HRESULT APIENTRY GaDdiDrawPrimitive(HANDLE hDevice, const D3DDDIARG_DRAWPRIMITIVE *pData, const UINT *pFlagBuffer)
{
    VBOXVDBG_BREAK_DDI();

    RT_NOREF(pFlagBuffer);

    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));

    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    IDirect3DDevice9 * pDevice9If = VBOXDISP_D3DEV(pDevice);

    Assert(!pFlagBuffer);
    HRESULT hr = S_OK;

    if (pDevice->cStreamSourcesUm)
    {
#ifdef DEBUG
        uint32_t cStreams = 0;
        for (UINT i = 0; i < RT_ELEMENTS(pDevice->aStreamSourceUm); ++i)
        {
            if(pDevice->aStreamSourceUm[i].pvBuffer)
            {
                ++cStreams;
            }
        }

        Assert(cStreams);
        Assert(cStreams == pDevice->cStreamSourcesUm);
#endif
        if (pDevice->cStreamSourcesUm == 1)
        {
            for (UINT i = 0; i < RT_ELEMENTS(pDevice->aStreamSourceUm); ++i)
            {
                VBOXWDDMDISP_STREAMSOURCEUM *pStreamSourceUm = &pDevice->aStreamSourceUm[i];
                if (pStreamSourceUm->pvBuffer)
                {
                    const void *pvVertexStream = (uint8_t *)pStreamSourceUm->pvBuffer
                                                 + pData->VStart * pStreamSourceUm->cbStride;
                    hr = pDevice9If->DrawPrimitiveUP(pData->PrimitiveType,
                                                     pData->PrimitiveCount,
                                                     pvVertexStream,
                                                     pStreamSourceUm->cbStride);
                    Assert(hr == S_OK);
                    break;
                }
            }
        }
        else
        {
            /** @todo impl */
            WARN(("multiple user stream sources (%d) not implemented!!", pDevice->cStreamSourcesUm));
        }
    }
    else
    {
#ifdef DEBUG
        Assert(!pDevice->cStreamSourcesUm);
        for (UINT i = 0; i < RT_ELEMENTS(pDevice->aStreamSourceUm); ++i)
        {
            Assert(!pDevice->aStreamSourceUm[i].pvBuffer);
        }

        uint32_t cStreams = 0;
        for (UINT i = 0; i < RT_ELEMENTS(pDevice->aStreamSource); ++i)
        {
            if (pDevice->aStreamSource[i])
            {
                ++cStreams;
                Assert(!pDevice->aStreamSource[i]->LockInfo.cLocks);
            }
        }

        Assert(cStreams);
        Assert(cStreams == pDevice->cStreamSources);
#endif
        hr = pDevice9If->DrawPrimitive(pData->PrimitiveType,
                                       pData->VStart,
                                       pData->PrimitiveCount);
        Assert(hr == S_OK);
    }

    vboxWddmDalCheckAddOnDraw(pDevice);

    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p), hr(0x%x)\n", hDevice, hr));
    return hr;
}


HRESULT APIENTRY GaDdiDrawIndexedPrimitive(HANDLE hDevice, const D3DDDIARG_DRAWINDEXEDPRIMITIVE *pData)
{
    VBOXVDBG_BREAK_DDI();

    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));

    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    IDirect3DDevice9 * pDevice9If = VBOXDISP_D3DEV(pDevice);

#ifdef DEBUG
    uint32_t cStreams = 0;
    for (UINT i = 0; i < RT_ELEMENTS(pDevice->aStreamSourceUm); ++i)
    {
        if(pDevice->aStreamSourceUm[i].pvBuffer)
            ++cStreams;
    }

    Assert(cStreams == pDevice->cStreamSourcesUm);

    cStreams = 0;
    for (UINT i = 0; i < RT_ELEMENTS(pDevice->aStreamSource); ++i)
    {
        if (pDevice->aStreamSource[i])
        {
            ++cStreams;
            Assert(!pDevice->aStreamSource[i]->LockInfo.cLocks);
        }
    }

    Assert(cStreams == pDevice->cStreamSources);
#endif

    HRESULT hr = S_OK;

    if (pDevice->cStreamSourcesUm)
    {
        Assert(pDevice->cStreamSourcesUm == 1);
        Assert(pDevice->IndiciesInfo.uiStride == 2 || pDevice->IndiciesInfo.uiStride == 4);

        const uint8_t *pu8IndexBuffer = NULL;
        if (pDevice->IndiciesInfo.pIndicesAlloc)
        {
            Assert(!pDevice->IndiciesInfo.pvIndicesUm);

            pu8IndexBuffer = (const uint8_t *)pDevice->IndiciesInfo.pIndicesAlloc->pvMem;
        }
        else
        {
            pu8IndexBuffer = (const uint8_t *)pDevice->IndiciesInfo.pvIndicesUm;
        }

        if (pu8IndexBuffer)
        {
            hr = E_FAIL; /* If nothing found. */

            for (UINT i = 0; i < RT_ELEMENTS(pDevice->aStreamSourceUm); ++i)
            {
                VBOXWDDMDISP_STREAMSOURCEUM *pStreamSourceUm = &pDevice->aStreamSourceUm[i];
                if (pStreamSourceUm->pvBuffer)
                {
                    hr = pDevice9If->DrawIndexedPrimitiveUP(pData->PrimitiveType,
                                    pData->MinIndex,
                                    pData->NumVertices,
                                    pData->PrimitiveCount,
                                    pu8IndexBuffer + pDevice->IndiciesInfo.uiStride * pData->StartIndex,
                                    pDevice->IndiciesInfo.uiStride == 2 ? D3DFMT_INDEX16 : D3DFMT_INDEX32,
                                    pStreamSourceUm->pvBuffer,
                                    pStreamSourceUm->cbStride);
                    Assert(hr == S_OK);

                    if (SUCCEEDED(hr))
                    {
                        if (pDevice->IndiciesInfo.pIndicesAlloc)
                        {
                            HRESULT hr2 = pDevice9If->SetIndices((IDirect3DIndexBuffer9*)pDevice->IndiciesInfo.pIndicesAlloc->pD3DIf);
                            if(!SUCCEEDED(hr2))
                                WARN(("SetIndices failed hr = 0x%x", hr2));
                        }
                    }

                    break;
                }
            }
        }
        else
        {
            WARN(("not expected!"));
            hr = E_FAIL;
        }
    }
    else
    {
        Assert(pDevice->IndiciesInfo.pIndicesAlloc);
        Assert(!pDevice->IndiciesInfo.pvIndicesUm);
        Assert(!pDevice->IndiciesInfo.pIndicesAlloc->LockInfo.cLocks);
        Assert(!pDevice->cStreamSourcesUm);

        hr = pDevice9If->DrawIndexedPrimitive(pData->PrimitiveType,
                                              pData->BaseVertexIndex,
                                              pData->MinIndex,
                                              pData->NumVertices,
                                              pData->StartIndex,
                                              pData->PrimitiveCount);
        Assert(hr == S_OK);
    }

    vboxWddmDalCheckAddOnDraw(pDevice);

    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p), hr(0x%x)\n", hDevice, hr));
    return hr;
}


HRESULT APIENTRY GaDdiDrawPrimitive2(HANDLE hDevice, const D3DDDIARG_DRAWPRIMITIVE2 *pData)
{
    VBOXVDBG_BREAK_DDI();

    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));

    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    IDirect3DDevice9 *pDevice9If = VBOXDISP_D3DEV(pDevice);

    HRESULT hr = S_OK;

    /* "Stream zero contains transform vertices and is the only stream that should be accessed." */
    if (pDevice->aStreamSource[0])
    {
        VBOXWDDMDISP_ALLOCATION *pStreamSource = pDevice->aStreamSource[0];
        VBOXWDDMDISP_STREAM_SOURCE_INFO *pStreamSourceInfo = &pDevice->StreamSourceInfo[0];

        Assert(pStreamSourceInfo->uiStride != 0);

        VBOXWDDMDISP_LOCKINFO *pLock = &pStreamSource->LockInfo;
        if (pLock->cLocks)
        {
            Assert(pLock->fFlags.MightDrawFromLocked && (pLock->fFlags.Discard || pLock->fFlags.NoOverwrite));

            hr = pDevice9If->DrawPrimitiveUP(pData->PrimitiveType, pData->PrimitiveCount,
                                             (void *)(  (uintptr_t)pStreamSource->pvMem
                                                      + pStreamSourceInfo->uiOffset + pData->FirstVertexOffset),
                                             pStreamSourceInfo->uiStride);
            Assert(hr == S_OK);

            hr = pDevice9If->SetStreamSource(0, (IDirect3DVertexBuffer9 *)pStreamSource->pD3DIf,
                                             pStreamSourceInfo->uiOffset,
                                             pStreamSourceInfo->uiStride);
            Assert(hr == S_OK);
        }
        else
        {
            hr = pDevice9If->DrawPrimitive(pData->PrimitiveType,
                                           pData->FirstVertexOffset / pStreamSourceInfo->uiStride,
                                           pData->PrimitiveCount);
            Assert(hr == S_OK);
        }
    }
    else
    {
        hr = E_FAIL;
    }

    vboxWddmDalCheckAddOnDraw(pDevice);

    Assert(hr == S_OK);
    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p), hr(0x%x)\n", hDevice, hr));
    return hr;
}


static UINT vboxWddmVertexCountFromPrimitive(D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount)
{
    Assert(PrimitiveCount > 0); /* Callers ensure this. */

    UINT cVertices;
    switch (PrimitiveType)
    {
        case D3DPT_POINTLIST:
            cVertices = PrimitiveCount;     /* Vertex per point. */
            break;

        case D3DPT_LINELIST:
            cVertices = PrimitiveCount * 2; /* Two vertices for each line. */
            break;

        case D3DPT_LINESTRIP:
            cVertices = PrimitiveCount + 1; /* Two vertices for the first line and one vertex for each subsequent. */
            break;

        case D3DPT_TRIANGLELIST:
            cVertices = PrimitiveCount * 3; /* Three vertices for each triangle. */
            break;

        case D3DPT_TRIANGLESTRIP:
        case D3DPT_TRIANGLEFAN:
            cVertices = PrimitiveCount + 2; /* Three vertices for the first triangle and one vertex for each subsequent. */
            break;

        default:
            cVertices = 0; /* No such primitive in d3d9types.h. */
            break;
    }

    return cVertices;
}


HRESULT APIENTRY GaDdiDrawIndexedPrimitive2(HANDLE hDevice, const D3DDDIARG_DRAWINDEXEDPRIMITIVE2 *pData,
                                            UINT dwIndicesSize, const VOID *pIndexBuffer, const UINT *pFlagBuffer)
{
    VBOXVDBG_BREAK_DDI();

    RT_NOREF(pFlagBuffer);

    vboxVDbgPrintF(("==> "__FUNCTION__", hDevice(0x%p)\n", hDevice));

    PVBOXWDDMDISP_DEVICE pDevice = (PVBOXWDDMDISP_DEVICE)hDevice;
    IDirect3DDevice9 * pDevice9If = VBOXDISP_D3DEV(pDevice);

    HRESULT hr = S_OK;

    const uint8_t *pu8VertexBuffer = NULL;
    DWORD cbVertexStride = 0;

    LOGF(("\n  PrimitiveType %d, BaseVertexOffset %d, MinIndex %d, NumVertices %d, StartIndexOffset %d, PrimitiveCount %d,\n"
          "  dwIndicesSize %d, pIndexBuffer %p, pFlagBuffer %p\n",
          pData->PrimitiveType,
          pData->BaseVertexOffset,
          pData->MinIndex,
          pData->NumVertices,
          pData->StartIndexOffset,
          pData->PrimitiveCount,
          dwIndicesSize,
          pIndexBuffer,
          pFlagBuffer));

    if (dwIndicesSize != 2 && dwIndicesSize != 4)
    {
        WARN(("unsupported dwIndicesSize %d", dwIndicesSize));
        return E_INVALIDARG;
    }

    if (pData->PrimitiveCount == 0)
    {
        /* Nothing to draw. */
        return S_OK;
    }

    /* Fetch the appropriate stream source:
     * "Stream zero contains transform indices and is the only stream that should be accessed."
     */
    if (pDevice->aStreamSourceUm[0].pvBuffer)
    {
        Assert(pDevice->aStreamSourceUm[0].cbStride);

        pu8VertexBuffer = (const uint8_t *)pDevice->aStreamSourceUm[0].pvBuffer;
        cbVertexStride = pDevice->aStreamSourceUm[0].cbStride;
        LOGF(("aStreamSourceUm %p, stride %d\n",
              pu8VertexBuffer, cbVertexStride));
    }
    else if (pDevice->aStreamSource[0])
    {
        PVBOXWDDMDISP_ALLOCATION pAlloc = pDevice->aStreamSource[0];
        if (pAlloc->pvMem)
        {
            Assert(pDevice->StreamSourceInfo[0].uiStride);
            pu8VertexBuffer = ((const uint8_t *)pAlloc->pvMem) + pDevice->StreamSourceInfo[0].uiOffset;
            cbVertexStride = pDevice->StreamSourceInfo[0].uiStride;
            LOGF(("aStreamSource %p, cbSize %d, stride %d, uiOffset %d (elements %d)\n",
                  pu8VertexBuffer, pAlloc->SurfDesc.cbSize, cbVertexStride, pDevice->StreamSourceInfo[0].uiOffset,
                  cbVertexStride? pAlloc->SurfDesc.cbSize / cbVertexStride: 0));
        }
        else
        {
            WARN(("unsupported!!"));
            hr = E_FAIL;
        }
    }
    else
    {
        WARN(("not expected!"));
        hr = E_FAIL;
    }

    if (SUCCEEDED(hr))
    {
        /* Convert input data to appropriate DrawIndexedPrimitiveUP parameters.
         * In particular prepare zero based vertex array becuase wine does not
         * handle MinVertexIndex correctly.
         */

        /* Take the offset, which corresponds to the index == 0, into account. */
        const uint8_t *pu8VertexStart = pu8VertexBuffer + pData->BaseVertexOffset;

        /* Where the pData->MinIndex starts. */
        pu8VertexStart += pData->MinIndex * cbVertexStride;

        /* Convert indexes to zero based relative to pData->MinIndex. */
        const uint8_t *pu8IndicesStartSrc = (uint8_t *)pIndexBuffer + pData->StartIndexOffset;
        UINT cIndices = vboxWddmVertexCountFromPrimitive(pData->PrimitiveType, pData->PrimitiveCount);

        /* Allocate memory for converted indices. */
        uint8_t *pu8IndicesStartConv = (uint8_t *)RTMemAlloc(cIndices * dwIndicesSize);
        if (pu8IndicesStartConv != NULL)
        {
            UINT i;
            if (dwIndicesSize == 2)
            {
                uint16_t *pu16Src = (uint16_t *)pu8IndicesStartSrc;
                uint16_t *pu16Dst = (uint16_t *)pu8IndicesStartConv;
                for (i = 0; i < cIndices; ++i, ++pu16Dst, ++pu16Src)
                {
                    *pu16Dst = *pu16Src - pData->MinIndex;
                }
            }
            else
            {
                uint32_t *pu32Src = (uint32_t *)pu8IndicesStartSrc;
                uint32_t *pu32Dst = (uint32_t *)pu8IndicesStartConv;
                for (i = 0; i < cIndices; ++i, ++pu32Dst, ++pu32Src)
                {
                    *pu32Dst = *pu32Src - pData->MinIndex;
                }
            }

            hr = pDevice9If->DrawIndexedPrimitiveUP(pData->PrimitiveType,
                                                    0,
                                                    pData->NumVertices,
                                                    pData->PrimitiveCount,
                                                    pu8IndicesStartConv,
                                                    dwIndicesSize == 2 ? D3DFMT_INDEX16 : D3DFMT_INDEX32,
                                                    pu8VertexStart,
                                                    cbVertexStride);

            if (SUCCEEDED(hr))
                hr = S_OK;
            else
                WARN(("DrawIndexedPrimitiveUP failed hr = 0x%x", hr));

            RTMemFree(pu8IndicesStartConv);

            /* Following any IDirect3DDevice9::DrawIndexedPrimitiveUP call, the stream 0 settings,
             * referenced by IDirect3DDevice9::GetStreamSource, are set to NULL. Also, the index
             * buffer setting for IDirect3DDevice9::SetIndices is set to NULL.
             */
            if (pDevice->aStreamSource[0])
            {
                HRESULT tmpHr = pDevice9If->SetStreamSource(0, (IDirect3DVertexBuffer9*)pDevice->aStreamSource[0]->pD3DIf,
                                                            pDevice->StreamSourceInfo[0].uiOffset,
                                                            pDevice->StreamSourceInfo[0].uiStride);
                if(!SUCCEEDED(tmpHr))
                    WARN(("SetStreamSource failed hr = 0x%x", tmpHr));
            }

            if (pDevice->IndiciesInfo.pIndicesAlloc)
            {
                HRESULT tmpHr = pDevice9If->SetIndices((IDirect3DIndexBuffer9*)pDevice->IndiciesInfo.pIndicesAlloc->pD3DIf);
                if(!SUCCEEDED(tmpHr))
                    WARN(("SetIndices failed hr = 0x%x", tmpHr));
            }
        }
        else
        {
            hr = E_OUTOFMEMORY;
        }
    }

    vboxWddmDalCheckAddOnDraw(pDevice);

    vboxVDbgPrintF(("<== "__FUNCTION__", hDevice(0x%p)\n", hDevice));
    return hr;
}
