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
* Module Name: screen.c
*
* Initializes the GDIINFO and DEVINFO structures for DrvEnablePDEV.
*
* Copyright (c) 1992-1998 Microsoft Corporation
\**************************************************************************/

#include "driver.h"

#ifdef VBOX_WITH_HGSMI
#include <iprt/asm.h>
#include <VBox/HGSMI/HGSMI.h>
#include <VBox/HGSMI/HGSMIChSetup.h>
#endif

#define SYSTM_LOGFONT {16,7,0,0,700,0,0,0,ANSI_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,VARIABLE_PITCH | FF_DONTCARE,L"System"}
#define HELVE_LOGFONT {12,9,0,0,400,0,0,0,ANSI_CHARSET,OUT_DEFAULT_PRECIS,CLIP_STROKE_PRECIS,PROOF_QUALITY,VARIABLE_PITCH | FF_DONTCARE,L"MS Sans Serif"}
#define COURI_LOGFONT {12,9,0,0,400,0,0,0,ANSI_CHARSET,OUT_DEFAULT_PRECIS,CLIP_STROKE_PRECIS,PROOF_QUALITY,FIXED_PITCH | FF_DONTCARE, L"Courier"}

// This is the basic devinfo for a default driver.  This is used as a base and customized based
// on information passed back from the miniport driver.

const DEVINFO gDevInfoFrameBuffer = {
    ( GCAPS_OPAQUERECT
#ifdef VBOX_WITH_DDRAW
    | GCAPS_DIRECTDRAW
#endif
    | GCAPS_MONO_DITHER
                   ), /* Graphics capabilities         */
    SYSTM_LOGFONT,    /* Default font description */
    HELVE_LOGFONT,    /* ANSI variable font description   */
    COURI_LOGFONT,    /* ANSI fixed font description          */
    0,                /* Count of device fonts          */
    0,                /* Preferred DIB format          */
    8,                /* Width of color dither          */
    8,                /* Height of color dither   */
    0                 /* Default palette to use for this device */
};

static void vboxInitVBoxVideo (PPDEV ppdev, const VIDEO_MEMORY_INFORMATION *pMemoryInformation)
{
    ULONG cbAvailable = 0;

    DWORD returnedDataLength;

    ULONG iDevice;
    uint32_t u32DisplayInfoSize;
    uint32_t u32MinVBVABufferSize;

#ifndef VBOX_WITH_HGSMI
    QUERYDISPLAYINFORESULT DispInfo;
    RtlZeroMemory(&DispInfo, sizeof (DispInfo));

    ppdev->bVBoxVideoSupported = !EngDeviceIoControl(ppdev->hDriver,
                                                     IOCTL_VIDEO_QUERY_DISPLAY_INFO,
                                                     NULL,
                                                     0,
                                                     &DispInfo,
                                                     sizeof(DispInfo),
                                                     &returnedDataLength);
    if (ppdev->bVBoxVideoSupported)
    {
        iDevice = DispInfo.iDevice;
        u32DisplayInfoSize = DispInfo.u32DisplayInfoSize;
        u32MinVBVABufferSize = 0; /* In old mode the buffer is not used at all. */
    }
#else
    QUERYHGSMIRESULT info;
    RtlZeroMemory(&info, sizeof (info));

    ppdev->bHGSMISupported = !EngDeviceIoControl(ppdev->hDriver,
                                                 IOCTL_VIDEO_QUERY_HGSMI_INFO,
                                                 NULL,
                                                 0,
                                                 &info,
                                                 sizeof(info),
                                                 &returnedDataLength);
    if (ppdev->bHGSMISupported)
    {
        iDevice = info.iDevice;
        u32DisplayInfoSize = info.u32DisplayInfoSize;
        u32MinVBVABufferSize = info.u32MinVBVABufferSize;
    }
#endif /* VBOX_WITH_HGSMI */

#ifndef VBOX_WITH_HGSMI
    if (ppdev->bVBoxVideoSupported)
    {
#else
    if (ppdev->bHGSMISupported)
    {
#endif /* VBOX_WITH_HGSMI */
        ppdev->iDevice = iDevice;
    
        ppdev->layout.cbVRAM = pMemoryInformation->VideoRamLength;
    
        ppdev->layout.offFrameBuffer = 0;
        ppdev->layout.cbFrameBuffer  = RT_ALIGN_32(pMemoryInformation->FrameBufferLength, 0x1000);
    
        cbAvailable = ppdev->layout.cbVRAM - ppdev->layout.cbFrameBuffer;
        
        if (cbAvailable <= u32DisplayInfoSize)
        {
#ifndef VBOX_WITH_HGSMI
            ppdev->bVBoxVideoSupported = FALSE;
#else
            ppdev->bHGSMISupported = FALSE;
#endif /* VBOX_WITH_HGSMI */
        }
        else
        {
            ppdev->layout.offDisplayInformation = ppdev->layout.cbVRAM - u32DisplayInfoSize;
            ppdev->layout.cbDisplayInformation  = u32DisplayInfoSize;
            
            cbAvailable -= ppdev->layout.cbDisplayInformation;
            
            /* Use minimum 64K and maximum the cbFrameBuffer for the VBVA buffer. */
            for (ppdev->layout.cbVBVABuffer = ppdev->layout.cbFrameBuffer;
#ifndef VBOX_WITH_HGSMI
                 ppdev->layout.cbVBVABuffer >= 0x10000;
#else
                 ppdev->layout.cbVBVABuffer >= u32MinVBVABufferSize;
#endif /* VBOX_WITH_HGSMI */
                 ppdev->layout.cbVBVABuffer /= 2)
            {
                if (ppdev->layout.cbVBVABuffer < cbAvailable)
                {
                    break;
                }
            }
            
            if (ppdev->layout.cbVBVABuffer >= cbAvailable)
            {
#ifndef VBOX_WITH_HGSMI
                ppdev->bVBoxVideoSupported = FALSE;
#else
                ppdev->bHGSMISupported = FALSE;
#endif /* VBOX_WITH_HGSMI */
            }
            else
            {
                /* Now the offscreen heap followed by the VBVA buffer. */
                ppdev->layout.offDDRAWHeap = ppdev->layout.offFrameBuffer + ppdev->layout.cbFrameBuffer;
                
                cbAvailable -= ppdev->layout.cbVBVABuffer;
                ppdev->layout.cbDDRAWHeap = cbAvailable;
                
                ppdev->layout.offVBVABuffer = ppdev->layout.offDDRAWHeap + ppdev->layout.cbDDRAWHeap;
            }
        }
    }
    
#ifndef VBOX_WITH_HGSMI
    if (!ppdev->bVBoxVideoSupported)
#else
    if (!ppdev->bHGSMISupported)
#endif /* VBOX_WITH_HGSMI */
    {
        ppdev->iDevice = 0;

        /* Setup a layout without both the VBVA buffer and the display information. */
        ppdev->layout.cbVRAM = pMemoryInformation->VideoRamLength;
    
        ppdev->layout.offFrameBuffer = 0;
        ppdev->layout.cbFrameBuffer  = RT_ALIGN_32(pMemoryInformation->FrameBufferLength, 0x1000);
    
        ppdev->layout.offDDRAWHeap = ppdev->layout.offFrameBuffer + ppdev->layout.cbFrameBuffer;
        ppdev->layout.cbDDRAWHeap  = ppdev->layout.cbVRAM - ppdev->layout.offDDRAWHeap;
    
        ppdev->layout.offVBVABuffer = ppdev->layout.offDDRAWHeap + ppdev->layout.cbDDRAWHeap;
        ppdev->layout.cbVBVABuffer  = 0;
    
        ppdev->layout.offDisplayInformation = ppdev->layout.offVBVABuffer + ppdev->layout.cbVBVABuffer;
        ppdev->layout.cbDisplayInformation  = 0;
    }
#ifdef VBOX_WITH_HGSMI
    else
    {
        /* Setup HGSMI heap in the display information area. The area has some space reserved for
         * HGSMI event flags in the beginning.
         */
        int rc = HGSMIHeapSetup (&ppdev->hgsmiDisplayHeap,
                                 (uint8_t *)ppdev->pjScreen + ppdev->layout.offDisplayInformation + sizeof (HGSMIHOSTFLAGS),
                                 ppdev->layout.cbDisplayInformation - sizeof (HGSMIHOSTFLAGS),
                                 ppdev->layout.offDisplayInformation + sizeof (HGSMIHOSTFLAGS));

        if (RT_FAILURE (rc))
        {
            DISPDBG((0, "VBoxDISP::vboxInitVBoxVideo: HGSMIHeapSetup failed rc = %d\n",
                     rc));

            ppdev->bHGSMISupported = FALSE;
        }
        else
        {
            /* Inform the host about the HGSMIHOSTEVENTS location. */
            void *p = HGSMIHeapAlloc (&ppdev->hgsmiDisplayHeap,
                                      sizeof (HGSMI_BUFFER_LOCATION),
                                      HGSMI_CH_HGSMI,
                                      HGSMI_CC_HOST_FLAGS_LOCATION);

            if (!p)
            {
                DISPDBG((0, "VBoxDISP::vboxInitVBoxVideo: HGSMIHeapAlloc failed\n"));
                rc = VERR_NO_MEMORY;
            }
            else
            {
                HGSMIOFFSET offBuffer = HGSMIHeapBufferOffset (&ppdev->hgsmiDisplayHeap,
                                                               p);

                ((HGSMI_BUFFER_LOCATION *)p)->offLocation = ppdev->layout.offDisplayInformation;
                ((HGSMI_BUFFER_LOCATION *)p)->cbLocation = sizeof (HGSMIHOSTFLAGS);

                /* Submit the buffer to the host. */
                ASMOutU16 (VBE_DISPI_IOPORT_INDEX, VBE_DISPI_INDEX_VBVA_GUEST);
                ASMOutU32 (VBE_DISPI_IOPORT_DATA, offBuffer);

                HGSMIHeapFree (&ppdev->hgsmiDisplayHeap, p);
            }
        }
    }
#endif /* VBOX_WITH_HGSMI */

    DISPDBG((0, "vboxInitVBoxVideo:\n"
                "    cbVRAM = 0x%X\n"
                "    offFrameBuffer = 0x%X\n"
                "    cbFrameBuffer = 0x%X\n"
                "    offDDRAWHeap = 0x%X\n"
                "    cbDDRAWHeap = 0x%X\n"
                "    offVBVABuffer = 0x%X\n"
                "    cbVBVABuffer = 0x%X\n"
                "    offDisplayInformation = 0x%X\n"
                "    cbDisplayInformation = 0x%X\n",
                ppdev->layout.cbVRAM,
                ppdev->layout.offFrameBuffer,
                ppdev->layout.cbFrameBuffer,
                ppdev->layout.offDDRAWHeap,
                ppdev->layout.cbDDRAWHeap,
                ppdev->layout.offVBVABuffer,
                ppdev->layout.cbVBVABuffer,
                ppdev->layout.offDisplayInformation,
                ppdev->layout.cbDisplayInformation
                ));
}


#ifndef VBOX_WITH_HGSMI
/* Setup display information after remapping. */
static void vboxSetupDisplayInfo (PPDEV ppdev, VIDEO_MEMORY_INFORMATION *pMemoryInformation)
{
    VBOXDISPLAYINFO *pInfo;
    uint8_t *pu8;
    
    pu8 = (uint8_t *)ppdev->pjScreen + ppdev->layout.offDisplayInformation;

    pInfo = (VBOXDISPLAYINFO *)pu8;
    pu8 += sizeof (VBOXDISPLAYINFO);

    pInfo->hdrLink.u8Type     = VBOX_VIDEO_INFO_TYPE_LINK;
    pInfo->hdrLink.u8Reserved = 0;
    pInfo->hdrLink.u16Length  = sizeof (VBOXVIDEOINFOLINK);
    pInfo->link.i32Offset = 0;

    pInfo->hdrScreen.u8Type     = VBOX_VIDEO_INFO_TYPE_SCREEN;
    pInfo->hdrScreen.u8Reserved = 0;
    pInfo->hdrScreen.u16Length  = sizeof (VBOXVIDEOINFOSCREEN);
    DISPDBG((1, "Setup: %d,%d\n", ppdev->ptlDevOrg.x, ppdev->ptlDevOrg.y));
    pInfo->screen.xOrigin      = ppdev->ptlDevOrg.x;
    pInfo->screen.yOrigin      = ppdev->ptlDevOrg.y;
    pInfo->screen.u32LineSize  = 0;
    pInfo->screen.u16Width     = 0;
    pInfo->screen.u16Height    = 0;
    pInfo->screen.bitsPerPixel = 0;
    pInfo->screen.u8Flags      = VBOX_VIDEO_INFO_SCREEN_F_NONE;
    
    pInfo->hdrHostEvents.u8Type     = VBOX_VIDEO_INFO_TYPE_HOST_EVENTS;
    pInfo->hdrHostEvents.u8Reserved = 0;
    pInfo->hdrHostEvents.u16Length  = sizeof (VBOXVIDEOINFOHOSTEVENTS);
    pInfo->hostEvents.fu32Events = VBOX_VIDEO_INFO_HOST_EVENTS_F_NONE;

    pInfo->hdrEnd.u8Type     = VBOX_VIDEO_INFO_TYPE_END;
    pInfo->hdrEnd.u8Reserved = 0;
    pInfo->hdrEnd.u16Length  = 0;

    ppdev->pInfo = pInfo;
}


static void vboxUpdateDisplayInfo (PPDEV ppdev)
{
    if (ppdev->pInfo)
    {
        ppdev->pInfo->screen.u32LineSize  = ppdev->lDeltaScreen;
        ppdev->pInfo->screen.u16Width     = (uint16_t)ppdev->cxScreen;
        ppdev->pInfo->screen.u16Height    = (uint16_t)ppdev->cyScreen;
        ppdev->pInfo->screen.bitsPerPixel = (uint8_t)ppdev->ulBitCount;
        ppdev->pInfo->screen.u8Flags      = VBOX_VIDEO_INFO_SCREEN_F_ACTIVE;

        DISPDBG((1, "Update: %d,%d\n", ppdev->ptlDevOrg.x, ppdev->ptlDevOrg.y));
        VBoxProcessDisplayInfo(ppdev);
    }
}
#endif /* !VBOX_WITH_HGSMI */


/******************************Public*Routine******************************\
* bInitSURF
*
* Enables the surface.        Maps the frame buffer into memory.
*
\**************************************************************************/

BOOL bInitSURF(PPDEV ppdev, BOOL bFirst)
{
    DWORD returnedDataLength;
    DWORD MaxWidth, MaxHeight;
    VIDEO_MEMORY videoMemory;
    VIDEO_MEMORY_INFORMATION videoMemoryInformation;
    ULONG RemappingNeeded = 0;  

    //
    // Set the current mode into the hardware.
    //

    if (EngDeviceIoControl(ppdev->hDriver,
                           IOCTL_VIDEO_SET_CURRENT_MODE,
                           &(ppdev->ulMode),
                           sizeof(ULONG),
                           &RemappingNeeded,
                           sizeof(ULONG),
                           &returnedDataLength))
    {
        DISPDBG((1, "DISP bInitSURF failed IOCTL_SET_MODE\n"));
        return(FALSE);
    }

    //
    // If this is the first time we enable the surface we need to map in the
    // memory also.
    //

    if (bFirst || RemappingNeeded)
    {
        videoMemory.RequestedVirtualAddress = NULL;

        if (EngDeviceIoControl(ppdev->hDriver,
                               IOCTL_VIDEO_MAP_VIDEO_MEMORY,
                               &videoMemory,
                               sizeof(VIDEO_MEMORY),
                               &videoMemoryInformation,
                               sizeof(VIDEO_MEMORY_INFORMATION),
                               &returnedDataLength))
        {
            DISPDBG((1, "DISP bInitSURF failed IOCTL_VIDEO_MAP\n"));
            return(FALSE);
        }

        ppdev->pjScreen = (PBYTE)(videoMemoryInformation.FrameBufferBase);

        if (videoMemoryInformation.FrameBufferBase !=
            videoMemoryInformation.VideoRamBase)
        {
            DISPDBG((0, "VideoRamBase does not correspond to FrameBufferBase\n"));
        }

        //
        // Make sure we can access this video memory
        //

        *(PULONG)(ppdev->pjScreen) = 0xaa55aa55;

        if (*(PULONG)(ppdev->pjScreen) != 0xaa55aa55) {

            DISPDBG((1, "Frame buffer memory is not accessible.\n"));
            return(FALSE);
        }

        /* Clear VRAM to avoid distortions during the video mode change. */
        RtlZeroMemory(ppdev->pjScreen,
                      ppdev->cyScreen * (ppdev->lDeltaScreen > 0? ppdev->lDeltaScreen: -ppdev->lDeltaScreen));

        //
        // Initialize the head of the offscreen list to NULL.
        //

        ppdev->pOffscreenList = NULL;

        // It's a hardware pointer; set up pointer attributes.

        MaxHeight = ppdev->PointerCapabilities.MaxHeight;

        // Allocate space for two DIBs (data/mask) for the pointer. If this
        // device supports a color Pointer, we will allocate a larger bitmap.
        // If this is a color bitmap we allocate for the largest possible
        // bitmap because we have no idea of what the pixel depth might be.

        // Width rounded up to nearest byte multiple

        if (!(ppdev->PointerCapabilities.Flags & VIDEO_MODE_COLOR_POINTER))
        {
            MaxWidth = (ppdev->PointerCapabilities.MaxWidth + 7) / 8;
        }
        else
        {
            MaxWidth = ppdev->PointerCapabilities.MaxWidth * sizeof(DWORD);
        }

        ppdev->cjPointerAttributes =
                sizeof(VIDEO_POINTER_ATTRIBUTES) +
                ((sizeof(UCHAR) * MaxWidth * MaxHeight) * 2);

        ppdev->pPointerAttributes = (PVIDEO_POINTER_ATTRIBUTES)
                EngAllocMem(0, ppdev->cjPointerAttributes, ALLOC_TAG);

        if (ppdev->pPointerAttributes == NULL) {

            DISPDBG((0, "bInitPointer EngAllocMem failed\n"));
            return(FALSE);
        }

        ppdev->pPointerAttributes->Flags = ppdev->PointerCapabilities.Flags;
        ppdev->pPointerAttributes->WidthInBytes = MaxWidth;
        ppdev->pPointerAttributes->Width = ppdev->PointerCapabilities.MaxWidth;
        ppdev->pPointerAttributes->Height = MaxHeight;
        ppdev->pPointerAttributes->Column = 0;
        ppdev->pPointerAttributes->Row = 0;
        ppdev->pPointerAttributes->Enable = 0;
        
        vboxInitVBoxVideo (ppdev, &videoMemoryInformation);

#ifndef VBOX_WITH_HGSMI
        if (ppdev->bVBoxVideoSupported)
        {
            /* Setup the display information. */
            vboxSetupDisplayInfo (ppdev, &videoMemoryInformation);
        }
#endif /* !VBOX_WITH_HGSMI */
    }

        
    DISPDBG((1, "DISP bInitSURF: ppdev->ulBitCount %d\n", ppdev->ulBitCount));
    
    if (   ppdev->ulBitCount == 16
        || ppdev->ulBitCount == 24
        || ppdev->ulBitCount == 32)
    {
#ifndef VBOX_WITH_HGSMI
        if (ppdev->pInfo) /* Do not use VBVA on old hosts. */
        {
            /* Enable VBVA for this video mode. */
            vboxVbvaEnable (ppdev);
        }
#else
        if (ppdev->bHGSMISupported)
        {
            /* Enable VBVA for this video mode. */
            vboxVbvaEnable (ppdev);
        }
#endif /* VBOX_WITH_HGSMI */
    }

    DISPDBG((1, "DISP bInitSURF success\n"));

#ifndef VBOX_WITH_HGSMI
    /* Update the display information. */
    vboxUpdateDisplayInfo (ppdev);
#else
    /* Inform the host about this screen layout. */
    DISPDBG((1, "bInitSURF: %d,%d\n", ppdev->ptlDevOrg.x, ppdev->ptlDevOrg.y));
    VBoxProcessDisplayInfo (ppdev);
#endif /* VBOX_WITH_HGSMI */
    
    return(TRUE);
}

/******************************Public*Routine******************************\
* vDisableSURF
*
* Disable the surface. Un-Maps the frame in memory.
*
\**************************************************************************/

VOID vDisableSURF(PPDEV ppdev)
{
    DWORD returnedDataLength;
    VIDEO_MEMORY videoMemory;

    videoMemory.RequestedVirtualAddress = (PVOID) ppdev->pjScreen;

    if (EngDeviceIoControl(ppdev->hDriver,
                           IOCTL_VIDEO_UNMAP_VIDEO_MEMORY,
                           &videoMemory,
                           sizeof(VIDEO_MEMORY),
                           NULL,
                           0,
                           &returnedDataLength))
    {
        DISPDBG((0, "DISP vDisableSURF failed IOCTL_VIDEO_UNMAP\n"));
    }
}


/******************************Public*Routine******************************\
* bInitPDEV
*
* Determine the mode we should be in based on the DEVMODE passed in.
* Query mini-port to get information needed to fill in the DevInfo and the
* GdiInfo .
*
\**************************************************************************/

BOOL bInitPDEV(
PPDEV ppdev,
DEVMODEW *pDevMode,
GDIINFO *pGdiInfo,
DEVINFO *pDevInfo)
{
    ULONG cModes;
    PVIDEO_MODE_INFORMATION pVideoBuffer, pVideoModeSelected, pVideoTemp;
    VIDEO_COLOR_CAPABILITIES colorCapabilities;
    ULONG ulTemp;
    BOOL bSelectDefault;
    ULONG cbModeSize;

    //
    // calls the miniport to get mode information.
    //

    cModes = getAvailableModes(ppdev->hDriver, &pVideoBuffer, &cbModeSize);

    if (cModes == 0)
    {
        return(FALSE);
    }

    //
    // Now see if the requested mode has a match in that table.
    //

    pVideoModeSelected = NULL;
    pVideoTemp = pVideoBuffer;

    if ((pDevMode->dmPelsWidth        == 0) &&
        (pDevMode->dmPelsHeight       == 0) &&
        (pDevMode->dmBitsPerPel       == 0) &&
        (pDevMode->dmDisplayFrequency == 0))
    {
        DISPDBG((2, "Default mode requested"));
        bSelectDefault = TRUE;
    }
    else
    {
        DISPDBG((2, "Requested mode...\n"));
        DISPDBG((2, "   Screen width  -- %li\n", pDevMode->dmPelsWidth));
        DISPDBG((2, "   Screen height -- %li\n", pDevMode->dmPelsHeight));
        DISPDBG((2, "   Bits per pel  -- %li\n", pDevMode->dmBitsPerPel));
        DISPDBG((2, "   Frequency     -- %li\n", pDevMode->dmDisplayFrequency));

        bSelectDefault = FALSE;
    }

    while (cModes--)
    {
        if (pVideoTemp->Length != 0)
        {
            if (bSelectDefault ||
                ((pVideoTemp->VisScreenWidth  == pDevMode->dmPelsWidth) &&
                 (pVideoTemp->VisScreenHeight == pDevMode->dmPelsHeight) &&
                 (pVideoTemp->BitsPerPlane *
                  pVideoTemp->NumberOfPlanes  == pDevMode->dmBitsPerPel) &&
                 (pVideoTemp->Frequency  == pDevMode->dmDisplayFrequency)))
            {
                pVideoModeSelected = pVideoTemp;
                DISPDBG((3, "Found a match\n")) ;
                break;
            }
        }

        pVideoTemp = (PVIDEO_MODE_INFORMATION)
            (((PUCHAR)pVideoTemp) + cbModeSize);
    }

    //
    // If no mode has been found, return an error
    //

    if (pVideoModeSelected == NULL)
    {
        EngFreeMem(pVideoBuffer);
        DISPDBG((0,"DISP bInitPDEV failed - no valid modes\n"));
        return(FALSE);
    }

    //
    // Fill in the GDIINFO data structure with the information returned from
    // the kernel driver.
    //

    ppdev->ulMode = pVideoModeSelected->ModeIndex;
    ppdev->cxScreen = pVideoModeSelected->VisScreenWidth;
    ppdev->cyScreen = pVideoModeSelected->VisScreenHeight;
    ppdev->ulBitCount = pVideoModeSelected->BitsPerPlane *
                        pVideoModeSelected->NumberOfPlanes;
    ppdev->lDeltaScreen = pVideoModeSelected->ScreenStride;

    ppdev->flRed = pVideoModeSelected->RedMask;
    ppdev->flGreen = pVideoModeSelected->GreenMask;
    ppdev->flBlue = pVideoModeSelected->BlueMask;


    if (g_bOnNT40)
    {
        DISPDBG((0,"DISP bInitPDEV pGdiInfo->ulVersion = %x\n", GDI_DRIVER_VERSION));
        pGdiInfo->ulVersion = GDI_DRIVER_VERSION; /* 0x4000 -> NT4 */
    }
    else
    {
        DISPDBG((0,"DISP bInitPDEV pGdiInfo->ulVersion = %x\n", 0x5000));
        pGdiInfo->ulVersion = 0x5000;
    }

    pGdiInfo->ulTechnology = DT_RASDISPLAY;
    pGdiInfo->ulHorzSize   = pVideoModeSelected->XMillimeter;
    pGdiInfo->ulVertSize   = pVideoModeSelected->YMillimeter;

    pGdiInfo->ulHorzRes        = ppdev->cxScreen;
    pGdiInfo->ulVertRes        = ppdev->cyScreen;
    pGdiInfo->ulPanningHorzRes = ppdev->cxScreen;
    pGdiInfo->ulPanningVertRes = ppdev->cyScreen;
    pGdiInfo->cBitsPixel       = pVideoModeSelected->BitsPerPlane;
    pGdiInfo->cPlanes          = pVideoModeSelected->NumberOfPlanes;
    pGdiInfo->ulVRefresh       = pVideoModeSelected->Frequency;
    /* bit block transfers are accelerated */
    pGdiInfo->ulBltAlignment   = 0;

    pGdiInfo->ulLogPixelsX = pDevMode->dmLogPixels;
    pGdiInfo->ulLogPixelsY = pDevMode->dmLogPixels;

#ifdef MIPS
    if (ppdev->ulBitCount == 8)
        pGdiInfo->flTextCaps = (TC_RA_ABLE | TC_SCROLLBLT);
    else
#endif
    pGdiInfo->flTextCaps = TC_RA_ABLE;

    pGdiInfo->flRaster = 0;           // flRaster is reserved by DDI

    pGdiInfo->ulDACRed   = pVideoModeSelected->NumberRedBits;
    pGdiInfo->ulDACGreen = pVideoModeSelected->NumberGreenBits;
    pGdiInfo->ulDACBlue  = pVideoModeSelected->NumberBlueBits;

    pGdiInfo->ulAspectX    = 0x24;    // One-to-one aspect ratio
    pGdiInfo->ulAspectY    = 0x24;
    pGdiInfo->ulAspectXY   = 0x33;

    pGdiInfo->xStyleStep   = 1;       // A style unit is 3 pels
    pGdiInfo->yStyleStep   = 1;
    pGdiInfo->denStyleStep = 3;

    pGdiInfo->ptlPhysOffset.x = 0;
    pGdiInfo->ptlPhysOffset.y = 0;
    pGdiInfo->szlPhysSize.cx  = 0;
    pGdiInfo->szlPhysSize.cy  = 0;

    // RGB and CMY color info.

    //
    // try to get it from the miniport.
    // if the miniport doesn ot support this feature, use defaults.
    //

    if (EngDeviceIoControl(ppdev->hDriver,
                           IOCTL_VIDEO_QUERY_COLOR_CAPABILITIES,
                           NULL,
                           0,
                           &colorCapabilities,
                           sizeof(VIDEO_COLOR_CAPABILITIES),
                           &ulTemp))
    {

        DISPDBG((2, "getcolorCapabilities failed \n"));

        pGdiInfo->ciDevice.Red.x = 6700;
        pGdiInfo->ciDevice.Red.y = 3300;
        pGdiInfo->ciDevice.Red.Y = 0;
        pGdiInfo->ciDevice.Green.x = 2100;
        pGdiInfo->ciDevice.Green.y = 7100;
        pGdiInfo->ciDevice.Green.Y = 0;
        pGdiInfo->ciDevice.Blue.x = 1400;
        pGdiInfo->ciDevice.Blue.y = 800;
        pGdiInfo->ciDevice.Blue.Y = 0;
        pGdiInfo->ciDevice.AlignmentWhite.x = 3127;
        pGdiInfo->ciDevice.AlignmentWhite.y = 3290;
        pGdiInfo->ciDevice.AlignmentWhite.Y = 0;

        pGdiInfo->ciDevice.RedGamma = 20000;
        pGdiInfo->ciDevice.GreenGamma = 20000;
        pGdiInfo->ciDevice.BlueGamma = 20000;

    }
    else
    {
        pGdiInfo->ciDevice.Red.x = colorCapabilities.RedChromaticity_x;
        pGdiInfo->ciDevice.Red.y = colorCapabilities.RedChromaticity_y;
        pGdiInfo->ciDevice.Red.Y = 0;
        pGdiInfo->ciDevice.Green.x = colorCapabilities.GreenChromaticity_x;
        pGdiInfo->ciDevice.Green.y = colorCapabilities.GreenChromaticity_y;
        pGdiInfo->ciDevice.Green.Y = 0;
        pGdiInfo->ciDevice.Blue.x = colorCapabilities.BlueChromaticity_x;
        pGdiInfo->ciDevice.Blue.y = colorCapabilities.BlueChromaticity_y;
        pGdiInfo->ciDevice.Blue.Y = 0;
        pGdiInfo->ciDevice.AlignmentWhite.x = colorCapabilities.WhiteChromaticity_x;
        pGdiInfo->ciDevice.AlignmentWhite.y = colorCapabilities.WhiteChromaticity_y;
        pGdiInfo->ciDevice.AlignmentWhite.Y = colorCapabilities.WhiteChromaticity_Y;

        // if we have a color device store the three color gamma values,
        // otherwise store the unique gamma value in all three.

        if (colorCapabilities.AttributeFlags & VIDEO_DEVICE_COLOR)
        {
            pGdiInfo->ciDevice.RedGamma = colorCapabilities.RedGamma;
            pGdiInfo->ciDevice.GreenGamma = colorCapabilities.GreenGamma;
            pGdiInfo->ciDevice.BlueGamma = colorCapabilities.BlueGamma;
        }
        else
        {
            pGdiInfo->ciDevice.RedGamma = colorCapabilities.WhiteGamma;
            pGdiInfo->ciDevice.GreenGamma = colorCapabilities.WhiteGamma;
            pGdiInfo->ciDevice.BlueGamma = colorCapabilities.WhiteGamma;
        }

    };

    pGdiInfo->ciDevice.Cyan.x = 0;
    pGdiInfo->ciDevice.Cyan.y = 0;
    pGdiInfo->ciDevice.Cyan.Y = 0;
    pGdiInfo->ciDevice.Magenta.x = 0;
    pGdiInfo->ciDevice.Magenta.y = 0;
    pGdiInfo->ciDevice.Magenta.Y = 0;
    pGdiInfo->ciDevice.Yellow.x = 0;
    pGdiInfo->ciDevice.Yellow.y = 0;
    pGdiInfo->ciDevice.Yellow.Y = 0;

    // No dye correction for raster displays.

    pGdiInfo->ciDevice.MagentaInCyanDye = 0;
    pGdiInfo->ciDevice.YellowInCyanDye = 0;
    pGdiInfo->ciDevice.CyanInMagentaDye = 0;
    pGdiInfo->ciDevice.YellowInMagentaDye = 0;
    pGdiInfo->ciDevice.CyanInYellowDye = 0;
    pGdiInfo->ciDevice.MagentaInYellowDye = 0;

    pGdiInfo->ulDevicePelsDPI = 0;   // For printers only
    pGdiInfo->ulPrimaryOrder = PRIMARY_ORDER_CBA;

    // Note: this should be modified later to take into account the size
    // of the display and the resolution.

    pGdiInfo->ulHTPatternSize = HT_PATSIZE_4x4_M;

    pGdiInfo->flHTFlags = HT_FLAG_ADDITIVE_PRIMS;

    // Fill in the basic devinfo structure

    *pDevInfo = gDevInfoFrameBuffer;

    // Fill in the rest of the devinfo and GdiInfo structures.

    if (ppdev->ulBitCount == 8)
    {
        // It is Palette Managed.

        pGdiInfo->ulNumColors = 20;
        pGdiInfo->ulNumPalReg = 1 << ppdev->ulBitCount;

        pDevInfo->flGraphicsCaps |= (GCAPS_PALMANAGED | GCAPS_COLOR_DITHER);

        pGdiInfo->ulHTOutputFormat = HT_FORMAT_8BPP;
        pDevInfo->iDitherFormat = BMF_8BPP;

        // Assuming palette is orthogonal - all colors are same size.

        ppdev->cPaletteShift   = 8 - pGdiInfo->ulDACRed;
    }
    else
    {
        pGdiInfo->ulNumColors = (ULONG) (-1);
        pGdiInfo->ulNumPalReg = 0;

        if (ppdev->ulBitCount == 16)
        {
            pGdiInfo->ulHTOutputFormat = HT_FORMAT_16BPP;
            pDevInfo->iDitherFormat = BMF_16BPP;
        }
        else if (ppdev->ulBitCount == 24)
        {
            pGdiInfo->ulHTOutputFormat = HT_FORMAT_24BPP;
            pDevInfo->iDitherFormat = BMF_24BPP;
        }
        else
        {
            pGdiInfo->ulHTOutputFormat = HT_FORMAT_32BPP;
            pDevInfo->iDitherFormat = BMF_32BPP;
        }
    }

    EngFreeMem(pVideoBuffer);

    return(TRUE);
}


/******************************Public*Routine******************************\
* getAvailableModes
*
* Calls the miniport to get the list of modes supported by the kernel driver,
* and returns the list of modes supported by the diplay driver among those
*
* returns the number of entries in the videomode buffer.
* 0 means no modes are supported by the miniport or that an error occured.
*
* NOTE: the buffer must be freed up by the caller.
*
\**************************************************************************/

DWORD getAvailableModes(
HANDLE hDriver,
PVIDEO_MODE_INFORMATION *modeInformation,
DWORD *cbModeSize)
{
    ULONG ulTemp;
    VIDEO_NUM_MODES modes;
    PVIDEO_MODE_INFORMATION pVideoTemp;

    //
    // Get the number of modes supported by the mini-port
    //

    if (EngDeviceIoControl(hDriver,
                           IOCTL_VIDEO_QUERY_NUM_AVAIL_MODES,
                           NULL,
                           0,
                           &modes,
                           sizeof(VIDEO_NUM_MODES),
                           &ulTemp))
    {
        DISPDBG((0, "getAvailableModes failed VIDEO_QUERY_NUM_AVAIL_MODES\n"));
        return(0);
    }

    *cbModeSize = modes.ModeInformationLength;

    //
    // Allocate the buffer for the mini-port to write the modes in.
    //

    *modeInformation = (PVIDEO_MODE_INFORMATION)
                        EngAllocMem(0, modes.NumModes *
                                    modes.ModeInformationLength, ALLOC_TAG);

    if (*modeInformation == (PVIDEO_MODE_INFORMATION) NULL)
    {
        DISPDBG((0, "getAvailableModes failed EngAllocMem\n"));

        return 0;
    }

    //
    // Ask the mini-port to fill in the available modes.
    //

    if (EngDeviceIoControl(hDriver,
                           IOCTL_VIDEO_QUERY_AVAIL_MODES,
                           NULL,
                           0,
                           *modeInformation,
                           modes.NumModes * modes.ModeInformationLength,
                           &ulTemp))
    {

        DISPDBG((0, "getAvailableModes failed VIDEO_QUERY_AVAIL_MODES\n"));

        EngFreeMem(*modeInformation);
        *modeInformation = (PVIDEO_MODE_INFORMATION) NULL;

        return(0);
    }

    //
    // Now see which of these modes are supported by the display driver.
    // As an internal mechanism, set the length to 0 for the modes we
    // DO NOT support.
    //

    ulTemp = modes.NumModes;
    pVideoTemp = *modeInformation;

    //
    // Mode is rejected if it is not one plane, or not graphics, or is not
    // one of 8, 16 or 32 bits per pel.
    //

    while (ulTemp--)
    {
        if ((pVideoTemp->NumberOfPlanes != 1 ) ||
            !(pVideoTemp->AttributeFlags & VIDEO_MODE_GRAPHICS) ||
             (pVideoTemp->AttributeFlags & VIDEO_MODE_BANKED) ||
            ((pVideoTemp->BitsPerPlane != 8) &&
             (pVideoTemp->BitsPerPlane != 16) &&
             (pVideoTemp->BitsPerPlane != 24) &&
             (pVideoTemp->BitsPerPlane != 32)))
        {
            pVideoTemp->Length = 0;
        }

        pVideoTemp = (PVIDEO_MODE_INFORMATION)
            (((PUCHAR)pVideoTemp) + modes.ModeInformationLength);
    }

    return modes.NumModes;

}

