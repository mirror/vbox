/*
 * VirtualBox Video miniport driver for NT/2k/XP
 *
 * Based on DDK sample code.
 *
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 *
 */

#include "VBoxVideo.h"
#include "Helper.h"

#include <VBox/VBoxGuest.h>
#include <VBox/VBoxVideo.h>

#include <VBox/VBoxGuestLib.h>
#include <VBoxDisplay.h>

#if _MSC_VER >= 1400 /* bird: MS fixed swprintf to be standard-conforming... */
#define _INC_SWPRINTF_INL_
extern "C" int __cdecl swprintf(wchar_t *, const wchar_t *, ...);
#endif 
#include <wchar.h>

#include "vboxioctl.h"


static WCHAR VBoxChipType[] = L"VBOX";
static WCHAR VBoxDACType[] = L"Integrated RAMDAC";
static WCHAR VBoxAdapterString[] = L"VirtualBox Video Adapter";
static WCHAR VBoxBiosString[] = L"Version 0xB0C2 or later";

/* Number of reported monitors. Defaults to 1 (no DualView) */
static int gNumDisplays = 1;

/*
 * Globals for the last custom resolution set. This is important
 * for system startup so that we report the last currently set
 * custom resolution and Windows can use it again.
 */
ULONG gCustomXRes = 0;
ULONG gCustomYRes = 0;
ULONG gCustomBPP  = 0;

int vboxVbvaEnable (PDEVICE_EXTENSION pDevExt, ULONG ulEnable, VBVAENABLERESULT *pVbvaResult);

ULONG DriverEntry(IN PVOID Context1, IN PVOID Context2)
{
    VIDEO_HW_INITIALIZATION_DATA InitData;
    ULONG rc;

    dprintf(("VBoxVideo::DriverEntry. Built %s %s\n", __DATE__, __TIME__));

    VideoPortZeroMemory(&InitData, sizeof(VIDEO_HW_INITIALIZATION_DATA));
    InitData.HwInitDataSize = sizeof(VIDEO_HW_INITIALIZATION_DATA);
    InitData.HwFindAdapter = VBoxVideoFindAdapter;
    InitData.HwInitialize = VBoxVideoInitialize;
    InitData.HwInterrupt = NULL;
    InitData.HwStartIO = VBoxVideoStartIO;
    InitData.HwResetHw = VBoxVideoResetHW;
    InitData.HwDeviceExtensionSize = 0;
    // nowhere documented but without the following line, NT4 SP0 will choke
    InitData.AdapterInterfaceType = PCIBus;
    InitData.HwGetPowerState = VBoxVideoGetPowerState;
    InitData.HwSetPowerState = VBoxVideoSetPowerState;
    InitData.HwGetVideoChildDescriptor = VBoxVideoGetChildDescriptor;
    InitData.HwDeviceExtensionSize = sizeof(DEVICE_EXTENSION);

    // our DDK is at the Win2k3 level so we have to take special measures
    // for backwards compatibility
    switch (vboxQueryWinVersion())
    {
        case WINNT4:
            InitData.HwInitDataSize = SIZE_OF_NT4_VIDEO_HW_INITIALIZATION_DATA;
            break;
        case WIN2K:
            InitData.HwInitDataSize = SIZE_OF_W2K_VIDEO_HW_INITIALIZATION_DATA;
            break;
    }
    rc = VideoPortInitialize(Context1, Context2, &InitData, NULL);

    dprintf(("VBoxVideo::DriverEntry: returning with rc = 0x%x\n", rc));
    return rc;
}

/*+++

Routine Description:

    This routine is used to read back various registry values.

Arguments:

    HwDeviceExtension
        Supplies a pointer to the miniport's device extension.

    Context
        Context value passed to the get registry parameters routine.
        If this is not null assume it's a ULONG* and save the data value in it.

    ValueName
        Name of the value requested.

    ValueData
        Pointer to the requested data.

    ValueLength
        Length of the requested data.

Return Value:

    If the variable doesn't exist return an error,
    else if a context is supplied assume it's a PULONG and fill in the value
    and return no error, else if the value is non-zero return an error.

---*/
VP_STATUS VBoxRegistryCallback(PVOID HwDeviceExtension, PVOID Context,
                               PWSTR ValueName, PVOID ValueData, ULONG ValueLength)
{
    //dprintf(("VBoxVideo::VBoxRegistryCallback: Context: %p, ValueName: %S, ValueData: %p, ValueLength: %d\n",
    //         Context, ValueName, ValueData, ValueLength));
    if (ValueLength)
    {
        if (Context)
            *(ULONG *)Context = *(PULONG)ValueData;
        else if (*((PULONG)ValueData) != 0)
            return ERROR_INVALID_PARAMETER;
        return NO_ERROR;
    }
    else
        return ERROR_INVALID_PARAMETER;
}

/*
 * Global list of supported standard video modes. It will be
 * filled dynamically.
 */
#define MAX_VIDEO_MODES 128
static VIDEO_MODE_INFORMATION VideoModes[MAX_VIDEO_MODES + 2] = { 0 };
/* number of available video modes, set by VBoxBuildModesTable  */
static uint32_t gNumVideoModes = 0;

/**
 * Helper function to dynamically build our table of standard video
 * modes. We take the amount of VRAM and create modes with standard
 * geometries until we've either reached the maximum number of modes
 * or the available VRAM does not allow for additional modes.
 */
VOID VBoxBuildModesTable(PDEVICE_EXTENSION DeviceExtension)
{
    /* we need this static counter to always have a new mode index for our */
    /* custom video mode, otherwise Windows thinks there is no mode switch */
    static int gInvocationCounter = 0;

    /* the resolution matrix */
    struct
    {
        uint16_t xRes;
        uint16_t yRes;
    } resolutionMatrix[] =
    {
        /* standard modes */
        { 640,   480 },
        { 800,   600 },
        { 1024,  768 },
        { 1152,  864 },
        { 1280,  960 },
        { 1280, 1024 },
        { 1400, 1050 },
        { 1600, 1200 },
        { 1920, 1440 },
        /* multi screen modes with 1280x1024 */
        { 2560, 1024 },
        { 3840, 1024 },
        { 5120, 1024 },
        /* multi screen modes with 1600x1200 */
        { 3200, 1200 },
        { 4800, 1200 },
        { 6400, 1200 },
    };
    size_t matrixSize = sizeof(resolutionMatrix) / sizeof(resolutionMatrix[0]);

    /* there are 4 color depths: 8, 16, 24 and 32bpp and we reserve 50% of the modes for other sources */
    size_t maxModesPerColorDepth = MAX_VIDEO_MODES / 2 / 4;

    /* size of the VRAM in bytes */
    ULONG vramSize = DeviceExtension->ulMaxFrameBufferSize;

    gNumVideoModes = 0;

    size_t numModesCurrentColorDepth;
    size_t matrixIndex;
    VP_STATUS status = 0;

    /*
     * Query the y-offset from the host
     */
    ULONG yOffset = vboxGetHeightReduction();

#ifdef VBOX_WITH_8BPP_MODES
    /*
     * 8 bit video modes
     */
    numModesCurrentColorDepth = 0;
    matrixIndex = 0;
    while (numModesCurrentColorDepth < maxModesPerColorDepth)
    {
        /* are there any modes left in the matrix? */
        if (matrixIndex >= matrixSize)
            break;

        /* does the mode fit into the VRAM? */
        if (resolutionMatrix[matrixIndex].xRes * resolutionMatrix[matrixIndex].yRes * 1 > (LONG)vramSize)
        {
            ++matrixIndex;
            continue;
        }

        /* does the host like that mode? */
        if (!vboxLikesVideoMode(resolutionMatrix[matrixIndex].xRes, resolutionMatrix[matrixIndex].yRes - yOffset, 8))
        {
            ++matrixIndex;
            continue;
        }

        VideoModes[gNumVideoModes].Length                       = sizeof(VIDEO_MODE_INFORMATION);
        VideoModes[gNumVideoModes].ModeIndex                    = gNumVideoModes + 1;
        VideoModes[gNumVideoModes].VisScreenWidth               = resolutionMatrix[matrixIndex].xRes;
        VideoModes[gNumVideoModes].VisScreenHeight              = resolutionMatrix[matrixIndex].yRes - yOffset;
        VideoModes[gNumVideoModes].ScreenStride                 = resolutionMatrix[matrixIndex].xRes * 1;
        VideoModes[gNumVideoModes].NumberOfPlanes               = 1;
        VideoModes[gNumVideoModes].BitsPerPlane                 = 8;
        VideoModes[gNumVideoModes].Frequency                    = 1;
        VideoModes[gNumVideoModes].XMillimeter                  = 320;
        VideoModes[gNumVideoModes].YMillimeter                  = 240;
        VideoModes[gNumVideoModes].NumberRedBits                = 6;
        VideoModes[gNumVideoModes].NumberGreenBits              = 6;
        VideoModes[gNumVideoModes].NumberBlueBits               = 6;
        VideoModes[gNumVideoModes].RedMask                      = 0;
        VideoModes[gNumVideoModes].GreenMask                    = 0;
        VideoModes[gNumVideoModes].BlueMask                     = 0;
        VideoModes[gNumVideoModes].AttributeFlags               = VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR | VIDEO_MODE_NO_OFF_SCREEN |
                                                                  VIDEO_MODE_PALETTE_DRIVEN | VIDEO_MODE_MANAGED_PALETTE;
        VideoModes[gNumVideoModes].VideoMemoryBitmapWidth       = resolutionMatrix[matrixIndex].xRes;
        VideoModes[gNumVideoModes].VideoMemoryBitmapHeight      = resolutionMatrix[matrixIndex].yRes - yOffset;
        VideoModes[gNumVideoModes].DriverSpecificAttributeFlags = 0;

        /* a new mode has been filled in */
        ++gNumVideoModes;
        ++numModesCurrentColorDepth;
        /* advance to the next mode matrix entry */
        ++matrixIndex;
    }
#endif /* VBOX_WITH_8BPP_MODES */

    /*
     * 16 bit video modes
     */
    numModesCurrentColorDepth = 0;
    matrixIndex = 0;
    while (numModesCurrentColorDepth < maxModesPerColorDepth)
    {
        /* are there any modes left in the matrix? */
        if (matrixIndex >= matrixSize)
            break;

        /* does the mode fit into the VRAM? */
        if (resolutionMatrix[matrixIndex].xRes * resolutionMatrix[matrixIndex].yRes * 2 > (LONG)vramSize)
        {
            ++matrixIndex;
            continue;
        }

        /* does the host like that mode? */
        if (!vboxLikesVideoMode(resolutionMatrix[matrixIndex].xRes, resolutionMatrix[matrixIndex].yRes - yOffset, 16))
        {
            ++matrixIndex;
            continue;
        }

        VideoModes[gNumVideoModes].Length                       = sizeof(VIDEO_MODE_INFORMATION);
        VideoModes[gNumVideoModes].ModeIndex                    = gNumVideoModes + 1;
        VideoModes[gNumVideoModes].VisScreenWidth               = resolutionMatrix[matrixIndex].xRes;
        VideoModes[gNumVideoModes].VisScreenHeight              = resolutionMatrix[matrixIndex].yRes - yOffset;
        VideoModes[gNumVideoModes].ScreenStride                 = resolutionMatrix[matrixIndex].xRes * 2;
        VideoModes[gNumVideoModes].NumberOfPlanes               = 1;
        VideoModes[gNumVideoModes].BitsPerPlane                 = 16;
        VideoModes[gNumVideoModes].Frequency                    = 1;
        VideoModes[gNumVideoModes].XMillimeter                  = 320;
        VideoModes[gNumVideoModes].YMillimeter                  = 240;
        VideoModes[gNumVideoModes].NumberRedBits                = 5;
        VideoModes[gNumVideoModes].NumberGreenBits              = 6;
        VideoModes[gNumVideoModes].NumberBlueBits               = 5;
        VideoModes[gNumVideoModes].RedMask                      = 0xF800;
        VideoModes[gNumVideoModes].GreenMask                    = 0x7E0;
        VideoModes[gNumVideoModes].BlueMask                     = 0x1F;
        VideoModes[gNumVideoModes].AttributeFlags               = VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR | VIDEO_MODE_NO_OFF_SCREEN;
        VideoModes[gNumVideoModes].VideoMemoryBitmapWidth       = resolutionMatrix[matrixIndex].xRes;
        VideoModes[gNumVideoModes].VideoMemoryBitmapHeight      = resolutionMatrix[matrixIndex].yRes - yOffset;
        VideoModes[gNumVideoModes].DriverSpecificAttributeFlags = 0;

        /* a new mode has been filled in */
        ++gNumVideoModes;
        ++numModesCurrentColorDepth;
        /* advance to the next mode matrix entry */
        ++matrixIndex;
    }

    /*
     * 24 bit video modes
     */
    numModesCurrentColorDepth = 0;
    matrixIndex = 0;
    while (numModesCurrentColorDepth < maxModesPerColorDepth)
    {
        /* are there any modes left in the matrix? */
        if (matrixIndex >= matrixSize)
            break;

        /* does the mode fit into the VRAM? */
        if (resolutionMatrix[matrixIndex].xRes * resolutionMatrix[matrixIndex].yRes * 3 > (LONG)vramSize)
        {
            ++matrixIndex;
            continue;
        }

        /* does the host like that mode? */
        if (!vboxLikesVideoMode(resolutionMatrix[matrixIndex].xRes, resolutionMatrix[matrixIndex].yRes - yOffset, 24))
        {
            ++matrixIndex;
            continue;
        }

        VideoModes[gNumVideoModes].Length                       = sizeof(VIDEO_MODE_INFORMATION);
        VideoModes[gNumVideoModes].ModeIndex                    = gNumVideoModes + 1;
        VideoModes[gNumVideoModes].VisScreenWidth               = resolutionMatrix[matrixIndex].xRes;
        VideoModes[gNumVideoModes].VisScreenHeight              = resolutionMatrix[matrixIndex].yRes - yOffset;
        VideoModes[gNumVideoModes].ScreenStride                 = resolutionMatrix[matrixIndex].xRes * 3;
        VideoModes[gNumVideoModes].NumberOfPlanes               = 1;
        VideoModes[gNumVideoModes].BitsPerPlane                 = 24;
        VideoModes[gNumVideoModes].Frequency                    = 1;
        VideoModes[gNumVideoModes].XMillimeter                  = 320;
        VideoModes[gNumVideoModes].YMillimeter                  = 240;
        VideoModes[gNumVideoModes].NumberRedBits                = 8;
        VideoModes[gNumVideoModes].NumberGreenBits              = 8;
        VideoModes[gNumVideoModes].NumberBlueBits               = 8;
        VideoModes[gNumVideoModes].RedMask                      = 0xFF0000;
        VideoModes[gNumVideoModes].GreenMask                    = 0xFF00;
        VideoModes[gNumVideoModes].BlueMask                     = 0xFF;
        VideoModes[gNumVideoModes].AttributeFlags               = VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR | VIDEO_MODE_NO_OFF_SCREEN;
        VideoModes[gNumVideoModes].VideoMemoryBitmapWidth       = resolutionMatrix[matrixIndex].xRes;
        VideoModes[gNumVideoModes].VideoMemoryBitmapHeight      = resolutionMatrix[matrixIndex].yRes - yOffset;
        VideoModes[gNumVideoModes].DriverSpecificAttributeFlags = 0;

        /* a new mode has been filled in */
        ++gNumVideoModes;
        ++numModesCurrentColorDepth;
        /* advance to the next mode matrix entry */
        ++matrixIndex;
    }

    /*
     * 32 bit video modes
     */
    numModesCurrentColorDepth = 0;
    matrixIndex = 0;
    while (numModesCurrentColorDepth < maxModesPerColorDepth)
    {
        /* are there any modes left in the matrix? */
        if (matrixIndex >= matrixSize)
            break;

        /* does the mode fit into the VRAM? */
        if (resolutionMatrix[matrixIndex].xRes * resolutionMatrix[matrixIndex].yRes * 4 > (LONG)vramSize)
        {
            ++matrixIndex;
            continue;
        }

        /* does the host like that mode? */
        if (!vboxLikesVideoMode(resolutionMatrix[matrixIndex].xRes, resolutionMatrix[matrixIndex].yRes - yOffset, 32))
        {
            ++matrixIndex;
            continue;
        }

        VideoModes[gNumVideoModes].Length                       = sizeof(VIDEO_MODE_INFORMATION);
        VideoModes[gNumVideoModes].ModeIndex                    = gNumVideoModes + 1;
        VideoModes[gNumVideoModes].VisScreenWidth               = resolutionMatrix[matrixIndex].xRes;
        VideoModes[gNumVideoModes].VisScreenHeight              = resolutionMatrix[matrixIndex].yRes - yOffset;
        VideoModes[gNumVideoModes].ScreenStride                 = resolutionMatrix[matrixIndex].xRes * 4;
        VideoModes[gNumVideoModes].NumberOfPlanes               = 1;
        VideoModes[gNumVideoModes].BitsPerPlane                 = 32;
        VideoModes[gNumVideoModes].Frequency                    = 1;
        VideoModes[gNumVideoModes].XMillimeter                  = 320;
        VideoModes[gNumVideoModes].YMillimeter                  = 240;
        VideoModes[gNumVideoModes].NumberRedBits                = 8;
        VideoModes[gNumVideoModes].NumberGreenBits              = 8;
        VideoModes[gNumVideoModes].NumberBlueBits               = 8;
        VideoModes[gNumVideoModes].RedMask                      = 0xFF0000;
        VideoModes[gNumVideoModes].GreenMask                    = 0xFF00;
        VideoModes[gNumVideoModes].BlueMask                     = 0xFF;
        VideoModes[gNumVideoModes].AttributeFlags               = VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR | VIDEO_MODE_NO_OFF_SCREEN;
        VideoModes[gNumVideoModes].VideoMemoryBitmapWidth       = resolutionMatrix[matrixIndex].xRes;
        VideoModes[gNumVideoModes].VideoMemoryBitmapHeight      = resolutionMatrix[matrixIndex].yRes - yOffset;
        VideoModes[gNumVideoModes].DriverSpecificAttributeFlags = 0;

        /* a new mode has been filled in */
        ++gNumVideoModes;
        ++numModesCurrentColorDepth;
        /* advance to the next mode matrix entry */
        ++matrixIndex;
    }

    /*
     * Next, check the registry for additional modes
     */
    int curKeyNo = 0;
    do
    {
        /* check if there is space in the mode list */
        if (gNumVideoModes >= MAX_VIDEO_MODES)
            break;

        wchar_t keyname[24];
        uint32_t xres, yres, bpp = 0;
        swprintf(keyname, L"CustomMode%dWidth", curKeyNo);
        status = VideoPortGetRegistryParameters(DeviceExtension,
                                                keyname,
                                                FALSE,
                                                VBoxRegistryCallback,
                                                &xres);
        /* upon the first error, we give up */
        if (status != NO_ERROR)
            break;
        swprintf(keyname, L"CustomMode%dHeight", curKeyNo);
        status = VideoPortGetRegistryParameters(DeviceExtension,
                                                keyname,
                                                FALSE,
                                                VBoxRegistryCallback,
                                                &yres);
        /* upon the first error, we give up */
        if (status != NO_ERROR)
            break;
        swprintf(keyname, L"CustomMode%dBPP", curKeyNo);
        status = VideoPortGetRegistryParameters(DeviceExtension,
                                                keyname,
                                                FALSE,
                                                VBoxRegistryCallback,
                                                &bpp);
        /* upon the first error, we give up */
        if (status != NO_ERROR)
            break;

        dprintf(("VBoxVideo: custom mode %u returned: xres = %u, yres = %u, bpp = %u\n",
                 curKeyNo, xres, yres, bpp));

        /* first test: do the values make sense? */
        if (   (xres > (1 << 16))
            || (yres > (1 << 16))
            || (   (bpp != 16)
                && (bpp != 24)
                && (bpp != 32)))
            break;

        /* round down width to be a multiple of 8 */
        xres &= 0xFFF8;

        /* second test: does it fit within our VRAM? */
        if (xres * yres * (bpp / 8) > vramSize)
            break;

        /* third test: does the host like the video mode? */
        if (!vboxLikesVideoMode(xres, yres, bpp))
            break;

        dprintf(("VBoxVideo: adding mode from registry: xres = %d, yres = %d, bpp = %d\n", xres, yres, bpp));
        /*
         * Build mode entry.
         * Note that we have to apply the y offset for the custom mode.
         */
        VideoModes[gNumVideoModes].Length                       = sizeof(VIDEO_MODE_INFORMATION);
        VideoModes[gNumVideoModes].ModeIndex                    = gNumVideoModes + 1;
        VideoModes[gNumVideoModes].VisScreenWidth               = xres;
        VideoModes[gNumVideoModes].VisScreenHeight              = yres - yOffset;
        VideoModes[gNumVideoModes].ScreenStride                 = xres * (bpp / 8);
        VideoModes[gNumVideoModes].NumberOfPlanes               = 1;
        VideoModes[gNumVideoModes].BitsPerPlane                 = bpp;
        VideoModes[gNumVideoModes].Frequency                    = 1;
        VideoModes[gNumVideoModes].XMillimeter                  = 320;
        VideoModes[gNumVideoModes].YMillimeter                  = 240;
        switch (bpp)
        {
            case 16:
                VideoModes[gNumVideoModes].NumberRedBits        = 5;
                VideoModes[gNumVideoModes].NumberGreenBits      = 6;
                VideoModes[gNumVideoModes].NumberBlueBits       = 5;
                VideoModes[gNumVideoModes].RedMask              = 0xF800;
                VideoModes[gNumVideoModes].GreenMask            = 0x7E0;
                VideoModes[gNumVideoModes].BlueMask             = 0x1F;
                break;
            case 24:
                VideoModes[gNumVideoModes].NumberRedBits        = 8;
                VideoModes[gNumVideoModes].NumberGreenBits      = 8;
                VideoModes[gNumVideoModes].NumberBlueBits       = 8;
                VideoModes[gNumVideoModes].RedMask              = 0xFF0000;
                VideoModes[gNumVideoModes].GreenMask            = 0xFF00;
                VideoModes[gNumVideoModes].BlueMask             = 0xFF;
                break;
            case 32:
                VideoModes[gNumVideoModes].NumberRedBits        = 8;
                VideoModes[gNumVideoModes].NumberGreenBits      = 8;
                VideoModes[gNumVideoModes].NumberBlueBits       = 8;
                VideoModes[gNumVideoModes].RedMask              = 0xFF0000;
                VideoModes[gNumVideoModes].GreenMask            = 0xFF00;
                VideoModes[gNumVideoModes].BlueMask             = 0xFF;
                break;
        }
        VideoModes[gNumVideoModes].AttributeFlags               = VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR | VIDEO_MODE_NO_OFF_SCREEN;
        VideoModes[gNumVideoModes].VideoMemoryBitmapWidth       = xres;
        VideoModes[gNumVideoModes].VideoMemoryBitmapHeight      = yres - yOffset;
        VideoModes[gNumVideoModes].DriverSpecificAttributeFlags = 0;
        ++gNumVideoModes;

        /* next run */
        curKeyNo++;
        /* only support 128 modes for now */
        if (curKeyNo >= 128)
            break;

    } while(1);


    /*
     * Now we ask the host for a display change request. If there's one,
     * this will be appended as a special mode so that it can be used by
     * the Additions service process. The mode table is guaranteed to have
     * two spare entries for this mode (alternating index thus 2).
     */
    uint32_t xres, yres, bpp = 0;
    if (   (   vboxQueryDisplayRequest(&xres, &yres, &bpp)
            && (xres || yres || bpp))
        || (gCustomXRes || gCustomYRes || gCustomBPP))
    {
        dprintf(("VBoxVideo: adding custom video mode as #%d, current mode: %d \n", gNumVideoModes + 1, DeviceExtension->CurrentMode));
        /* handle the startup case */
        if (DeviceExtension->CurrentMode == 0)
        {
            xres = gCustomXRes;
            yres = gCustomYRes;
            bpp  = gCustomBPP;
            dprintf(("VBoxVideo: using stored custom resolution %dx%dx%d\n", xres, yres, bpp));
        }
        /* round down to multiple of 8 */
        if ((xres & 0xfff8) != xres)
            dprintf(("VBoxVideo: rounding down xres from %d to %d\n", xres, xres & 0xfff8));
        xres &= 0xfff8;
        /* take the current values for the fields that are not set */
        if (DeviceExtension->CurrentMode != 0)
        {
            if (!xres)
                xres = VideoModes[DeviceExtension->CurrentMode - 1].VisScreenWidth;
            if (!yres)
                yres = VideoModes[DeviceExtension->CurrentMode - 1].VisScreenHeight;
            if (!bpp)
                bpp  = VideoModes[DeviceExtension->CurrentMode - 1].BitsPerPlane;
        }

        /* does the host like that mode? */
        if (vboxLikesVideoMode(xres, yres, bpp))
        {
            /* we must have a valid video mode by now and it must fit within the VRAM */
            if (   (   xres
                    && yres
                    && (   (bpp == 16)
#ifdef VBOX_WITH_8BPP_MODES
                        || (bpp == 8)
#endif
                        || (bpp == 24)
                        || (bpp == 32)))
                && (xres * yres * (bpp / 8) < vramSize))

            {
                /* we need an alternating index */
                if (DeviceExtension->CurrentMode != 0)
                {
                    if (gInvocationCounter % 2)
                        gNumVideoModes++;
                    gInvocationCounter++;
                }

                dprintf(("VBoxVideo: setting special mode to xres = %d, yres = %d, bpp = %d\n", xres, yres, bpp));
                /*
                 * Build mode entry.
                 * Note that we do not apply the y offset for the custom mode. It is
                 * only used for the predefined modes that the user can configure in
                 * the display properties dialog.
                 */
                VideoModes[gNumVideoModes].Length                       = sizeof(VIDEO_MODE_INFORMATION);
                VideoModes[gNumVideoModes].ModeIndex                    = gNumVideoModes + 1;
                VideoModes[gNumVideoModes].VisScreenWidth               = xres;
                VideoModes[gNumVideoModes].VisScreenHeight              = yres;
                VideoModes[gNumVideoModes].ScreenStride                 = xres * (bpp / 8);
                VideoModes[gNumVideoModes].NumberOfPlanes               = 1;
                VideoModes[gNumVideoModes].BitsPerPlane                 = bpp;
                VideoModes[gNumVideoModes].Frequency                    = 1;
                VideoModes[gNumVideoModes].XMillimeter                  = 320;
                VideoModes[gNumVideoModes].YMillimeter                  = 240;
                switch (bpp)
                {
#ifdef VBOX_WITH_8BPP_MODES
                    case 8:
                        VideoModes[gNumVideoModes].NumberRedBits        = 6;
                        VideoModes[gNumVideoModes].NumberGreenBits      = 6;
                        VideoModes[gNumVideoModes].NumberBlueBits       = 6;
                        VideoModes[gNumVideoModes].RedMask              = 0;
                        VideoModes[gNumVideoModes].GreenMask            = 0;
                        VideoModes[gNumVideoModes].BlueMask             = 0;
                        break;
#endif
                    case 16:
                        VideoModes[gNumVideoModes].NumberRedBits        = 5;
                        VideoModes[gNumVideoModes].NumberGreenBits      = 6;
                        VideoModes[gNumVideoModes].NumberBlueBits       = 5;
                        VideoModes[gNumVideoModes].RedMask              = 0xF800;
                        VideoModes[gNumVideoModes].GreenMask            = 0x7E0;
                        VideoModes[gNumVideoModes].BlueMask             = 0x1F;
                        break;
                    case 24:
                        VideoModes[gNumVideoModes].NumberRedBits        = 8;
                        VideoModes[gNumVideoModes].NumberGreenBits      = 8;
                        VideoModes[gNumVideoModes].NumberBlueBits       = 8;
                        VideoModes[gNumVideoModes].RedMask              = 0xFF0000;
                        VideoModes[gNumVideoModes].GreenMask            = 0xFF00;
                        VideoModes[gNumVideoModes].BlueMask             = 0xFF;
                        break;
                    case 32:
                        VideoModes[gNumVideoModes].NumberRedBits        = 8;
                        VideoModes[gNumVideoModes].NumberGreenBits      = 8;
                        VideoModes[gNumVideoModes].NumberBlueBits       = 8;
                        VideoModes[gNumVideoModes].RedMask              = 0xFF0000;
                        VideoModes[gNumVideoModes].GreenMask            = 0xFF00;
                        VideoModes[gNumVideoModes].BlueMask             = 0xFF;
                        break;
                }
                VideoModes[gNumVideoModes].AttributeFlags               = VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR | VIDEO_MODE_NO_OFF_SCREEN;
#ifdef VBOX_WITH_8BPP_MODES
                if (bpp == 8)
                    VideoModes[gNumVideoModes].AttributeFlags          |= VIDEO_MODE_PALETTE_DRIVEN | VIDEO_MODE_MANAGED_PALETTE;
#endif
                VideoModes[gNumVideoModes].VideoMemoryBitmapWidth       = xres;
                VideoModes[gNumVideoModes].VideoMemoryBitmapHeight      = yres;
                VideoModes[gNumVideoModes].DriverSpecificAttributeFlags = 0;
                ++gNumVideoModes;

                /* for the startup case, we need this mode twice due to the alternating mode number */
                if (DeviceExtension->CurrentMode == 0)
                {
                    dprintf(("VBoxVideo: making a copy of the custom mode as #%d\n", gNumVideoModes + 1));
                    memcpy(&VideoModes[gNumVideoModes], &VideoModes[gNumVideoModes - 1], sizeof(VIDEO_MODE_INFORMATION));
                    VideoModes[gNumVideoModes].ModeIndex = gNumVideoModes + 1;
                    gNumVideoModes++;
                }

                /* store this video mode as the last custom video mode */
                status = VideoPortSetRegistryParameters(DeviceExtension, L"CustomXRes",
                                                        &xres, sizeof(ULONG));
                if (status != NO_ERROR)
                    dprintf(("VBoxVideo: error %d writing CustomXRes\n", status));
                status = VideoPortSetRegistryParameters(DeviceExtension, L"CustomYRes",
                                                        &yres, sizeof(ULONG));
                if (status != NO_ERROR)
                    dprintf(("VBoxVideo: error %d writing CustomYRes\n", status));
                status = VideoPortSetRegistryParameters(DeviceExtension, L"CustomBPP",
                                                        &bpp, sizeof(ULONG));
                if (status != NO_ERROR)
                    dprintf(("VBoxVideo: error %d writing CustomBPP\n", status));
            }
            else
                dprintf(("VBoxVideo: invalid parameters for special mode: (xres = %d, yres = %d, bpp = %d, vramSize = %d)\n",
                         xres, yres, bpp, vramSize));
        }
        else
            dprintf(("VBoxVideo: host does not like special mode: (xres = %d, yres = %d, bpp = %d)\n",
                     xres, yres, bpp));
    }
}

/* Computes the size of a framebuffer. DualView has a few framebuffers of the computed size. */
static ULONG VBoxVideoComputeMaxFrameBufferSize (ULONG AdapterMemorySize)
{
    /* The VRAM layout: 
     *     Last 4096 bytes - Adapter information area.
     *     Slack - what left after dividing the VRAM.
     *     4096 bytes aligned framebuffers:
     *       last 4096 bytes of each framebuffer is the display information area.
     */

    /* Size of a framebuffer. */
    ULONG ulSize = (AdapterMemorySize - VBOX_VIDEO_ADAPTER_INFORMATION_SIZE) / gNumDisplays;
    
    /* Align down to 4096 bytes. */
    ulSize &= ~0xFFF;
    
    dprintf(("VBoxVideo::VBoxVideoComputeMaxFrameBufferSize: AdapterMemorySize = 0x%08X, gNumDisplays = %d, ulSize = 0x%08X, ulSize * gNumDisplays = 0x%08X, slack = 0x%08X\n",
             AdapterMemorySize, gNumDisplays, ulSize, ulSize * gNumDisplays, (AdapterMemorySize - 4096) - ulSize * gNumDisplays));
    
    if (ulSize > VBOX_VIDEO_DISPLAY_INFORMATION_SIZE)
    {
        /* Compute the size of the framebuffer. */
        ulSize -= VBOX_VIDEO_DISPLAY_INFORMATION_SIZE;
    }
    else
    {
        ulSize = 0;
    }
    
    return ulSize;
}

static VOID VBoxMapAdapterInfo (PDEVICE_EXTENSION PrimaryExtension, ULONG AdapterMemorySize)
{
    PHYSICAL_ADDRESS FrameBuffer;
    ULONG inIoSpace = 0;
    VP_STATUS Status;

    dprintf(("VBoxVideo::VBoxSetupAdapterInfo\n"));

    FrameBuffer.QuadPart = VBE_DISPI_LFB_PHYSICAL_ADDRESS + AdapterMemorySize - VBOX_VIDEO_ADAPTER_INFORMATION_SIZE;

    PVOID VideoRamBase = NULL;
    ULONG VideoRamLength = VBOX_VIDEO_ADAPTER_INFORMATION_SIZE;

    Status = VideoPortMapMemory(PrimaryExtension, FrameBuffer,
       &VideoRamLength, &inIoSpace,
       &VideoRamBase);

    if (Status == NO_ERROR)
    {
        PrimaryExtension->AdapterInformation = VideoRamBase;
    }
    else
    {
        PrimaryExtension->AdapterInformation = NULL;
    }
}

static VOID VBoxSetupAdapterInfo (PDEVICE_EXTENSION PrimaryExtension)
{
    if (!PrimaryExtension->AdapterInformation)
    {
        return;
    }
    
    /* That dublicates the code in VBoxSetupDisplays, better would be to have 
     * linked list of device extestions and fill the adapter memory with
     * information from these extension structures.
     */
    uint8_t *pu8 = (uint8_t *)PrimaryExtension->AdapterInformation;
    uint32_t u32Offset = 0;
    
    VBOXVIDEOINFOHDR *pHdr;
    int iDisplay;

    for (iDisplay = 0; iDisplay < gNumDisplays; ++iDisplay)
    {
        pHdr = (VBOXVIDEOINFOHDR *)pu8;
        pu8 += sizeof (VBOXVIDEOINFOHDR);

        pHdr->u8Type     = VBOX_VIDEO_INFO_TYPE_DISPLAY;
        pHdr->u8Reserved = 0;
        pHdr->u16Length  = sizeof (VBOXVIDEOINFODISPLAY);

        VBOXVIDEOINFODISPLAY *pDisplay = (VBOXVIDEOINFODISPLAY *)pu8;
        pu8 += sizeof (VBOXVIDEOINFODISPLAY);

        pDisplay->u32Index           = iDisplay;
        pDisplay->u32Offset          = u32Offset;
        pDisplay->u32FramebufferSize = PrimaryExtension->ulMaxFrameBufferSize;
        pDisplay->u32InformationSize = VBOX_VIDEO_DISPLAY_INFORMATION_SIZE;
        
        u32Offset += PrimaryExtension->ulMaxFrameBufferSize + VBOX_VIDEO_DISPLAY_INFORMATION_SIZE;
    }

    pHdr = (VBOXVIDEOINFOHDR *)pu8;
    pu8 += sizeof (VBOXVIDEOINFOHDR);

    pHdr->u8Type     = VBOX_VIDEO_INFO_TYPE_END;
    pHdr->u8Reserved = 0;
    pHdr->u16Length  = 0;

    /* Inform the host about the display configuration. */
    VideoPortWritePortUshort((PUSHORT)VBE_DISPI_IOPORT_INDEX, VBE_DISPI_INDEX_CMONITORS);
    VideoPortWritePortUlong((PULONG)VBE_DISPI_IOPORT_DATA, VBOX_VIDEO_INTERPRET_ADAPTER_MEMORY);
}

/**
 * Helper function to register secondary displays (DualView). Note that this will not
 * be available on pre-XP versions, and some editions on XP will fail because they are
 * intentionally crippled.
 */
VOID VBoxSetupDisplays(PDEVICE_EXTENSION PrimaryExtension, PVIDEO_PORT_CONFIG_INFO pConfigInfo)
{
   typedef VP_STATUS (*pCreateSecDisp)(PVOID, PVOID *, ULONG);
   pCreateSecDisp pVPCreateSecDisp;
   PVOID pFunc;
   int iDisplay;
   VP_STATUS rc;
   PDEVICE_EXTENSION pSecExt;
   ULONG AdapterMemorySize;

   dprintf(("VBoxVideo::VBoxRegisterSecondaryDisplays: PrimaryExtension = %p\n", PrimaryExtension));

   AdapterMemorySize = VideoPortReadPortUlong((PULONG)VBE_DISPI_IOPORT_DATA);

   /* Initialize DualView related stuff in device extension for 1 monitor (must be done always!).
    * Assume that the is no dual view and initialize the maximum possible frame buffer size.
    * Also assume no VBox extension support.
    */
   PrimaryExtension->iDevice      = 0;
   PrimaryExtension->pvPrimaryExt = PrimaryExtension;

   PrimaryExtension->ulFrameBufferOffset = 0;
   PrimaryExtension->ulMaxFrameBufferSize = AdapterMemorySize -
                                            VBOX_VIDEO_ADAPTER_INFORMATION_SIZE -
                                            VBOX_VIDEO_DISPLAY_INFORMATION_SIZE;
   PrimaryExtension->bDualViewSupported = FALSE;
   PrimaryExtension->AdapterInformation = NULL;
   
   /* Verify that the HW support VirtualBox extensions. */
   VideoPortWritePortUshort((PUSHORT)VBE_DISPI_IOPORT_INDEX, VBE_DISPI_INDEX_ID);
   VideoPortWritePortUshort((PUSHORT)VBE_DISPI_IOPORT_DATA, VBE_DISPI_ID_VBOX_VIDEO);
   if (VideoPortReadPortUshort((PUSHORT)VBE_DISPI_IOPORT_DATA) != VBE_DISPI_ID_VBOX_VIDEO)
   {
       dprintf(("VBoxVideo::VBoxSetupDisplays: virtual hardware do not support VBox extensions!!!\n"));
       return;
   }

   /* Map the adapter information memory. */
   VBoxMapAdapterInfo (PrimaryExtension, AdapterMemorySize);
   
   if (!PrimaryExtension->AdapterInformation)
   {
       dprintf(("VBoxVideo::VBoxSetupDisplays: failed to map adapter memory!!!\n"));
       return;
   }

   /* Dynamically query the VideoPort import to be binary compatible across Windows versions */
   if (vboxQueryWinVersion() <= WINNT4) 
   {
       pFunc = NULL;
   } 
   else 
   {
       /* This bluescreens on NT4, hence the above version check */
       pFunc = (pConfigInfo->VideoPortGetProcAddress)(PrimaryExtension,
                                                      (PUCHAR)"VideoPortCreateSecondaryDisplay");
   }

   if (pFunc != NULL) {
      pVPCreateSecDisp = (pCreateSecDisp)pFunc;

      /* Query the configured number of displays */
      VideoPortWritePortUshort((PUSHORT)VBE_DISPI_IOPORT_INDEX, VBE_DISPI_INDEX_CMONITORS);
      gNumDisplays = VideoPortReadPortUshort((PUSHORT)VBE_DISPI_IOPORT_DATA);
      dprintf(("VBoxVideo::VBoxRegisterSecondaryDisplays: gNumDisplays = %d\n", gNumDisplays));

      PrimaryExtension->bDualViewSupported = (gNumDisplays != 0);

      /* Now when the number of monitors is known, update the maximum size. */
      PrimaryExtension->ulMaxFrameBufferSize = VBoxVideoComputeMaxFrameBufferSize (AdapterMemorySize);

      for (iDisplay = 1; iDisplay < gNumDisplays; ++iDisplay)
      {
         rc = pVPCreateSecDisp(PrimaryExtension, (PVOID*)&pSecExt, VIDEO_DUALVIEW_REMOVABLE);
         dprintf(("VBoxVideo::VBoxRegisterSecondaryDisplays: VideoPortCreateSecondaryDisplay returned %#x, pSecExt = %p\n", rc, pSecExt));
         if (rc != NO_ERROR)   /* Failure to create secondary displays is not fatal */
            break;

         pSecExt->iDevice = iDisplay;
         pSecExt->pvPrimaryExt = PrimaryExtension;

         pSecExt->ulFrameBufferOffset = iDisplay * (PrimaryExtension->ulMaxFrameBufferSize + VBOX_VIDEO_DISPLAY_INFORMATION_SIZE);
         pSecExt->ulMaxFrameBufferSize = PrimaryExtension->ulMaxFrameBufferSize;
         pSecExt->bDualViewSupported = PrimaryExtension->bDualViewSupported;
         pSecExt->AdapterInformation = PrimaryExtension->AdapterInformation;
      }
   }

   VBoxSetupAdapterInfo (PrimaryExtension);
}

VP_STATUS VBoxVideoFindAdapter(IN PVOID HwDeviceExtension,
                               IN PVOID HwContext, IN PWSTR ArgumentString,
                               IN OUT PVIDEO_PORT_CONFIG_INFO ConfigInfo,
                               OUT PUCHAR Again)
{
   VP_STATUS rc;
   USHORT DispiId;
   ULONG AdapterMemorySize = VBE_DISPI_TOTAL_VIDEO_MEMORY_BYTES;
   VIDEO_ACCESS_RANGE AccessRanges[] =
   {
      {
         {0, VBE_DISPI_LFB_PHYSICAL_ADDRESS},
         VBE_DISPI_TOTAL_VIDEO_MEMORY_BYTES,
         0,
         FALSE,
         FALSE,
         0
      }
   };

   dprintf(("VBoxVideo::VBoxVideoFindAdapter\n"));

   VideoPortWritePortUshort((PUSHORT)VBE_DISPI_IOPORT_INDEX, VBE_DISPI_INDEX_ID);
   VideoPortWritePortUshort((PUSHORT)VBE_DISPI_IOPORT_DATA, VBE_DISPI_ID2);
   DispiId = VideoPortReadPortUshort((PUSHORT)VBE_DISPI_IOPORT_DATA);
   if (DispiId == VBE_DISPI_ID2)
   {
      dprintf(("VBoxVideo::VBoxVideoFoundAdapter: found the VBE card\n"));
      /*
       * Write some hardware information to registry, so that
       * it's visible in Windows property dialog.
       */

      rc = VideoPortSetRegistryParameters(
         HwDeviceExtension,
         L"HardwareInformation.ChipType",
         VBoxChipType,
         sizeof(VBoxChipType));

      rc = VideoPortSetRegistryParameters(
         HwDeviceExtension,
         L"HardwareInformation.DacType",
         VBoxDACType,
         sizeof(VBoxDACType));

      /*
       * Query the adapter's memory size. It's a bit of a hack, we just read
       * an ULONG from the data port without setting an index before.
       */
      AdapterMemorySize = VideoPortReadPortUlong((PULONG)VBE_DISPI_IOPORT_DATA);
      rc = VideoPortSetRegistryParameters(
         HwDeviceExtension,
         L"HardwareInformation.MemorySize",
         &AdapterMemorySize,
         sizeof(ULONG));

      rc = VideoPortSetRegistryParameters(
         HwDeviceExtension,
         L"HardwareInformation.AdapterString",
         VBoxAdapterString,
         sizeof(VBoxAdapterString));

      rc = VideoPortSetRegistryParameters(
         HwDeviceExtension,
         L"HardwareInformation.BiosString",
         VBoxBiosString,
         sizeof(VBoxBiosString));

      rc = VideoPortVerifyAccessRanges(HwDeviceExtension, 1, AccessRanges);
      dprintf(("VBoxVideo::VBoxVideoFindAdapter: VideoPortVerifyAccessRanges returned 0x%x\n", rc));
      // @todo for some reason, I get an ERROR_INVALID_PARAMETER from NT4 SP0
      // It does not seem to like getting me these port addresses. So I just
      // pretend success to make the driver work.
      rc = NO_ERROR;

      /* Initialize VBoxGuest library */
      rc = VbglInit ();

      dprintf(("VBoxVideo::VBoxVideoFindAdapter: VbglInit returned 0x%x\n", rc));

      /* Attempt to register secondary displays */
      VBoxSetupDisplays((PDEVICE_EXTENSION)HwDeviceExtension, ConfigInfo);

      // pretend success to make the driver work.
      rc = NO_ERROR;
   } else
   {
       dprintf(("VBoxVideo::VBoxVideoFindAdapter: VBE card not found, returning ERROR_DEV_NOT_EXIST\n"));
       rc = ERROR_DEV_NOT_EXIST;
   }
   dprintf(("VBoxVideo::VBoxVideoFindAdapter: returning with rc = 0x%x\n", rc));
   return rc;
}

/**
 * VBoxVideoInitialize
 *
 * Performs the first initialization of the adapter, after the HAL has given
 * up control of the video hardware to the video port driver.
 */
BOOLEAN VBoxVideoInitialize(PVOID HwDeviceExtension)
{
    VP_STATUS status;

    dprintf(("VBoxVideo::VBoxVideoInitialize\n"));

    PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)HwDeviceExtension;

    /* Initialize the request pointer. */
    pDevExt->pvReqFlush = NULL;

    /*
     * Get the last custom resolution
     */
    status = VideoPortGetRegistryParameters(HwDeviceExtension,
                                            L"CustomXRes",
                                            FALSE,
                                            VBoxRegistryCallback,
                                            &gCustomXRes);
    if (status != NO_ERROR)
        gCustomXRes = 0;
    status = VideoPortGetRegistryParameters(HwDeviceExtension,
                                            L"CustomYRes",
                                            FALSE,
                                            VBoxRegistryCallback,
                                            &gCustomYRes);
    if (status != NO_ERROR)
        gCustomYRes = 0;
    status = VideoPortGetRegistryParameters(HwDeviceExtension,
                                            L"CustomBPP",
                                            FALSE,
                                            VBoxRegistryCallback,
                                            &gCustomBPP);
    if (status != NO_ERROR)
        gCustomBPP = 0;

   dprintf(("VBoxVideo: got stored custom resolution %dx%dx%d\n", gCustomXRes, gCustomYRes, gCustomBPP));

   return TRUE;
}

/**
 * VBoxVideoStartIO
 *
 * Processes the specified Video Request Packet.
 */
BOOLEAN VBoxVideoStartIO(PVOID HwDeviceExtension,
                         PVIDEO_REQUEST_PACKET RequestPacket)
{
    PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)HwDeviceExtension;

    BOOLEAN Result;

//    dprintf(("VBoxVideo::VBoxVideoStartIO: code %08X\n", RequestPacket->IoControlCode));

    RequestPacket->StatusBlock->Status = ERROR_INVALID_FUNCTION;

    switch (RequestPacket->IoControlCode)
    {
        case IOCTL_VIDEO_SET_CURRENT_MODE:
        {
            if (RequestPacket->InputBufferLength < sizeof(VIDEO_MODE))
            {
                RequestPacket->StatusBlock->Status = ERROR_INSUFFICIENT_BUFFER;
                return TRUE;
            }
            Result = VBoxVideoSetCurrentMode((PDEVICE_EXTENSION)HwDeviceExtension,
                                             (PVIDEO_MODE)RequestPacket->InputBuffer,
                                             RequestPacket->StatusBlock);
            break;
        }

        case IOCTL_VIDEO_RESET_DEVICE:
        {
            Result = VBoxVideoResetDevice((PDEVICE_EXTENSION)HwDeviceExtension,
                                          RequestPacket->StatusBlock);
            break;
        }

        case IOCTL_VIDEO_MAP_VIDEO_MEMORY:
        {
            if (RequestPacket->OutputBufferLength < sizeof(VIDEO_MEMORY_INFORMATION) ||
                RequestPacket->InputBufferLength < sizeof(VIDEO_MEMORY))
            {
                RequestPacket->StatusBlock->Status = ERROR_INSUFFICIENT_BUFFER;
                return TRUE;
            }
            Result = VBoxVideoMapVideoMemory((PDEVICE_EXTENSION)HwDeviceExtension,
                                             (PVIDEO_MEMORY)RequestPacket->InputBuffer,
                                             (PVIDEO_MEMORY_INFORMATION)RequestPacket->OutputBuffer,
                                             RequestPacket->StatusBlock);
            break;
        }

        case IOCTL_VIDEO_UNMAP_VIDEO_MEMORY:
        {
            if (RequestPacket->InputBufferLength < sizeof(VIDEO_MEMORY))
            {
                RequestPacket->StatusBlock->Status = ERROR_INSUFFICIENT_BUFFER;
                return TRUE;
            }
            Result = VBoxVideoUnmapVideoMemory((PDEVICE_EXTENSION)HwDeviceExtension,
                                               (PVIDEO_MEMORY)RequestPacket->InputBuffer,
                                               RequestPacket->StatusBlock);
            break;
        }

        /*
         * The display driver asks us how many video modes we support
         * so that it can supply an appropriate buffer for the next call.
         */
        case IOCTL_VIDEO_QUERY_NUM_AVAIL_MODES:
        {
            if (RequestPacket->OutputBufferLength < sizeof(VIDEO_NUM_MODES))
            {
                RequestPacket->StatusBlock->Status = ERROR_INSUFFICIENT_BUFFER;
                return TRUE;
            }
            Result = VBoxVideoQueryNumAvailModes((PDEVICE_EXTENSION)HwDeviceExtension,
                                                 (PVIDEO_NUM_MODES)RequestPacket->OutputBuffer,
                                                 RequestPacket->StatusBlock);
            break;
        }

        /*
         * The display driver asks us to provide a list of supported video modes
         * into a buffer it has allocated.
         */
        case IOCTL_VIDEO_QUERY_AVAIL_MODES:
        {
            if (RequestPacket->OutputBufferLength <
                gNumVideoModes * sizeof(VIDEO_MODE_INFORMATION))
            {
                RequestPacket->StatusBlock->Status = ERROR_INSUFFICIENT_BUFFER;
                return TRUE;
            }
            Result = VBoxVideoQueryAvailModes((PDEVICE_EXTENSION)HwDeviceExtension,
                                              (PVIDEO_MODE_INFORMATION)RequestPacket->OutputBuffer,
                                              RequestPacket->StatusBlock);
            break;
        }

        case IOCTL_VIDEO_SET_COLOR_REGISTERS:
        {
            if (RequestPacket->InputBufferLength < sizeof(VIDEO_CLUT) ||
                RequestPacket->InputBufferLength <
                (((PVIDEO_CLUT)RequestPacket->InputBuffer)->NumEntries * sizeof(ULONG)) +
                sizeof(VIDEO_CLUT))
            {
                RequestPacket->StatusBlock->Status = ERROR_INSUFFICIENT_BUFFER;
                return TRUE;
            }
            Result = VBoxVideoSetColorRegisters((PDEVICE_EXTENSION)HwDeviceExtension,
                                                (PVIDEO_CLUT)RequestPacket->InputBuffer,
                                                RequestPacket->StatusBlock);
            break;
        }

        case IOCTL_VIDEO_QUERY_CURRENT_MODE:
        {
            if (RequestPacket->OutputBufferLength < sizeof(VIDEO_MODE_INFORMATION))
            {
                RequestPacket->StatusBlock->Status = ERROR_INSUFFICIENT_BUFFER;
                return TRUE;
            }
            Result = VBoxVideoQueryCurrentMode((PDEVICE_EXTENSION)HwDeviceExtension,
                                               (PVIDEO_MODE_INFORMATION)RequestPacket->OutputBuffer,
                                               RequestPacket->StatusBlock);
            break;
        }

        // show the pointer
        case IOCTL_VIDEO_ENABLE_POINTER:
        {
            dprintf(("VBoxVideo::VBoxVideoStartIO: IOCTL_VIDEO_ENABLE_POINTER\n"));
            // find out whether the host wants absolute positioning
            if (vboxQueryHostWantsAbsolute())
            {
                // tell the host to use the guest's pointer
                VIDEO_POINTER_ATTRIBUTES PointerAttributes;

                /* Visible and No Shape means Show the pointer.
                 * It is enough to init only this field.
                 */
                PointerAttributes.Enable = VBOX_MOUSE_POINTER_VISIBLE;

                Result = vboxUpdatePointerShape(&PointerAttributes, sizeof (PointerAttributes));

                if (!Result)
                    dprintf(("VBoxVideo::VBoxVideoStartIO: could not hide hardware pointer -> fallback\n"));
            } else
            {
                // fallback to software pointer
                RequestPacket->StatusBlock->Status = ERROR_INVALID_FUNCTION;
                Result = FALSE;
            }
            break;
        }

        // hide the pointer
        case IOCTL_VIDEO_DISABLE_POINTER:
        {
            dprintf(("VBoxVideo::VBoxVideoStartIO: IOCTL_VIDEO_DISABLE_POINTER\n"));
            // find out whether the host wants absolute positioning
            if (vboxQueryHostWantsAbsolute())
            {
                // tell the host to hide pointer
                VIDEO_POINTER_ATTRIBUTES PointerAttributes;

                /* Enable == 0 means no shape, not visible.
                 * It is enough to init only this field.
                 */
                PointerAttributes.Enable = 0;

                Result = vboxUpdatePointerShape(&PointerAttributes, sizeof (PointerAttributes));

                if (!Result)
                    dprintf(("VBoxVideo::VBoxVideoStartIO: could not hide hardware pointer -> fallback\n"));
            } else
            {
                // fallback to software pointer
                RequestPacket->StatusBlock->Status = ERROR_INVALID_FUNCTION;
                Result = FALSE;
            }
            break;
        }

        /*
         * Change the pointer shape
         */
        case IOCTL_VIDEO_SET_POINTER_ATTR:
        {
            dprintf(("VBoxVideo::VBoxVideoStartIO: IOCTL_VIDEO_SET_POINTER_ATTR\n"));
            if (RequestPacket->InputBufferLength < sizeof(VIDEO_POINTER_ATTRIBUTES))
            {
                dprintf(("VBoxVideo::VBoxVideoStartIO: Input buffer too small (%d bytes)\n", RequestPacket->InputBufferLength));
                RequestPacket->StatusBlock->Status = ERROR_INSUFFICIENT_BUFFER;
                return TRUE;
            }
            // find out whether the host wants absolute positioning
            if (vboxQueryHostWantsAbsolute())
            {
                PVIDEO_POINTER_ATTRIBUTES pPointerAttributes = (PVIDEO_POINTER_ATTRIBUTES)RequestPacket->InputBuffer;
#if 0
                dprintf(("Pointer shape information:\n"
                         "\tFlags:          %d\n"
                         "\tWidth:          %d\n"
                         "\tHeight:         %d\n"
                         "\tWidthInBytes:   %d\n"
                         "\tEnable:         %d\n"
                         "\tColumn:         %d\n"
                         "\tRow:            %d\n",
                         pPointerAttributes->Flags, pPointerAttributes->Width, pPointerAttributes->Height,
                         pPointerAttributes->WidthInBytes, pPointerAttributes->Enable, pPointerAttributes->Column,
                         pPointerAttributes->Row));
                dprintf(("\tBytes attached: %d\n", RequestPacket->InputBufferLength - sizeof(VIDEO_POINTER_ATTRIBUTES)));
#endif
                Result = vboxUpdatePointerShape(pPointerAttributes, RequestPacket->InputBufferLength);
                if (!Result)
                    dprintf(("VBoxVideo::VBoxVideoStartIO: could not set hardware pointer -> fallback\n"));
            } else
            {
                dprintf(("VBoxVideo::VBoxVideoStartIO: fallback to software pointer\n"));
                // fallback to software pointer
                RequestPacket->StatusBlock->Status = ERROR_INVALID_FUNCTION;
                Result = FALSE;
            }
            break;
        }

        // query pointer information
        case IOCTL_VIDEO_QUERY_POINTER_ATTR:
        {
            dprintf(("VBoxVideo::VBoxVideoStartIO: IOCTL_VIDEO_QUERY_POINTER_ATTR\n"));
            RequestPacket->StatusBlock->Status = ERROR_INVALID_FUNCTION;
            Result = FALSE;
            break;
        }

        // set the pointer position
        case IOCTL_VIDEO_SET_POINTER_POSITION:
        {
            /// @todo There is an issue when we disable pointer integration.
            // The guest pointer will be invisible. We have to somehow cause
            // the pointer attributes to be set again. But how? The same holds
            // true for the opposite case where we get two pointers.

            //dprintf(("VBoxVideo::VBoxVideoStartIO: IOCTL_VIDEO_SET_POINTER_POSITION\n"));
            // find out whether the host wants absolute positioning
            if (vboxQueryHostWantsAbsolute())
            {
                // @todo we are supposed to show currently invisible pointer?
                Result = TRUE;
            } else
            {
                // fallback to software pointer
                RequestPacket->StatusBlock->Status = ERROR_INVALID_FUNCTION;
                Result = FALSE;
            }
            break;
        }

        // query the pointer position
        case IOCTL_VIDEO_QUERY_POINTER_POSITION:
        {
            dprintf(("VBoxVideo::VBoxVideoStartIO: IOCTL_VIDEO_QUERY_POINTER_POSITION\n"));
            if (RequestPacket->OutputBufferLength < sizeof(VIDEO_POINTER_POSITION))
            {
                dprintf(("VBoxVideo::VBoxVideoStartIO: output buffer too small!\n"));
                RequestPacket->StatusBlock->Status = ERROR_INSUFFICIENT_BUFFER;
                return TRUE;
            }
            Result = FALSE;
            uint16_t mousePosX;
            uint16_t mousePosY;
            if (vboxQueryPointerPos(&mousePosX, &mousePosY))
            {
                PVIDEO_POINTER_POSITION pointerPos = (PVIDEO_POINTER_POSITION)RequestPacket->OutputBuffer;
                PVIDEO_MODE_INFORMATION ModeInfo;
                ModeInfo = &VideoModes[((PDEVICE_EXTENSION)HwDeviceExtension)->CurrentMode - 1];
                // map from 0xFFFF to the current resolution
                pointerPos->Column = (SHORT)(mousePosX    / (0xFFFF / ModeInfo->VisScreenWidth));
                pointerPos->Row    = (SHORT)(mousePosY / (0xFFFF / ModeInfo->VisScreenHeight));
                RequestPacket->StatusBlock->Information = sizeof(VIDEO_POINTER_POSITION);
                Result = TRUE;
            }
            if (!Result)
            {
                // fallback to software pointer
                RequestPacket->StatusBlock->Status = ERROR_INVALID_FUNCTION;
            }
            break;
        }

        // Determine hardware cursor capabilities. We will always report that we are
        // very capable even though the host might not want to do pointer integration.
        // This is done because we can still return errors on the actual calls later to
        // make the display driver go to the fallback routines.
        case IOCTL_VIDEO_QUERY_POINTER_CAPABILITIES:
        {
            dprintf(("VBoxVideo::VBoxVideoStartIO: IOCTL_VIDEO_QUERY_POINTER_CAPABILITIES\n"));
            if (RequestPacket->OutputBufferLength < sizeof(VIDEO_POINTER_CAPABILITIES))
            {
                dprintf(("VBoxVideo::VBoxVideoStartIO: output buffer too small!\n"));
                RequestPacket->StatusBlock->Status = ERROR_INSUFFICIENT_BUFFER;
                return TRUE;
            }
            PVIDEO_POINTER_CAPABILITIES pCaps = (PVIDEO_POINTER_CAPABILITIES)RequestPacket->OutputBuffer;
            pCaps->Flags = VIDEO_MODE_ASYNC_POINTER |
                           VIDEO_MODE_COLOR_POINTER |
                           VIDEO_MODE_MONO_POINTER;
            // for now we go with 64x64 cursors
            pCaps->MaxWidth  = 64;
            pCaps->MaxHeight = 64;
            // that doesn't seem to be relevant, VBoxDisp doesn't use it
            pCaps->HWPtrBitmapStart = -1;
            pCaps->HWPtrBitmapEnd   = -1;
            RequestPacket->StatusBlock->Information = sizeof(VIDEO_POINTER_CAPABILITIES);
            Result = TRUE;
            break;
        }

        /* Attach/detach DualView devices */
        case IOCTL_VIDEO_SWITCH_DUALVIEW:
        {
            ULONG ulAttach;

            ulAttach = *((PULONG)RequestPacket->InputBuffer);
            dprintf(("VBoxVideo::VBoxVideoStartIO: IOCTL_VIDEO_SWITCH_DUALVIEW (%ld)\n", ulAttach));

            pDevExt->bEnabled = (BOOLEAN)ulAttach;
            Result = TRUE;
            break;
        }

        case IOCTL_VIDEO_INTERPRET_DISPLAY_MEMORY:
        {
            dprintf(("VBoxVideo::VBoxVideoStartIO: IOCTL_VIDEO_INTERPRET_DISPLAY_MEMORY\n"));

            if (pDevExt->bDualViewSupported)
            {
                /* The display driver must have prepared the monitor information. */
                VideoPortWritePortUshort((PUSHORT)VBE_DISPI_IOPORT_INDEX, VBE_DISPI_INDEX_CMONITORS);
                VideoPortWritePortUlong((PULONG)VBE_DISPI_IOPORT_DATA, VBOX_VIDEO_INTERPRET_DISPLAY_MEMORY_BASE + pDevExt->iDevice);
            }
            else
            {
                RequestPacket->StatusBlock->Status = ERROR_INVALID_FUNCTION;
            }
            Result = pDevExt->bDualViewSupported;
            break;
        }

        case IOCTL_VIDEO_QUERY_DISPLAY_INFO:
        {
            dprintf(("VBoxVideo::VBoxVideoStartIO: IOCTL_VIDEO_QUERY_DISPLAY_INFO\n"));

            if (RequestPacket->OutputBufferLength < sizeof(QUERYDISPLAYINFORESULT))
            {
                dprintf(("VBoxVideo::VBoxVideoStartIO: output buffer too small: %d needed: %d!!!\n",
                         RequestPacket->OutputBufferLength, sizeof(QUERYDISPLAYINFORESULT)));
                RequestPacket->StatusBlock->Status = ERROR_INSUFFICIENT_BUFFER;
                return FALSE;
            }

            QUERYDISPLAYINFORESULT *pDispInfo = (QUERYDISPLAYINFORESULT *)RequestPacket->OutputBuffer;
            
            pDispInfo->iDevice = pDevExt->iDevice;
            pDispInfo->u32DisplayInfoSize = VBOX_VIDEO_DISPLAY_INFORMATION_SIZE;

            RequestPacket->StatusBlock->Information = sizeof(QUERYDISPLAYINFORESULT);
            Result = TRUE;

            break;
        }

        case IOCTL_VIDEO_VBVA_ENABLE:
        {
            int rc;
            ULONG ulEnable;

            dprintf(("VBoxVideo::VBoxVideoStartIO: IOCTL_VIDEO_VBVA_ENABLE\n"));

            if (RequestPacket->InputBufferLength < sizeof(ULONG))
            {
                dprintf(("VBoxVideo::VBoxVideoStartIO: input buffer too small: %d needed: %d!!!\n",
                         RequestPacket->InputBufferLength, sizeof(ULONG)));
                RequestPacket->StatusBlock->Status = ERROR_INSUFFICIENT_BUFFER;
                return FALSE;
            }

            if (RequestPacket->OutputBufferLength < sizeof(VBVAENABLERESULT))
            {
                dprintf(("VBoxVideo::VBoxVideoStartIO: output buffer too small: %d needed: %d!!!\n",
                         RequestPacket->OutputBufferLength, sizeof(VBVAENABLERESULT)));
                RequestPacket->StatusBlock->Status = ERROR_INSUFFICIENT_BUFFER;
                return FALSE;
            }

            ulEnable = *(ULONG *)RequestPacket->InputBuffer;
            dprintf(("VBoxVideo::VBoxVideoStartIO: IOCTL_VIDEO_VBVA_ENABLE ulEnable = %08X\n", ulEnable));

            rc = vboxVbvaEnable (pDevExt, ulEnable, (VBVAENABLERESULT *)RequestPacket->OutputBuffer);
            dprintf(("VBoxVideo::VBoxVideoStartIO: IOCTL_VIDEO_VBVA_ENABLE completed rc = %Vrc\n", rc));

            if (VBOX_FAILURE (rc))
            {
                dprintf(("VBoxVideo::VBoxVideoStartIO: IOCTL_VIDEO_VBVA_ENABLE: failed to enable VBVA\n"));
                RequestPacket->StatusBlock->Status = ERROR_INVALID_FUNCTION;
                return FALSE;
            }

            RequestPacket->StatusBlock->Information = sizeof(VBVAENABLERESULT);
            Result = TRUE;

            break;
        }

        /* Private ioctls */
        case IOCTL_VIDEO_VBOX_SETVISIBLEREGION:
        {
            if (RequestPacket->InputBufferLength < sizeof(RTRECT))
            {
                dprintf(("VBoxVideo::IOCTL_VIDEO_VBOX_SETVISIBLEREGION: output buffer too small: %d needed: %d!!!\n",
                         RequestPacket->OutputBufferLength, sizeof(RTRECT)));
                RequestPacket->StatusBlock->Status = ERROR_INSUFFICIENT_BUFFER;
                return FALSE;
            }
        }

        default:
            dprintf(("VBoxVideo::VBoxVideoStartIO: unsupported %p, fn %d(0x%x)\n",
                      RequestPacket->IoControlCode,
                      (RequestPacket->IoControlCode >> 2) & 0xFFF,
                      (RequestPacket->IoControlCode >> 2) & 0xFFF));
            RequestPacket->StatusBlock->Status = ERROR_INVALID_FUNCTION;
            return FALSE;
    }

    if (Result)
        RequestPacket->StatusBlock->Status = NO_ERROR;
    else
        RequestPacket->StatusBlock->Information = 0;

//    dprintf(("VBoxVideo::VBoxVideoStartIO: completed\n"));

    return TRUE;
}

/**
 * VBoxVideoReset HW
 *
 * Resets the video hardware.
 */
BOOLEAN VBoxVideoResetHW(PVOID HwDeviceExtension, ULONG Columns, ULONG Rows)
{
    dprintf(("VBoxVideo::VBoxVideoResetHW\n"));

    PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)HwDeviceExtension;

    if (pDevExt->iDevice > 0)
    {
        dprintf(("VBoxVideo::VBoxVideoResetHW: Skipping for non-primary display %d\n",
                 pDevExt->iDevice));
        return TRUE;
    }

    VideoPortWritePortUshort((PUSHORT)VBE_DISPI_IOPORT_INDEX, VBE_DISPI_INDEX_ENABLE);
    VideoPortWritePortUshort((PUSHORT)VBE_DISPI_IOPORT_DATA, VBE_DISPI_DISABLED);

    if (pDevExt->pvReqFlush != NULL)
    {
        VbglGRFree ((VMMDevRequestHeader *)pDevExt->pvReqFlush);
        pDevExt->pvReqFlush = NULL;
    }

    VbglTerminate ();

    return TRUE;
}

/**
 * VBoxVideoGetPowerState
 *
 * Queries whether the device can support the requested power state.
 */
VP_STATUS VBoxVideoGetPowerState(PVOID HwDeviceExtension, ULONG HwId,
                                 PVIDEO_POWER_MANAGEMENT VideoPowerControl)
{
    dprintf(("VBoxVideo::VBoxVideoGetPowerState\n"));
    return NO_ERROR;
}

/**
 * VBoxVideoSetPowerState
 *
 * Sets the power state of the specified device
 */
VP_STATUS VBoxVideoSetPowerState(PVOID HwDeviceExtension, ULONG HwId,
                                 PVIDEO_POWER_MANAGEMENT VideoPowerControl)
{
    dprintf(("VBoxVideo::VBoxVideoSetPowerState\n"));
    return NO_ERROR;
}

/**
 * VBoxVideoSetCurrentMode
 *
 * Sets the adapter to the specified operating mode.
 */
BOOLEAN FASTCALL VBoxVideoSetCurrentMode(PDEVICE_EXTENSION DeviceExtension,
                                         PVIDEO_MODE RequestedMode, PSTATUS_BLOCK StatusBlock)
{
    PVIDEO_MODE_INFORMATION ModeInfo;

    dprintf(("VBoxVideo::VBoxVideoSetCurrentMode: mode = %d\n", RequestedMode->RequestedMode));

    DeviceExtension->CurrentMode = RequestedMode->RequestedMode;
    ModeInfo = &VideoModes[DeviceExtension->CurrentMode - 1];
    dprintf(("VBoxVideoSetCurrentMode: width: %d, height: %d, bpp: %d\n", ModeInfo->VisScreenWidth,
             ModeInfo->VisScreenHeight, ModeInfo->BitsPerPlane));

    if (DeviceExtension->iDevice > 0)
    {
        dprintf(("VBoxVideo::VBoxVideoSetCurrentMode: Skipping for non-primary display %d\n",
                 DeviceExtension->iDevice));
        return TRUE;
    }

    /* set the mode characteristics */
    VideoPortWritePortUshort((PUSHORT)VBE_DISPI_IOPORT_INDEX, VBE_DISPI_INDEX_XRES);
    VideoPortWritePortUshort((PUSHORT)VBE_DISPI_IOPORT_DATA, (USHORT)ModeInfo->VisScreenWidth);
    VideoPortWritePortUshort((PUSHORT)VBE_DISPI_IOPORT_INDEX, VBE_DISPI_INDEX_YRES);
    VideoPortWritePortUshort((PUSHORT)VBE_DISPI_IOPORT_DATA, (USHORT)ModeInfo->VisScreenHeight);
    VideoPortWritePortUshort((PUSHORT)VBE_DISPI_IOPORT_INDEX, VBE_DISPI_INDEX_BPP);
    VideoPortWritePortUshort((PUSHORT)VBE_DISPI_IOPORT_DATA, (USHORT)ModeInfo->BitsPerPlane);
    /* enable the mode */
    VideoPortWritePortUshort((PUSHORT)VBE_DISPI_IOPORT_INDEX, VBE_DISPI_INDEX_ENABLE);
    VideoPortWritePortUshort((PUSHORT)VBE_DISPI_IOPORT_DATA, VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED);
    /** @todo read from the port to see if the mode switch was successful */

    return TRUE;
}

/*
 * VBoxVideoResetDevice
 *
 * Resets the video hardware to the default mode, to which it was initialized
 * at system boot.
 */

BOOLEAN FASTCALL VBoxVideoResetDevice(
   PDEVICE_EXTENSION DeviceExtension,
   PSTATUS_BLOCK StatusBlock)
{
   dprintf(("VBoxVideo::VBoxVideoResetDevice\n"));

    if (DeviceExtension->iDevice > 0)
    {
        /* If the device is the secondary display, however, it is recommended that no action be taken. */
        dprintf(("VBoxVideo::VBoxVideoResetDevice: Skipping for non-primary display %d\n",
                 DeviceExtension->iDevice));
        return TRUE;
    }

#if 0
   /* Don't disable the extended video mode. This would only switch the video mode
    * to <current width> x <current height> x 0 bpp which is not what we want. And
    * even worse, it causes an disturbing additional mode switch */
   VideoPortWritePortUshort((PUSHORT)VBE_DISPI_IOPORT_INDEX, VBE_DISPI_INDEX_ENABLE);
   VideoPortWritePortUshort((PUSHORT)VBE_DISPI_IOPORT_DATA, VBE_DISPI_DISABLED);
#endif

   return TRUE;
}

/**
 * VBoxVideoMapVideoMemory
 *
 * Maps the video hardware frame buffer and video RAM into the virtual address
 * space of the requestor.
 */
BOOLEAN FASTCALL VBoxVideoMapVideoMemory(PDEVICE_EXTENSION DeviceExtension,
                                         PVIDEO_MEMORY RequestedAddress,
                                         PVIDEO_MEMORY_INFORMATION MapInformation,
                                         PSTATUS_BLOCK StatusBlock)
{
    PHYSICAL_ADDRESS FrameBuffer;
    ULONG inIoSpace = 0;
    VP_STATUS Status;

    dprintf(("VBoxVideo::VBoxVideoMapVideoMemory\n"));

    FrameBuffer.QuadPart = VBE_DISPI_LFB_PHYSICAL_ADDRESS + DeviceExtension->ulFrameBufferOffset;

    MapInformation->VideoRamBase = RequestedAddress->RequestedVirtualAddress;
    MapInformation->VideoRamLength = DeviceExtension->ulMaxFrameBufferSize + VBOX_VIDEO_DISPLAY_INFORMATION_SIZE;

    Status = VideoPortMapMemory(DeviceExtension, FrameBuffer,
       &MapInformation->VideoRamLength, &inIoSpace,
       &MapInformation->VideoRamBase);

    if (Status == NO_ERROR)
    {
        MapInformation->FrameBufferBase = (PUCHAR)MapInformation->VideoRamBase;
        MapInformation->FrameBufferLength =
            VideoModes[DeviceExtension->CurrentMode - 1].VisScreenHeight *
            VideoModes[DeviceExtension->CurrentMode - 1].ScreenStride;
        StatusBlock->Information = sizeof(VIDEO_MEMORY_INFORMATION);

        return TRUE;
    }

    return FALSE;
}

/**
 * VBoxVideoUnmapVideoMemory
 *
 * Releases a mapping between the virtual address space and the adapter's
 * frame buffer and video RAM.
 */
BOOLEAN FASTCALL VBoxVideoUnmapVideoMemory(PDEVICE_EXTENSION DeviceExtension,
                                           PVIDEO_MEMORY VideoMemory, PSTATUS_BLOCK StatusBlock)
{
    dprintf(("VBoxVideo::VBoxVideoUnmapVideoMemory\n"));
    VideoPortUnmapMemory(DeviceExtension, VideoMemory->RequestedVirtualAddress, NULL);
    return TRUE;
}

/**
 * VBoxVideoQueryNumAvailModes
 *
 * Returns the number of video modes supported by the adapter and the size
 * in bytes of the video mode information, which can be used to allocate a
 * buffer for an IOCTL_VIDEO_QUERY_AVAIL_MODES request.
 */
BOOLEAN FASTCALL VBoxVideoQueryNumAvailModes(PDEVICE_EXTENSION DeviceExtension,
                                             PVIDEO_NUM_MODES Modes, PSTATUS_BLOCK StatusBlock)
{
    dprintf(("VBoxVideo::VBoxVideoQueryNumAvailModes\n"));
    /* calculate the video modes table */
    VBoxBuildModesTable(DeviceExtension);
    Modes->NumModes = gNumVideoModes;
    Modes->ModeInformationLength = sizeof(VIDEO_MODE_INFORMATION);
    StatusBlock->Information = sizeof(VIDEO_NUM_MODES);
    dprintf(("VBoxVideo::VBoxVideoQueryNumAvailModes: number of modes: %d\n", Modes->NumModes));
    return TRUE;
}

/**
 * VBoxVideoQueryAvailModes
 *
 * Returns information about each video mode supported by the adapter.
 */
BOOLEAN FASTCALL VBoxVideoQueryAvailModes(PDEVICE_EXTENSION DeviceExtension,
                                          PVIDEO_MODE_INFORMATION ReturnedModes,
                                          PSTATUS_BLOCK StatusBlock)
{
    ULONG Size;

    dprintf(("VBoxVideo::VBoxVideoQueryAvailModes\n"));

    Size = gNumVideoModes * sizeof(VIDEO_MODE_INFORMATION);
    VideoPortMoveMemory(ReturnedModes, VideoModes, Size);
    StatusBlock->Information = Size;

    return TRUE;
}

/**
 * VBoxVideoQueryCurrentMode
 *
 * Returns information about current video mode.
 */
BOOLEAN FASTCALL VBoxVideoQueryCurrentMode(PDEVICE_EXTENSION DeviceExtension,
                                           PVIDEO_MODE_INFORMATION VideoModeInfo,
                                           PSTATUS_BLOCK StatusBlock)
{
    dprintf(("VBoxVideo::VBoxVideoQueryCurrentMode\n"));

    StatusBlock->Information = sizeof(VIDEO_MODE_INFORMATION);
    VideoPortMoveMemory(VideoModeInfo, VideoModes + DeviceExtension->CurrentMode - 1, 1);

    return TRUE;
}

/*
 * VBoxVideoSetColorRegisters
 *
 * Sets the adapter's color registers to the specified RGB values. There
 * are code paths in this function, one generic and one for VGA compatible
 * controllers. The latter is needed for Bochs, where the generic one isn't
 * yet implemented.
 */

BOOLEAN FASTCALL VBoxVideoSetColorRegisters(
   PDEVICE_EXTENSION DeviceExtension,
   PVIDEO_CLUT ColorLookUpTable,
   PSTATUS_BLOCK StatusBlock)
{
   LONG Entry;

   dprintf(("VBoxVideo::VBoxVideoSetColorRegisters first entry %d num entries %d\n", ColorLookUpTable->FirstEntry, ColorLookUpTable->NumEntries));

   if (ColorLookUpTable->NumEntries + ColorLookUpTable->FirstEntry > 256)
      return FALSE;

   for (Entry = ColorLookUpTable->FirstEntry;
        Entry < ColorLookUpTable->NumEntries + ColorLookUpTable->FirstEntry;
        Entry++)
   {
      VideoPortWritePortUchar((PUCHAR)0x03c8, (UCHAR)Entry);
      VideoPortWritePortUchar((PUCHAR)0x03c9, ColorLookUpTable->LookupTable[Entry].RgbArray.Red);
      VideoPortWritePortUchar((PUCHAR)0x03c9, ColorLookUpTable->LookupTable[Entry].RgbArray.Green);
      VideoPortWritePortUchar((PUCHAR)0x03c9, ColorLookUpTable->LookupTable[Entry].RgbArray.Blue);
   }

   return TRUE;
}

VP_STATUS VBoxVideoGetChildDescriptor(
   PVOID HwDeviceExtension,
   PVIDEO_CHILD_ENUM_INFO ChildEnumInfo,
   PVIDEO_CHILD_TYPE VideoChildType,
   PUCHAR pChildDescriptor,
   PULONG pUId,
   PULONG pUnused)
{
   dprintf(("VBoxVideo::VBoxVideoGetChildDescriptor: HwDeviceExtension = %p, ChildEnumInfo = %p\n",
            HwDeviceExtension, ChildEnumInfo));

    DEVICE_EXTENSION *pDevExt = (DEVICE_EXTENSION *)HwDeviceExtension;

    if (ChildEnumInfo->ChildIndex > 0)
    {
        if (   (ChildEnumInfo->ChildIndex == 1 && gNumDisplays == 0)
            || ((int)ChildEnumInfo->ChildIndex <= gNumDisplays)
           )
        {
            *VideoChildType = Monitor;
            *pUId = ChildEnumInfo->ChildIndex;

            return VIDEO_ENUM_MORE_DEVICES;
        }
    }

    return ERROR_NO_MORE_DEVICES;
}


static DECLCALLBACK(void) vboxVbvaFlush (void *pvFlush)
{
    DEVICE_EXTENSION *pDevExt = (DEVICE_EXTENSION *)pvFlush;
    DEVICE_EXTENSION *pPrimaryDevExt = (DEVICE_EXTENSION *)(pDevExt? pDevExt->pvPrimaryExt: NULL);

    if (pPrimaryDevExt)
    {
        VMMDevVideoAccelFlush *req = (VMMDevVideoAccelFlush *)pPrimaryDevExt->pvReqFlush;

        if (req)
        {
            int rc = VbglGRPerform (&req->header);

            if (VBOX_FAILURE(rc) || VBOX_FAILURE(req->header.rc))
            {
                dprintf(("VBoxVideo::vbvaFlush: rc = %Vrc, VMMDev rc = %Vrc!!!\n", rc, req->header.rc));
            }
        }
    }

    return;
}

int vboxVbvaEnable (PDEVICE_EXTENSION pDevExt, ULONG ulEnable, VBVAENABLERESULT *pVbvaResult)
{
    int rc = VINF_SUCCESS;

    dprintf(("VBoxVideo::vboxVbvaEnable: ulEnable = %08X, pVbvaResult = %p\n", ulEnable, pVbvaResult));

    /*
     * Query the VMMDev memory pointer. There we need VBVAMemory.
     */
    VMMDevMemory *pVMMDevMemory = NULL;

    rc = VbglQueryVMMDevMemory (&pVMMDevMemory);

    dprintf(("VBoxVideo::vboxVbvaEnable: VbglQueryVMMDevMemory rc = %d, pVMMDevMemory = %p\n", rc, pVMMDevMemory));
    
    if (pDevExt->iDevice > 0)
    {
        DEVICE_EXTENSION *pPrimaryDevExt = (DEVICE_EXTENSION *)pDevExt->pvPrimaryExt;

        dprintf(("VBoxVideo::vboxVbvaEnable: Skipping for non-primary display %d\n",
                 pDevExt->iDevice));
        
        if (   ulEnable
            && pPrimaryDevExt->ulVbvaEnabled)
        {
            pVbvaResult->pVbvaMemory = &pVMMDevMemory->vbvaMemory;
            pVbvaResult->pfnFlush    = vboxVbvaFlush;
            pVbvaResult->pvFlush     = pDevExt;
        }
        else
        {
            VideoPortZeroMemory(&pVbvaResult, sizeof(VBVAENABLERESULT));
        }

        return rc;
    }

    if (VBOX_SUCCESS(rc))
    {
        /* Allocate the memory block for VMMDevReq_VideoAccelFlush request. */
        if (pDevExt->pvReqFlush == NULL)
        {
            VMMDevVideoAccelFlush *req = NULL;

            rc = VbglGRAlloc ((VMMDevRequestHeader **)&req,
                              sizeof (VMMDevVideoAccelFlush),
                              VMMDevReq_VideoAccelFlush);

            if (VBOX_SUCCESS (rc))
            {
                pDevExt->pvReqFlush = req;
            }
            else
            {
                dprintf(("VBoxVideo::vboxVbvaEnable: VbglGRAlloc (VMMDevReq_VideoAccelFlush) rc = %Vrc!!!\n", rc));
            }
        }
    }
    else
    {
        dprintf(("VBoxVideo::vboxVbvaEnable: VbglQueryVMMDevMemory rc = %Vrc!!!\n", rc));
    }

    if (VBOX_SUCCESS(rc))
    {
        ULONG ulEnabled = 0;

        /*
         * Tell host that VBVA status is changed.
         */
        VMMDevVideoAccelEnable *req = NULL;

        rc = VbglGRAlloc ((VMMDevRequestHeader **)&req,
                          sizeof (VMMDevVideoAccelEnable),
                          VMMDevReq_VideoAccelEnable);

        if (VBOX_SUCCESS(rc))
        {
            req->u32Enable = ulEnable;
            req->cbRingBuffer = VBVA_RING_BUFFER_SIZE;
            req->fu32Status = 0;

            rc = VbglGRPerform (&req->header);

            if (VBOX_SUCCESS(rc) && VBOX_SUCCESS(req->header.rc))
            {
                if (req->fu32Status & VBVA_F_STATUS_ACCEPTED)
                {
                    /*
                     * Initialize the result information and VBVA memory.
                     */
                    if (req->fu32Status & VBVA_F_STATUS_ENABLED)
                    {
                        pVbvaResult->pVbvaMemory = &pVMMDevMemory->vbvaMemory;
                        pVbvaResult->pfnFlush    = vboxVbvaFlush;
                        pVbvaResult->pvFlush     = pDevExt;
                        ulEnabled = 1;
                    }
                    else
                    {
                        VideoPortZeroMemory(&pVbvaResult, sizeof(VBVAENABLERESULT));
                    }

                    dprintf(("VBoxVideo::vboxVbvaEnable: success.\n"));
                }
                else
                {
                    dprintf(("VBoxVideo::vboxVbvaEnable: not accepted.\n"));

                    /* Disable VBVA for old hosts. */
                    req->u32Enable = 0;
                    req->cbRingBuffer = VBVA_RING_BUFFER_SIZE;
                    req->fu32Status = 0;

                    VbglGRPerform (&req->header);

                    rc = VERR_NOT_SUPPORTED;
                }
            }
            else
            {
                dprintf(("VBoxVideo::vboxVbvaEnable: rc = %Vrc, VMMDev rc = %Vrc!!!\n", rc, req->header.rc));

                if (VBOX_SUCCESS(rc))
                {
                    rc = req->header.rc;
                }
            }

            VbglGRFree (&req->header);
        }
        else
        {
            dprintf(("VBoxVideo::vboxVbvaEnable: VbglGRAlloc rc = %Vrc!!!\n", rc));
        }

        pDevExt->ulVbvaEnabled = ulEnabled;
    }

    return rc;
}
