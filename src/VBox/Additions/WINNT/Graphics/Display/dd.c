/******************************Module*Header**********************************\
*
*                           **************************
*                           * DirectDraw SAMPLE CODE *
*                           **************************
*
* Module Name: ddenable.c
*
* Content:    
*
* Copyright (c) 1994-1998 3Dlabs Inc. Ltd. All rights reserved.
* Copyright (c) 1995-1999 Microsoft Corporation.  All rights reserved.
\*****************************************************************************/

#include "driver.h"
#include "dd.h"

#if 0
static DWORD APIENTRY DdCreateSurface(PDD_CREATESURFACEDATA  lpCreateSurface);
#endif

/**
 * DrvGetDirectDrawInfo
 *
 * The DrvGetDirectDrawInfo function returns the capabilities of the graphics hardware.
 *
 * Parameters:
 * 
 * dhpdev
 *     Handle to the PDEV returned by the driver’s DrvEnablePDEV routine.
 * pHalInfo
 *     Points to a DD_HALINFO structure in which the driver should return the hardware capabilities that it supports.
 * pdwNumHeaps
 *     Points to the location in which the driver should return the number of VIDEOMEMORY structures pointed to by pvmList.
 * pvmList
 *     Points to an array of VIDEOMEMORY structures in which the driver should return information about each display memory chunk that it controls. The driver should ignore this parameter when it is NULL.
 * pdwNumFourCCCodes
 *     Points to the location in which the driver should return the number of DWORDs pointed to by pdwFourCC.
 * pdwFourCC
 *     Points to an array of DWORDs in which the driver should return information about each FOURCC that it supports. The driver should ignore this parameter when it is NULL. 
 *
 * Return Value:
 *
 * DrvGetDirectDrawInfo returns TRUE if it succeeds; otherwise, it returns FALSE.
 *
 */
BOOL APIENTRY DrvGetDirectDrawInfo(
    DHPDEV        dhpdev,
    DD_HALINFO   *pHalInfo,
    DWORD        *pdwNumHeaps,
    VIDEOMEMORY  *pvmList,
    DWORD        *pdwNumFourCCCodes,
    DWORD        *pdwFourCC
    )
{
    PPDEV pDev = (PPDEV)dhpdev;

    DISPDBG((0, "%s: %p, %p, %p, %p, %p. %p\n", __FUNCTION__, dhpdev, pHalInfo, pdwNumHeaps, pvmList, pdwNumFourCCCodes, pdwFourCC));

    *pdwNumFourCCCodes = 0;
    *pdwNumHeaps       = 0;

    /* Setup the HAL driver caps. */
    pHalInfo->dwSize    = sizeof(DD_HALINFO);
    pHalInfo->dwFlags   = 0;

    if (!(pvmList && pdwFourCC)) 
    {
        /* Create primary surface attributes */
        pHalInfo->vmiData.pvPrimary                 = pDev->pjScreen;
        pHalInfo->vmiData.fpPrimary                 = 0;
        pHalInfo->vmiData.dwDisplayWidth            = pDev->cxScreen;
        pHalInfo->vmiData.dwDisplayHeight           = pDev->cyScreen;
        pHalInfo->vmiData.lDisplayPitch             = pDev->lDeltaScreen;
        pHalInfo->vmiData.ddpfDisplay.dwSize        = sizeof(DDPIXELFORMAT);
        pHalInfo->vmiData.ddpfDisplay.dwFlags       = DDPF_RGB;
        pHalInfo->vmiData.ddpfDisplay.dwRGBBitCount = pDev->ulBitCount;
        DISPDBG((0, "pvPrimary                      %x\n", pHalInfo->vmiData.pvPrimary));
        DISPDBG((0, "fpPrimary                      %x\n", pHalInfo->vmiData.fpPrimary));
        DISPDBG((0, "dwDisplayWidth                 %x\n", pHalInfo->vmiData.dwDisplayWidth));
        DISPDBG((0, "dwDisplayHeight                %x\n", pHalInfo->vmiData.dwDisplayHeight));
        DISPDBG((0, "lDisplayPitch                  %x\n", pHalInfo->vmiData.lDisplayPitch));
        DISPDBG((0, "dwRGBBitCount                  %x\n", pHalInfo->vmiData.ddpfDisplay.dwRGBBitCount));

        if (pDev->ulBitmapType == BMF_8BPP)
        {
            pHalInfo->vmiData.ddpfDisplay.dwFlags |= DDPF_PALETTEINDEXED8;
            DISPDBG((0, "DDPF_PALETTEINDEXED8\n"));
        }

        pHalInfo->vmiData.ddpfDisplay.dwRBitMask    = pDev->flRed;
        pHalInfo->vmiData.ddpfDisplay.dwGBitMask    = pDev->flGreen;
        pHalInfo->vmiData.ddpfDisplay.dwBBitMask    = pDev->flBlue;

        pHalInfo->vmiData.dwOffscreenAlign          = 4;
        pHalInfo->vmiData.dwZBufferAlign            = 4;
        pHalInfo->vmiData.dwTextureAlign            = 4;
    }
    pHalInfo->ddCaps.dwSize         = sizeof(DDNTCORECAPS);
    pHalInfo->ddCaps.dwVidMemTotal  = pDev->cScreenSize;
    pHalInfo->ddCaps.dwVidMemFree   = pDev->cScreenSize;

    pHalInfo->ddCaps.dwCaps         = DDCAPS_NOHARDWARE; /* ??? */
    pHalInfo->ddCaps.dwCaps2        = DDCAPS2_CERTIFIED;

    pHalInfo->ddCaps.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_VIDEOMEMORY;

#if 0
    /* DX5 and up */
    pHalInfo->GetDriverInfo = DdGetDriverInfo;
    pHalInfo->dwFlags |= DDHALINFO_GETDRIVERINFOSET;
#endif

    /* No 3D capabilities */
#if 0
    if (pHalInfo->lpD3DGlobalDriverData)
    {
        LPD3DHAL_GLOBALDRIVERDATA lpD3DGlobalDriverData = (LPD3DHAL_GLOBALDRIVERDATA)pHalInfo->lpD3DGlobalDriverData;
        lpD3DGlobalDriverData->dwSize = sizeof(D3DHAL_GLOBALDRIVERDATA);
    }
#endif
    
    return TRUE;
}

/**
 * DrvEnableDirectDraw
 * 
 * The DrvEnableDirectDraw function enables hardware for DirectDraw use.
 * 
 * Parameters
 * 
 * dhpdev
 *     Handle to the PDEV returned by the driver’s DrvEnablePDEV routine.
 * pCallBacks
 *     Points to the DD_CALLBACKS structure to be initialized by the driver.
 * pSurfaceCallBacks
 *     Points to the DD_SURFACECALLBACKS structure to be initialized by the driver.
 * pPaletteCallBacks
 *     Points to the DD_PALETTECALLBACKS structure to be initialized by the driver. 
 * 
 * Return Value
 * 
 * DrvEnableDirectDraw returns TRUE if it succeeds; otherwise, it returns FALSE.
 *
 */
BOOL APIENTRY DrvEnableDirectDraw(
    DHPDEV                  dhpdev,
    DD_CALLBACKS           *pCallBacks,
    DD_SURFACECALLBACKS    *pSurfaceCallBacks,
    DD_PALETTECALLBACKS    *pPaletteCallBacks
    )
{
    DISPDBG((0, "%s: %p, %p, %p, %p\n", __FUNCTION__, dhpdev, pCallBacks, pSurfaceCallBacks, pPaletteCallBacks));

    // Fill in the HAL Callback pointers
    memset(&pCallBacks, 0, sizeof(DD_CALLBACKS));
    pCallBacks->dwSize                = sizeof(DD_CALLBACKS);
    pCallBacks->dwFlags               = 0;

    /*
    pCallBacks->dwFlags               = DDHAL_CB32_CREATESURFACE;
    pCallBacks->CreateSurface         = DdCreateSurface;
    pCallBacks->CanCreateSurface      = DdCanCreateSurface;
    pCallBacks->WaitForVerticalBlank  = DdWaitForVerticalBlank;
    pCallBacks->GetScanLine           = DdGetScanLine;
    pCallBacks->MapMemory             = DdMapMemory;
    DDHAL_CB32_CANCREATESURFACE | DDHAL_CB32_WAITFORVERTICALBLANK | DDHAL_CB32_MAPMEMORY | DDHAL_CB32_GETSCANLINE 
    */
    /* Note: pCallBacks->SetMode & pCallBacks->DestroyDriver are unused in Windows 2000 and up */

    // Fill in the Surface Callback pointers
    memset(&pSurfaceCallBacks, 0, sizeof(DD_SURFACECALLBACKS));
    pSurfaceCallBacks->dwSize           = sizeof(DD_SURFACECALLBACKS);
    pSurfaceCallBacks->dwFlags          = 0;

    /*
    pSurfaceCallBacks->dwFlags          = DDHAL_SURFCB32_DESTROYSURFACE | DDHAL_SURFCB32_LOCK;
    pSurfaceCallBacks->DestroySurface   = DdDestroySurface;
    pSurfaceCallBacks->Lock             = DdLock;
    pSurfaceCallBacks->Flip             = DdFlip;
    pSurfaceCallBacks->GetBltStatus     = DdGetBltStatus;
    pSurfaceCallBacks->GetFlipStatus    = DdGetFlipStatus;
    pSurfaceCallBacks->Blt              = DdBlt;
    DDHAL_SURFCB32_FLIP | DDHAL_SURFCB32_BLT | DDHAL_SURFCB32_GETBLTSTATUS | DDHAL_SURFCB32_GETFLIPSTATUS;
    */

//    pSurfaceCallBacks.SetColorKey = DdSetColorKey;
//    pSurfaceCallBacks.dwFlags |= DDHAL_SURFCB32_SETCOLORKEY;

    memset(&pPaletteCallBacks, 0, sizeof(DD_PALETTECALLBACKS));
    pPaletteCallBacks->dwSize           = sizeof(DD_PALETTECALLBACKS);
    pPaletteCallBacks->dwFlags          = 0;

    return TRUE;
}

/**
 * DrvDisableDirectDraw
 * 
 * The DrvDisableDirectDraw function disables hardware for DirectDraw use.
 * 
 * Parameters
 * 
 * dhpdev
 *     Handle to the PDEV returned by the driver’s DrvEnablePDEV routine.
 *
 */
VOID APIENTRY DrvDisableDirectDraw( DHPDEV dhpdev)
{
    DISPDBG((0, "%s: %p\n", __FUNCTION__, dhpdev));
}


#if 0
/**
 * DdCreateSurface
 * 
 * The DdCreateSurface callback function creates a DirectDraw surface.
 * 
 * lpCreateSurface
 *     Points to a DD_CREATESURFACEDATA structure that contains the information required to create a surface.
 * 
 * Return Value
 * 
 * DdCreateSurface returns one of the following callback codes:
 * DDHAL_DRIVER_HANDLED
 * DDHAL_DRIVER_NOTHANDLED
 * 
 */
static DWORD APIENTRY DdCreateSurface(PDD_CREATESURFACEDATA  lpCreateSurface)
{
    lpCreateSurface->lpDD->fpVidMem = DDHAL_PLEASEALLOC_USERMEM;

    return DDHAL_DRIVER_NOTHANDLED;
}
#endif
