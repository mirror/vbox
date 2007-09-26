#ifdef VBOX_WITH_DDRAW

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
#undef CO_E_NOTINITIALIZED
#include <winerror.h>


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
    BOOL bDefineDDrawHeap = FALSE;
    DWORD cHeaps = 0;
    VIDEOMEMORY *pVm = NULL;

    DISPDBG((0, "%s: %p, %p, %p, %p, %p. %p\n", __FUNCTION__, dhpdev, pHalInfo, pdwNumHeaps, pvmList, pdwNumFourCCCodes, pdwFourCC));

    *pdwNumFourCCCodes = 0;
    *pdwNumHeaps       = 0;

    /* Setup the HAL driver caps. */
    pHalInfo->dwSize    = sizeof(DD_HALINFO);
    pHalInfo->dwFlags   = 0;

    if (!(pvmList && pdwFourCC)) 
    {
        memset(&pHalInfo->ddCaps, 0, sizeof(DDNTCORECAPS));
        pHalInfo->ddCaps.dwSize         = sizeof(DDNTCORECAPS);
        pHalInfo->ddCaps.dwVidMemTotal  = pDev->layout.cbDDRAWHeap;
        pHalInfo->ddCaps.dwVidMemFree   = pHalInfo->ddCaps.dwVidMemTotal;

        pHalInfo->ddCaps.dwCaps         = 0;
        pHalInfo->ddCaps.dwCaps2        = 0;

        /* Declare we can handle textures wider than the primary */
        pHalInfo->ddCaps.dwCaps2 |= DDCAPS2_WIDESURFACES;

        pHalInfo->ddCaps.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
        
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
        DISPDBG((0, "dwDisplayWidth                 %d\n", pHalInfo->vmiData.dwDisplayWidth));
        DISPDBG((0, "dwDisplayHeight                %d\n", pHalInfo->vmiData.dwDisplayHeight));
        DISPDBG((0, "lDisplayPitch                  %d\n", pHalInfo->vmiData.lDisplayPitch));
        DISPDBG((0, "dwRGBBitCount                  %d\n", pHalInfo->vmiData.ddpfDisplay.dwRGBBitCount));

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
    
    cHeaps = 0;

    /* Do we have sufficient videomemory to create an off-screen heap for DDraw? */
    if (pDev->layout.cbDDRAWHeap > 0)
    {
        bDefineDDrawHeap = TRUE;
        cHeaps++;
    }

    pDev->cHeaps = cHeaps;
    *pdwNumHeaps  = cHeaps;

    // If pvmList is not NULL then we can go ahead and fill out the VIDEOMEMORY
    // structures which define our requested heaps.

    if(pvmList) {

        pVm=pvmList;

        //
        // Snag a pointer to the video-memory list so that we can use it to
        // call back to DirectDraw to allocate video memory:
        //
        pDev->pvmList = pVm;

        //
        // Define the heap for DirectDraw
        //
        if ( bDefineDDrawHeap )
        {
            pVm->dwFlags        = VIDMEM_ISLINEAR ;
            pVm->fpStart        = pDev->layout.offDDRAWHeap;
            pVm->fpEnd          = pDev->layout.offDDRAWHeap + pDev->layout.cbDDRAWHeap - 1; /* inclusive */

            pVm->ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;
            DISPDBG((0, "fpStart %x fpEnd %x\n", pVm->fpStart, pVm->fpEnd));

            pVm++;
        }
    }

#if 0 /* not mandatory */
    /* DX5 and up */
    pHalInfo->GetDriverInfo = DdGetDriverInfo;
    pHalInfo->dwFlags |= DDHALINFO_GETDRIVERINFOSET;
#endif

#if 0
    /* No 3D capabilities */
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

    /* Fill in the HAL Callback pointers */
    pCallBacks->dwSize                = sizeof(DD_CALLBACKS);
    pCallBacks->dwFlags               = 0;

    pCallBacks->dwFlags               = DDHAL_CB32_CREATESURFACE | DDHAL_CB32_CANCREATESURFACE | DDHAL_CB32_MAPMEMORY;
    pCallBacks->CreateSurface         = DdCreateSurface;
    pCallBacks->CanCreateSurface      = DdCanCreateSurface;
    pCallBacks->MapMemory             = DdMapMemory;
    // pCallBacks->WaitForVerticalBlank  = DdWaitForVerticalBlank;
    // pCallBacks->GetScanLine           = DdGetScanLine;
    // DDHAL_CB32_WAITFORVERTICALBLANK | DDHAL_CB32_GETSCANLINE 
    /* Note: pCallBacks->SetMode & pCallBacks->DestroyDriver are unused in Windows 2000 and up */

    /* Fill in the Surface Callback pointers */
    pSurfaceCallBacks->dwSize           = sizeof(DD_SURFACECALLBACKS);
    pSurfaceCallBacks->dwFlags          = DDHAL_SURFCB32_LOCK | DDHAL_SURFCB32_UNLOCK;
    pSurfaceCallBacks->Lock             = DdLock;
    pSurfaceCallBacks->Unlock           = DdUnlock;

    /*
    pSurfaceCallBacks->dwFlags          = DDHAL_SURFCB32_DESTROYSURFACE | DDHAL_SURFCB32_LOCK; // DDHAL_SURFCB32_UNLOCK;
    pSurfaceCallBacks->DestroySurface   = DdDestroySurface;
    pSurfaceCallBacks->Flip             = DdFlip;
    pSurfaceCallBacks->GetBltStatus     = DdGetBltStatus;
    pSurfaceCallBacks->GetFlipStatus    = DdGetFlipStatus;
    pSurfaceCallBacks->Blt              = DdBlt;
    DDHAL_SURFCB32_FLIP | DDHAL_SURFCB32_BLT | DDHAL_SURFCB32_GETBLTSTATUS | DDHAL_SURFCB32_GETFLIPSTATUS;
    */

//    pSurfaceCallBacks.SetColorKey = DdSetColorKey;
//    pSurfaceCallBacks.dwFlags |= DDHAL_SURFCB32_SETCOLORKEY;

    /* Fill in the Palette Callback pointers */
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

/**
 * DdGetDriverInfo
 * 
 * The DdGetDriverInfo function queries the driver for additional DirectDraw and Direct3D functionality that the driver supports.
 * 
 * Parameters
 * lpGetDriverInfo
 *     Points to a DD_GETDRIVERINFODATA structure that contains the information required to perform the query.
 * 
 * Return Value
 * 
 * DdGetDriverInfo must return DDHAL_DRIVER_HANDLED. 
 *
 */
DWORD CALLBACK DdGetDriverInfo(DD_GETDRIVERINFODATA *lpData)
{
    PPDEV pDev = (PPDEV)lpData->dhpdev;
    DWORD dwSize;

    DISPDBG((0, "%s: %p\n", __FUNCTION__, lpData->dhpdev));

    /* Default to 'not supported' */
    lpData->ddRVal = DDERR_CURRENTLYNOTAVAIL;

    /* Fill in supported stuff */
    if (IsEqualIID(&lpData->guidInfo, &GUID_D3DCallbacks3))
    {
        DISPDBG((0, " -> GUID_D3DCallbacks3\n"));
    }
    else 
    if (IsEqualIID(&lpData->guidInfo, &GUID_D3DExtendedCaps))
    {
        DISPDBG((0, " -> GUID_D3DExtendedCaps\n"));
    }
    else 
    if (IsEqualIID(&lpData->guidInfo, &GUID_ZPixelFormats))
    {
        DISPDBG((0, " -> GUID_ZPixelFormats\n"));
    }
    else 
    if (IsEqualIID(&(lpData->guidInfo), &GUID_D3DParseUnknownCommandCallback))
    {
        DISPDBG((0, " -> GUID_D3DParseUnknownCommandCallback\n"));
    }
    else 
    if (IsEqualIID(&(lpData->guidInfo), &GUID_Miscellaneous2Callbacks))
    {
        DISPDBG((0, " -> GUID_Miscellaneous2Callbacks\n"));
    }
    else 
    if (IsEqualIID(&(lpData->guidInfo), &GUID_UpdateNonLocalHeap))
    {
        DISPDBG((0, " -> GUID_UpdateNonLocalHeap\n"));
    }
    else 
    if (IsEqualIID(&(lpData->guidInfo), &GUID_GetHeapAlignment))
    {
        DISPDBG((0, " -> GUID_GetHeapAlignment\n"));
    }
    else 
    if (IsEqualIID(&(lpData->guidInfo), &GUID_NTPrivateDriverCaps))
    {
        DD_NTPRIVATEDRIVERCAPS DDPrivateDriverCaps;

        DISPDBG((0, " -> GUID_NTPrivateDriverCaps\n"));

        memset(&DDPrivateDriverCaps, 0, sizeof(DDPrivateDriverCaps));
        DDPrivateDriverCaps.dwSize=sizeof(DDPrivateDriverCaps);

        DDPrivateDriverCaps.dwPrivateCaps = 0; /* DDHAL_PRIVATECAP_NOTIFYPRIMARYCREATION -> call CreateSurface for the primary surface */

        lpData->dwActualSize =sizeof(DDPrivateDriverCaps);

        dwSize = min(sizeof(DDPrivateDriverCaps),lpData->dwExpectedSize);
        memcpy(lpData->lpvData, &DDPrivateDriverCaps, dwSize);
        lpData->ddRVal = DD_OK;
    }
    else 
    if (IsEqualIID(&(lpData->guidInfo), &GUID_DDMoreSurfaceCaps))
    {
        DD_MORESURFACECAPS DDMoreSurfaceCaps;
        DDSCAPSEX   ddsCapsEx, ddsCapsExAlt;

        DISPDBG((0, " -> GUID_DDMoreSurfaceCaps\n"));

        // fill in everything until expectedsize...
        memset(&DDMoreSurfaceCaps, 0, sizeof(DDMoreSurfaceCaps));

        // Caps for heaps 2..n
        memset(&ddsCapsEx, 0, sizeof(ddsCapsEx));
        memset(&ddsCapsExAlt, 0, sizeof(ddsCapsEx));

        DDMoreSurfaceCaps.dwSize=lpData->dwExpectedSize;

        lpData->dwActualSize = lpData->dwExpectedSize;

        dwSize = min(sizeof(DDMoreSurfaceCaps),lpData->dwExpectedSize);
        memcpy(lpData->lpvData, &DDMoreSurfaceCaps, dwSize);

        // now fill in other heaps...
        while (dwSize < lpData->dwExpectedSize)
        {
            memcpy( (PBYTE)lpData->lpvData+dwSize,
                    &ddsCapsEx,
                    sizeof(DDSCAPSEX));
            dwSize += sizeof(DDSCAPSEX);
            memcpy( (PBYTE)lpData->lpvData+dwSize,
                    &ddsCapsExAlt,
                    sizeof(DDSCAPSEX));
            dwSize += sizeof(DDSCAPSEX);
        }

        lpData->ddRVal = DD_OK;
    }
    else 
    if (IsEqualIID(&(lpData->guidInfo), &GUID_DDStereoMode))
    {
        DISPDBG((0, " -> GUID_DDStereoMode\n"));
    }
    else 
    if (IsEqualIID(&(lpData->guidInfo), &GUID_NonLocalVidMemCaps))
    {
        DISPDBG((0, " -> GUID_NonLocalVidMemCaps\n"));
    } 
    else 
    if (IsEqualIID(&lpData->guidInfo, &GUID_NTCallbacks))
    {
        DD_NTCALLBACKS NtCallbacks;

        DISPDBG((0, " -> GUID_NTCallbacks\n"));
        memset(&NtCallbacks, 0, sizeof(NtCallbacks));

        dwSize = min(lpData->dwExpectedSize, sizeof(DD_NTCALLBACKS));

        NtCallbacks.dwSize           = dwSize;
        NtCallbacks.dwFlags          =   DDHAL_NTCB32_FREEDRIVERMEMORY
                                       | DDHAL_NTCB32_SETEXCLUSIVEMODE
                                       | DDHAL_NTCB32_FLIPTOGDISURFACE
                                       ;
        NtCallbacks.FreeDriverMemory = DdFreeDriverMemory;
        NtCallbacks.SetExclusiveMode = DdSetExclusiveMode;
        NtCallbacks.FlipToGDISurface = DdFlipToGDISurface;

        memcpy(lpData->lpvData, &NtCallbacks, dwSize);

        lpData->ddRVal = DD_OK;
    }
    else
    if (IsEqualIID(&lpData->guidInfo, &GUID_KernelCaps))
    {
        DISPDBG((0, " -> GUID_KernelCaps\n"));
    }
    else
    if (IsEqualIID(&lpData->guidInfo, &GUID_KernelCallbacks))
    {
        DISPDBG((0, " -> GUID_KernelCallbacks\n"));
    }
    else
    if (IsEqualIID(&lpData->guidInfo, &GUID_MotionCompCallbacks))
    {
        DISPDBG((0, " -> GUID_MotionCompCallbacks\n"));
    }
    else
    if (IsEqualIID(&lpData->guidInfo, &GUID_VideoPortCallbacks))
    {
        DISPDBG((0, " -> GUID_VideoPortCallbacks\n"));
    }
    else
    if (IsEqualIID(&lpData->guidInfo, &GUID_ColorControlCallbacks))
    {
        DISPDBG((0, " -> GUID_ColorControlCallbacks\n"));
    }
    else
    if (IsEqualIID(&lpData->guidInfo, &GUID_VideoPortCaps))
    {
        DISPDBG((0, " -> GUID_VideoPortCaps\n"));
    }
    else
    if (IsEqualIID(&lpData->guidInfo, &GUID_D3DCallbacks2))
    {
        DISPDBG((0, " -> GUID_D3DCallbacks2\n"));
    }
    else
    if (IsEqualIID(&lpData->guidInfo, &GUID_D3DCallbacks3))
    {
        DISPDBG((0, " -> GUID_D3DCallbacks3\n"));
    }

    /* Always return this */
    return DDHAL_DRIVER_HANDLED;
}

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
DWORD APIENTRY DdCreateSurface(PDD_CREATESURFACEDATA  lpCreateSurface)
{
    PPDEV pDev = (PPDEV)lpCreateSurface->lpDD->dhpdev;
    DD_SURFACE_LOCAL*   lpSurfaceLocal;
    DD_SURFACE_GLOBAL*  lpSurfaceGlobal;
    LPDDSURFACEDESC     lpSurfaceDesc;
    LONG                lPitch, lBpp;

    DISPDBG((0, "%s: %p\n", __FUNCTION__, pDev));

    lpSurfaceLocal                  = lpCreateSurface->lplpSList[0];
    lpSurfaceGlobal                 = lpSurfaceLocal->lpGbl;    
    lpSurfaceDesc                   = lpCreateSurface->lpDDSurfaceDesc;

    lpSurfaceGlobal->dwReserved1    = 0;

    if (lpSurfaceDesc->ddpfPixelFormat.dwFlags & DDPF_PALETTEINDEXED4)
    {
        lBpp = 4;
        lPitch = lpSurfaceGlobal->wWidth/2;
        lPitch = (lPitch + 31) & ~31;
    }
    else 
    if (lpSurfaceDesc->ddpfPixelFormat.dwFlags & DDPF_PALETTEINDEXED8)
    {
        lBpp = 8;
        lPitch = lpSurfaceGlobal->wWidth;
        lPitch = (lPitch + 31) & ~31;
    }
    else
    {
        lBpp   = lpSurfaceDesc->ddpfPixelFormat.dwRGBBitCount;
        lPitch = lpSurfaceGlobal->wWidth*(lBpp/8);
    }
    DISPDBG((0, "New surface (%d,%d)\n", lpSurfaceGlobal->wWidth, lpSurfaceGlobal->wHeight));
    DISPDBG((0, "BPP %d lPitch=%d\n", lBpp, lPitch));

    lpSurfaceGlobal->dwBlockSizeX   = lPitch;
    lpSurfaceGlobal->dwBlockSizeY   = lpSurfaceGlobal->wHeight;
    lpSurfaceGlobal->lPitch         = lPitch;

    //
    // Modify surface descriptions as appropriate and let Direct
    // Draw perform the allocation if the surface was not the primary
    //
    if (lpSurfaceLocal->ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE)
    {
        DISPDBG((0, "-> primary surface\n"));
        lpSurfaceGlobal->fpVidMem       = 0;
    }
    else
    {
        DISPDBG((0, "-> secondary surface\n"));
        lpSurfaceGlobal->fpVidMem       = DDHAL_PLEASEALLOC_BLOCKSIZE;
    }
    
    lpSurfaceDesc->lPitch   = lpSurfaceGlobal->lPitch;
    lpSurfaceDesc->dwFlags |= DDSD_PITCH;


    return DDHAL_DRIVER_NOTHANDLED;
}

/**
 * DdCanCreateSurface
 * 
 * The DdCanCreateSurface callback function indicates whether the driver can create a surface of the specified surface description.
 * 
 * 
 * Parameters
 * lpCanCreateSurface
 *     Points to the DD_CANCREATESURFACEDATA structure containing the information required for the driver to determine whether a surface can be created.
 * 
 * Return Value
 * 
 * DdCanCreateSurface returns one of the following callback codes:
 * 
 * DDHAL_DRIVER_HANDLED
 * DDHAL_DRIVER_NOTHANDLED
 *
 */
DWORD APIENTRY DdCanCreateSurface(PDD_CANCREATESURFACEDATA lpCanCreateSurface)
{
    PPDEV pDev = (PPDEV)lpCanCreateSurface->lpDD->dhpdev;

    PDD_SURFACEDESC lpDDS = lpCanCreateSurface->lpDDSurfaceDesc;

    DISPDBG((0, "%s: %p\n", __FUNCTION__, pDev));

    if (lpDDS->ddsCaps.dwCaps & DDSCAPS_ZBUFFER)
    {
        DISPDBG((0, "No Z-Bufer support\n"));
        lpCanCreateSurface->ddRVal = DDERR_INVALIDPIXELFORMAT;
        return DDHAL_DRIVER_HANDLED;
    }
    if (lpDDS->ddsCaps.dwCaps & DDSCAPS_TEXTURE)
    {
        DISPDBG((0, "No texture support\n"));
        lpCanCreateSurface->ddRVal = DDERR_INVALIDPIXELFORMAT;
        return DDHAL_DRIVER_HANDLED;
    }

    if (lpCanCreateSurface->bIsDifferentPixelFormat && (lpDDS->ddpfPixelFormat.dwFlags & DDPF_FOURCC))
    {
        DISPDBG((0, "FOURCC not supported\n"));
        lpCanCreateSurface->ddRVal = DDERR_INVALIDPIXELFORMAT;
        return DDHAL_DRIVER_HANDLED;
    }

    lpCanCreateSurface->ddRVal = DD_OK;
    return DDHAL_DRIVER_HANDLED;
}

// ***************************WIN NT ONLY**********************************
//
// DdMapMemory
//
// Maps application-modifiable portions of the frame buffer into the 
// user-mode address space of the specified process, or unmaps memory.
//
// DdMapMemory is called to perform memory mapping before the first call to 
// DdLock. The handle returned by the driver in fpProcess will be passed to 
// every DdLock call made on the driver. 
//
// DdMapMemory is also called to unmap memory after the last DdUnLock call is 
// made.
//
// To prevent driver crashes, the driver must not map any portion of the frame
// buffer that must not be modified by an application.
//
// Parameters
//      lpMapMemory 
//          Points to a DD_MAPMEMORYDATA structure that contains details for 
//          the memory mapping or unmapping operation. 
//
//          .lpDD 
//              Points to a DD_DIRECTDRAW_GLOBAL structure that represents 
//              the driver. 
//          .bMap 
//              Specifies the memory operation that the driver should perform. 
//              A value of TRUE indicates that the driver should map memory; 
//              FALSE means that the driver should unmap memory. 
//          .hProcess 
//              Specifies a handle to the process whose address space is 
//              affected. 
//          .fpProcess 
//              Specifies the location in which the driver should return the 
//              base address of the process's memory mapped space when bMap 
//              is TRUE. When bMap is FALSE, fpProcess contains the base 
//              address of the memory to be unmapped by the driver.
//          .ddRVal 
//              Specifies the location in which the driver writes the return 
//              value of the DdMapMemory callback. A return code of DD_OK 
//              indicates success. 
//
//-----------------------------------------------------------------------------

DWORD CALLBACK DdMapMemory(PDD_MAPMEMORYDATA lpMapMemory)
{
    PPDEV pDev = (PPDEV)lpMapMemory->lpDD->dhpdev;

    VIDEO_SHARE_MEMORY              ShareMemory;
    VIDEO_SHARE_MEMORY_INFORMATION  ShareMemoryInformation;
    DWORD                           ReturnedDataLength;

    DISPDBG((0, "%s: %p bMap %d\n", __FUNCTION__, pDev, lpMapMemory->bMap));

    if (lpMapMemory->bMap)
    {
        ShareMemory.ProcessHandle = lpMapMemory->hProcess;

        // 'RequestedVirtualAddress' isn't actually used for the SHARE IOCTL:

        ShareMemory.RequestedVirtualAddress = 0;

        // We map in starting at the top of the frame buffer:

        ShareMemory.ViewOffset = 0;
        ShareMemory.ViewSize   = pDev->cyScreen * pDev->lDeltaScreen;

        DISPDBG((0, "ViewSize = %x\n", ShareMemory.ViewSize));

        if (EngDeviceIoControl(pDev->hDriver,
                       IOCTL_VIDEO_SHARE_VIDEO_MEMORY,
                       &ShareMemory,
                       sizeof(VIDEO_SHARE_MEMORY),
                       &ShareMemoryInformation,
                       sizeof(VIDEO_SHARE_MEMORY_INFORMATION),
                       &ReturnedDataLength))
        {
            DISPDBG((0, "Failed IOCTL_VIDEO_SHARE_MEMORY"));

            lpMapMemory->ddRVal = DDERR_GENERIC;
     
            DISPDBG((0, "DdMapMemory: Exit GEN, DDHAL_DRIVER_HANDLED\n"));
            
            return(DDHAL_DRIVER_HANDLED);
        }

        lpMapMemory->fpProcess = 
                            (FLATPTR) ShareMemoryInformation.VirtualAddress;
    }
    else
    {
        ShareMemory.ProcessHandle           = lpMapMemory->hProcess;
        ShareMemory.ViewOffset              = 0;
        ShareMemory.ViewSize                = 0;
        ShareMemory.RequestedVirtualAddress = (VOID*) lpMapMemory->fpProcess;

        if (EngDeviceIoControl(pDev->hDriver,
                       IOCTL_VIDEO_UNSHARE_VIDEO_MEMORY,
                       &ShareMemory,
                       sizeof(VIDEO_SHARE_MEMORY),
                       NULL,
                       0,
                       &ReturnedDataLength))
        {
            DISPDBG((0, "Failed IOCTL_VIDEO_UNSHARE_MEMORY\n"));
        }
    }

    lpMapMemory->ddRVal = DD_OK;

    return(DDHAL_DRIVER_HANDLED);
}

/**
 * DdLock
 * 
 * The DdLock callback function locks a specified area of surface memory and provides a valid pointer to a block of memory associated with a surface.
 * 
 * Parameters
 * lpLock
 *     Points to a DD_LOCKDATA structure that contains the information required to perform the lockdown.
 * 
 * Return Value
 * 
 * DdLock returns one of the following callback codes:
 * 
 * DDHAL_DRIVER_HANDLED
 * DDHAL_DRIVER_NOTHANDLED
 *
 */
DWORD APIENTRY DdLock(PDD_LOCKDATA lpLock)
{
    PPDEV pDev = (PPDEV)lpLock->lpDD->dhpdev;

    DISPDBG((0, "%s: %p bHasRect = %d\n", __FUNCTION__, pDev, lpLock->bHasRect));
    
    pDev->ddLock.bHasRect = lpLock->bHasRect;
    
    if (lpLock->bHasRect)
    {
        DISPDBG((0, "%d,%d %dx%d\n", lpLock->rArea.left, lpLock->rArea.top, lpLock->rArea.right - lpLock->rArea.left, lpLock->rArea.bottom - lpLock->rArea.top));
        pDev->ddLock.rArea = lpLock->rArea;
    }

    // Because we correctly set 'fpVidMem' to be the offset into our frame
    // buffer when we created the surface, DirectDraw will automatically take
    // care of adding in the user-mode frame buffer address if we return
    // DDHAL_DRIVER_NOTHANDLED:
    lpLock->ddRVal = DD_OK;
    return DDHAL_DRIVER_NOTHANDLED;
}

/**
 * DdUnlock
 * 
 * The DdUnLock callback function releases the lock held on the specified surface.
 * 
 * Parameters
 * lpUnlock
 *     Points to a DD_UNLOCKDATA structure that contains the information required to perform the lock release. * 
 * 
 * Return Value
 * 
 * DdLock returns one of the following callback codes:
 * 
 * DDHAL_DRIVER_HANDLED
 * DDHAL_DRIVER_NOTHANDLED
 *
 */
DWORD APIENTRY DdUnlock(PDD_UNLOCKDATA lpUnlock)
{
    PPDEV pDev = (PPDEV)lpUnlock->lpDD->dhpdev;
    DISPDBG((0, "%s: %p\n", __FUNCTION__, pDev));

    if (pDev->ddLock.bHasRect)
    {
        DISPDBG((0, "%d,%d %dx%d\n", pDev->ddLock.rArea.left, pDev->ddLock.rArea.top, pDev->ddLock.rArea.right - pDev->ddLock.rArea.left, pDev->ddLock.rArea.bottom - pDev->ddLock.rArea.top));
        
        if (pDev->pInfo && vboxHwBufferBeginUpdate (pDev))
        {
            vbvaReportDirtyRect (pDev, &pDev->ddLock.rArea);

            if (  pDev->pInfo->hostEvents.fu32Events
                & VBOX_VIDEO_INFO_HOST_EVENTS_F_VRDP_RESET)
            {
                vrdpReset (pDev);

                pDev->pInfo->hostEvents.fu32Events &=
                          ~VBOX_VIDEO_INFO_HOST_EVENTS_F_VRDP_RESET;
            }

            if (pDev->vbva.pVbvaMemory->fu32ModeFlags
                & VBVA_F_MODE_VRDP)
            {
                vrdpReportDirtyRect (pDev, &pDev->ddLock.rArea);
            }

            vboxHwBufferEndUpdate (pDev);
        }
        pDev->ddLock.bHasRect = 0;
    }

    lpUnlock->ddRVal = DD_OK;
    return DDHAL_DRIVER_NOTHANDLED;
}

/**
 * DdDestroySurface
 * 
 * The DdDestroySurface callback function destroys a DirectDraw surface.
 * 
 * Parameters
 * lpDestroySurface
 *     Points to a DD_DESTROYSURFACEDATA structure that contains the information needed to destroy a surface.
 * 
 * Return Value
 * 
 * DdDestroySurface returns one of the following callback codes:
 * 
 * DDHAL_DRIVER_HANDLED
 * DDHAL_DRIVER_NOTHANDLED
 *
 */
DWORD APIENTRY DdDestroySurface(PDD_DESTROYSURFACEDATA lpDestroySurface)
{
    PPDEV pDev = (PPDEV)lpDestroySurface->lpDD->dhpdev;
    DISPDBG((0, "%s: %p\n", __FUNCTION__, pDev));

    lpDestroySurface->ddRVal = DD_OK;
    return DDHAL_DRIVER_HANDLED;
}


//-----------------------------------------------------------------------------
//
// DdSetExclusiveMode
//
// This function is called by DirectDraw when we switch from the GDI surface,
// to DirectDraw exclusive mode, e.g. to run a game in fullcreen mode.
// You only need to implement this function when you are using the
// 'HeapVidMemAllocAligned' function and allocate memory for Device Bitmaps
// and DirectDraw surfaces from the same heap.
//
// We use this call to disable GDI DeviceBitMaps when we are running in
// DirectDraw exclusive mode. Otherwise a DD app gets confused if both GDI and
// DirectDraw allocate memory from the same heap.
//
// See also DdFlipToGDISurface.
//
//-----------------------------------------------------------------------------


DWORD APIENTRY DdSetExclusiveMode(PDD_SETEXCLUSIVEMODEDATA lpSetExclusiveMode)
{
    PPDEV pDev = (PPDEV)lpSetExclusiveMode->lpDD->dhpdev;
    DISPDBG((0, "%s: %p\n", __FUNCTION__, pDev));

    // remember setting of exclusive mode in pDev,
    // so GDI can stop to promote DeviceBitmaps into
    // video memory

    pDev->bDdExclusiveMode = lpSetExclusiveMode->dwEnterExcl;

    lpSetExclusiveMode->ddRVal = DD_OK;

    return DDHAL_DRIVER_HANDLED;
}

//-----------------------------------------------------------------------------
//
// DWORD DdFlipToGDISurface
//
// This function is called by DirectDraw when it flips to the surface on which
// GDI can write to.
//
//-----------------------------------------------------------------------------

DWORD APIENTRY DdFlipToGDISurface(PDD_FLIPTOGDISURFACEDATA lpFlipToGDISurface)
{
    PPDEV pDev = (PPDEV)lpFlipToGDISurface->lpDD->dhpdev;
    DISPDBG((0, "%s: %p\n", __FUNCTION__, pDev));

    pDev->dwNewDDSurfaceOffset = 0xffffffff;

    lpFlipToGDISurface->ddRVal = DD_OK;

    //
    //  we return NOTHANDLED, then the ddraw runtime takes
    //  care that we flip back to the primary...
    //
    return DDHAL_DRIVER_NOTHANDLED;
}
//-----------------------------------------------------------------------------
//
// DWORD DdFreeDriverMemory
//
// This function called by DirectDraw when it's running low on memory in
// our heap.  You only need to implement this function if you use the
// DirectDraw 'HeapVidMemAllocAligned' function in your driver, and you
// can boot those allocations out of memory to make room for DirectDraw.
//
//-----------------------------------------------------------------------------

DWORD APIENTRY DdFreeDriverMemory(PDD_FREEDRIVERMEMORYDATA lpFreeDriverMemory)
{
    PPDEV pDev = (PPDEV)lpFreeDriverMemory->lpDD->dhpdev;
    DISPDBG((0, "%s: %p\n", __FUNCTION__, pDev));

    lpFreeDriverMemory->ddRVal = DDERR_OUTOFMEMORY;
    return DDHAL_DRIVER_HANDLED;
}


#endif /* VBOX_WITH_DDRAW */
