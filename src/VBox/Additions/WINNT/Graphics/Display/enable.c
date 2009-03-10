/******************************Module*Header*******************************\
*
* Copyright (C) 2006-2007 Sun Microsystems, Inc.
*
* This file is part of VirtualBox Open Source Edition (OSE), as
* available from http://www.virtualbox.org. This file is free software;
* you can redistribute it and/or modify it under the terms of the GNU
* General Public License (GPL) as published by the Free Software
* Foundation, in version 2 as it comes in the "COPYING" file of the
* VirtualBox OSE distribution. VirtualBox OSE is distributed in the
* hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
*
* Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
* Clara, CA 95054 USA or visit http://www.sun.com if you need
* additional information or have any questions.
*/
/*
* Based in part on Microsoft DDK sample code
*
*                           *******************
*                           * GDI SAMPLE CODE *
*                           *******************
*
* Module Name: enable.c
*
* This module contains the functions that enable and disable the
* driver, the pdev, and the surface.
*
* Copyright (c) 1992-1998 Microsoft Corporation
\**************************************************************************/

#include "driver.h"
#include "dd.h"
#include <VBoxDisplay.h>

// The driver function table with all function index/address pairs

// Hook functions to track dirty rectangles and generate RDP orders.
// NT4 functions
DRVFN gadrvfn_nt4[] = {
    {   INDEX_DrvEnablePDEV,            (PFN) DrvEnablePDEV         },	//  0
    {   INDEX_DrvCompletePDEV,          (PFN) DrvCompletePDEV       },	//  1
    {   INDEX_DrvDisablePDEV,           (PFN) DrvDisablePDEV        },	//  2
    {   INDEX_DrvEnableSurface,         (PFN) DrvEnableSurface      },	//  3
    {   INDEX_DrvDisableSurface,        (PFN) DrvDisableSurface     },	//  4
    {   INDEX_DrvAssertMode,            (PFN) DrvAssertMode         },	//  5
    {   INDEX_DrvOffset,                (PFN) DrvOffset             },  //  6
    {   INDEX_DrvDisableDriver,         (PFN) DrvDisableDriver      },  //  8
//    {   INDEX_DrvCreateDeviceBitmap,    (PFN) DrvCreateDeviceBitmap },  // 10
//    {   INDEX_DrvDeleteDeviceBitmap,    (PFN) DrvDeleteDeviceBitmap },  // 11
    {   INDEX_DrvRealizeBrush,          (PFN) DrvRealizeBrush       },  // 12
    {   INDEX_DrvDitherColor,           (PFN) DrvDitherColor        },	// 13
    {   INDEX_DrvStrokePath,            (PFN) DrvStrokePath         },	// 14
    {   INDEX_DrvFillPath,              (PFN) DrvFillPath           },	// 15
    {   INDEX_DrvPaint,                 (PFN) DrvPaint              },	// 17
    {   INDEX_DrvBitBlt,                (PFN) DrvBitBlt             },	// 18
    {   INDEX_DrvCopyBits,              (PFN) DrvCopyBits           },	// 19
    {   INDEX_DrvStretchBlt,            (PFN) DrvStretchBlt,        },	// 20
    {   INDEX_DrvSetPalette,            (PFN) DrvSetPalette         },	// 22
    {   INDEX_DrvTextOut,               (PFN) DrvTextOut            },	// 23
    {   INDEX_DrvEscape,                (PFN) DrvEscape             },	// 24
    {   INDEX_DrvSetPointerShape,       (PFN) DrvSetPointerShape    },	// 29
    {   INDEX_DrvMovePointer,           (PFN) DrvMovePointer        },	// 30
    {   INDEX_DrvLineTo,                (PFN) DrvLineTo             },	// 31
    {   INDEX_DrvSynchronize,           (PFN) DrvSynchronize        },  // 38
    {   INDEX_DrvSaveScreenBits,        (PFN) DrvSaveScreenBits     },  // 40
    {   INDEX_DrvGetModes,              (PFN) DrvGetModes           },	// 41
#ifdef VBOX_WITH_DDRAW
    {   INDEX_DrvGetDirectDrawInfo,     (PFN) DrvGetDirectDrawInfo  },	// 59 0x3b
    {   INDEX_DrvEnableDirectDraw,      (PFN) DrvEnableDirectDraw   },	// 60 0x3c
    {   INDEX_DrvDisableDirectDraw,     (PFN) DrvDisableDirectDraw  },	// 61 0x3d
#endif
};
/* Experimental begin */
BOOL APIENTRY DrvResetPDEV(DHPDEV dhpdevOld, DHPDEV dhpdevNew)
{
    DISPDBG((0, "Experimental %s: %p, %p\n", __FUNCTION__, dhpdevOld, dhpdevNew));
    return TRUE;
}

BOOL DrvNineGrid (PVOID x1, PVOID x2, PVOID x3, PVOID x4, PVOID x5, PVOID x6, PVOID x7, PVOID x8, PVOID x9)
{
    DISPDBG((0, "Experimental %s: %p, %p, %p, %p, %p, %p, %p, %p, %p\n", __FUNCTION__, x1, x2, x3, x4, x5, x6, x7, x8, x9));
    return FALSE;
}

VOID APIENTRY DrvDestroyFont(FONTOBJ *pfo)
{
    DISPDBG((0, "Experimental %s: %p\n", __FUNCTION__, pfo));
}

ULONG APIENTRY DrvEscape(SURFOBJ *pso, ULONG iEsc, ULONG cjIn, PVOID pvIn, ULONG cjOut, PVOID pvOut)
{
    PDEV*   ppdev = (PDEV*) pso->dhpdev;

    DISPDBG((0, "%s: %p, %p, %p, %p, %p, %p\n", __FUNCTION__, pso, iEsc, cjIn, pvIn, cjOut, pvOut));

    switch(iEsc)
    {
#ifdef VBOX_WITH_OPENGL
    case OPENGL_GETINFO:
    {
        if (    cjOut >= sizeof(OPENGL_INFO) 
            &&  pvOut) 
        { 
            POPENGL_INFO pInfo = (POPENGL_INFO)pvOut; 

            pInfo->dwVersion        = 2; 
            pInfo->dwDriverVersion  = 1; 
            pInfo->szDriverName[0]  = 'V'; 
            pInfo->szDriverName[1]  = 'B'; 
            pInfo->szDriverName[2]  = 'o'; 
            pInfo->szDriverName[3]  = 'x'; 
            pInfo->szDriverName[4]  = 'O'; 
            pInfo->szDriverName[5]  = 'G'; 
            pInfo->szDriverName[6]  = 'L'; 
            pInfo->szDriverName[7]  = 0; 

            DISPDBG((0, "OPENGL_GETINFO\n")); 
            return cjOut; 
        } 
        else 
            DISPDBG((0, "OPENGL_GETINFO invalid size %d\n", cjOut));         /* It doesn't matter that we fail here. Opengl32 will fall back to software rendering when this escape is not supported. */
        break;
    }
#endif

    case VBOXESC_ISVRDPACTIVE:
    {
        ULONG ret = 0;

#ifndef VBOX_WITH_HGSMI
        if (ppdev && ppdev->pInfo && vboxHwBufferBeginUpdate (ppdev))
        {
            if (ppdev->vbva.pVbvaMemory->fu32ModeFlags
                & VBVA_F_MODE_VRDP)
            {
                ret = 1;
            }
            DISPDBG((0, "VBOXESC_ISVRDPACTIVE -> %d (%x)\n", ret, ppdev->vbva.pVbvaMemory->fu32ModeFlags));
            vboxHwBufferEndUpdate (ppdev);
        }
        else
            DISPDBG((0, "VBOXESC_ISVRDPACTIVE -> 0\n"));
#else
        if (ppdev && ppdev->pVBVA)
        {
            if (ppdev->pVBVA->u32HostEvents & VBVA_F_MODE_VRDP)
            {
                ret = 1;
            }
            DISPDBG((0, "VBOXESC_ISVRDPACTIVE -> %d (%x)\n", ret, ppdev->pVBVA->u32HostEvents));
        }
        else
            DISPDBG((0, "VBOXESC_ISVRDPACTIVE -> 0\n"));
#endif  /* VBOX_WITH_HGSMI */
        return ret;
    }

    case VBOXESC_SETVISIBLEREGION:
    {
        LPRGNDATA lpRgnData = (LPRGNDATA)pvIn;

        DISPDBG((0, "VBOXESC_SETVISIBLEREGION\n"));

        if (    cjIn >= sizeof(RGNDATAHEADER)
            &&  pvIn
            &&  lpRgnData->rdh.dwSize == sizeof(RGNDATAHEADER)
            &&  lpRgnData->rdh.iType  == RDH_RECTANGLES
            &&  cjIn == lpRgnData->rdh.nCount * sizeof(RECT) + sizeof(RGNDATAHEADER))
        {
            DWORD   ulReturn, i;
            PRTRECT pRTRect;
            RECT   *pRect = (RECT *)&lpRgnData->Buffer;

            pRTRect = (PRTRECT) EngAllocMem(0, lpRgnData->rdh.nCount*sizeof(RTRECT), ALLOC_TAG);
            for (i=0;i<lpRgnData->rdh.nCount;i++)
            {
                DISPDBG((0, "New visible rectangle (%d,%d) (%d,%d)\n", pRect[i].left, pRect[i].bottom, pRect[i].right, pRect[i].top));
                pRTRect[i].xLeft   = pRect[i].left;
                pRTRect[i].yBottom = pRect[i].bottom;
                pRTRect[i].xRight  = pRect[i].right;
                pRTRect[i].yTop    = pRect[i].top;
            }

            if (EngDeviceIoControl(ppdev->hDriver,
                                   IOCTL_VIDEO_VBOX_SETVISIBLEREGION,
                                   pRTRect,
                                   lpRgnData->rdh.nCount*sizeof(RTRECT),
                                   NULL,
                                   0,
                                   &ulReturn))
            {
                DISPDBG((0, "DISP DrvAssertMode failed IOCTL_VIDEO_VBOX_SETVISIBLEREGION\n"));
                return 0;
            }
            else
            {
                DISPDBG((0, "DISP IOCTL_VIDEO_VBOX_SETVISIBLEREGION successful\n"));
                return 1;
            }

        }
        else
        {
            if (pvIn)
                DISPDBG((0, "check failed rdh.dwSize=%x iType=%d size=%d expected size=%d\n", lpRgnData->rdh.dwSize, lpRgnData->rdh.iType, cjIn, lpRgnData->rdh.nCount * sizeof(RECT) + sizeof(RGNDATAHEADER)));
        }

        break;
    }

    case QUERYESCSUPPORT:
        if (    cjIn == sizeof(DWORD)
            &&  pvIn)
        {
            DWORD nEscapeQuery = *(DWORD *)pvIn;

            switch(nEscapeQuery)
            {
#ifdef VBOX_WITH_OPENGL
            case OPENGL_GETINFO:
                return 1;
#endif
            default:
                DISPDBG((0, "QUERYESCSUPPORT %d unsupported\n", nEscapeQuery));
                break;
            }
        }
        else
            DISPDBG((0, "QUERYESCSUPPORT invalid size %d\n", cjOut));
        break;

    default:
        DISPDBG((0, "Unsupported Escape %d\n", iEsc));
        break;
    }
    return 0;
}
    
BOOL DrvConnect (PVOID x1, PVOID x2, PVOID x3, PVOID x4)
{
    DISPDBG((0, "Experimental %s: %p, %p, %p, %p\n", __FUNCTION__, x1, x2, x3, x4));
    return TRUE;
}

BOOL DrvDisconnect (PVOID x1, PVOID x2)
{
    DISPDBG((0, "Experimental %s: %p, %p\n", __FUNCTION__, x1, x2));
    return FALSE;
}

BOOL DrvReconnect (PVOID x1, PVOID x2)
{
    DISPDBG((0, "Experimental %s: %p, %p\n", __FUNCTION__, x1, x2));
    return FALSE;
}

BOOL DrvShadowConnect (PVOID x1, PVOID x2)
{
    DISPDBG((0, "Experimental %s: %p, %p\n", __FUNCTION__, x1, x2));
    return FALSE;
}

BOOL DrvShadowDisconnect (PVOID x1, PVOID x2)
{
    DISPDBG((0, "Experimental %s: %p, %p\n", __FUNCTION__, x1, x2));
    return FALSE;
}


/* Experimental end */

// W2K,XP functions
DRVFN gadrvfn_nt5[] = {
    {   INDEX_DrvEnablePDEV,            (PFN) DrvEnablePDEV         },	//  0 0x0
    {   INDEX_DrvCompletePDEV,          (PFN) DrvCompletePDEV       },	//  1 0x1
    {   INDEX_DrvDisablePDEV,           (PFN) DrvDisablePDEV        },	//  2 0x2
    {   INDEX_DrvEnableSurface,         (PFN) DrvEnableSurface      },	//  3 0x3
    {   INDEX_DrvDisableSurface,        (PFN) DrvDisableSurface     },	//  4 0x4
    {   INDEX_DrvAssertMode,            (PFN) DrvAssertMode         },	//  5 0x5
    {   INDEX_DrvDisableDriver,         (PFN) DrvDisableDriver      },  //  8 0x8
//    {   INDEX_DrvCreateDeviceBitmap,    (PFN) DrvCreateDeviceBitmap },  // 10
//    {   INDEX_DrvDeleteDeviceBitmap,    (PFN) DrvDeleteDeviceBitmap },  // 11
    {   INDEX_DrvRealizeBrush,          (PFN) DrvRealizeBrush       },  // 12 0xc
    {   INDEX_DrvDitherColor,           (PFN) DrvDitherColor        },	// 13 0xd
    {   INDEX_DrvStrokePath,            (PFN) DrvStrokePath         },	// 14 0xe
    {   INDEX_DrvFillPath,              (PFN) DrvFillPath           },	// 15 0xf
//    {   INDEX_DrvStrokeAndFillPath,     (PFN) DrvStrokeAndFillPath  },	// 16 0x10
    {   INDEX_DrvPaint,                 (PFN) DrvPaint              },	// 17 0x11
    {   INDEX_DrvBitBlt,                (PFN) DrvBitBlt             },	// 18 0x12
    {   INDEX_DrvCopyBits,              (PFN) DrvCopyBits           },	// 19 0x13
    {   INDEX_DrvStretchBlt,            (PFN) DrvStretchBlt,        },	// 20 0x14
    {   INDEX_DrvSetPalette,            (PFN) DrvSetPalette         },	// 22 0x16
    {   INDEX_DrvTextOut,               (PFN) DrvTextOut            },	// 23 0x17
    {   INDEX_DrvEscape,                (PFN) DrvEscape             },	// 24 0x18
    {   INDEX_DrvSetPointerShape,       (PFN) DrvSetPointerShape    },	// 29 0x1d
    {   INDEX_DrvMovePointer,           (PFN) DrvMovePointer        },	// 30 0x1e
    {   INDEX_DrvLineTo,                (PFN) DrvLineTo             },	// 31 0x1f
    {   INDEX_DrvSynchronize,           (PFN) DrvSynchronize        },  // 38 0x26
    {   INDEX_DrvSaveScreenBits,        (PFN) DrvSaveScreenBits     },  // 40 0x28
    {   INDEX_DrvGetModes,              (PFN) DrvGetModes           },	// 41 0x29
#ifdef VBOX_WITH_DDRAW
    {   INDEX_DrvGetDirectDrawInfo,     (PFN) DrvGetDirectDrawInfo  },	// 59 0x3b
    {   INDEX_DrvEnableDirectDraw,      (PFN) DrvEnableDirectDraw   },	// 60 0x3c
    {   INDEX_DrvDisableDirectDraw,     (PFN) DrvDisableDirectDraw  },	// 61 0x3d
    {   INDEX_DrvDeriveSurface,         (PFN) DrvDeriveSurface      },  // 85 0x55
#endif
    {   INDEX_DrvNotify,                (PFN) DrvNotify             },	// 87 0x57
//     /* Experimental. */
//     {   0x7,                            (PFN) DrvResetPDEV          },	// 0x7
//     {   0x5b,                           (PFN) DrvNineGrid           },	// 0x5b
//     {   0x2b,                           (PFN) DrvDestroyFont        },	// 0x2b
//     {   0x18,                           (PFN) DrvEscape             },	// 0x18
//     {   0x4d,                           (PFN) DrvConnect            },	// 0x4d
//     {   0x4e,                           (PFN) DrvDisconnect         },	// 0x4e
//     {   0x4f,                           (PFN) DrvReconnect          },	// 0x4f
//     {   0x50,                           (PFN) DrvShadowConnect      },	// 0x50
//     {   0x51,                           (PFN) DrvShadowDisconnect   },	// 0x51
    
};

// Required hook bits will be set up according to DDI version
static ULONG gflHooks = 0;
       BOOL  g_bOnNT40 = TRUE;      /* assume NT4 guest by default */

#define HOOKS_BMF8BPP  gflHooks
#define HOOKS_BMF16BPP gflHooks
#define HOOKS_BMF24BPP gflHooks
#define HOOKS_BMF32BPP gflHooks

#ifndef VBOX_WITH_HGSMI
HSEMAPHORE ghsemHwBuffer = 0;
#endif /* !VBOX_WITH_HGSMI */

/******************************Public*Routine******************************\
* DrvEnableDriver
*
* Enables the driver by retrieving the drivers function table and version.
*
\**************************************************************************/

BOOL DrvEnableDriver(ULONG iEngineVersion, ULONG cj, PDRVENABLEDATA pded)
{
// Engine Version is passed down so future drivers can support previous
// engine versions.  A next generation driver can support both the old
// and new engine conventions if told what version of engine it is
// working with.  For the first version the driver does nothing with it.

    DISPDBG((0, "VBoxDisp::DrvEnableDriver called. iEngine version = %08X\n", iEngineVersion));

    // Set up hook flags to intercept all functions which can generate VRDP orders
    gflHooks = HOOK_BITBLT | HOOK_TEXTOUT | HOOK_FILLPATH |
               HOOK_COPYBITS | HOOK_STROKEPATH | HOOK_LINETO | /* HOOK_STROKEANDFILLPATH | */
#ifdef VBOX_NEW_SURFACE_CODE
               HOOK_PAINT | HOOK_STRETCHBLT | HOOK_SYNCHRONIZE;
#else
               HOOK_PAINT | HOOK_STRETCHBLT | HOOK_SYNCHRONIZEACCESS;
#endif
    // Set up g_bOnNT40 based on the value in iEngineVersion
    if(iEngineVersion >= DDI_DRIVER_VERSION_NT5)
        g_bOnNT40 = FALSE;

// Fill in as much as we can.

    if (cj >= sizeof(DRVENABLEDATA))
        pded->pdrvfn = (iEngineVersion >= DDI_DRIVER_VERSION_NT5)?
             gadrvfn_nt5:
             gadrvfn_nt4;


    if (cj >= (sizeof(ULONG) * 2))
        pded->c = (iEngineVersion >= DDI_DRIVER_VERSION_NT5)?
            sizeof(gadrvfn_nt5) / sizeof(DRVFN):
            sizeof(gadrvfn_nt4) / sizeof(DRVFN);

// DDI version this driver was targeted for is passed back to engine.
// Future graphic's engine may break calls down to old driver format.

    if (cj >= sizeof(ULONG))
        pded->iDriverVersion = (iEngineVersion >= DDI_DRIVER_VERSION_NT5)?
            DDI_DRIVER_VERSION_NT5:
            DDI_DRIVER_VERSION_NT4;

#ifndef VBOX_WITH_HGSMI
    if (!ghsemHwBuffer)
    {
        ghsemHwBuffer = EngCreateSemaphore ();
    }
#endif /* !VBOX_WITH_HGSMI */

    return(TRUE);
}

/******************************Public*Routine******************************\
* DrvDisableDriver
*
* Tells the driver it is being disabled. Release any resources allocated in
* DrvEnableDriver.
*
\**************************************************************************/

VOID DrvDisableDriver(VOID)
{
    DISPDBG((0, "VBoxDisp::DrvDisableDriver called.\n"));

#ifndef VBOX_WITH_HGSMI
    if (ghsemHwBuffer)
    {
        EngDeleteSemaphore (ghsemHwBuffer);
        ghsemHwBuffer = NULL;
    }
#endif /* !VBOX_WITH_HGSMI */

    return;
}

/******************************Public*Routine******************************\
* DrvEnablePDEV
*
* DDI function, Enables the Physical Device.
*
* Return Value: device handle to pdev.
*
\**************************************************************************/

DHPDEV DrvEnablePDEV(
DEVMODEW   *pDevmode,       // Pointer to DEVMODE
PWSTR       pwszLogAddress, // Logical address
ULONG       cPatterns,      // number of patterns
HSURF      *ahsurfPatterns, // return standard patterns
ULONG       cjGdiInfo,      // Length of memory pointed to by pGdiInfo
ULONG      *pGdiInfo,       // Pointer to GdiInfo structure
ULONG       cjDevInfo,      // Length of following PDEVINFO structure
DEVINFO    *pDevInfo,       // physical device information structure
HDEV        hdev,           // HDEV, used for callbacks
PWSTR       pwszDeviceName, // DeviceName - not used
HANDLE      hDriver)        // Handle to base driver
{
    GDIINFO GdiInfo;
    DEVINFO DevInfo;
    PPDEV   ppdev = (PPDEV) NULL;

    DISPDBG((0, "VBoxDisp::DrvEnablePDEV called\n"));
    
    UNREFERENCED_PARAMETER(pwszLogAddress);
    UNREFERENCED_PARAMETER(pwszDeviceName);
    
    RtlZeroMemory(&DevInfo, sizeof (DEVINFO));
    RtlZeroMemory(&GdiInfo, sizeof (GDIINFO));

    // Allocate a physical device structure.

    ppdev = (PPDEV) EngAllocMem(0, sizeof(PDEV), ALLOC_TAG);

    if (ppdev == (PPDEV) NULL)
    {
        DISPDBG((0, "DISP DrvEnablePDEV failed EngAllocMem\n"));
        return((DHPDEV) 0);
    }

    memset(ppdev, 0, sizeof(PDEV));

    // Save the screen handle in the PDEV.

    ppdev->hDriver = hDriver;
    
    // Get the current screen mode information.  Set up device caps and devinfo.

    if (!bInitPDEV(ppdev, pDevmode, &GdiInfo, &DevInfo))
    {
        DISPDBG((0,"DISP DrvEnablePDEV failed\n"));
        goto error_free;
    }

    // Initialize the cursor information.

    if (!bInitPointer(ppdev, &DevInfo))
    {
        // Not a fatal error...
        DISPDBG((0, "DrvEnablePDEV failed bInitPointer\n"));
    }
    
    // Initialize palette information.

    if (!bInitPaletteInfo(ppdev, &DevInfo))
    {
        DISPDBG((0, "DrvEnablePDEV failed bInitPalette\n"));
        goto error_free;
    }
    
//    // Start a thread that will process notifications from VMMDev
//    if (!bInitNotificationThread(ppdev))
//    {
//        DISPDBG((0, "DrvEnablePDEV failed bInitNotificationThread\n"));
//        goto error_free;
//    }
    
    // Copy the devinfo into the engine buffer.
    
    DISPDBG((0, "VBoxDisp::DrvEnablePDEV: sizeof(DEVINFO) = %d, cjDevInfo = %d, alpha = %d\n", sizeof(DEVINFO), cjDevInfo, DevInfo.flGraphicsCaps2 & GCAPS2_ALPHACURSOR));
    
// @todo seems to be not necessary. these bits are initialized in screen.c    DevInfo.flGraphicsCaps |= GCAPS_OPAQUERECT       |
//                              GCAPS_DITHERONREALIZE  |
//                              GCAPS_PALMANAGED       |
//                              GCAPS_ALTERNATEFILL    |
//                              GCAPS_WINDINGFILL      |
//                              GCAPS_MONO_DITHER      |
//                              GCAPS_COLOR_DITHER     |
//                              GCAPS_ASYNCMOVE;
//                              
//    DevInfo.flGraphicsCaps |= GCAPS_DITHERONREALIZE;
    
    DevInfo.flGraphicsCaps2 |= GCAPS2_RESERVED1; /* @todo figure out what is this. */

    memcpy(pDevInfo, &DevInfo, min(sizeof(DEVINFO), cjDevInfo));

    // Set the pdevCaps with GdiInfo we have prepared to the list of caps for this
    // pdev.

    memcpy(pGdiInfo, &GdiInfo, min(cjGdiInfo, sizeof(GDIINFO)));

    DISPDBG((0, "VBoxDisp::DrvEnablePDEV completed %x\n", ppdev));
    
    return((DHPDEV) ppdev);

    // Error case for failure.
error_free:
    EngFreeMem(ppdev);
    return((DHPDEV) 0);
}

/******************************Public*Routine******************************\
* DrvCompletePDEV
*
* Store the HPDEV, the engines handle for this PDEV, in the DHPDEV.
*
\**************************************************************************/

VOID DrvCompletePDEV(DHPDEV dhpdev, HDEV hdev)
{
    DISPDBG((0, "VBoxDisp::DrvCompletePDEV called %x\n", dhpdev));
    ((PPDEV) dhpdev)->hdevEng = hdev;
}

/******************************Public*Routine******************************\
* DrvDisablePDEV
*
* Release the resources allocated in DrvEnablePDEV.  If a surface has been
* enabled DrvDisableSurface will have already been called.
*
\**************************************************************************/

VOID DrvDisablePDEV(DHPDEV dhpdev)
{
    DISPDBG((0, "VBoxDisp::DrvDisablePDEV called %x\n", dhpdev));
//    vStopNotificationThread ((PPDEV) dhpdev);
    vDisablePalette((PPDEV) dhpdev);
    
    /* Free the driver's VBVA resources. */
    vboxVbvaDisable ((PPDEV) dhpdev);

    EngFreeMem(dhpdev);
}

/******************************Public*Routine******************************\
* VOID DrvOffset
*
* DescriptionText
*
\**************************************************************************/

BOOL DrvOffset(SURFOBJ *pso, LONG x, LONG y, FLONG flReserved)
{
    PDEV*   ppdev = (PDEV*) pso->dhpdev;

    DISPDBG((0, "VBoxDisp::DrvOffset %x %x %x\n", x, y, flReserved));

    // Add back last offset that we subtracted.  I could combine the next
    // two statements, but I thought this was more clear.  It's not
    // performance critical anyway.

    ppdev->pjScreen += ((ppdev->ptlOrg.y * ppdev->lDeltaScreen) +
                        (ppdev->ptlOrg.x * ((ppdev->ulBitCount+1) >> 3)));

    // Subtract out new offset

    ppdev->pjScreen -= ((y * ppdev->lDeltaScreen) +
                        (x * ((ppdev->ulBitCount+1) >> 3)));

    ppdev->ptlOrg.x = x;
    ppdev->ptlOrg.y = y;

    return(TRUE);
}

/******************************Public*Routine******************************\
* DrvEnableSurface
*
* Enable the surface for the device.  Hook the calls this driver supports.
*
* Return: Handle to the surface if successful, 0 for failure.
*
\**************************************************************************/

HSURF DrvEnableSurface(DHPDEV dhpdev)
{
    PPDEV ppdev;
    HSURF hsurf;
    SIZEL sizl;
    ULONG ulBitmapType;
    FLONG flHooks;
#ifdef VBOX_NEW_SURFACE_CODE
    PVBOXSURF psurf;
#endif
    DISPDBG((0, "DISP DrvEnableSurface called\n"));
        
    // Create engine bitmap around frame buffer.

    ppdev = (PPDEV) dhpdev;

    ppdev->ptlOrg.x = 0;
    ppdev->ptlOrg.y = 0;

    if (!bInitSURF(ppdev, TRUE))
    {
        DISPDBG((0, "DISP DrvEnableSurface failed bInitSURF\n"));
        return(FALSE);
    }

    DISPDBG((0, "DISP DrvEnableSurface bInitSURF success\n"));
    
    sizl.cx = ppdev->cxScreen;
    sizl.cy = ppdev->cyScreen;

    if (ppdev->ulBitCount == 8)
    {
        if (!bInit256ColorPalette(ppdev)) {
            DISPDBG((0, "DISP DrvEnableSurface failed to init the 8bpp palette\n"));
            return(FALSE);
        }
        ulBitmapType = BMF_8BPP;
        flHooks = HOOKS_BMF8BPP;
    }
    else if (ppdev->ulBitCount == 16)
    {
        ulBitmapType = BMF_16BPP;
        flHooks = HOOKS_BMF16BPP;
    }
    else if (ppdev->ulBitCount == 24)
    {
        ulBitmapType = BMF_24BPP;
        flHooks = HOOKS_BMF24BPP;
    }
    else
    {
        ulBitmapType = BMF_32BPP;
        flHooks = HOOKS_BMF32BPP;
    }

#ifdef VBOX_NEW_SURFACE_CODE
    psurf = (PVBOXSURF)EngAllocMem(0, sizeof(VBOXSURF), ALLOC_TAG);
    if (psurf == NULL)
    {
        DISPDBG((0, "DrvEnableSurface: failed pdsurf memory allocation\n"));
        goto l_Failure;
    }
    ppdev->pdsurfScreen = psurf;
    psurf->ppdev        = ppdev;

    //
    // On NT4.0 we create a GDI managed bitmap as the primay surface. But
    // on NT5.0 we create a device managed primary.
    //
    // On NT4.0 we still use our driver's accleration capabilities by
    // doing a trick with EngLockSurface on the GDI managed primary.
    //

    if(g_bOnNT40)
    {
        hsurf = (HSURF) EngCreateBitmap(sizl,
                                        ppdev->lDeltaScreen,
                                        ulBitmapType,
                                        (ppdev->lDeltaScreen > 0) ? BMF_TOPDOWN : 0,
                                        (PVOID)(ppdev->pjScreen));
    }
    else
    {
        hsurf = (HSURF)EngCreateDeviceSurface((DHSURF)psurf, sizl,
                                              ulBitmapType);
    }
 
    if ( hsurf == 0 )
    {
        DISPDBG((0, "DrvEnableSurface: failed EngCreateDeviceBitmap\n"));
        goto l_Failure;
    }

    //
    // On NT5.0 we call EngModifSurface to expose our device surface to
    // GDI. We cant do this on NT4.0 hence we call EngAssociateSurface.
    //
     
    if(g_bOnNT40)
    {
        //
        // We have to associate the surface we just created with our physical
        // device so that GDI can get information related to the PDEV when
        // it's drawing to the surface (such as, for example, the length of 
        // styles on the device when simulating styled lines).
        //

        //
        // On NT4.0 we dont want to be called to Synchronize Access
        //
        LONG myflHooks = flHooks;
        myflHooks &= ~HOOK_SYNCHRONIZE;

        if (!EngAssociateSurface(hsurf, ppdev->hdevEng, myflHooks))
        {
            DISPDBG((0, "DrvEnableSurface: failed EngAssociateSurface\n"));
            goto l_Failure; 
        }

        //
        // Jam in the value of dhsurf into screen SURFOBJ. We do this to
        // make sure the driver acclerates Drv calls we hook and not
        // punt them back to GDI as the SURFOBJ's dhsurf = 0. 
        //
        ppdev->psoScreenBitmap = EngLockSurface(hsurf);
        if(ppdev->psoScreenBitmap == 0)
        {
            DISPDBG((0, "DrvEnableSurface: failed EngLockSurface\n"));
            goto l_Failure; 
        }

        ppdev->psoScreenBitmap->dhsurf = (DHSURF)hsurf;

    }
    else
    {
        //
        // Tell GDI about the screen surface.  This will enable GDI to render
        // directly to the screen.
        //

        if ( !EngModifySurface(hsurf,
                               ppdev->hdevEng,
                               flHooks,
                               MS_NOTSYSTEMMEMORY,
                               (DHSURF)psurf,
                               ppdev->pjScreen,
                               ppdev->lDeltaScreen,
                               NULL))
        {
            DISPDBG((0, "DrvEnableSurface: failed EngModifySurface"));
            goto l_Failure;
        }
    }
    ppdev->hsurfScreen  = hsurf;
    ppdev->flHooks      = flHooks;
    ppdev->ulBitmapType = ulBitmapType;
#else
    hsurf = (HSURF) EngCreateBitmap(sizl,
                                    ppdev->lDeltaScreen,
                                    ulBitmapType,
                                    (ppdev->lDeltaScreen > 0) ? BMF_TOPDOWN : 0,
                                    (PVOID) (ppdev->pjScreen));
                                     
    if (hsurf == (HSURF) 0)
    {
        DISPDBG((0, "DISP DrvEnableSurface failed EngCreateBitmap\n"));
        goto l_Failure;
    }
    else
    {
        ppdev->hsurfScreenBitmap = hsurf;
         
        if (!EngAssociateSurface(hsurf, ppdev->hdevEng, 0))
        {
            DISPDBG((0, "DISP DrvEnableSurface failed EngAssociateSurface for ScreenBitmap.\n"));
            goto l_Failure;
        }
        else
        {
            SURFOBJ *pso = EngLockSurface(hsurf);
             
            ppdev->psoScreenBitmap = pso;
   
            hsurf = (HSURF) EngCreateDeviceSurface((DHSURF)pso,
                                                    sizl,
                                                    ulBitmapType);

            if (hsurf == (HSURF) 0)
            {
                DISPDBG((0, "DISP DrvEnableSurface failed EngCreateDeviceSurface\n"));
                goto l_Failure;
            }
            else
            {
                ppdev->hsurfScreen = hsurf;
                /* Must set dhsurf to make sure GDI doesn't ignore our hooks */
                ppdev->psoScreenBitmap->dhsurf = (DHSURF)hsurf;

                if (!EngAssociateSurface(hsurf, ppdev->hdevEng, flHooks))
                {
                    DISPDBG((0, "DISP DrvEnableSurface failed EngAssociateSurface for Screen.\n"));
                    goto l_Failure;
                }
                else
                {
                    ppdev->flHooks = flHooks;
                    ppdev->ulBitmapType = ulBitmapType;
                }
            }
        }
    }
#endif /* VBOX_NEW_SURFACE_CODE */    
    return ppdev->hsurfScreen;
     
l_Failure:

    DrvDisableSurface(dhpdev);
    
    return((HSURF)0);
}

/******************************Public*Routine******************************\
* DrvDisableSurface
*
* Free resources allocated by DrvEnableSurface.  Release the surface.
*
\**************************************************************************/

VOID DrvDisableSurface(DHPDEV dhpdev)
{
    PPDEV ppdev = (PPDEV)dhpdev;
    
    DISPDBG((0, "VBoxDisp::DrvDisableSurface called\n"));
    if (ppdev->psoScreenBitmap)
    {
        EngUnlockSurface (ppdev->psoScreenBitmap);
        ppdev->psoScreenBitmap = NULL;
    }

    if (ppdev->hsurfScreen)
    {
        EngDeleteSurface(ppdev->hsurfScreen);
        ppdev->hsurfScreen = (HSURF)0;
    }
#ifdef VBOX_NEW_SURFACE_CODE
    if (ppdev->pdsurfScreen)
    {
        EngFreeMem(ppdev->pdsurfScreen);
        ppdev->pdsurfScreen = NULL;
    }
#else
    if (ppdev->hsurfScreenBitmap)
    {
        EngDeleteSurface(ppdev->hsurfScreenBitmap);
        ppdev->hsurfScreenBitmap = (HSURF)0;
    }
#endif    
    vDisableSURF(ppdev);
}

/******************************Public*Routine******************************\
* DrvAssertMode
*
* This asks the device to reset itself to the mode of the pdev passed in.
*
\**************************************************************************/

BOOL DrvAssertMode(DHPDEV dhpdev, BOOL bEnable)
{
    PPDEV   ppdev = (PPDEV) dhpdev;
    ULONG   ulReturn;
    PBYTE   pjScreen;

    DISPDBG((0, "DISP DrvAssertMode called bEnable = %d\n", bEnable));
    
    if (bEnable)
    {
        pjScreen = ppdev->pjScreen;

        if (!bInitSURF(ppdev, FALSE))
        {
            DISPDBG((0, "DISP DrvAssertMode failed bInitSURF\n"));
            return (FALSE);
        }

#ifdef VBOX_NEW_SURFACE_CODE
        todo
#endif
        if (pjScreen != ppdev->pjScreen)
        {
            HSURF hsurf;
            SIZEL sizl;
            SURFOBJ *pso;
            
            DISPDBG((0, "DISP DrvAssertMode Screen pointer has changed!!!\n"));
            
            sizl.cx = ppdev->cxScreen;
            sizl.cy = ppdev->cyScreen;
            
            hsurf = (HSURF) EngCreateBitmap(sizl,
                                            ppdev->lDeltaScreen,
                                            ppdev->ulBitmapType,
                                            (ppdev->lDeltaScreen > 0) ? BMF_TOPDOWN : 0,
                                            (PVOID) (ppdev->pjScreen));
                                     
            if (hsurf == (HSURF) 0)
            {
                DISPDBG((0, "DISP DrvAssertMode failed EngCreateBitmap\n"));
                return FALSE;
            }
            
            pso = EngLockSurface(hsurf);
             
            if (ppdev->psoScreenBitmap)
            {
                EngUnlockSurface (ppdev->psoScreenBitmap);
                ppdev->psoScreenBitmap = NULL;
            }

            if (ppdev->hsurfScreenBitmap)
            {
                EngDeleteSurface(ppdev->hsurfScreenBitmap);
                ppdev->hsurfScreenBitmap = (HSURF)0;
            }
    
            ppdev->hsurfScreenBitmap = hsurf;
            ppdev->psoScreenBitmap = pso;
        }

        if (!EngAssociateSurface(ppdev->hsurfScreenBitmap, ppdev->hdevEng, 0))
        {
            DISPDBG((0, "DISP DrvAssertMode failed EngAssociateSurface for ScreenBitmap.\n"));
            return FALSE;
        }
            
        if (!EngAssociateSurface(ppdev->hsurfScreen, ppdev->hdevEng, ppdev->flHooks))
        {
            DISPDBG((0, "DISP DrvAssertMode failed EngAssociateSurface for Screen.\n"));
            return FALSE;
        }
            
        return TRUE;
    }
    else
    {
        //
        // We must give up the display.
        // Call the kernel driver to reset the device to a known state.
        //

        if (EngDeviceIoControl(ppdev->hDriver,
                               IOCTL_VIDEO_RESET_DEVICE,
                               NULL,
                               0,
                               NULL,
                               0,
                               &ulReturn))
        {
            DISPDBG((0, "DISP DrvAssertMode failed IOCTL\n"));
            return FALSE;
        }
        else
        {
            return TRUE;
        }
    }
}

#if 0
/******************************Public*Routine**********************************\
 * HBITMAP DrvCreateDeviceBitmap
 *
 * Function called by GDI to create a device-format-bitmap (DFB).  We will
 * always try to allocate the bitmap in off-screen; if we can't, we simply
 * fail the call and GDI will create and manage the bitmap itself.
 *
 * Note: We do not have to zero the bitmap bits.  GDI will automatically
 *       call us via DrvBitBlt to zero the bits (which is a security
 *       consideration).
 *
\******************************************************************************/

HBITMAP 
DrvCreateDeviceBitmap(
    DHPDEV      dhpdev,
    SIZEL       sizl,
    ULONG       iFormat)
{
    DISPDBG((0, "DISP DrvCreateDeviceBitmap %x (%d,%d) %x\n", dhpdev, sizl.cx, sizl.cy, iFormat));
    /* Let GDI manage the bitmap */
    return (HBITMAP)0;
}

/******************************Public*Routine**********************************\
 * VOID DrvDeleteDeviceBitmap
 *
 * Deletes a DFB.
 *
\******************************************************************************/

VOID 
DrvDeleteDeviceBitmap(
    DHSURF      dhsurf)
{
    DISPDBG((0, "DISP DrvDeleteDeviceBitmap %x", dhsurf));
}
#endif /* 0 */

/******************************Public*Routine******************************\
* DrvGetModes
*
* Returns the list of available modes for the device.
*
\**************************************************************************/

ULONG DrvGetModes(HANDLE hDriver, ULONG cjSize, DEVMODEW *pdm)
{

    DWORD cModes;
    DWORD cbOutputSize;
    PVIDEO_MODE_INFORMATION pVideoModeInformation, pVideoTemp;
    DWORD cOutputModes = cjSize / (sizeof(DEVMODEW) + DRIVER_EXTRA_SIZE);
    DWORD cbModeSize;

    DISPDBG((3, "DrvGetModes\n"));

    cModes = getAvailableModes(hDriver,
                               (PVIDEO_MODE_INFORMATION *) &pVideoModeInformation,
                               &cbModeSize);

    if (cModes == 0)
    {
        DISPDBG((0, "DrvGetModes failed to get mode information"));
        return 0;
    }

    if (pdm == NULL)
    {
        cbOutputSize = cModes * (sizeof(DEVMODEW) + DRIVER_EXTRA_SIZE);
    }
    else
    {
        //
        // Now copy the information for the supported modes back into the output
        // buffer
        //

        cbOutputSize = 0;

        pVideoTemp = pVideoModeInformation;

        do
        {
            if (pVideoTemp->Length != 0)
            {
                if (cOutputModes == 0)
                {
                    break;
                }

                //
                // Zero the entire structure to start off with.
                //

                memset(pdm, 0, sizeof(DEVMODEW));

                //
                // Set the name of the device to the name of the DLL.
                //

                memcpy(pdm->dmDeviceName, DLL_NAME, sizeof(DLL_NAME));

                pdm->dmSpecVersion      = DM_SPECVERSION;
                pdm->dmDriverVersion    = DM_SPECVERSION;
                pdm->dmSize             = sizeof(DEVMODEW);
                pdm->dmDriverExtra      = DRIVER_EXTRA_SIZE;

                pdm->dmBitsPerPel       = pVideoTemp->NumberOfPlanes *
                                          pVideoTemp->BitsPerPlane;
                pdm->dmPelsWidth        = pVideoTemp->VisScreenWidth;
                pdm->dmPelsHeight       = pVideoTemp->VisScreenHeight;
                pdm->dmDisplayFrequency = pVideoTemp->Frequency;
                pdm->dmDisplayFlags     = 0;

                pdm->dmFields           = DM_BITSPERPEL       |
                                          DM_PELSWIDTH        |
                                          DM_PELSHEIGHT       |
                                          DM_DISPLAYFREQUENCY |
                                          DM_DISPLAYFLAGS     ;

                //
                // Go to the next DEVMODE entry in the buffer.
                //

                cOutputModes--;

                pdm = (LPDEVMODEW) ( ((ULONG_PTR)pdm) + sizeof(DEVMODEW)
                                                     + DRIVER_EXTRA_SIZE);

                cbOutputSize += (sizeof(DEVMODEW) + DRIVER_EXTRA_SIZE);

            }

            pVideoTemp = (PVIDEO_MODE_INFORMATION)
                (((PUCHAR)pVideoTemp) + cbModeSize);

        } while (--cModes);
    }

    EngFreeMem(pVideoModeInformation);

    return cbOutputSize;

}

VOID DrvSynchronize(IN DHPDEV dhpdev,IN RECTL *prcl)
{
    DISPDBG((0, "VBoxDisp::DrvSynchronize\n"));
}

/******************************Public*Routine******************************\
* DrvNotify
*
* Called by GDI to notify us of certain "interesting" events
*
* DN_DEVICE_ORIGIN is used to communicate the X/Y offsets of individual monitors
*                  when DualView is in effect.
*
\**************************************************************************/

VOID DrvNotify(
SURFOBJ *pso,
ULONG iType,
PVOID pvData)
{
    PDEV*   ppdev = (PDEV*) pso->dhpdev;

    DISPDBG((0, "VBoxDisp::DrvNotify called.\n"));

    switch(iType)
    {
        case DN_DEVICE_ORIGIN:
            ppdev->ptlDevOrg = *(PPOINTL)pvData;
#ifndef VBOX_WITH_HGSMI
            DISPDBG((3, "DN_DEVICE_ORIGIN: %d, %d (PSO = %p, pInfo = %p)\n", ppdev->ptlDevOrg.x, 
                     ppdev->ptlDevOrg.y, pso, ppdev->pInfo));
            if (ppdev->pInfo)
            {
                ppdev->pInfo->screen.xOrigin = ppdev->ptlDevOrg.x;
                ppdev->pInfo->screen.yOrigin = ppdev->ptlDevOrg.y;
                VBoxProcessDisplayInfo(ppdev);
            }
#else
            DISPDBG((3, "DN_DEVICE_ORIGIN: %d, %d (PSO = %p)\n", ppdev->ptlDevOrg.x, 
                     ppdev->ptlDevOrg.y, pso));
            if (ppdev->bHGSMISupported)
            {
                VBoxProcessDisplayInfo(ppdev);
            }
#endif /* VBOX_WITH_HGSMI */
            break;
        case DN_DRAWING_BEGIN:
            DISPDBG((3, "DN_DRAWING_BEGIN (PSO = %p)\n", pso));
            break;
    }
}

#ifdef VBOX_WITH_DDRAW
//--------------------------Public Routine-------------------------------------
//
// HBITMAP DrvDeriveSurface
//
// This function derives and creates a GDI surface from the specified
// DirectDraw surface.
//
// Parameters
//  pDirectDraw-----Points to a DD_DIRECTDRAW_GLOBAL structure that describes
//                  the DirectDraw object. 
//  pSurface--------Points to a DD_SURFACE_LOCAL structure that describes the
//                  DirectDraw surface around which to wrap a GDI surface.
//
// Return Value
//  DrvDeriveSurface returns a handle to the created GDI surface upon success.
//  It returns NULL if the call fails or if the driver cannot accelerate GDI
//  drawing to the specified DirectDraw surface.
//
// Comments
//  DrvDeriveSurface allows the driver to create a GDI surface around a
//  DirectDraw video memory or AGP surface object in order to allow accelerated
//  GDI drawing to the surface. If the driver does not hook this call, all GDI
//  drawing to DirectDraw surfaces is done in software using the DIB engine.
//
//  GDI calls DrvDeriveSurface with RGB surfaces only.
//
//  The driver should call DrvCreateDeviceBitmap to create a GDI surface of the
//  same size and format as that of the DirectDraw surface. Space for the
//  actual pixels need not be allocated since it already exists.
//
//-----------------------------------------------------------------------------
HBITMAP DrvDeriveSurface(DD_DIRECTDRAW_GLOBAL*  pDirectDraw, DD_SURFACE_LOCAL* pSurface)
{
    PPDEV               pDev = (PPDEV)pDirectDraw->dhpdev;
    HBITMAP             hbmDevice;
    DD_SURFACE_GLOBAL*  pSurfaceGlobal;

    DISPDBG((0, "%s: %p\n", __FUNCTION__, pDev));

    pSurfaceGlobal = pSurface->lpGbl;

    //
    // GDI should never call us for a non-RGB surface, but let's assert just
    // to make sure they're doing their job properly.
    //
    AssertMsg(!(pSurfaceGlobal->ddpfSurface.dwFlags & DDPF_FOURCC), ("GDI called us with a non-RGB surface!"));

    // The GDI driver does not accelerate surfaces in AGP memory,
    // thus we fail the call

    if (pSurface->ddsCaps.dwCaps & DDSCAPS_NONLOCALVIDMEM)
    {
        DISPDBG((0, "DrvDeriveSurface return NULL, surface in AGP memory\n"));
        return 0;
    }

    // The GDI driver does not accelerate managed surface,
    // thus we fail the call
    if (pSurface->lpSurfMore->ddsCapsEx.dwCaps2 & DDSCAPS2_TEXTUREMANAGE)
    {
        DISPDBG((0, "DrvDeriveSurface return NULL, surface is managed\n"));
        return 0;
    }

    //
    // The rest of our driver expects GDI calls to come in with the same
    // format as the primary surface.  So we'd better not wrap a device
    // bitmap around an RGB format that the rest of our driver doesn't
    // understand.  Also, we must check to see that it is not a surface
    // whose pitch does not match the primary surface.
    //
    // NOTE: Most surfaces created by this driver are allocated as 2D surfaces
    // whose lPitch's are equal to the screen pitch.  However, overlay surfaces
    // are allocated such that there lPitch's are usually different then the
    // screen pitch.  The hardware can not accelerate drawing operations to
    // these surfaces and thus we fail to derive these surfaces.
    //
    if ( (pSurfaceGlobal->ddpfSurface.dwRGBBitCount == pDev->ulBitCount) )
    {
        SIZEL sizel;
        DWORD ulBitmapType, flHooks;        

        sizel.cx = pSurfaceGlobal->wWidth;
        sizel.cy = pSurfaceGlobal->wHeight;

        if (pDev->ulBitCount == 8)
        {
            ulBitmapType = BMF_8BPP;
            flHooks = HOOKS_BMF8BPP;
        }
        else if (pDev->ulBitCount == 16)
        {
            ulBitmapType = BMF_16BPP;
            flHooks = HOOKS_BMF16BPP;
        }
        else if (pDev->ulBitCount == 24)
        {
            ulBitmapType = BMF_24BPP;
            flHooks = HOOKS_BMF24BPP;
        }
        else
        {
            ulBitmapType = BMF_32BPP;
            flHooks = HOOKS_BMF32BPP;
        }

        /* Create a bitmap that represents the DDRAW bits.
         * Important is to calculate the address of the bitmap.
         */
        hbmDevice = EngCreateBitmap(sizel,
                                    pSurfaceGlobal->lPitch,
                                    ulBitmapType,
                                    (pDev->lDeltaScreen > 0) ? BMF_TOPDOWN : 0,
                                    (PVOID) (pDev->pjScreen + pSurfaceGlobal->fpVidMem));
        if (hbmDevice)
        {
            if (pSurfaceGlobal->fpVidMem == 0)
            {
                /* Screen surface, mark it so it will be recognized by the driver.
                 * and so the driver will be called on any operations on the surface
                 * (required for VBVA and VRDP).
                 */
                if (EngAssociateSurface((HSURF)hbmDevice, pDev->hdevEng, flHooks))
                {
                    SURFOBJ *surfobj = EngLockSurface ((HSURF)hbmDevice);
                    DISPDBG((0, "DrvDeriveSurface surfobj %x, hsurf = %x\n", surfobj, surfobj->hsurf));
                
                    surfobj->dhpdev = (DHPDEV)pDev;
                
                    EngUnlockSurface(surfobj);

                    DISPDBG((0, "DrvDeriveSurface return succeed %x at %x\n", hbmDevice, pSurfaceGlobal->fpVidMem));
                    return(hbmDevice);
                }
            }
            else
            {
                DISPDBG((0, "DrvDeriveSurface return succeed %x at %x\n", hbmDevice, pSurfaceGlobal->fpVidMem));
                return(hbmDevice);
            }

            DISPDBG((0, "DrvDeriveSurface: EngAssociateSurface failed\n"));
            EngDeleteSurface((HSURF)hbmDevice);
        }
    }

    DISPDBG((0, "DrvDeriveSurface return NULL\n"));
    DISPDBG((0, "pSurfaceGlobal->ddpfSurface.dwRGBBitCount = %d, lPitch =%ld\n", pSurfaceGlobal->ddpfSurface.dwRGBBitCount,pSurfaceGlobal->lPitch));
    
    return(0);
}
#endif /* VBOX_WITH_DDRAW */
