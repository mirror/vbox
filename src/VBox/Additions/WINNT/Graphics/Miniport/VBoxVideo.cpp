/*
 * VirtualBox Video miniport driver for NT/2k/XP
 *
 * Based on DDK sample code.
 *
 * Copyright (C) 2006-2007 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "VBoxVideo.h"
#include "Helper.h"
#ifdef VBOX_WITH_WDDM
#include "wddm/VBoxVideoMisc.h"
#endif

#include <iprt/log.h>
#include <VBox/VMMDev.h>
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

VIDEO_ACCESS_RANGE  VGARanges[] = {
    { 0x000003B0, 0x00000000, 0x0000000C, 1, 1, 1, 0 }, /* 0x3B0-0x3BB */
    { 0x000003C0, 0x00000000, 0x00000020, 1, 1, 1, 0 }, /* 0x3C0-0x3DF */
    { 0x000A0000, 0x00000000, 0x00020000, 0, 0, 1, 0 }, /* 0xA0000-0xBFFFF */
};
/*
 * Globals for the last custom resolution set. This is important
 * for system startup so that we report the last currently set
 * custom resolution and Windows can use it again.
 */
#ifndef VBOX_WITH_MULTIMONITOR_FIX
uint32_t gCustomXRes = 0;
uint32_t gCustomYRes = 0;
uint32_t gCustomBPP  = 0;
#endif /* !VBOX_WITH_MULTIMONITOR_FIX */

int vboxVbvaEnable (PDEVICE_EXTENSION pDevExt, ULONG ulEnable, VBVAENABLERESULT *pVbvaResult);

static VP_STATUS VBoxVideoFindAdapter(
   IN PVOID HwDeviceExtension,
   IN PVOID HwContext,
   IN PWSTR ArgumentString,
   IN OUT PVIDEO_PORT_CONFIG_INFO ConfigInfo,
   OUT PUCHAR Again);

static BOOLEAN VBoxVideoInitialize(PVOID HwDeviceExtension);

static BOOLEAN VBoxVideoStartIO(
   PVOID HwDeviceExtension,
   PVIDEO_REQUEST_PACKET RequestPacket);

#ifdef VBOX_WITH_VIDEOHWACCEL
static BOOLEAN VBoxVideoInterrupt(PVOID  HwDeviceExtension);
#endif


static BOOLEAN VBoxVideoResetHW(
   PVOID HwDeviceExtension,
   ULONG Columns,
   ULONG Rows);

static VP_STATUS VBoxVideoGetPowerState(
   PVOID HwDeviceExtension,
   ULONG HwId,
   PVIDEO_POWER_MANAGEMENT VideoPowerControl);

static VP_STATUS VBoxVideoSetPowerState(
   PVOID HwDeviceExtension,
   ULONG HwId,
   PVIDEO_POWER_MANAGEMENT VideoPowerControl);

static VP_STATUS VBoxVideoGetChildDescriptor(
   PVOID HwDeviceExtension,
   PVIDEO_CHILD_ENUM_INFO ChildEnumInfo,
   PVIDEO_CHILD_TYPE VideoChildType,
   PUCHAR pChildDescriptor,
   PULONG pUId,
   PULONG pUnused);

/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/

#ifndef VBOX_WITH_WDDM
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
static VP_STATUS VBoxRegistryCallback(PVOID HwDeviceExtension, PVOID Context,
                                      PWSTR ValueName, PVOID ValueData,
                                      ULONG ValueLength)
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
#endif /* #ifndef VBOX_WITH_WDDM */

static VP_STATUS VBoxVideoCmnRegQueryDword(IN VBOXCMNREG Reg, PWSTR pName, uint32_t *pVal)
{
#ifndef VBOX_WITH_WDDM
    return VideoPortGetRegistryParameters(Reg, pName, FALSE, VBoxRegistryCallback, pVal);
#else
    if(!Reg)
        return ERROR_INVALID_PARAMETER;
    NTSTATUS Status = vboxWddmRegQueryValueDword(Reg, pName, (PDWORD)pVal);
    return Status == STATUS_SUCCESS ? NO_ERROR : ERROR_INVALID_PARAMETER;
#endif
}

static VP_STATUS VBoxVideoCmnRegSetDword(IN VBOXCMNREG Reg, PWSTR pName, uint32_t Val)
{
#ifndef VBOX_WITH_WDDM
    return VideoPortSetRegistryParameters(Reg, pName, &Val, sizeof(Val));
#else
    if(!Reg)
        return ERROR_INVALID_PARAMETER;
    NTSTATUS Status = vboxWddmRegSetValueDword(Reg, pName, Val);
    return Status == STATUS_SUCCESS ? NO_ERROR : ERROR_INVALID_PARAMETER;
#endif
}

static VP_STATUS VBoxVideoCmnRegInit(IN PDEVICE_EXTENSION pDeviceExtension, OUT VBOXCMNREG *pReg)
{
#ifndef VBOX_WITH_WDDM
    *pReg = pDeviceExtension->pPrimary;
    return NO_ERROR;
#else
    WCHAR Buf[512];
    ULONG cbBuf = sizeof(Buf);
    NTSTATUS Status = vboxWddmRegQueryDrvKeyName(pDeviceExtension, cbBuf, Buf, &cbBuf);
    Assert(Status == STATUS_SUCCESS);
    if (Status == STATUS_SUCCESS)
    {
        Status = vboxWddmRegOpenKey(pReg, Buf, GENERIC_READ | GENERIC_WRITE);
        Assert(Status == STATUS_SUCCESS);
        if(Status == STATUS_SUCCESS)
            return NO_ERROR;
    }

    /* fall-back to make the subsequent VBoxVideoCmnRegXxx calls treat the fail accordingly
     * basically needed to make as less modifications to the current XPDM code as possible */
    *pReg = NULL;

    return ERROR_INVALID_PARAMETER;
#endif
}

static VP_STATUS VBoxVideoCmnRegFini(IN VBOXCMNREG Reg)
{
#ifndef VBOX_WITH_WDDM
    return NO_ERROR;
#else
    if(!Reg)
        return ERROR_INVALID_PARAMETER;

    NTSTATUS Status = ZwClose(Reg);
    return Status == STATUS_SUCCESS ? NO_ERROR : ERROR_INVALID_PARAMETER;
#endif
}

void VBoxVideoCmnSignalEvent(PVBOXVIDEO_COMMON pCommon, uint64_t pvEvent)
{
    PDEVICE_EXTENSION PrimaryExtension = commonToPrimaryExt(pCommon);
#ifndef VBOX_WITH_WDDM
    PEVENT pEvent = (PEVENT)pvEvent;
    PrimaryExtension->u.primary.VideoPortProcs.pfnSetEvent(PrimaryExtension,
                                                           pEvent);
#else
    PKEVENT pEvent = (PKEVENT)pvEvent;
    KeSetEvent(pEvent, 0, FALSE);
#endif
}


#define MEM_TAG 'HVBV'

void *VBoxVideoCmnMemAllocDriver(PVBOXVIDEO_COMMON pCommon, size_t cb)
{
    ULONG Tag = MEM_TAG;
#ifndef VBOX_WITH_WDDM
    PDEVICE_EXTENSION PrimaryExtension = commonToPrimaryExt(pCommon);
    return PrimaryExtension->u.primary.VideoPortProcs.pfnAllocatePool(PrimaryExtension, (VBOXVP_POOL_TYPE)VpNonPagedPool, cb, Tag);
#else
    return ExAllocatePoolWithTag(NonPagedPool, cb, Tag);
#endif
}


void VBoxVideoCmnMemFreeDriver(PVBOXVIDEO_COMMON pCommon, void *pv)
{
#ifndef VBOX_WITH_WDDM
    PDEVICE_EXTENSION PrimaryExtension = commonToPrimaryExt(pCommon);
    PrimaryExtension->u.primary.VideoPortProcs.pfnFreePool(PrimaryExtension,
                                                           pv);
#else
    ExFreePool(pv);
#endif
}

static VOID VBoxVideoCmnMemZero(PVOID pvMem, ULONG cbMem)
{
#ifndef VBOX_WITH_WDDM
    VideoPortZeroMemory(pvMem, cbMem);
#else
    memset(pvMem, 0, cbMem);
#endif
}


#ifndef VBOX_WITH_WDDM
static void VBoxSetupVideoPortFunctions(PDEVICE_EXTENSION PrimaryExtension,
                                VBOXVIDEOPORTPROCS *pCallbacks,
                                PVIDEO_PORT_CONFIG_INFO pConfigInfo);

ULONG DriverEntry(IN PVOID Context1, IN PVOID Context2)
{
    VIDEO_HW_INITIALIZATION_DATA InitData;
    ULONG rc;

    dprintf(("VBoxVideo::DriverEntry. Built %s %s\n", __DATE__, __TIME__));

    VideoPortZeroMemory(&InitData, sizeof(VIDEO_HW_INITIALIZATION_DATA));
    InitData.HwInitDataSize = sizeof(VIDEO_HW_INITIALIZATION_DATA);
    InitData.HwFindAdapter = VBoxVideoFindAdapter;
    InitData.HwInitialize = VBoxVideoInitialize;
#ifdef VBOX_WITH_VIDEOHWACCEL
    InitData.HwInterrupt = VBoxVideoInterrupt;
#else
    InitData.HwInterrupt = NULL;
#endif
    InitData.HwStartIO = VBoxVideoStartIO;
    InitData.HwResetHw = VBoxVideoResetHW;
    InitData.HwDeviceExtensionSize = 0;
    // nowhere documented but without the following line, NT4 SP0 will choke
    InitData.AdapterInterfaceType = PCIBus;
    InitData.HwGetPowerState = VBoxVideoGetPowerState;
    InitData.HwSetPowerState = VBoxVideoSetPowerState;
    InitData.HwGetVideoChildDescriptor = VBoxVideoGetChildDescriptor;
    InitData.HwDeviceExtensionSize = sizeof(DEVICE_EXTENSION);
    // report legacy VGA resource ranges
    InitData.HwLegacyResourceList  = VGARanges;
    InitData.HwLegacyResourceCount = sizeof(VGARanges) / sizeof(VGARanges[0]);

    // our DDK is at the Win2k3 level so we have to take special measures
    // for backwards compatibility
    switch (vboxQueryWinVersion())
    {
        case WINNT4:
            dprintf(("VBoxVideo::DriverEntry: WINNT4\n"));
            InitData.HwInitDataSize = SIZE_OF_NT4_VIDEO_HW_INITIALIZATION_DATA;
            break;
        case WIN2K:
            dprintf(("VBoxVideo::DriverEntry: WIN2K\n"));
            InitData.HwInitDataSize = SIZE_OF_W2K_VIDEO_HW_INITIALIZATION_DATA;
            break;
    }
    rc = VideoPortInitialize(Context1, Context2, &InitData, NULL);

    dprintf(("VBoxVideo::DriverEntry: returning with rc = 0x%x\n", rc));
    return rc;
}
#endif

#ifdef VBOX_WITH_GENERIC_MULTIMONITOR

/* builds a g_VBoxWddmVideoResolutions given VideoModes info */
static int vboxVideoBuildResolutionTable(VIDEO_MODE_INFORMATION *VideoModes, uint32_t cNumVideoModes, SIZE *pResolutions, uint32_t * pcResolutions)
{
    uint32_t cResolutionsArray = *pcResolutions;
    uint32_t cResolutions = 0;
    int rc = VINF_SUCCESS;

    /* we don't care about the efficiency at this time */
    for (uint32_t i = 0; i < cNumVideoModes; ++i)
    {
        VIDEO_MODE_INFORMATION *pMode = &VideoModes[i];
        bool bFound = false;
        for (uint32_t j = 0; j < cResolutions; ++j)
        {
            if (pResolutions[j].cx == pMode->VisScreenWidth
                    && pResolutions[j].cy == pMode->VisScreenHeight)
            {
                bFound = true;
                break;
            }
        }

        if (!bFound)
        {
            if (cResolutions >= cResolutionsArray)
            {
                rc = VERR_BUFFER_OVERFLOW;
                break;
            }

            pResolutions[cResolutions].cx = pMode->VisScreenWidth;
            pResolutions[cResolutions].cy = pMode->VisScreenHeight;
            ++cResolutions;
        }
    }

    *pcResolutions = cResolutions;
    return rc;
}

/* On the driver startup this is initialized from registry (replaces gCustom*). */
static VIDEO_MODE_INFORMATION CustomVideoModes[64] = { 0 };

static bool vboxVideoIsVideoModeSupported(PDEVICE_EXTENSION DeviceExtension, int iDisplay, ULONG vramSize,
        uint32_t xres, uint32_t yres, uint32_t bpp)
{
    static uint32_t xresNoVRAM = 0;
    static uint32_t yresNoVRAM = 0;
    static uint32_t bppNoVRAM = 0;
    if (!((   xres
            && yres
            && (   (bpp == 16)
#ifdef VBOX_WITH_8BPP_MODES
                || (bpp == 8)
#endif
                || (bpp == 24)
                || (bpp == 32)))
        && (xres * yres * (bpp / 8) < vramSize)))
    {
        dprintf(("VBoxVideo: invalid parameters for special mode: (xres = %d, yres = %d, bpp = %d, vramSize = %d)\n",
                         xres, yres, bpp, vramSize));
        if (xres * yres * (bpp / 8) >= vramSize
                    && (xres != xresNoVRAM || yres != yresNoVRAM || bpp != bppNoVRAM))
        {
            LogRel(("VBoxVideo: not enough VRAM for video mode %dx%dx%dbpp. Available: %d bytes. Required: more than %d bytes.\n",
                            xres, yres, bpp, vramSize, xres * yres * (bpp / 8)));
            xresNoVRAM = xres;
            yresNoVRAM = yres;
            bppNoVRAM = bpp;
        }
        return false;
    }

    /* does the host like that mode? */
    if (!vboxLikesVideoMode(iDisplay, xres, yres, bpp))
    {
        dprintf(("VBoxVideo: host does not like special mode: (xres = %d, yres = %d, bpp = %d)\n",
                 xres, yres, bpp));
        return false;
    }

    return true;
}

/*
 * @return index for the changed mode, or -1 of none
 */
static int vboxVideoUpdateCustomVideoModes(PDEVICE_EXTENSION DeviceExtension, VBOXCMNREG Reg)
{
    uint32_t xres = 0, yres = 0, bpp = 0, display = 0;
    if (!vboxQueryDisplayRequest(&xres, &yres, &bpp, &display)
        && (xres || yres || bpp))
        return -1;

    if (display > RT_ELEMENTS(CustomVideoModes))
        return -1;

#ifndef VBOX_WITH_WDDM
    dprintf(("VBoxVideo: adding custom video mode as #%d, current mode: %d \n", gNumVideoModes + 1, DeviceExtension->CurrentMode));
    /* handle the startup case */
    if (DeviceExtension->CurrentMode == 0)
#else
    if (!commonFromDeviceExt(DeviceExtension)->cDisplays || !DeviceExtension->aSources[0].pPrimaryAllocation)
#endif
    {
        /* Use the stored custom resolution values only if nothing was read from host.
         * The custom mode might be not valid anymore and would block any hints from host.
         */
        if (!xres)
            xres = CustomVideoModes[display].VisScreenWidth;
        if (!yres)
            yres = CustomVideoModes[display].VisScreenHeight;
        if (!bpp)
            bpp  = CustomVideoModes[display].BitsPerPlane;
        dprintf(("VBoxVideo: using stored custom resolution %dx%dx%d for %d\n", xres, yres, bpp, display));
    }

    /* round down to multiple of 8 if necessary */
    if (!DeviceExtension->fAnyX) {
        if ((xres & 0xfff8) != xres)
            dprintf(("VBoxVideo: rounding down xres from %d to %d\n", xres, xres & 0xfff8));
        xres &= 0xfff8;
    }

        /* take the current values for the fields that are not set */
#ifndef VBOX_WITH_WDDM
    if (DeviceExtension->CurrentMode != 0)
    {
        if (!xres)
            xres = DeviceExtension->CurrentModeWidth;
        if (!yres)
            yres = DeviceExtension->CurrentModeHeight;
        if (!bpp)
            bpp  = DeviceExtension->CurrentModeBPP;
    }
#else
    if (commonFromDeviceExt(DeviceExtension)->cDisplays && DeviceExtension->aSources[0].pPrimaryAllocation)
    {
        if (!xres)
            xres = DeviceExtension->aSources[0].pPrimaryAllocation->SurfDesc.width;
        if (!yres)
            yres = DeviceExtension->aSources[0].pPrimaryAllocation->SurfDesc.height;
        if (!bpp)
            bpp  = DeviceExtension->aSources[0].pPrimaryAllocation->SurfDesc.bpp;
    }
#endif

    /* Use a default value. */
    if (!bpp)
        bpp = 32;

#ifndef VBOX_WITH_WDDM
    ULONG vramSize = DeviceExtension->pPrimary->u.primary.ulMaxFrameBufferSize;
#else
    ULONG vramSize = vboxWddmVramCpuVisibleSegmentSize(DeviceExtension);
    /* at least two surfaces will be needed: primary & shadow */
    vramSize /= 2 * DeviceExtension->u.primary.commonInfo.cDisplays;
#endif

    if (!vboxVideoIsVideoModeSupported(DeviceExtension, display, vramSize, xres, yres, bpp))
    {
        return -1;
    }

    dprintf(("VBoxVideo: setting special mode to xres = %d, yres = %d, bpp = %d, display = %d\n", xres, yres, bpp, display));
    /*
     * Build mode entry.
     * Note that we do not apply the y offset for the custom mode. It is
     * only used for the predefined modes that the user can configure in
     * the display properties dialog.
     */
    CustomVideoModes[display].Length                       = sizeof(VIDEO_MODE_INFORMATION);
    CustomVideoModes[display].ModeIndex                    = display + 1; /* ensure it is not zero, zero means the mode is loaded from registry and needs verification */
    CustomVideoModes[display].VisScreenWidth               = xres;
    CustomVideoModes[display].VisScreenHeight              = yres;
    CustomVideoModes[display].ScreenStride                 = xres * (bpp / 8);
    CustomVideoModes[display].NumberOfPlanes               = 1;
    CustomVideoModes[display].BitsPerPlane                 = bpp;
    CustomVideoModes[display].Frequency                    = 60;
    CustomVideoModes[display].XMillimeter                  = 320;
    CustomVideoModes[display].YMillimeter                  = 240;

    switch (bpp)
    {
#ifdef VBOX_WITH_8BPP_MODES
        case 8:
            CustomVideoModes[display].NumberRedBits        = 6;
            CustomVideoModes[display].NumberGreenBits      = 6;
            CustomVideoModes[display].NumberBlueBits       = 6;
            CustomVideoModes[display].RedMask              = 0;
            CustomVideoModes[display].GreenMask            = 0;
            CustomVideoModes[display].BlueMask             = 0;
            break;
#endif
        case 16:
            CustomVideoModes[display].NumberRedBits        = 5;
            CustomVideoModes[display].NumberGreenBits      = 6;
            CustomVideoModes[display].NumberBlueBits       = 5;
            CustomVideoModes[display].RedMask              = 0xF800;
            CustomVideoModes[display].GreenMask            = 0x7E0;
            CustomVideoModes[display].BlueMask             = 0x1F;
            break;
        case 24:
            CustomVideoModes[display].NumberRedBits        = 8;
            CustomVideoModes[display].NumberGreenBits      = 8;
            CustomVideoModes[display].NumberBlueBits       = 8;
            CustomVideoModes[display].RedMask              = 0xFF0000;
            CustomVideoModes[display].GreenMask            = 0xFF00;
            CustomVideoModes[display].BlueMask             = 0xFF;
            break;
        case 32:
            CustomVideoModes[display].NumberRedBits        = 8;
            CustomVideoModes[display].NumberGreenBits      = 8;
            CustomVideoModes[display].NumberBlueBits       = 8;
            CustomVideoModes[display].RedMask              = 0xFF0000;
            CustomVideoModes[display].GreenMask            = 0xFF00;
            CustomVideoModes[display].BlueMask             = 0xFF;
            break;
    }
    CustomVideoModes[display].AttributeFlags               = VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR | VIDEO_MODE_NO_OFF_SCREEN;
#ifdef VBOX_WITH_8BPP_MODES
    if (bpp == 8)
        CustomVideoModes[display].AttributeFlags          |= VIDEO_MODE_PALETTE_DRIVEN | VIDEO_MODE_MANAGED_PALETTE;
#endif
    CustomVideoModes[display].VideoMemoryBitmapWidth       = xres;
    CustomVideoModes[display].VideoMemoryBitmapHeight      = yres;
    CustomVideoModes[display].DriverSpecificAttributeFlags = 0;

    VP_STATUS status = 0;

    /* Save the custom mode for this display. */
    if (display)
    {
        /* Name without a suffix */
        status = VBoxVideoCmnRegSetDword(Reg, L"CustomXRes", xres);
        if (status != NO_ERROR)
            dprintf(("VBoxVideo: error %d writing CustomXRes\n", status));
        status = VBoxVideoCmnRegSetDword(Reg, L"CustomYRes", yres);
        if (status != NO_ERROR)
            dprintf(("VBoxVideo: error %d writing CustomYRes\n", status));
        status = VBoxVideoCmnRegSetDword(Reg, L"CustomBPP", bpp);
        if (status != NO_ERROR)
            dprintf(("VBoxVideo: error %d writing CustomBPP\n", status));
    }
    else
    {
        wchar_t keyname[32];
        swprintf(keyname, L"CustomXRes%d", display);
        status = VBoxVideoCmnRegSetDword(Reg, keyname, xres);
        if (status != NO_ERROR)
            dprintf(("VBoxVideo: error %d writing CustomXRes%d\n", status, display));
        swprintf(keyname, L"CustomYRes%d", display);
        status = VBoxVideoCmnRegSetDword(Reg, keyname, yres);
        if (status != NO_ERROR)
            dprintf(("VBoxVideo: error %d writing CustomYRes%d\n", status, display));
        swprintf(keyname, L"CustomBPP%d", display);
        status = VBoxVideoCmnRegSetDword(Reg, keyname, bpp);
        if (status != NO_ERROR)
            dprintf(("VBoxVideo: error %d writing CustomBPP%d\n", status, display));
    }

    return display;
}

static int vboxVideoBuildModesTable(PDEVICE_EXTENSION DeviceExtension, int iDisplay,
        VIDEO_MODE_INFORMATION * VideoModes, uint32_t * pcVideoModes, int32_t * pPreferrableMode)
{
    int iPreferredVideoMode = 0;
    int cNumVideoModes = 0;
    int cModesTable = *pcVideoModes;
    int rc = VINF_SUCCESS;

    VBOXCMNREG Reg;
    VBoxVideoCmnRegInit(DeviceExtension, &Reg);

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
#ifndef VBOX_WITH_WDDM
        /* multi screen modes with 1280x1024 */
        { 2560, 1024 },
        { 3840, 1024 },
        { 5120, 1024 },
        /* multi screen modes with 1600x1200 */
        { 3200, 1200 },
        { 4800, 1200 },
        { 6400, 1200 },
#endif
    };
    size_t matrixSize = sizeof(resolutionMatrix) / sizeof(resolutionMatrix[0]);

    /* there are 4 color depths: 8, 16, 24 and 32bpp and we reserve 50% of the modes for other sources */
    size_t maxModesPerColorDepth = cModesTable / 2 / 4;

    /* size of the VRAM in bytes */

#ifndef VBOX_WITH_WDDM
    ULONG vramSize = DeviceExtension->pPrimary->u.primary.ulMaxFrameBufferSize;
#else
    ULONG vramSize = vboxWddmVramCpuVisibleSegmentSize(DeviceExtension);
    /* at least two surfaces will be needed: primary & shadow */
    vramSize /= 2 * DeviceExtension->u.primary.commonInfo.cDisplays;
#endif

    size_t numModesCurrentColorDepth;
    size_t matrixIndex;
    VP_STATUS status = 0;

    do
    {
        /* Always add 800x600 video modes. Windows XP+ needs at least 800x600 resolution
         * and fallbacks to 800x600x4bpp VGA mode if the driver did not report suitable modes.
         * This resolution could be rejected by a low resolution host (netbooks, etc).
         */
        int cBytesPerPixel;
        for (cBytesPerPixel = 1; cBytesPerPixel <= 4; cBytesPerPixel++)
        {
            int cBitsPerPixel = cBytesPerPixel * 8; /* 8, 16, 24, 32 */

    #ifndef VBOX_WITH_8BPP_MODES
            if (cBitsPerPixel == 8)
            {
                 continue;
            }
    #endif /* !VBOX_WITH_8BPP_MODES */

            /* does the mode fit into the VRAM? */
            if (800 * 600 * cBytesPerPixel > (LONG)vramSize)
            {
                continue;
            }

            if (cModesTable <= cNumVideoModes)
            {
                rc = VERR_BUFFER_OVERFLOW;
                break;
            }

            VideoModes[cNumVideoModes].Length                       = sizeof(VIDEO_MODE_INFORMATION);
            VideoModes[cNumVideoModes].ModeIndex                    = cNumVideoModes + 1;
            VideoModes[cNumVideoModes].VisScreenWidth               = 800;
            VideoModes[cNumVideoModes].VisScreenHeight              = 600;
            VideoModes[cNumVideoModes].ScreenStride                 = 800 * cBytesPerPixel;
            VideoModes[cNumVideoModes].NumberOfPlanes               = 1;
            VideoModes[cNumVideoModes].BitsPerPlane                 = cBitsPerPixel;
            VideoModes[cNumVideoModes].Frequency                    = 60;
            VideoModes[cNumVideoModes].XMillimeter                  = 320;
            VideoModes[cNumVideoModes].YMillimeter                  = 240;
            switch (cBytesPerPixel)
            {
                case 1:
                {
                    VideoModes[cNumVideoModes].NumberRedBits                = 6;
                    VideoModes[cNumVideoModes].NumberGreenBits              = 6;
                    VideoModes[cNumVideoModes].NumberBlueBits               = 6;
                    VideoModes[cNumVideoModes].RedMask                      = 0;
                    VideoModes[cNumVideoModes].GreenMask                    = 0;
                    VideoModes[cNumVideoModes].BlueMask                     = 0;
                    VideoModes[cNumVideoModes].AttributeFlags               = VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR | VIDEO_MODE_NO_OFF_SCREEN |
                                                                              VIDEO_MODE_PALETTE_DRIVEN | VIDEO_MODE_MANAGED_PALETTE;
                } break;
                case 2:
                {
                    VideoModes[cNumVideoModes].NumberRedBits                = 5;
                    VideoModes[cNumVideoModes].NumberGreenBits              = 6;
                    VideoModes[cNumVideoModes].NumberBlueBits               = 5;
                    VideoModes[cNumVideoModes].RedMask                      = 0xF800;
                    VideoModes[cNumVideoModes].GreenMask                    = 0x7E0;
                    VideoModes[cNumVideoModes].BlueMask                     = 0x1F;
                    VideoModes[cNumVideoModes].AttributeFlags               = VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR | VIDEO_MODE_NO_OFF_SCREEN;
                } break;
                case 3:
                {
                    VideoModes[cNumVideoModes].NumberRedBits                = 8;
                    VideoModes[cNumVideoModes].NumberGreenBits              = 8;
                    VideoModes[cNumVideoModes].NumberBlueBits               = 8;
                    VideoModes[cNumVideoModes].RedMask                      = 0xFF0000;
                    VideoModes[cNumVideoModes].GreenMask                    = 0xFF00;
                    VideoModes[cNumVideoModes].BlueMask                     = 0xFF;
                    VideoModes[cNumVideoModes].AttributeFlags               = VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR | VIDEO_MODE_NO_OFF_SCREEN;
                } break;
                default:
                case 4:
                {
                    VideoModes[cNumVideoModes].NumberRedBits                = 8;
                    VideoModes[cNumVideoModes].NumberGreenBits              = 8;
                    VideoModes[cNumVideoModes].NumberBlueBits               = 8;
                    VideoModes[cNumVideoModes].RedMask                      = 0xFF0000;
                    VideoModes[cNumVideoModes].GreenMask                    = 0xFF00;
                    VideoModes[cNumVideoModes].BlueMask                     = 0xFF;
                    VideoModes[cNumVideoModes].AttributeFlags               = VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR | VIDEO_MODE_NO_OFF_SCREEN;
                    iPreferredVideoMode = cNumVideoModes;
                } break;
            }
            VideoModes[cNumVideoModes].VideoMemoryBitmapWidth       = 800;
            VideoModes[cNumVideoModes].VideoMemoryBitmapHeight      = 600;
            VideoModes[cNumVideoModes].DriverSpecificAttributeFlags = 0;

            /* a new mode has been filled in */
            ++cNumVideoModes;
        }

        if (RT_FAILURE(rc))
            break;

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

            if (yOffset == 0 && resolutionMatrix[matrixIndex].xRes == 800 && resolutionMatrix[matrixIndex].yRes == 600)
            {
                /* This mode was already added. */
                ++matrixIndex;
                continue;
            }

            /* does the host like that mode? */
            if (!vboxLikesVideoMode(iDisplay, resolutionMatrix[matrixIndex].xRes, resolutionMatrix[matrixIndex].yRes - yOffset, 8))
            {
                ++matrixIndex;
                continue;
            }

            if (cModesTable <= cNumVideoModes)
            {
                rc = VERR_BUFFER_OVERFLOW;
                break;
            }

            VideoModes[cNumVideoModes].Length                       = sizeof(VIDEO_MODE_INFORMATION);
            VideoModes[cNumVideoModes].ModeIndex                    = cNumVideoModes + 1;
            VideoModes[cNumVideoModes].VisScreenWidth               = resolutionMatrix[matrixIndex].xRes;
            VideoModes[cNumVideoModes].VisScreenHeight              = resolutionMatrix[matrixIndex].yRes - yOffset;
            VideoModes[cNumVideoModes].ScreenStride                 = resolutionMatrix[matrixIndex].xRes * 1;
            VideoModes[cNumVideoModes].NumberOfPlanes               = 1;
            VideoModes[cNumVideoModes].BitsPerPlane                 = 8;
            VideoModes[cNumVideoModes].Frequency                    = 60;
            VideoModes[cNumVideoModes].XMillimeter                  = 320;
            VideoModes[cNumVideoModes].YMillimeter                  = 240;
            VideoModes[cNumVideoModes].NumberRedBits                = 6;
            VideoModes[cNumVideoModes].NumberGreenBits              = 6;
            VideoModes[cNumVideoModes].NumberBlueBits               = 6;
            VideoModes[cNumVideoModes].RedMask                      = 0;
            VideoModes[cNumVideoModes].GreenMask                    = 0;
            VideoModes[cNumVideoModes].BlueMask                     = 0;
            VideoModes[cNumVideoModes].AttributeFlags               = VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR | VIDEO_MODE_NO_OFF_SCREEN |
                                                                      VIDEO_MODE_PALETTE_DRIVEN | VIDEO_MODE_MANAGED_PALETTE;
            VideoModes[cNumVideoModes].VideoMemoryBitmapWidth       = resolutionMatrix[matrixIndex].xRes;
            VideoModes[cNumVideoModes].VideoMemoryBitmapHeight      = resolutionMatrix[matrixIndex].yRes - yOffset;
            VideoModes[cNumVideoModes].DriverSpecificAttributeFlags = 0;

            /* a new mode has been filled in */
            ++cNumVideoModes;
            ++numModesCurrentColorDepth;
            /* advance to the next mode matrix entry */
            ++matrixIndex;
        }

        if (RT_FAILURE(rc))
            break;

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

            if (yOffset == 0 && resolutionMatrix[matrixIndex].xRes == 800 && resolutionMatrix[matrixIndex].yRes == 600)
            {
                /* This mode was already added. */
                ++matrixIndex;
                continue;
            }

            /* does the host like that mode? */
            if (!vboxLikesVideoMode(iDisplay, resolutionMatrix[matrixIndex].xRes, resolutionMatrix[matrixIndex].yRes - yOffset, 16))
            {
                ++matrixIndex;
                continue;
            }

            if (cModesTable <= cNumVideoModes)
            {
                rc = VERR_BUFFER_OVERFLOW;
                break;
            }

            VideoModes[cNumVideoModes].Length                       = sizeof(VIDEO_MODE_INFORMATION);
            VideoModes[cNumVideoModes].ModeIndex                    = cNumVideoModes + 1;
            VideoModes[cNumVideoModes].VisScreenWidth               = resolutionMatrix[matrixIndex].xRes;
            VideoModes[cNumVideoModes].VisScreenHeight              = resolutionMatrix[matrixIndex].yRes - yOffset;
            VideoModes[cNumVideoModes].ScreenStride                 = resolutionMatrix[matrixIndex].xRes * 2;
            VideoModes[cNumVideoModes].NumberOfPlanes               = 1;
            VideoModes[cNumVideoModes].BitsPerPlane                 = 16;
            VideoModes[cNumVideoModes].Frequency                    = 60;
            VideoModes[cNumVideoModes].XMillimeter                  = 320;
            VideoModes[cNumVideoModes].YMillimeter                  = 240;
            VideoModes[cNumVideoModes].NumberRedBits                = 5;
            VideoModes[cNumVideoModes].NumberGreenBits              = 6;
            VideoModes[cNumVideoModes].NumberBlueBits               = 5;
            VideoModes[cNumVideoModes].RedMask                      = 0xF800;
            VideoModes[cNumVideoModes].GreenMask                    = 0x7E0;
            VideoModes[cNumVideoModes].BlueMask                     = 0x1F;
            VideoModes[cNumVideoModes].AttributeFlags               = VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR | VIDEO_MODE_NO_OFF_SCREEN;
            VideoModes[cNumVideoModes].VideoMemoryBitmapWidth       = resolutionMatrix[matrixIndex].xRes;
            VideoModes[cNumVideoModes].VideoMemoryBitmapHeight      = resolutionMatrix[matrixIndex].yRes - yOffset;
            VideoModes[cNumVideoModes].DriverSpecificAttributeFlags = 0;

            /* a new mode has been filled in */
            ++cNumVideoModes;
            ++numModesCurrentColorDepth;
            /* advance to the next mode matrix entry */
            ++matrixIndex;
        }

        if (RT_FAILURE(rc))
            break;

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

            if (yOffset == 0 && resolutionMatrix[matrixIndex].xRes == 800 && resolutionMatrix[matrixIndex].yRes == 600)
            {
                /* This mode was already added. */
                ++matrixIndex;
                continue;
            }

            /* does the host like that mode? */
            if (!vboxLikesVideoMode(iDisplay, resolutionMatrix[matrixIndex].xRes, resolutionMatrix[matrixIndex].yRes - yOffset, 24))
            {
                ++matrixIndex;
                continue;
            }

            if (cModesTable <= cNumVideoModes)
            {
                rc = VERR_BUFFER_OVERFLOW;
                break;
            }

            VideoModes[cNumVideoModes].Length                       = sizeof(VIDEO_MODE_INFORMATION);
            VideoModes[cNumVideoModes].ModeIndex                    = cNumVideoModes + 1;
            VideoModes[cNumVideoModes].VisScreenWidth               = resolutionMatrix[matrixIndex].xRes;
            VideoModes[cNumVideoModes].VisScreenHeight              = resolutionMatrix[matrixIndex].yRes - yOffset;
            VideoModes[cNumVideoModes].ScreenStride                 = resolutionMatrix[matrixIndex].xRes * 3;
            VideoModes[cNumVideoModes].NumberOfPlanes               = 1;
            VideoModes[cNumVideoModes].BitsPerPlane                 = 24;
            VideoModes[cNumVideoModes].Frequency                    = 60;
            VideoModes[cNumVideoModes].XMillimeter                  = 320;
            VideoModes[cNumVideoModes].YMillimeter                  = 240;
            VideoModes[cNumVideoModes].NumberRedBits                = 8;
            VideoModes[cNumVideoModes].NumberGreenBits              = 8;
            VideoModes[cNumVideoModes].NumberBlueBits               = 8;
            VideoModes[cNumVideoModes].RedMask                      = 0xFF0000;
            VideoModes[cNumVideoModes].GreenMask                    = 0xFF00;
            VideoModes[cNumVideoModes].BlueMask                     = 0xFF;
            VideoModes[cNumVideoModes].AttributeFlags               = VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR | VIDEO_MODE_NO_OFF_SCREEN;
            VideoModes[cNumVideoModes].VideoMemoryBitmapWidth       = resolutionMatrix[matrixIndex].xRes;
            VideoModes[cNumVideoModes].VideoMemoryBitmapHeight      = resolutionMatrix[matrixIndex].yRes - yOffset;
            VideoModes[cNumVideoModes].DriverSpecificAttributeFlags = 0;

            /* a new mode has been filled in */
            ++cNumVideoModes;
            ++numModesCurrentColorDepth;
            /* advance to the next mode matrix entry */
            ++matrixIndex;
        }

        if (RT_FAILURE(rc))
            break;

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

            if (yOffset == 0 && resolutionMatrix[matrixIndex].xRes == 800 && resolutionMatrix[matrixIndex].yRes == 600)
            {
                /* This mode was already added. */
                ++matrixIndex;
                continue;
            }

            /* does the host like that mode? */
            if (!vboxLikesVideoMode(iDisplay, resolutionMatrix[matrixIndex].xRes, resolutionMatrix[matrixIndex].yRes - yOffset, 32))
            {
                ++matrixIndex;
                continue;
            }

            if (cModesTable <= cNumVideoModes)
            {
                rc = VERR_BUFFER_OVERFLOW;
                break;
            }

            VideoModes[cNumVideoModes].Length                       = sizeof(VIDEO_MODE_INFORMATION);
            VideoModes[cNumVideoModes].ModeIndex                    = cNumVideoModes + 1;
            VideoModes[cNumVideoModes].VisScreenWidth               = resolutionMatrix[matrixIndex].xRes;
            VideoModes[cNumVideoModes].VisScreenHeight              = resolutionMatrix[matrixIndex].yRes - yOffset;
            VideoModes[cNumVideoModes].ScreenStride                 = resolutionMatrix[matrixIndex].xRes * 4;
            VideoModes[cNumVideoModes].NumberOfPlanes               = 1;
            VideoModes[cNumVideoModes].BitsPerPlane                 = 32;
            VideoModes[cNumVideoModes].Frequency                    = 60;
            VideoModes[cNumVideoModes].XMillimeter                  = 320;
            VideoModes[cNumVideoModes].YMillimeter                  = 240;
            VideoModes[cNumVideoModes].NumberRedBits                = 8;
            VideoModes[cNumVideoModes].NumberGreenBits              = 8;
            VideoModes[cNumVideoModes].NumberBlueBits               = 8;
            VideoModes[cNumVideoModes].RedMask                      = 0xFF0000;
            VideoModes[cNumVideoModes].GreenMask                    = 0xFF00;
            VideoModes[cNumVideoModes].BlueMask                     = 0xFF;
            VideoModes[cNumVideoModes].AttributeFlags               = VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR | VIDEO_MODE_NO_OFF_SCREEN;
            VideoModes[cNumVideoModes].VideoMemoryBitmapWidth       = resolutionMatrix[matrixIndex].xRes;
            VideoModes[cNumVideoModes].VideoMemoryBitmapHeight      = resolutionMatrix[matrixIndex].yRes - yOffset;
            VideoModes[cNumVideoModes].DriverSpecificAttributeFlags = 0;

            /* a new mode has been filled in */
            ++cNumVideoModes;
            ++numModesCurrentColorDepth;
            /* advance to the next mode matrix entry */
            ++matrixIndex;
        }

        if (RT_FAILURE(rc))
            break;

        /*
         * Next, check the registry for additional modes
         */
        int curKeyNo = 0;
        int fPreferredSet = 0;
        do
        {
            wchar_t keyname[24];
            uint32_t xres, yres, bpp = 0;
            swprintf(keyname, L"CustomMode%dWidth", curKeyNo);
            status = VBoxVideoCmnRegQueryDword(Reg, keyname, &xres);
            /* upon the first error, we give up */
            if (status != NO_ERROR)
                break;
            swprintf(keyname, L"CustomMode%dHeight", curKeyNo);
            status = VBoxVideoCmnRegQueryDword(Reg, keyname, &yres);
            /* upon the first error, we give up */
            if (status != NO_ERROR)
                break;
            swprintf(keyname, L"CustomMode%dBPP", curKeyNo);
            status = VBoxVideoCmnRegQueryDword(Reg, keyname, &bpp);
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

            /* round down width to be a multiple of 8 if necessary */
            if (!DeviceExtension->fAnyX)
                xres &= 0xFFF8;

            /* second test: does it fit within our VRAM? */
            if (xres * yres * (bpp / 8) > vramSize)
                break;

            /* third test: does the host like the video mode? */
            if (!vboxLikesVideoMode(iDisplay, xres, yres, bpp))
                break;

            if (cModesTable <= cNumVideoModes)
            {
                rc = VERR_BUFFER_OVERFLOW;
                break;
            }

            dprintf(("VBoxVideo: adding mode from registry: xres = %d, yres = %d, bpp = %d\n", xres, yres, bpp));

            if (!fPreferredSet)
            {
                iPreferredVideoMode = cNumVideoModes;
                fPreferredSet = 1;
            }

            /*
             * Build mode entry.
             * Note that we have to apply the y offset for the custom mode.
             */
            VideoModes[cNumVideoModes].Length                       = sizeof(VIDEO_MODE_INFORMATION);
            VideoModes[cNumVideoModes].ModeIndex                    = cNumVideoModes + 1;
            VideoModes[cNumVideoModes].VisScreenWidth               = xres;
            VideoModes[cNumVideoModes].VisScreenHeight              = yres - yOffset;
            VideoModes[cNumVideoModes].ScreenStride                 = xres * (bpp / 8);
            VideoModes[cNumVideoModes].NumberOfPlanes               = 1;
            VideoModes[cNumVideoModes].BitsPerPlane                 = bpp;
            VideoModes[cNumVideoModes].Frequency                    = 60;
            VideoModes[cNumVideoModes].XMillimeter                  = 320;
            VideoModes[cNumVideoModes].YMillimeter                  = 240;
            switch (bpp)
            {
                case 16:
                    VideoModes[cNumVideoModes].NumberRedBits        = 5;
                    VideoModes[cNumVideoModes].NumberGreenBits      = 6;
                    VideoModes[cNumVideoModes].NumberBlueBits       = 5;
                    VideoModes[cNumVideoModes].RedMask              = 0xF800;
                    VideoModes[cNumVideoModes].GreenMask            = 0x7E0;
                    VideoModes[cNumVideoModes].BlueMask             = 0x1F;
                    break;
                case 24:
                    VideoModes[cNumVideoModes].NumberRedBits        = 8;
                    VideoModes[cNumVideoModes].NumberGreenBits      = 8;
                    VideoModes[cNumVideoModes].NumberBlueBits       = 8;
                    VideoModes[cNumVideoModes].RedMask              = 0xFF0000;
                    VideoModes[cNumVideoModes].GreenMask            = 0xFF00;
                    VideoModes[cNumVideoModes].BlueMask             = 0xFF;
                    break;
                case 32:
                    VideoModes[cNumVideoModes].NumberRedBits        = 8;
                    VideoModes[cNumVideoModes].NumberGreenBits      = 8;
                    VideoModes[cNumVideoModes].NumberBlueBits       = 8;
                    VideoModes[cNumVideoModes].RedMask              = 0xFF0000;
                    VideoModes[cNumVideoModes].GreenMask            = 0xFF00;
                    VideoModes[cNumVideoModes].BlueMask             = 0xFF;
                    /* 32-bit mode is more preferable, select it if not yet */
                    if (fPreferredSet < 2)
                    {
                        iPreferredVideoMode = cNumVideoModes;
                        fPreferredSet = 2;
                    }
                    break;
            }
            VideoModes[cNumVideoModes].AttributeFlags               = VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR | VIDEO_MODE_NO_OFF_SCREEN;
            VideoModes[cNumVideoModes].VideoMemoryBitmapWidth       = xres;
            VideoModes[cNumVideoModes].VideoMemoryBitmapHeight      = yres - yOffset;
            VideoModes[cNumVideoModes].DriverSpecificAttributeFlags = 0;
            ++cNumVideoModes;

            /* next run */
            curKeyNo++;
            /* only support 128 modes for now */
            if (curKeyNo >= 128)
                break;

        } while(1);

        if (RT_FAILURE(rc))
            break;

        /*
         * Now we ask the host for a display change request. If there's one,
         * this will be appended as a special mode so that it can be used by
         * the Additions service process. The mode table is guaranteed to have
         * two spare entries for this mode (alternating index thus 2).
         *
         * ... or ...
         *
         * Also we check if we got an user-stored custom resolution in the adapter
         * registry key add it to the modes table.
         */

        /* Add custom resolutions for each display and then for the display change request, if exists.
         */
        vboxVideoUpdateCustomVideoModes(DeviceExtension, Reg);
        /* check if the mode is verified */
        if (!CustomVideoModes[iDisplay].ModeIndex)
        {
            /* the mode is loaded from registry and not verified yet */
            if (vboxVideoIsVideoModeSupported(DeviceExtension, iDisplay, vramSize,
                    CustomVideoModes[iDisplay].VideoMemoryBitmapWidth,
                    CustomVideoModes[iDisplay].VideoMemoryBitmapHeight,
                    CustomVideoModes[iDisplay].BitsPerPlane))
            {
                CustomVideoModes[iDisplay].ModeIndex = iDisplay;
            }
        }


        if (CustomVideoModes[iDisplay].ModeIndex)
        {
            if (cModesTable <= cNumVideoModes)
            {
                rc = VERR_BUFFER_OVERFLOW;
                break;
            }
            CustomVideoModes[iDisplay].ModeIndex = cNumVideoModes;
            VideoModes[cNumVideoModes] = CustomVideoModes[iDisplay];
            iPreferredVideoMode = cNumVideoModes;
            ++cNumVideoModes;
        }

    } while(0);

    *pcVideoModes = cNumVideoModes;
    *pPreferrableMode = iPreferredVideoMode;

    VBoxVideoCmnRegFini(Reg);

    return rc;
}
#else /* if !(defined VBOX_WITH_GENERIC_MULTIMONITOR) */
/*
 * Global list of supported standard video modes. It will be
 * filled dynamically.
 */
#define MAX_VIDEO_MODES 128
#ifndef VBOX_WITH_MULTIMONITOR_FIX
static VIDEO_MODE_INFORMATION VideoModes[MAX_VIDEO_MODES + 2] = { 0 };
#else
/*
 * Additional space is reserved for custom video modes for 64 guest monitors.
 * The custom video mode index is alternating and 2 indexes are reserved for the last custom mode.
 */
static VIDEO_MODE_INFORMATION VideoModes[MAX_VIDEO_MODES + 64 + 2] = { 0 };
/* On the driver startup this is initialized from registry (replaces gCustom*). */
static VIDEO_MODE_INFORMATION CustomVideoModes[64] = { 0 };
#endif /* VBOX_WITH_MULTIMONITOR_FIX */
/* number of available video modes, set by VBoxBuildModesTable  */
static uint32_t gNumVideoModes = 0;

#ifdef VBOX_WITH_MULTIMONITOR_FIX
static void initVideoModeInformation(VIDEO_MODE_INFORMATION *pVideoMode, ULONG xres, ULONG yres, ULONG bpp, ULONG index, ULONG yoffset)
{
    /*
     * Build mode entry.
     */
    memset(pVideoMode, 0, sizeof(VIDEO_MODE_INFORMATION));

    pVideoMode->Length                       = sizeof(VIDEO_MODE_INFORMATION);
    pVideoMode->ModeIndex                    = index;
    pVideoMode->VisScreenWidth               = xres;
    pVideoMode->VisScreenHeight              = yres - yoffset;
    pVideoMode->ScreenStride                 = xres * ((bpp + 7) / 8);
    pVideoMode->NumberOfPlanes               = 1;
    pVideoMode->BitsPerPlane                 = bpp;
    pVideoMode->Frequency                    = 60;
    pVideoMode->XMillimeter                  = 320;
    pVideoMode->YMillimeter                  = 240;
    switch (bpp)
    {
#ifdef VBOX_WITH_8BPP_MODES
        case 8:
            pVideoMode->NumberRedBits        = 6;
            pVideoMode->NumberGreenBits      = 6;
            pVideoMode->NumberBlueBits       = 6;
            pVideoMode->RedMask              = 0;
            pVideoMode->GreenMask            = 0;
            pVideoMode->BlueMask             = 0;
            break;
#endif
        case 16:
            pVideoMode->NumberRedBits        = 5;
            pVideoMode->NumberGreenBits      = 6;
            pVideoMode->NumberBlueBits       = 5;
            pVideoMode->RedMask              = 0xF800;
            pVideoMode->GreenMask            = 0x7E0;
            pVideoMode->BlueMask             = 0x1F;
            break;
        case 24:
            pVideoMode->NumberRedBits        = 8;
            pVideoMode->NumberGreenBits      = 8;
            pVideoMode->NumberBlueBits       = 8;
            pVideoMode->RedMask              = 0xFF0000;
            pVideoMode->GreenMask            = 0xFF00;
            pVideoMode->BlueMask             = 0xFF;
            break;
        case 32:
            pVideoMode->NumberRedBits        = 8;
            pVideoMode->NumberGreenBits      = 8;
            pVideoMode->NumberBlueBits       = 8;
            pVideoMode->RedMask              = 0xFF0000;
            pVideoMode->GreenMask            = 0xFF00;
            pVideoMode->BlueMask             = 0xFF;
            break;
    }
    pVideoMode->AttributeFlags               = VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR | VIDEO_MODE_NO_OFF_SCREEN;
#ifdef VBOX_WITH_8BPP_MODES
    if (bpp == 8)
        pVideoMode->AttributeFlags          |= VIDEO_MODE_PALETTE_DRIVEN | VIDEO_MODE_MANAGED_PALETTE;
#endif
    pVideoMode->VideoMemoryBitmapWidth       = xres;
    pVideoMode->VideoMemoryBitmapHeight      = yres - yoffset;
    pVideoMode->DriverSpecificAttributeFlags = 0;
}
#endif /* VBOX_WITH_MULTIMONITOR_FIX */


static uint32_t g_xresNoVRAM = 0, g_yresNoVRAM = 0, g_bppNoVRAM = 0;

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

    VBOXCMNREG Reg;
    VBoxVideoCmnRegInit(DeviceExtension, &Reg);

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
#ifndef VBOX_WITH_WDDM
        /* multi screen modes with 1280x1024 */
        { 2560, 1024 },
        { 3840, 1024 },
        { 5120, 1024 },
        /* multi screen modes with 1600x1200 */
        { 3200, 1200 },
        { 4800, 1200 },
        { 6400, 1200 },
#endif
    };
    size_t matrixSize = sizeof(resolutionMatrix) / sizeof(resolutionMatrix[0]);

    /* there are 4 color depths: 8, 16, 24 and 32bpp and we reserve 50% of the modes for other sources */
    size_t maxModesPerColorDepth = MAX_VIDEO_MODES / 2 / 4;

    /* size of the VRAM in bytes */

#ifndef VBOX_WITH_WDDM
    ULONG vramSize = DeviceExtension->pPrimary->u.primary.ulMaxFrameBufferSize;
#else
    ULONG vramSize = vboxWddmVramCpuVisibleSegmentSize(DeviceExtension);
    /* at least two surfaces will be needed: primary & shadow */
    vramSize /= 2 * DeviceExtension->u.primary.commonInfo.cDisplays;

    gPreferredVideoMode = 0;
#endif

    gNumVideoModes = 0;

    size_t numModesCurrentColorDepth;
    size_t matrixIndex;
    VP_STATUS status = 0;

    /* Always add 800x600 video modes. Windows XP+ needs at least 800x600 resolution
     * and fallbacks to 800x600x4bpp VGA mode if the driver did not report suitable modes.
     * This resolution could be rejected by a low resolution host (netbooks, etc).
     */
    int cBytesPerPixel;
    for (cBytesPerPixel = 1; cBytesPerPixel <= 4; cBytesPerPixel++)
    {
        int cBitsPerPixel = cBytesPerPixel * 8; /* 8, 16, 24, 32 */

#ifndef VBOX_WITH_8BPP_MODES
        if (cBitsPerPixel == 8)
        {
             continue;
        }
#endif /* !VBOX_WITH_8BPP_MODES */

        /* does the mode fit into the VRAM? */
        if (800 * 600 * cBytesPerPixel > (LONG)vramSize)
        {
            continue;
        }

        VideoModes[gNumVideoModes].Length                       = sizeof(VIDEO_MODE_INFORMATION);
        VideoModes[gNumVideoModes].ModeIndex                    = gNumVideoModes + 1;
        VideoModes[gNumVideoModes].VisScreenWidth               = 800;
        VideoModes[gNumVideoModes].VisScreenHeight              = 600;
        VideoModes[gNumVideoModes].ScreenStride                 = 800 * cBytesPerPixel;
        VideoModes[gNumVideoModes].NumberOfPlanes               = 1;
        VideoModes[gNumVideoModes].BitsPerPlane                 = cBitsPerPixel;
        VideoModes[gNumVideoModes].Frequency                    = 60;
        VideoModes[gNumVideoModes].XMillimeter                  = 320;
        VideoModes[gNumVideoModes].YMillimeter                  = 240;
        switch (cBytesPerPixel)
        {
            case 1:
            {
                VideoModes[gNumVideoModes].NumberRedBits                = 6;
                VideoModes[gNumVideoModes].NumberGreenBits              = 6;
                VideoModes[gNumVideoModes].NumberBlueBits               = 6;
                VideoModes[gNumVideoModes].RedMask                      = 0;
                VideoModes[gNumVideoModes].GreenMask                    = 0;
                VideoModes[gNumVideoModes].BlueMask                     = 0;
                VideoModes[gNumVideoModes].AttributeFlags               = VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR | VIDEO_MODE_NO_OFF_SCREEN |
                                                                          VIDEO_MODE_PALETTE_DRIVEN | VIDEO_MODE_MANAGED_PALETTE;
            } break;
            case 2:
            {
                VideoModes[gNumVideoModes].NumberRedBits                = 5;
                VideoModes[gNumVideoModes].NumberGreenBits              = 6;
                VideoModes[gNumVideoModes].NumberBlueBits               = 5;
                VideoModes[gNumVideoModes].RedMask                      = 0xF800;
                VideoModes[gNumVideoModes].GreenMask                    = 0x7E0;
                VideoModes[gNumVideoModes].BlueMask                     = 0x1F;
                VideoModes[gNumVideoModes].AttributeFlags               = VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR | VIDEO_MODE_NO_OFF_SCREEN;
            } break;
            case 3:
            {
                VideoModes[gNumVideoModes].NumberRedBits                = 8;
                VideoModes[gNumVideoModes].NumberGreenBits              = 8;
                VideoModes[gNumVideoModes].NumberBlueBits               = 8;
                VideoModes[gNumVideoModes].RedMask                      = 0xFF0000;
                VideoModes[gNumVideoModes].GreenMask                    = 0xFF00;
                VideoModes[gNumVideoModes].BlueMask                     = 0xFF;
                VideoModes[gNumVideoModes].AttributeFlags               = VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR | VIDEO_MODE_NO_OFF_SCREEN;
            } break;
            default:
            case 4:
            {
                VideoModes[gNumVideoModes].NumberRedBits                = 8;
                VideoModes[gNumVideoModes].NumberGreenBits              = 8;
                VideoModes[gNumVideoModes].NumberBlueBits               = 8;
                VideoModes[gNumVideoModes].RedMask                      = 0xFF0000;
                VideoModes[gNumVideoModes].GreenMask                    = 0xFF00;
                VideoModes[gNumVideoModes].BlueMask                     = 0xFF;
                VideoModes[gNumVideoModes].AttributeFlags               = VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR | VIDEO_MODE_NO_OFF_SCREEN;
#ifdef VBOX_WITH_WDDM
                gPreferredVideoMode = gNumVideoModes;
#endif
            } break;
        }
        VideoModes[gNumVideoModes].VideoMemoryBitmapWidth       = 800;
        VideoModes[gNumVideoModes].VideoMemoryBitmapHeight      = 600;
        VideoModes[gNumVideoModes].DriverSpecificAttributeFlags = 0;

        /* a new mode has been filled in */
        ++gNumVideoModes;
    }

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

        if (yOffset == 0 && resolutionMatrix[matrixIndex].xRes == 800 && resolutionMatrix[matrixIndex].yRes == 600)
        {
            /* This mode was already added. */
            ++matrixIndex;
            continue;
        }

        /* does the host like that mode? */
#ifndef VBOX_WITH_WDDM
        if (!vboxLikesVideoMode(DeviceExtension->iDevice, resolutionMatrix[matrixIndex].xRes, resolutionMatrix[matrixIndex].yRes - yOffset, 8))
#else
        if (!vboxLikesVideoMode(0 /* @todo: */, resolutionMatrix[matrixIndex].xRes, resolutionMatrix[matrixIndex].yRes - yOffset, 8))
#endif
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
        VideoModes[gNumVideoModes].Frequency                    = 60;
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

        if (yOffset == 0 && resolutionMatrix[matrixIndex].xRes == 800 && resolutionMatrix[matrixIndex].yRes == 600)
        {
            /* This mode was already added. */
            ++matrixIndex;
            continue;
        }

        /* does the host like that mode? */
#ifndef VBOX_WITH_WDDM
        if (!vboxLikesVideoMode(DeviceExtension->iDevice, resolutionMatrix[matrixIndex].xRes, resolutionMatrix[matrixIndex].yRes - yOffset, 16))
#else
        if (!vboxLikesVideoMode(0 /* @todo: */, resolutionMatrix[matrixIndex].xRes, resolutionMatrix[matrixIndex].yRes - yOffset, 16))
#endif

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
        VideoModes[gNumVideoModes].Frequency                    = 60;
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

        if (yOffset == 0 && resolutionMatrix[matrixIndex].xRes == 800 && resolutionMatrix[matrixIndex].yRes == 600)
        {
            /* This mode was already added. */
            ++matrixIndex;
            continue;
        }

        /* does the host like that mode? */
#ifndef VBOX_WITH_WDDM
        if (!vboxLikesVideoMode(DeviceExtension->iDevice, resolutionMatrix[matrixIndex].xRes, resolutionMatrix[matrixIndex].yRes - yOffset, 24))
#else
        if (!vboxLikesVideoMode(0, resolutionMatrix[matrixIndex].xRes, resolutionMatrix[matrixIndex].yRes - yOffset, 24))
#endif
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
        VideoModes[gNumVideoModes].Frequency                    = 60;
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

        if (yOffset == 0 && resolutionMatrix[matrixIndex].xRes == 800 && resolutionMatrix[matrixIndex].yRes == 600)
        {
            /* This mode was already added. */
            ++matrixIndex;
            continue;
        }

        /* does the host like that mode? */
#ifndef VBOX_WITH_WDDM
        if (!vboxLikesVideoMode(DeviceExtension->iDevice, resolutionMatrix[matrixIndex].xRes, resolutionMatrix[matrixIndex].yRes - yOffset, 32))
#else
        if (!vboxLikesVideoMode(0, resolutionMatrix[matrixIndex].xRes, resolutionMatrix[matrixIndex].yRes - yOffset, 32))
#endif
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
        VideoModes[gNumVideoModes].Frequency                    = 60;
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
#ifdef VBOX_WITH_WDDM
    int fPreferredSet = 0;
#endif
    do
    {
        /* check if there is space in the mode list */
        if (gNumVideoModes >= MAX_VIDEO_MODES)
            break;

        wchar_t keyname[24];
        uint32_t xres, yres, bpp = 0;
        swprintf(keyname, L"CustomMode%dWidth", curKeyNo);
        status = VBoxVideoCmnRegQueryDword(Reg, keyname, &xres);
        /* upon the first error, we give up */
        if (status != NO_ERROR)
            break;
        swprintf(keyname, L"CustomMode%dHeight", curKeyNo);
        status = VBoxVideoCmnRegQueryDword(Reg, keyname, &yres);
        /* upon the first error, we give up */
        if (status != NO_ERROR)
            break;
        swprintf(keyname, L"CustomMode%dBPP", curKeyNo);
        status = VBoxVideoCmnRegQueryDword(Reg, keyname, &bpp);
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

        /* round down width to be a multiple of 8 if necessary */
        if (!DeviceExtension->fAnyX)
            xres &= 0xFFF8;

        /* second test: does it fit within our VRAM? */
        if (xres * yres * (bpp / 8) > vramSize)
            break;

        /* third test: does the host like the video mode? */
#ifndef VBOX_WITH_WDDM
        if (!vboxLikesVideoMode(DeviceExtension->iDevice, xres, yres, bpp))
#else
        if (!vboxLikesVideoMode(0, xres, yres, bpp))
#endif
            break;

        dprintf(("VBoxVideo: adding mode from registry: xres = %d, yres = %d, bpp = %d\n", xres, yres, bpp));

#ifdef VBOX_WITH_WDDM
        if (!fPreferredSet)
        {
            gPreferredVideoMode = gNumVideoModes;
            fPreferredSet = 1;
        }
#endif
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
        VideoModes[gNumVideoModes].Frequency                    = 60;
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
#ifdef VBOX_WITH_WDDM
                /* 32-bit mode is more preferable, select it if not yet */
                if (fPreferredSet < 2)
                {
                    gPreferredVideoMode = gNumVideoModes;
                    fPreferredSet = 2;
                }
#endif
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
     *
     * ... or ...
     *
     * Also we check if we got an user-stored custom resolution in the adapter
     * registry key add it to the modes table.
     */

#ifdef VBOX_WITH_MULTIMONITOR_FIX
    /* Add custom resolutions for each display and then for the display change request, if exists.
     */
    BOOLEAN fDisplayChangeRequest = FALSE;

    uint32_t xres = 0, yres = 0, bpp = 0, display = 0;
    if (   vboxQueryDisplayRequest(&xres, &yres, &bpp, &display)
        && (xres || yres || bpp))
    {
        /* There is a pending display change request. */
        fDisplayChangeRequest = TRUE;
    }
    if (display > RT_ELEMENTS(CustomVideoModes))
    {
       display = RT_ELEMENTS(CustomVideoModes) - 1;
    }

    dprintf(("display = %d, DeviceExtension->iDevice = %d\n", display, DeviceExtension->iDevice));
    if (display != DeviceExtension->iDevice)
    {
        /* No need to go through the custom mode logic. And no need to clear the custom mode
         * entry in the next 'for' loop.
         */
        fDisplayChangeRequest = FALSE;
    }

    dprintf(("VBoxVideo: fDisplayChangeRequest = %d\n", fDisplayChangeRequest));

    /*
     * Reinsert custom video modes for all displays.
     */
    int iCustomMode;
    for (iCustomMode = 0; iCustomMode < commonFromDeviceExt(DeviceExtension)->cDisplays; iCustomMode++)
    {
        if (fDisplayChangeRequest && iCustomMode == display)
        {
            /* Do not keep info for this display, which received a video mode hint, to make sure that
             * the new mode will be taken from the alternating index entries actually.
             */
            memcpy(&VideoModes[gNumVideoModes], &VideoModes[3], sizeof(VIDEO_MODE_INFORMATION));
            VideoModes[gNumVideoModes].ModeIndex = gNumVideoModes + 1;
        }
        else
        {
            VideoModes[gNumVideoModes] = CustomVideoModes[iCustomMode];
            VideoModes[gNumVideoModes].ModeIndex = gNumVideoModes + 1;
        }
#ifdef LOG_ENABLED
        dprintf(("Custom mode for %2d: %4d x %4d @ %2d\n",
                iCustomMode, CustomVideoModes[iCustomMode].VisScreenWidth,
                CustomVideoModes[iCustomMode].VisScreenHeight, CustomVideoModes[iCustomMode].BitsPerPlane));
#endif
        gNumVideoModes++;
    }

    if (display != DeviceExtension->iDevice)
    {
        /* The display change is for another monitor. Just add 2 standard modes to the table
         * to make enough entries. This is not necessary if it is a first mode set (CurrentMode == 0),
         * because these 2 entries will be added by "if (fDisplayChangeRequest || DeviceExtension->CurrentMode == 0)"
         * code branch.
         */
        if (DeviceExtension->CurrentMode != 0)
        {
            dprintf(("Filling custom mode entries.\n"));
            memcpy(&VideoModes[gNumVideoModes], &VideoModes[3], sizeof(VIDEO_MODE_INFORMATION));
            VideoModes[gNumVideoModes].ModeIndex = gNumVideoModes + 1;
            gNumVideoModes++;
            memcpy(&VideoModes[gNumVideoModes], &VideoModes[3], sizeof(VIDEO_MODE_INFORMATION));
            VideoModes[gNumVideoModes].ModeIndex = gNumVideoModes + 1;
            gNumVideoModes++;
        }
    }
#endif /* VBOX_WITH_MULTIMONITOR_FIX */

#ifndef VBOX_WITH_MULTIMONITOR_FIX
    uint32_t xres = 0, yres = 0, bpp = 0, display = 0;
    if (   (   vboxQueryDisplayRequest(&xres, &yres, &bpp, &display)
            && (xres || yres || bpp))
        || (gCustomXRes || gCustomYRes || gCustomBPP))
#else
    if (fDisplayChangeRequest || DeviceExtension->CurrentMode == 0)
#endif /* VBOX_WITH_MULTIMONITOR_FIX */
    {
#ifndef VBOX_WITH_WDDM
        dprintf(("VBoxVideo: adding custom video mode as #%d, current mode: %d \n", gNumVideoModes + 1, DeviceExtension->CurrentMode));
        /* handle the startup case */
        if (DeviceExtension->CurrentMode == 0)
#else
        if (!commonFromDeviceExt(DeviceExtension)->cDisplays || !DeviceExtension->aSources[0].pPrimaryAllocation)
#endif
        {
            /* Use the stored custom resolution values only if nothing was read from host.
             * The custom mode might be not valid anymore and would block any hints from host.
             */
#ifndef VBOX_WITH_MULTIMONITOR_FIX
            if (!xres)
                xres = gCustomXRes;
            if (!yres)
                yres = gCustomYRes;
            if (!bpp)
                bpp  = gCustomBPP;
            dprintf(("VBoxVideo: using stored custom resolution %dx%dx%d\n", xres, yres, bpp));
#else
            if (!xres)
                xres = CustomVideoModes[DeviceExtension->iDevice].VisScreenWidth;
            if (!yres)
                yres = CustomVideoModes[DeviceExtension->iDevice].VisScreenHeight;
            if (!bpp)
                bpp  = CustomVideoModes[DeviceExtension->iDevice].BitsPerPlane;
            dprintf(("VBoxVideo: using stored custom resolution %dx%dx%d for %d\n", xres, yres, bpp, DeviceExtension->iDevice));
#endif /* VBOX_WITH_MULTIMONITOR_FIX */
        }
        /* round down to multiple of 8 if necessary */
        if (!DeviceExtension->fAnyX) {
            if ((xres & 0xfff8) != xres)
                dprintf(("VBoxVideo: rounding down xres from %d to %d\n", xres, xres & 0xfff8));
            xres &= 0xfff8;
        }
        /* take the current values for the fields that are not set */
#ifndef VBOX_WITH_WDDM
        if (DeviceExtension->CurrentMode != 0)
        {
            if (!xres)
                xres = DeviceExtension->CurrentModeWidth;
            if (!yres)
                yres = DeviceExtension->CurrentModeHeight;
            if (!bpp)
                bpp  = DeviceExtension->CurrentModeBPP;
        }
#else
        if (commonFromDeviceExt(DeviceExtension)->cDisplays && DeviceExtension->aSources[0].pPrimaryAllocation)
        {
            if (!xres)
                xres = DeviceExtension->aSources[0].pPrimaryAllocation->SurfDesc.width;
            if (!yres)
                yres = DeviceExtension->aSources[0].pPrimaryAllocation->SurfDesc.height;
            if (!bpp)
                bpp  = DeviceExtension->aSources[0].pPrimaryAllocation->SurfDesc.bpp;
        }
#endif

        /* Use a default value. */
        if (!bpp)
            bpp = 32;

        /* does the host like that mode? */
#ifndef VBOX_WITH_WDDM
        if (vboxLikesVideoMode(DeviceExtension->iDevice, xres, yres, bpp))
#else
        if (vboxLikesVideoMode(0, xres, yres, bpp))
#endif
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
#ifdef VBOX_WITH_MULTIMONITOR_FIX
                /* Only alternate index if the new custom mode differs from the last one
                 * (only resolution and bpp changes are important, a display change does not matter).
                 * Always add 2 last entries to the mode array, so number of video modes
                 * do not change.
                 */
                BOOLEAN fNewInvocation = FALSE;
                static uint32_t sPrev_xres    = 0;
                static uint32_t sPrev_yres    = 0;
                static uint32_t sPrev_bpp     = 0;
                if (   sPrev_xres != xres
                    || sPrev_yres != yres
                    || sPrev_bpp != bpp)
                {
                    sPrev_xres = xres;
                    sPrev_yres = yres;
                    sPrev_bpp = bpp;
                    fNewInvocation = TRUE;
                }
                BOOLEAN fAlternatedIndex = FALSE;
#endif /* VBOX_WITH_MULTIMONITOR_FIX */
#ifndef VBOX_WITH_WDDM
                if (DeviceExtension->CurrentMode != 0)
#else
                if (commonFromDeviceExt(DeviceExtension)->cDisplays && DeviceExtension->aSources[0].pPrimaryAllocation)
#endif
#ifndef VBOX_WITH_MULTIMONITOR_FIX
                {
                    if (gInvocationCounter % 2)
                        gNumVideoModes++;
                    gInvocationCounter++;
                }
#else
                {
                    if (fNewInvocation)
                        gInvocationCounter++;
                    if (gInvocationCounter % 2)
                    {
                        fAlternatedIndex = TRUE;

                        memcpy(&VideoModes[gNumVideoModes], &VideoModes[3], sizeof(VIDEO_MODE_INFORMATION));
                        VideoModes[gNumVideoModes].ModeIndex = gNumVideoModes + 1;
                        gNumVideoModes++;
                    }
                }
                else
                {
                    fNewInvocation = FALSE;
                }
#endif /* VBOX_WITH_MULTIMONITOR_FIX */

                dprintf(("VBoxVideo: setting special mode to xres = %d, yres = %d, bpp = %d, display = %d\n", xres, yres, bpp, display));
#ifdef VBOX_WITH_MULTIMONITOR_FIX
                dprintf(("VBoxVideo: fNewInvocation = %d, fAlternatedIndex = %d\n", fNewInvocation, fAlternatedIndex));
#endif /* VBOX_WITH_MULTIMONITOR_FIX */
#ifdef VBOX_WITH_WDDM
                /* assign host-supplied as the most preferable */
                gPreferredVideoMode = gNumVideoModes;
#endif
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
                VideoModes[gNumVideoModes].Frequency                    = 60;
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
#ifdef VBOX_WITH_MULTIMONITOR_FIX
                /* Save the mode in the list of custom modes for this display. */
                CustomVideoModes[DeviceExtension->iDevice] = VideoModes[gNumVideoModes];
#endif /* VBOX_WITH_MULTIMONITOR_FIX */
                ++gNumVideoModes;

                /* for the startup case, we need this mode twice due to the alternating mode number */
#ifndef VBOX_WITH_WDDM
                if (DeviceExtension->CurrentMode == 0)
#else
                if (!commonFromDeviceExt(DeviceExtension)->cDisplays || !DeviceExtension->aSources[0].pPrimaryAllocation)
#endif
                {
                    dprintf(("VBoxVideo: making a copy of the custom mode as #%d\n", gNumVideoModes + 1));
                    memcpy(&VideoModes[gNumVideoModes], &VideoModes[gNumVideoModes - 1], sizeof(VIDEO_MODE_INFORMATION));
                    VideoModes[gNumVideoModes].ModeIndex = gNumVideoModes + 1;
                    gNumVideoModes++;
                }
#ifdef VBOX_WITH_MULTIMONITOR_FIX
                else if (!fAlternatedIndex)
                {
                    dprintf(("VBoxVideo: making a copy of the custom mode as #%d\n", gNumVideoModes + 1));
                    memcpy(&VideoModes[gNumVideoModes], &VideoModes[3], sizeof(VIDEO_MODE_INFORMATION));
                    VideoModes[gNumVideoModes].ModeIndex = gNumVideoModes + 1;
                    gNumVideoModes++;
                }
#endif /* VBOX_WITH_MULTIMONITOR_FIX */

#ifndef VBOX_WITH_MULTIMONITOR_FIX
                /* store this video mode as the last custom video mode */
                status = VBoxVideoCmnRegSetDword(Reg, L"CustomXRes", xres);
                if (status != NO_ERROR)
                    dprintf(("VBoxVideo: error %d writing CustomXRes\n", status));
                status = VBoxVideoCmnRegSetDword(Reg, L"CustomYRes", yres);
                if (status != NO_ERROR)
                    dprintf(("VBoxVideo: error %d writing CustomYRes\n", status));
                status = VBoxVideoCmnRegSetDword(Reg, L"CustomBPP", bpp);
                if (status != NO_ERROR)
                    dprintf(("VBoxVideo: error %d writing CustomBPP\n", status));
#else
                /* Save the custom mode for this display. */
                if (DeviceExtension->iDevice == 0)
                {
                    /* Name without a suffix */
                    status = VBoxVideoCmnRegSetDword(Reg, L"CustomXRes", xres);
                    if (status != NO_ERROR)
                        dprintf(("VBoxVideo: error %d writing CustomXRes\n", status));
                    status = VBoxVideoCmnRegSetDword(Reg, L"CustomYRes", yres);
                    if (status != NO_ERROR)
                        dprintf(("VBoxVideo: error %d writing CustomYRes\n", status));
                    status = VBoxVideoCmnRegSetDword(Reg, L"CustomBPP", bpp);
                    if (status != NO_ERROR)
                        dprintf(("VBoxVideo: error %d writing CustomBPP\n", status));
                }
                else
                {
                    wchar_t keyname[32];
                    swprintf(keyname, L"CustomXRes%d", DeviceExtension->iDevice);
                    status = VBoxVideoCmnRegSetDword(Reg, keyname, xres);
                    if (status != NO_ERROR)
                        dprintf(("VBoxVideo: error %d writing CustomXRes%d\n", status, DeviceExtension->iDevice));
                    swprintf(keyname, L"CustomYRes%d", DeviceExtension->iDevice);
                    status = VBoxVideoCmnRegSetDword(Reg, keyname, yres);
                    if (status != NO_ERROR)
                        dprintf(("VBoxVideo: error %d writing CustomYRes%d\n", status, DeviceExtension->iDevice));
                    swprintf(keyname, L"CustomBPP%d", DeviceExtension->iDevice);
                    status = VBoxVideoCmnRegSetDword(Reg, keyname, bpp);
                    if (status != NO_ERROR)
                        dprintf(("VBoxVideo: error %d writing CustomBPP%d\n", status, DeviceExtension->iDevice));
                }
#endif /* VBOX_WITH_MULTIMONITOR_FIX */
            }
            else
            {
                dprintf(("VBoxVideo: invalid parameters for special mode: (xres = %d, yres = %d, bpp = %d, vramSize = %d)\n",
                         xres, yres, bpp, vramSize));
                if (xres * yres * (bpp / 8) >= vramSize
                    && (xres != g_xresNoVRAM || yres != g_yresNoVRAM || bpp != g_bppNoVRAM))
                {
                    LogRel(("VBoxVideo: not enough VRAM for video mode %dx%dx%dbpp. Available: %d bytes. Required: more than %d bytes.\n",
                            xres, yres, bpp, vramSize, xres * yres * (bpp / 8)));
                    g_xresNoVRAM = xres;
                    g_yresNoVRAM = yres;
                    g_bppNoVRAM = bpp;
                }
            }
        }
        else
            dprintf(("VBoxVideo: host does not like special mode: (xres = %d, yres = %d, bpp = %d)\n",
                     xres, yres, bpp));
    }
#if defined(LOG_ENABLED)
    {
        int i;
#ifndef VBOX_WITH_WDDM
        dprintf(("VBoxVideo: VideoModes (CurrentMode = %d, last #%d)\n", DeviceExtension->CurrentMode, gNumVideoModes));
#endif
        for (i = 0; i < RT_ELEMENTS(VideoModes); i++)
        {
            if (   VideoModes[i].VisScreenWidth
                || VideoModes[i].VisScreenHeight
                || VideoModes[i].BitsPerPlane)
            {
                dprintf((" %2d: #%d %4d x %4d @ %2d\n",
                        i, VideoModes[i].ModeIndex, VideoModes[i].VisScreenWidth,
                        VideoModes[i].VisScreenHeight, VideoModes[i].BitsPerPlane));
            }
        }
    }
#endif

#ifdef VBOX_WITH_WDDM
    vboxWddmBuildResolutionTable();
#endif

    VBoxVideoCmnRegFini(Reg);
}

#endif

#ifdef VBOX_WITH_WDDM
/**
 * Helper function to dynamically build our table of standard video
 * modes. We take the amount of VRAM and create modes with standard
 * geometries until we've either reached the maximum number of modes
 * or the available VRAM does not allow for additional modes.
 */

AssertCompile(sizeof (SIZE) == sizeof (D3DKMDT_2DREGION));
AssertCompile(RT_OFFSETOF(SIZE, cx) == RT_OFFSETOF(D3DKMDT_2DREGION, cx));
AssertCompile(RT_OFFSETOF(SIZE, cy) == RT_OFFSETOF(D3DKMDT_2DREGION, cy));
static VOID vboxWddmBuildVideoModesInfo(PDEVICE_EXTENSION DeviceExtension, D3DDDI_VIDEO_PRESENT_TARGET_ID VidPnTargetId,
        PVBOXWDDM_VIDEOMODES_INFO pModes)
{
    pModes->cModes = RT_ELEMENTS(pModes->aModes);
    pModes->cResolutions = RT_ELEMENTS(pModes->aResolutions);
    vboxVideoBuildModesTable(DeviceExtension, VidPnTargetId, pModes->aModes, &pModes->cModes, &pModes->iPreferredMode);
    vboxVideoBuildResolutionTable(pModes->aModes, pModes->cModes, (SIZE*)((void*)pModes->aResolutions), &pModes->cResolutions);
}

NTSTATUS vboxWddmGetModesForResolution(PDEVICE_EXTENSION DeviceExtension, PVBOXWDDM_VIDEOMODES_INFO pModeInfos,
        D3DKMDT_2DREGION *pResolution, VIDEO_MODE_INFORMATION * pModes, uint32_t cModes, uint32_t *pcModes, int32_t *piPreferrableMode)
{
    NTSTATUS Status = STATUS_SUCCESS;
    uint32_t cFound = 0;
    int iFoundPreferrableMode = -1;
    for (uint32_t i = 0; i < pModeInfos->cModes; ++i)
    {
        VIDEO_MODE_INFORMATION *pCur = &pModeInfos->aModes[i];
        if (pResolution->cx == pCur->VisScreenWidth
                        && pResolution->cy == pCur->VisScreenHeight)
        {
            if (pModes && cModes > cFound)
                memcpy(&pModes[cFound], pCur, sizeof (VIDEO_MODE_INFORMATION));
            else
                Status = STATUS_BUFFER_TOO_SMALL;

            if (i == pModeInfos->iPreferredMode)
                iFoundPreferrableMode = cFound;

            ++cFound;
        }
    }

    Assert(iFoundPreferrableMode < 0 || cFound > (uint32_t)iFoundPreferrableMode);

    *pcModes = cFound;
    if (piPreferrableMode)
        *piPreferrableMode = iFoundPreferrableMode;

    return Status;
}


static VBOXWDDM_VIDEOMODES_INFO g_aVBoxVideoModeInfos[VBOX_VIDEO_MAX_SCREENS] = {0};

PVBOXWDDM_VIDEOMODES_INFO vboxWddmGetVideoModesInfo(PDEVICE_EXTENSION DeviceExtension, D3DDDI_VIDEO_PRESENT_TARGET_ID VidPnTargetId)
{
    Assert(VidPnTargetId < (D3DDDI_VIDEO_PRESENT_TARGET_ID)commonFromDeviceExt(DeviceExtension)->cDisplays);
    if (VidPnTargetId >= (D3DDDI_VIDEO_PRESENT_TARGET_ID)commonFromDeviceExt(DeviceExtension)->cDisplays)
    {
        return NULL;
    }

    if (!g_aVBoxVideoModeInfos[VidPnTargetId].cModes)
    {
        vboxWddmBuildVideoModesInfo(DeviceExtension, VidPnTargetId, &g_aVBoxVideoModeInfos[VidPnTargetId]);
    }

    return &g_aVBoxVideoModeInfos[VidPnTargetId];
}

PVBOXWDDM_VIDEOMODES_INFO vboxWddmGetAllVideoModesInfos(PDEVICE_EXTENSION DeviceExtension)
{
    /* ensure all modes are initialized */
    for (int i = 0; i < commonFromDeviceExt(DeviceExtension)->cDisplays; ++i)
    {
        vboxWddmGetVideoModesInfo(DeviceExtension, (D3DDDI_VIDEO_PRESENT_TARGET_ID)i);
    }

    return g_aVBoxVideoModeInfos;
}

VOID vboxWddmInvalidateVideoModesInfo(PDEVICE_EXTENSION DeviceExtension)
{
    for (UINT i = 0; i < RT_ELEMENTS(g_aVBoxVideoModeInfos); ++i)
    {
        g_aVBoxVideoModeInfos[i].cModes = 0;
    }
}

#else

/* Computes the size of a framebuffer. DualView has a few framebuffers of the computed size. */
void VBoxComputeFrameBufferSizes (PDEVICE_EXTENSION PrimaryExtension)
{
    ULONG ulAvailable = commonFromDeviceExt(PrimaryExtension)->cbVRAM
                        - commonFromDeviceExt(PrimaryExtension)->cbMiniportHeap
                        - VBVA_ADAPTER_INFORMATION_SIZE;

    /* Size of a framebuffer. */

    ULONG ulSize = ulAvailable / commonFromDeviceExt(PrimaryExtension)->cDisplays;

    /* Align down to 4096 bytes. */
    ulSize &= ~0xFFF;

    dprintf(("VBoxVideo::VBoxComputeFrameBufferSizes: cbVRAM = 0x%08X, cDisplays = %d, ulSize = 0x%08X, ulSize * cDisplays = 0x%08X, slack = 0x%08X\n",
             commonFromDeviceExt(PrimaryExtension)->cbVRAM, commonFromDeviceExt(PrimaryExtension)->cDisplays,
             ulSize, ulSize * commonFromDeviceExt(PrimaryExtension)->cDisplays,
             ulAvailable - ulSize * commonFromDeviceExt(PrimaryExtension)->cDisplays));


    /* Update the primary info. */
    PrimaryExtension->u.primary.ulMaxFrameBufferSize     = ulSize;

    /* Update the per extension info. */
    PDEVICE_EXTENSION Extension = PrimaryExtension;
    ULONG ulFrameBufferOffset = 0;
    while (Extension)
    {
        Extension->ulFrameBufferOffset = ulFrameBufferOffset;
        /* That is assigned when a video mode is set. */
        Extension->ulFrameBufferSize = 0;

        dprintf(("VBoxVideo::VBoxComputeFrameBufferSizes: [%d] ulFrameBufferOffset 0x%08X\n",
                 Extension->iDevice, ulFrameBufferOffset));

        ulFrameBufferOffset += PrimaryExtension->u.primary.ulMaxFrameBufferSize;

        Extension = Extension->pNext;
    }
}

#endif

void VBoxVideoCmnPortWriteUchar(RTIOPORT Port, uint8_t Value)
{
#ifndef VBOX_WITH_WDDM
    VideoPortWritePortUchar((PUCHAR)Port,Value);
#else
    WRITE_PORT_UCHAR((PUCHAR)Port,Value);
#endif
}

void VBoxVideoCmnPortWriteUshort(RTIOPORT Port, uint16_t Value)
{
#ifndef VBOX_WITH_WDDM
    VideoPortWritePortUshort((PUSHORT)Port,Value);
#else
    WRITE_PORT_USHORT((PUSHORT)Port,Value);
#endif
}

void VBoxVideoCmnPortWriteUlong(RTIOPORT Port, uint32_t Value)
{
#ifndef VBOX_WITH_WDDM
    VideoPortWritePortUlong((PULONG)Port,Value);
#else
    WRITE_PORT_ULONG((PULONG)Port,Value);
#endif
}

uint8_t VBoxVideoCmnPortReadUchar(RTIOPORT Port)
{
#ifndef VBOX_WITH_WDDM
    return VideoPortReadPortUchar((PUCHAR)Port);
#else
    return READ_PORT_UCHAR((PUCHAR)Port);
#endif
}

uint16_t VBoxVideoCmnPortReadUshort(RTIOPORT Port)
{
#ifndef VBOX_WITH_WDDM
    return VideoPortReadPortUshort((PUSHORT)Port);
#else
    return READ_PORT_USHORT((PUSHORT)Port);
#endif
}

uint32_t VBoxVideoCmnPortReadUlong(RTIOPORT Port)
{
#ifndef VBOX_WITH_WDDM
    return VideoPortReadPortUlong((PULONG)Port);
#else
    return READ_PORT_ULONG((PULONG)Port);
#endif
}

int VBoxMapAdapterMemory (PVBOXVIDEO_COMMON pCommon, void **ppv, ULONG ulOffset, ULONG ulSize)
{
    PDEVICE_EXTENSION PrimaryExtension = commonToPrimaryExt(pCommon);
    dprintf(("VBoxVideo::VBoxMapAdapterMemory 0x%08X[0x%X]\n", ulOffset, ulSize));

    if (!ulSize)
    {
        dprintf(("Illegal length 0!\n"));
        return ERROR_INVALID_PARAMETER;
    }

    PHYSICAL_ADDRESS FrameBuffer;
    FrameBuffer.QuadPart = VBE_DISPI_LFB_PHYSICAL_ADDRESS + ulOffset;

    PVOID VideoRamBase = NULL;
    ULONG VideoRamLength = ulSize;
    VP_STATUS Status;
#ifndef VBOX_WITH_WDDM
    ULONG inIoSpace = 0;

    Status = VideoPortMapMemory (PrimaryExtension, FrameBuffer,
                                           &VideoRamLength, &inIoSpace,
                                           &VideoRamBase);
#else
    NTSTATUS ntStatus = PrimaryExtension->u.primary.DxgkInterface.DxgkCbMapMemory(PrimaryExtension->u.primary.DxgkInterface.DeviceHandle,
            FrameBuffer,
            VideoRamLength,
            FALSE, /* IN BOOLEAN InIoSpace */
            FALSE, /* IN BOOLEAN MapToUserMode */
            MmNonCached, /* IN MEMORY_CACHING_TYPE CacheType */
            &VideoRamBase /*OUT PVOID *VirtualAddress*/
            );
    Assert(ntStatus == STATUS_SUCCESS);
    Status = ntStatus == STATUS_SUCCESS ? NO_ERROR : ERROR_INVALID_PARAMETER; /*<- this is what VideoPortMapMemory returns according to the docs */
#endif

    if (Status == NO_ERROR)
    {
        *ppv = VideoRamBase;
    }

    dprintf(("VBoxVideo::VBoxMapAdapterMemory rc = %d\n", Status));

    return Status;
}

bool VBoxSyncToVideoIRQ(PVBOXVIDEO_COMMON pCommon, PFNVIDEOIRQSYNC pfnSync,
                        void *pvUser)
{
    PDEVICE_EXTENSION PrimaryExtension = commonToPrimaryExt(pCommon);
    PMINIPORT_SYNCHRONIZE_ROUTINE pfnSyncMiniport;
    pfnSyncMiniport = (PMINIPORT_SYNCHRONIZE_ROUTINE) pfnSync;
#ifndef VBOX_WITH_WDDM
    return !!VideoPortSynchronizeExecution(PrimaryExtension, VpMediumPriority,
                                           pfnSyncMiniport, pvUser);
#else
    BOOLEAN fRet;
    DXGKCB_SYNCHRONIZE_EXECUTION pfnDxgkCbSync =
        PrimaryExtension->u.primary.DxgkInterface.DxgkCbSynchronizeExecution;
    HANDLE hDev = PrimaryExtension->u.primary.DxgkInterface.DeviceHandle;
    NTSTATUS ntStatus = pfnDxgkCbSync(hDev, pfnSyncMiniport, pvUser, 0, &fRet);
    AssertReturn(ntStatus == STATUS_SUCCESS, false);
    return !!fRet;
#endif
}

void VBoxUnmapAdapterMemory (PVBOXVIDEO_COMMON pCommon, void **ppv)
{
    dprintf(("VBoxVideo::VBoxUnmapAdapterMemory\n"));

    PDEVICE_EXTENSION PrimaryExtension = commonToPrimaryExt(pCommon);

    if (*ppv)
    {
#ifndef VBOX_WITH_WDDM
        VideoPortUnmapMemory(PrimaryExtension, *ppv, NULL);
#else
        NTSTATUS ntStatus = PrimaryExtension->u.primary.DxgkInterface.DxgkCbUnmapMemory(PrimaryExtension->u.primary.DxgkInterface.DeviceHandle,
                *ppv);
        Assert(ntStatus == STATUS_SUCCESS);
#endif
    }

    *ppv = NULL;
}


void vboxVideoInitCustomVideoModes(PDEVICE_EXTENSION pDevExt)
{
    VP_STATUS status;
    VBOXCMNREG Reg;

    VBoxVideoCmnRegInit(pDevExt, &Reg);

    dprintf(("VBoxVideo::vboxVideoInitCustomVideoModes\n"));

#ifndef VBOX_WITH_MULTIMONITOR_FIX
    /*
     * Get the last custom resolution
     */
    status = VBoxVideoCmnRegQueryDword(Reg, L"CustomXRes", &gCustomXRes);
    if (status != NO_ERROR)
        gCustomXRes = 0;

    status = VBoxVideoCmnRegQueryDword(Reg, L"CustomYRes", &gCustomYRes);
    if (status != NO_ERROR)
        gCustomYRes = 0;
    status = VBoxVideoCmnRegQueryDword(Reg, L"CustomBPP", &gCustomBPP);
    if (status != NO_ERROR)
        gCustomBPP = 0;

   dprintf(("VBoxVideo: got stored custom resolution %dx%dx%d\n", gCustomXRes, gCustomYRes, gCustomBPP));
#else
    /* Initialize all custom modes to the 800x600x32. */
    initVideoModeInformation(&CustomVideoModes[0], 800, 600, 32, 0, 0);

    int iCustomMode;
    for (iCustomMode = 1; iCustomMode < RT_ELEMENTS(CustomVideoModes); iCustomMode++)
    {
        CustomVideoModes[iCustomMode] = CustomVideoModes[0];
    }

    /* Load stored custom resolution from the registry. */
    for (iCustomMode = 0;
#ifdef VBOX_WITH_WDDM
            iCustomMode < commonFromDeviceExt(pDevExt)->cDisplays;
#else
            iCustomMode < commonFromDeviceExt(pDevExt)->cDisplays;
#endif
            iCustomMode++)
    {
        /*
         * Get the last custom resolution
         */
        uint32_t CustomXRes = 0, CustomYRes = 0, CustomBPP = 0;

        if (iCustomMode == 0)
        {
            /* Name without a suffix */
            status = VBoxVideoCmnRegQueryDword(Reg, L"CustomXRes", &CustomXRes);
            if (status != NO_ERROR)
                CustomXRes = 0;
            status = VBoxVideoCmnRegQueryDword(Reg, L"CustomYRes", &CustomYRes);
            if (status != NO_ERROR)
                CustomYRes = 0;
            status = VBoxVideoCmnRegQueryDword(Reg, L"CustomBPP", &CustomBPP);
            if (status != NO_ERROR)
                CustomBPP = 0;
        }
        else
        {
            wchar_t keyname[32];
            swprintf(keyname, L"CustomXRes%d", iCustomMode);
            status = VBoxVideoCmnRegQueryDword(Reg, keyname, &CustomXRes);
            if (status != NO_ERROR)
                CustomXRes = 0;
            swprintf(keyname, L"CustomYRes%d", iCustomMode);
            status = VBoxVideoCmnRegQueryDword(Reg, keyname, &CustomYRes);
            if (status != NO_ERROR)
                CustomYRes = 0;
            swprintf(keyname, L"CustomBPP%d", iCustomMode);
            status = VBoxVideoCmnRegQueryDword(Reg, keyname, &CustomBPP);
            if (status != NO_ERROR)
                CustomBPP = 0;
        }

        dprintf(("VBoxVideo: got stored custom resolution[%d] %dx%dx%d\n", iCustomMode, CustomXRes, CustomYRes, CustomBPP));

        if (CustomXRes || CustomYRes || CustomBPP)
        {
            if (CustomXRes == 0)
            {
                CustomXRes = CustomVideoModes[iCustomMode].VisScreenWidth;
            }
            if (CustomYRes == 0)
            {
                CustomYRes = CustomVideoModes[iCustomMode].VisScreenHeight;
            }
            if (CustomBPP == 0)
            {
                CustomBPP = CustomVideoModes[iCustomMode].BitsPerPlane;
            }

            initVideoModeInformation(&CustomVideoModes[iCustomMode], CustomXRes, CustomYRes, CustomBPP, 0, 0);
        }
    }
#endif /* VBOX_WITH_MULTIMONITOR_FIX */

    VBoxVideoCmnRegFini(Reg);
}

#ifndef VBOX_WITH_WDDM
static int vbvaInitInfoDisplay (void *pvData, VBVAINFOVIEW *p)
{
    PDEVICE_EXTENSION PrimaryExtension = (PDEVICE_EXTENSION) pvData;

    int i;
    PDEVICE_EXTENSION Extension;

    for (i = 0, Extension = PrimaryExtension;
         i < commonFromDeviceExt(PrimaryExtension)->cDisplays && Extension;
         i++, Extension = Extension->pNext)
    {
        p[i].u32ViewIndex     = Extension->iDevice;
        p[i].u32ViewOffset    = Extension->ulFrameBufferOffset;
        p[i].u32ViewSize      = PrimaryExtension->u.primary.ulMaxFrameBufferSize;

        /* How much VRAM should be reserved for the guest drivers to use VBVA. */
        const uint32_t cbReservedVRAM = VBVA_DISPLAY_INFORMATION_SIZE + VBVA_MIN_BUFFER_SIZE;

        p[i].u32MaxScreenSize = p[i].u32ViewSize > cbReservedVRAM?
                                    p[i].u32ViewSize - cbReservedVRAM:
                                    0;
    }

    if (i == commonFromDeviceExt(PrimaryExtension)->cDisplays && Extension == NULL)
    {
        return VINF_SUCCESS;
    }

    AssertFailed ();
    return VERR_INTERNAL_ERROR;
}


static VOID VBoxCreateDisplaysXPDM(PDEVICE_EXTENSION PrimaryExtension,
                                   PVIDEO_PORT_CONFIG_INFO pConfigInfo)
{
    VP_STATUS rc;

    if (commonFromDeviceExt(PrimaryExtension)->bHGSMI)
    {
        typedef VP_STATUS (*PFNCREATESECONDARYDISPLAY)(PVOID, PVOID *, ULONG);
        PFNCREATESECONDARYDISPLAY pfnCreateSecondaryDisplay = NULL;

        /* Dynamically query the VideoPort import to be binary compatible across Windows versions */
        if (vboxQueryWinVersion() > WINNT4)
        {
            /* This bluescreens on NT4, hence the above version check */
            pfnCreateSecondaryDisplay = (PFNCREATESECONDARYDISPLAY)(pConfigInfo->VideoPortGetProcAddress)
                                                                       (PrimaryExtension,
                                                                        (PUCHAR)"VideoPortCreateSecondaryDisplay");
        }

        if (!pfnCreateSecondaryDisplay)
            commonFromDeviceExt(PrimaryExtension)->cDisplays = 1;
        else
        {
            PDEVICE_EXTENSION pPrev = PrimaryExtension;

            ULONG iDisplay;
            ULONG cDisplays = commonFromDeviceExt(PrimaryExtension)->cDisplays;
            commonFromDeviceExt(PrimaryExtension)->cDisplays = 1;
            for (iDisplay = 1; iDisplay < cDisplays; iDisplay++)
            {
               PDEVICE_EXTENSION SecondaryExtension = NULL;
               rc = pfnCreateSecondaryDisplay (PrimaryExtension, (PVOID*)&SecondaryExtension, VIDEO_DUALVIEW_REMOVABLE);

               dprintf(("VBoxVideo::VBoxSetupDisplays: VideoPortCreateSecondaryDisplay returned %#x, SecondaryExtension = %p\n",
                        rc, SecondaryExtension));

               if (rc != NO_ERROR)
               {
                   break;
               }

               SecondaryExtension->pNext                = NULL;
               SecondaryExtension->pPrimary             = PrimaryExtension;
               SecondaryExtension->iDevice              = iDisplay;
               SecondaryExtension->ulFrameBufferOffset  = 0;
               SecondaryExtension->ulFrameBufferSize    = 0;
               SecondaryExtension->u.secondary.bEnabled = FALSE;

               /* Update the list pointers. */
               pPrev->pNext = SecondaryExtension;
               pPrev = SecondaryExtension;

               /* Take the successfully created display into account. */
               commonFromDeviceExt(PrimaryExtension)->cDisplays++;
            }
        }

        /* Failure to create secondary displays is not fatal */
        rc = NO_ERROR;
    }

    /* Now when the number of monitors is known and extensions are created,
     * calculate the layout of framebuffers.
     */
    VBoxComputeFrameBufferSizes (PrimaryExtension);
    /* in case of WDDM we do not control the framebuffer location,
     * i.e. it is assigned by Video Memory Manager,
     * The FB information should be passed to guest from our
     * DxgkDdiSetVidPnSourceAddress callback */

    if (commonFromDeviceExt(PrimaryExtension)->bHGSMI)
    {
        if (RT_SUCCESS(rc))
        {
            rc = VBoxHGSMISendViewInfo (commonFromDeviceExt(PrimaryExtension),
                                   commonFromDeviceExt(PrimaryExtension)->cDisplays,
                                   vbvaInitInfoDisplay,
                                   (void *) PrimaryExtension);
            AssertRC(rc);
        }

        if (RT_FAILURE (rc))
        {
            commonFromDeviceExt(PrimaryExtension)->bHGSMI = FALSE;
        }
    }
}

VP_STATUS VBoxVideoFindAdapter(IN PVOID HwDeviceExtension,
                               IN PVOID HwContext, IN PWSTR ArgumentString,
                               IN OUT PVIDEO_PORT_CONFIG_INFO ConfigInfo,
                               OUT PUCHAR Again)
{
   VP_STATUS rc;
   USHORT DispiId;
   ULONG AdapterMemorySize = VBE_DISPI_TOTAL_VIDEO_MEMORY_BYTES;

   dprintf(("VBoxVideo::VBoxVideoFindAdapter %p\n", HwDeviceExtension));

   VBoxSetupVideoPortFunctions((PDEVICE_EXTENSION)HwDeviceExtension, &((PDEVICE_EXTENSION)HwDeviceExtension)->u.primary.VideoPortProcs, ConfigInfo);

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

      dprintf(("VBoxVideo::VBoxVideoFindAdapter: calling VideoPortGetAccessRanges\n"));

      VIDEO_ACCESS_RANGE tmpRanges[4];
      ULONG slot = 0;

      VideoPortZeroMemory(tmpRanges, sizeof(tmpRanges));

      /* need to call VideoPortGetAccessRanges to ensure interrupt info in ConfigInfo gets set up */
      VP_STATUS status;
      if (vboxQueryWinVersion() == WINNT4)
      {
          /* NT crashes if either of 'vendorId, 'deviceId' or 'slot' parameters is NULL,
           * and needs PCI ids for a successful VideoPortGetAccessRanges call.
           */
          ULONG vendorId = 0x80EE;
          ULONG deviceId = 0xBEEF;
          status = VideoPortGetAccessRanges(HwDeviceExtension,
                                            0,
                                            NULL,
                                            sizeof (tmpRanges)/sizeof (tmpRanges[0]),
                                            tmpRanges,
                                            &vendorId,
                                            &deviceId,
                                            &slot);
      }
      else
      {
          status = VideoPortGetAccessRanges(HwDeviceExtension,
                                            0,
                                            NULL,
                                            sizeof (tmpRanges)/sizeof (tmpRanges[0]),
                                            tmpRanges,
                                            NULL,
                                            NULL,
                                            &slot);
      }
      dprintf(("VBoxVideo::VBoxVideoFindAdapter: VideoPortGetAccessRanges status 0x%x\n", status));

      /* Initialize VBoxGuest library, which is used for requests which go through VMMDev. */
      rc = VbglInit ();
      dprintf(("VBoxVideo::VBoxVideoFindAdapter: VbglInit returned 0x%x\n", rc));

      /* Preinitialize the primary extension.
       */
      ((PDEVICE_EXTENSION)HwDeviceExtension)->pNext                   = NULL;
      ((PDEVICE_EXTENSION)HwDeviceExtension)->pPrimary                = (PDEVICE_EXTENSION)HwDeviceExtension;
      ((PDEVICE_EXTENSION)HwDeviceExtension)->iDevice                 = 0;
      ((PDEVICE_EXTENSION)HwDeviceExtension)->ulFrameBufferOffset     = 0;
      ((PDEVICE_EXTENSION)HwDeviceExtension)->ulFrameBufferSize       = 0;
      ((PDEVICE_EXTENSION)HwDeviceExtension)->u.primary.ulVbvaEnabled = 0;
      VBoxVideoCmnMemZero(&((PDEVICE_EXTENSION)HwDeviceExtension)->areaDisplay, sizeof(HGSMIAREA));
      /* Guest supports only HGSMI, the old VBVA via VMMDev is not supported. Old
       * code will be ifdef'ed and later removed.
       * The host will however support both old and new interface to keep compatibility
       * with old guest additions.
       */
      VBoxSetupDisplaysHGSMI(commonFromDeviceExt((PDEVICE_EXTENSION)HwDeviceExtension),
                             AdapterMemorySize, 0);

      if (commonFromDeviceExt((PDEVICE_EXTENSION)HwDeviceExtension)->bHGSMI)
      {
          LogRel(("VBoxVideo: using HGSMI\n"));
          VBoxCreateDisplaysXPDM((PDEVICE_EXTENSION)HwDeviceExtension, ConfigInfo);
      }

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
    dprintf(("VBoxVideo::VBoxVideoInitialize\n"));

    PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)HwDeviceExtension;
    USHORT DispiId;

    /* Initialize the request pointer. */
    pDevExt->u.primary.pvReqFlush = NULL;

    /* Check if the chip restricts horizontal resolution or not. */
    VideoPortWritePortUshort((PUSHORT)VBE_DISPI_IOPORT_INDEX, VBE_DISPI_INDEX_ID);
    VideoPortWritePortUshort((PUSHORT)VBE_DISPI_IOPORT_DATA, VBE_DISPI_ID_ANYX);
    DispiId = VideoPortReadPortUshort((PUSHORT)VBE_DISPI_IOPORT_DATA);
    if (DispiId == VBE_DISPI_ID_ANYX)
        pDevExt->fAnyX = TRUE;
    else
        pDevExt->fAnyX = FALSE;

    vboxVideoInitCustomVideoModes(pDevExt);

   return TRUE;
}

# ifdef VBOX_WITH_VIDEOHWACCEL

static VOID VBoxVideoHGSMIDpc(
    IN PVOID  HwDeviceExtension,
    IN PVOID  Context
    )
{
    PDEVICE_EXTENSION PrimaryExtension = (PDEVICE_EXTENSION)HwDeviceExtension;

    hgsmiProcessHostCommandQueue(commonFromDeviceExt(PrimaryExtension));
}

BOOLEAN VBoxVideoInterrupt(PVOID  HwDeviceExtension)
{
    PDEVICE_EXTENSION devExt = (PDEVICE_EXTENSION)HwDeviceExtension;
    PDEVICE_EXTENSION PrimaryExtension = devExt->pPrimary;
    if (PrimaryExtension)
    {
        if (commonFromDeviceExt(PrimaryExtension)->pHostFlags) /* If HGSMI is enabled at all. */
        {
            uint32_t flags = commonFromDeviceExt(PrimaryExtension)->pHostFlags->u32HostFlags;
            if((flags & HGSMIHOSTFLAGS_IRQ) != 0)
            {
                if((flags & HGSMIHOSTFLAGS_COMMANDS_PENDING) != 0)
                {
                    /* schedule a DPC*/
                    BOOLEAN bResult = PrimaryExtension->u.primary.VideoPortProcs.pfnQueueDpc(PrimaryExtension, VBoxVideoHGSMIDpc, NULL);
                    Assert(bResult);
                }
                /* clear the IRQ */
                HGSMIClearIrq (commonFromDeviceExt(PrimaryExtension));
                return TRUE;
            }
        }
    }
    return FALSE;
}
# endif /* #ifdef VBOX_WITH_VIDEOHWACCEL */
#endif  /* #ifndef VBOX_WITH_WDDM */
/**
 * Send a request to the host to make the absolute pointer visible
 */
static BOOLEAN ShowPointer(PVOID HwDeviceExtension)
{
    BOOLEAN Result = TRUE;

    /* Use primary device extension, because the show pointer request should be processed
     * in vboxUpdatePointerShape regardless of the device. */
#ifndef VBOX_WITH_WDDM
    PDEVICE_EXTENSION PrimaryExtension = ((PDEVICE_EXTENSION)HwDeviceExtension)->pPrimary;
#else
    PDEVICE_EXTENSION PrimaryExtension = (PDEVICE_EXTENSION)HwDeviceExtension;
#endif

    if (DEV_MOUSE_HIDDEN(PrimaryExtension))
    {
        // tell the host to use the guest's pointer
        VIDEO_POINTER_ATTRIBUTES PointerAttributes;

        /* Visible and No Shape means Show the pointer.
         * It is enough to init only this field.
         */
        PointerAttributes.Enable = VBOX_MOUSE_POINTER_VISIBLE;

        Result = vboxUpdatePointerShape(commonFromDeviceExt(PrimaryExtension), &PointerAttributes, sizeof (PointerAttributes));

        if (Result)
            DEV_SET_MOUSE_SHOWN(PrimaryExtension);
        else
            dprintf(("VBoxVideo::ShowPointer: Could not show the hardware pointer -> fallback\n"));
    }
    return Result;
}

#ifndef VBOX_WITH_WDDM
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

//    dprintf(("VBoxVideo::VBoxVideoStartIO: Code %08X\n", RequestPacket->IoControlCode));

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

        case IOCTL_VIDEO_SHARE_VIDEO_MEMORY:
        {
            PVIDEO_SHARE_MEMORY pShareMemory;
            PVIDEO_SHARE_MEMORY_INFORMATION pShareMemoryInformation;
            PHYSICAL_ADDRESS shareAddress;
            PVOID virtualAddress = NULL;
            ULONG sharedViewSize;
            ULONG inIoSpace = 0;
            VP_STATUS status;

            dprintf(("VBoxVideo::VBoxVideoStartIO: IOCTL_VIDEO_SHARE_VIDEO_MEMORY\n"));

            if (    (RequestPacket->OutputBufferLength < sizeof(VIDEO_SHARE_MEMORY_INFORMATION))
                ||  (RequestPacket->InputBufferLength < sizeof(VIDEO_MEMORY)) ) {

                dprintf(("VBoxVideo::VBoxVideoStartIO: IOCTL_VIDEO_SHARE_VIDEO_MEMORY: ERROR_INSUFFICIENT_BUFFER\n"));
                RequestPacket->StatusBlock->Status = ERROR_INSUFFICIENT_BUFFER;
                Result = FALSE;
                break;
            }

            pShareMemory = (PVIDEO_SHARE_MEMORY)RequestPacket->InputBuffer;

            if (    (pShareMemory->ViewOffset > pDevExt->pPrimary->u.primary.ulMaxFrameBufferSize)
                ||  ((pShareMemory->ViewOffset + pShareMemory->ViewSize) > pDevExt->pPrimary->u.primary.ulMaxFrameBufferSize) ) {

                dprintf(("VBoxVideo::VBoxVideoStartIO: IOCTL_VIDEO_SHARE_VIDEO_MEMORY - ERROR_INVALID_PARAMETER %x:%x size %x\n", pShareMemory->ViewOffset, pShareMemory->ViewSize, pDevExt->pPrimary->u.primary.ulMaxFrameBufferSize));
                RequestPacket->StatusBlock->Status = ERROR_INVALID_PARAMETER;
                Result = FALSE;
                break;
            }

            RequestPacket->StatusBlock->Information = sizeof(VIDEO_SHARE_MEMORY_INFORMATION);

            virtualAddress = pShareMemory->ProcessHandle;
            sharedViewSize = pShareMemory->ViewSize;

            shareAddress.QuadPart = VBE_DISPI_LFB_PHYSICAL_ADDRESS + pDevExt->ulFrameBufferOffset;

            status = VideoPortMapMemory(HwDeviceExtension, shareAddress, &sharedViewSize, &inIoSpace, &virtualAddress);
            if (status != NO_ERROR)
                dprintf(("VBoxVideo::VBoxVideoStartIO: VideoPortMapMemory failed with %x\n", status));
            Result = (status == NO_ERROR);

            pShareMemoryInformation = (PVIDEO_SHARE_MEMORY_INFORMATION)RequestPacket->OutputBuffer;
            pShareMemoryInformation->SharedViewOffset = pShareMemory->ViewOffset;
            pShareMemoryInformation->VirtualAddress = virtualAddress;
            pShareMemoryInformation->SharedViewSize = sharedViewSize;
            break;
        }

        case IOCTL_VIDEO_UNSHARE_VIDEO_MEMORY:
        {
            PVIDEO_SHARE_MEMORY pShareMemory;
            VP_STATUS status;

            dprintf(("VBoxVideo::VBoxVideoStartIO: IOCTL_VIDEO_UNSHARE_VIDEO_MEMORY\n"));

            if (RequestPacket->InputBufferLength < sizeof(VIDEO_SHARE_MEMORY))
            {
                dprintf(("VBoxVideo::VBoxVideoStartIO: IOCTL_VIDEO_UNSHARE_VIDEO_MEMORY: ERROR_INSUFFICIENT_BUFFER\n"));
                RequestPacket->StatusBlock->Status = ERROR_INSUFFICIENT_BUFFER;
                Result = FALSE;
                break;
            }

            pShareMemory = (PVIDEO_SHARE_MEMORY)RequestPacket->InputBuffer;

            status = VideoPortUnmapMemory(HwDeviceExtension, pShareMemory->RequestedVirtualAddress, pShareMemory->ProcessHandle);
            if (status != NO_ERROR)
                dprintf(("VBoxVideo::VBoxVideoStartIO: VideoPortUnmapMemory failed with %x\n", status));
            Result = (status == NO_ERROR);
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
            /// @todo this is now obsolete - remove it?
            if (vboxQueryHostWantsAbsolute())
                Result = ShowPointer(HwDeviceExtension);
            else
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

                Result = vboxUpdatePointerShape(commonFromDeviceExt((PDEVICE_EXTENSION)HwDeviceExtension), &PointerAttributes, sizeof (PointerAttributes));

                if (Result)
                    DEV_SET_MOUSE_HIDDEN((PDEVICE_EXTENSION)HwDeviceExtension);
                else
                    dprintf(("VBoxVideo::VBoxVideoStartIO: Could not hide hardware pointer -> fallback\n"));
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
                Result = vboxUpdatePointerShape(commonFromDeviceExt((PDEVICE_EXTENSION)HwDeviceExtension), pPointerAttributes, RequestPacket->InputBufferLength);
                if (!Result)
                    dprintf(("VBoxVideo::VBoxVideoStartIO: Could not set hardware pointer -> fallback\n"));
            } else
            {
                dprintf(("VBoxVideo::VBoxVideoStartIO: Fallback to software pointer\n"));
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
            // find out whether the host wants absolute positioning
            /// @todo this is now obsolete - remove it?
            if (vboxQueryHostWantsAbsolute())
                Result = ShowPointer(HwDeviceExtension);
            else
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
                dprintf(("VBoxVideo::VBoxVideoStartIO: Output buffer too small!\n"));
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
                dprintf(("VBoxVideo::VBoxVideoStartIO: Output buffer too small!\n"));
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
            dprintf(("VBoxVideo::VBoxVideoStartIO: IOCTL_VIDEO_SWITCH_DUALVIEW[%d] (%ld)\n", pDevExt->iDevice, ulAttach));

            if (pDevExt->iDevice > 0)
            {
                pDevExt->u.secondary.bEnabled = (BOOLEAN)ulAttach;
            }
            Result = TRUE;
            break;
        }

        case IOCTL_VIDEO_INTERPRET_DISPLAY_MEMORY:
        {
            dprintf(("VBoxVideo::VBoxVideoStartIO: IOCTL_VIDEO_INTERPRET_DISPLAY_MEMORY\n"));
            /* Pre-HGSMI IOCTL */
            RequestPacket->StatusBlock->Status = ERROR_INVALID_FUNCTION;
            Result = false;
            break;
        }


        case IOCTL_VIDEO_VBVA_ENABLE:
        {
            int rc;
            ULONG ulEnable;

            dprintf(("VBoxVideo::VBoxVideoStartIO: IOCTL_VIDEO_VBVA_ENABLE\n"));

            if (RequestPacket->InputBufferLength < sizeof(ULONG))
            {
                dprintf(("VBoxVideo::VBoxVideoStartIO: Input buffer too small: %d needed: %d!!!\n",
                         RequestPacket->InputBufferLength, sizeof(ULONG)));
                RequestPacket->StatusBlock->Status = ERROR_INSUFFICIENT_BUFFER;
                return FALSE;
            }

            if (RequestPacket->OutputBufferLength < sizeof(VBVAENABLERESULT))
            {
                dprintf(("VBoxVideo::VBoxVideoStartIO: Output buffer too small: %d needed: %d!!!\n",
                         RequestPacket->OutputBufferLength, sizeof(VBVAENABLERESULT)));
                RequestPacket->StatusBlock->Status = ERROR_INSUFFICIENT_BUFFER;
                return FALSE;
            }

            ulEnable = *(ULONG *)RequestPacket->InputBuffer;
            dprintf(("VBoxVideo::VBoxVideoStartIO: IOCTL_VIDEO_VBVA_ENABLE ulEnable = %08X\n", ulEnable));

            rc = vboxVbvaEnable (pDevExt, ulEnable, (VBVAENABLERESULT *)RequestPacket->OutputBuffer);
            dprintf(("VBoxVideo::VBoxVideoStartIO: IOCTL_VIDEO_VBVA_ENABLE completed rc = %Rrc\n", rc));

            if (RT_FAILURE (rc))
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
            uint32_t cRect = RequestPacket->InputBufferLength/sizeof(RTRECT);
            int      rc;

            dprintf(("IOCTL_VIDEO_VBOX_SETVISIBLEREGION cRect=%d\n", cRect));
            if (    RequestPacket->InputBufferLength < sizeof(RTRECT)
                ||  RequestPacket->InputBufferLength != cRect*sizeof(RTRECT))
            {
                dprintf(("VBoxVideo::IOCTL_VIDEO_VBOX_SETVISIBLEREGION: Output buffer too small: %d needed: %d!!!\n",
                         RequestPacket->OutputBufferLength, sizeof(RTRECT)));
                RequestPacket->StatusBlock->Status = ERROR_INSUFFICIENT_BUFFER;
                return FALSE;
            }
            /*
             * Inform the host about the visible region
             */
            VMMDevVideoSetVisibleRegion *req = NULL;

            rc = VbglGRAlloc ((VMMDevRequestHeader **)&req,
                              sizeof (VMMDevVideoSetVisibleRegion) + (cRect-1)*sizeof(RTRECT),
                              VMMDevReq_VideoSetVisibleRegion);

            if (RT_SUCCESS(rc))
            {
                req->cRect = cRect;
                memcpy(&req->Rect, RequestPacket->InputBuffer, cRect*sizeof(RTRECT));

                rc = VbglGRPerform (&req->header);

                if (RT_SUCCESS(rc))
                {
                    Result = TRUE;
                    break;
                }
            }

            dprintf(("VBoxVideo::VBoxVideoStartIO: Failed with rc=%x\n", rc));
            RequestPacket->StatusBlock->Status = ERROR_INVALID_FUNCTION;
            return FALSE;
        }

        case IOCTL_VIDEO_QUERY_HGSMI_INFO:
        {
            dprintf(("VBoxVideo::VBoxVideoStartIO: IOCTL_VIDEO_QUERY_HGSMI_INFO\n"));

            if (RequestPacket->OutputBufferLength < sizeof(QUERYHGSMIRESULT))
            {
                dprintf(("VBoxVideo::VBoxVideoStartIO: Output buffer too small: %d needed: %d!!!\n",
                         RequestPacket->OutputBufferLength, sizeof(QUERYHGSMIRESULT)));
                RequestPacket->StatusBlock->Status = ERROR_INSUFFICIENT_BUFFER;
                return FALSE;
            }

            if (!commonFromDeviceExt(pDevExt)->bHGSMI)
            {
                RequestPacket->StatusBlock->Status = ERROR_INVALID_FUNCTION;
                return FALSE;
            }

            QUERYHGSMIRESULT *pInfo = (QUERYHGSMIRESULT *)RequestPacket->OutputBuffer;

            pInfo->iDevice = pDevExt->iDevice;
            pInfo->ulFlags = 0;

            /* Describes VRAM chunk for this display device. */
            pInfo->areaDisplay = pDevExt->areaDisplay;

            pInfo->u32DisplayInfoSize   = VBVA_DISPLAY_INFORMATION_SIZE;
            pInfo->u32MinVBVABufferSize = VBVA_MIN_BUFFER_SIZE;

            pInfo->IOPortGuestCommand = commonFromDeviceExt(pDevExt)->IOPortGuest;

            RequestPacket->StatusBlock->Information = sizeof(QUERYHGSMIRESULT);
            Result = TRUE;

            break;
        }
        case IOCTL_VIDEO_HGSMI_QUERY_CALLBACKS:
        {
            dprintf(("VBoxVideo::VBoxVideoStartIO: IOCTL_VIDEO_HGSMI_QUERY_CALLBACKS\n"));

            if (RequestPacket->OutputBufferLength < sizeof(HGSMIQUERYCALLBACKS))
            {
                dprintf(("VBoxVideo::VBoxVideoStartIO: Output buffer too small: %d needed: %d!!!\n",
                         RequestPacket->OutputBufferLength, sizeof(HGSMIQUERYCALLBACKS)));
                RequestPacket->StatusBlock->Status = ERROR_INSUFFICIENT_BUFFER;
                return FALSE;
            }

            if (!commonFromDeviceExt(pDevExt)->bHGSMI)
            {
                RequestPacket->StatusBlock->Status = ERROR_INVALID_FUNCTION;
                return FALSE;
            }

            HGSMIQUERYCALLBACKS *pInfo = (HGSMIQUERYCALLBACKS *)RequestPacket->OutputBuffer;

            pInfo->hContext = pDevExt;
            pInfo->pfnCompletionHandler = hgsmiHostCmdComplete;
            pInfo->pfnRequestCommandsHandler = hgsmiHostCmdRequest;

            RequestPacket->StatusBlock->Information = sizeof(HGSMIQUERYCALLBACKS);
            Result = TRUE;
            break;
        }
        case IOCTL_VIDEO_HGSMI_QUERY_PORTPROCS:
        {
            dprintf(("VBoxVideo::VBoxVideoStartIO: IOCTL_VIDEO_HGSMI_QUERY_PORTPROCS\n"));

            if (RequestPacket->OutputBufferLength < sizeof(HGSMIQUERYCPORTPROCS))
            {
                dprintf(("VBoxVideo::VBoxVideoStartIO: Output buffer too small: %d needed: %d!!!\n",
                         RequestPacket->OutputBufferLength, sizeof(HGSMIQUERYCPORTPROCS)));
                RequestPacket->StatusBlock->Status = ERROR_INSUFFICIENT_BUFFER;
                return FALSE;
            }

            if (!commonFromDeviceExt(pDevExt)->bHGSMI)
            {
                RequestPacket->StatusBlock->Status = ERROR_INVALID_FUNCTION;
                return FALSE;
            }

            HGSMIQUERYCPORTPROCS *pInfo = (HGSMIQUERYCPORTPROCS *)RequestPacket->OutputBuffer;
            pInfo->pContext = pDevExt->pPrimary;
            pInfo->VideoPortProcs = pDevExt->pPrimary->u.primary.VideoPortProcs;

            RequestPacket->StatusBlock->Information = sizeof(HGSMIQUERYCPORTPROCS);
            Result = TRUE;
            break;
        }
        case IOCTL_VIDEO_HGSMI_HANDLER_ENABLE:
        {
            dprintf(("VBoxVideo::VBoxVideoStartIO: IOCTL_VIDEO_HGSMI_HANDLER_ENABLE\n"));

            if (RequestPacket->InputBufferLength< sizeof(HGSMIHANDLERENABLE))
            {
                dprintf(("VBoxVideo::VBoxVideoStartIO: Output buffer too small: %d needed: %d!!!\n",
                         RequestPacket->InputBufferLength, sizeof(HGSMIHANDLERENABLE)));
                RequestPacket->StatusBlock->Status = ERROR_INSUFFICIENT_BUFFER;
                return FALSE;
            }

            if (!commonFromDeviceExt(pDevExt)->bHGSMI)
            {
                RequestPacket->StatusBlock->Status = ERROR_INVALID_FUNCTION;
                return FALSE;
            }

            HGSMIHANDLERENABLE *pInfo = (HGSMIHANDLERENABLE *)RequestPacket->InputBuffer;

            int rc = vboxVBVAChannelDisplayEnable(pDevExt->pPrimary,
                    pDevExt->iDevice,
                    pInfo->u8Channel);
            if(RT_FAILURE(rc))
            {
                RequestPacket->StatusBlock->Status = ERROR_INVALID_NAME;
            }
            Result = TRUE;
            break;
        }
        case IOCTL_VIDEO_HGSMI_HANDLER_DISABLE:
        {
            /* TODO: implement */
            if (!commonFromDeviceExt(pDevExt)->bHGSMI)
            {
                RequestPacket->StatusBlock->Status = ERROR_INVALID_FUNCTION;
                return FALSE;
            }
            break;
        }
# ifdef VBOX_WITH_VIDEOHWACCEL
        case IOCTL_VIDEO_VHWA_QUERY_INFO:
        {
            if (RequestPacket->OutputBufferLength < sizeof (VHWAQUERYINFO))
            {
                dprintf(("VBoxVideo::VBoxVideoStartIO: Output buffer too small: %d needed: %d!!!\n",
                         RequestPacket->OutputBufferLength, sizeof(VHWAQUERYINFO)));
                RequestPacket->StatusBlock->Status = ERROR_INSUFFICIENT_BUFFER;
                return FALSE;
            }

            if (!commonFromDeviceExt(pDevExt)->bHGSMI)
            {
                RequestPacket->StatusBlock->Status = ERROR_INVALID_FUNCTION;
                return FALSE;
            }

            VHWAQUERYINFO *pInfo = (VHWAQUERYINFO *)RequestPacket->OutputBuffer;
            pInfo->offVramBase = (ULONG_PTR)pDevExt->ulFrameBufferOffset;
            RequestPacket->StatusBlock->Information = sizeof (VHWAQUERYINFO);
            Result = TRUE;
            break;
        }
# endif
        default:
            dprintf(("VBoxVideo::VBoxVideoStartIO: Unsupported %p, fn %d(0x%x)\n",
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

//    dprintf(("VBoxVideo::VBoxVideoStartIO: Completed\n"));

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

    if (pDevExt->u.primary.pvReqFlush != NULL)
    {
        VbglGRFree ((VMMDevRequestHeader *)pDevExt->u.primary.pvReqFlush);
        pDevExt->u.primary.pvReqFlush = NULL;
    }

    VbglTerminate ();

    VBoxFreeDisplaysHGSMI(commonFromDeviceExt(pDevExt));
    /** @note using this callback instead of doing things manually adds an
     *        additional call to HGSMIHeapDestroy().  I assume that call was
     *        merely forgotton in the first place. */

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
#endif /* #ifndef VBOX_WITH_WDDM */

/**
 * VBoxVideoSetGraphicsCap
 *
 * Tells the host whether or not we currently support graphics in the
 * additions
 */
BOOLEAN FASTCALL VBoxVideoSetGraphicsCap(BOOLEAN isEnabled)
{
    VMMDevReqGuestCapabilities2 *req = NULL;
    int rc;

    rc = VbglGRAlloc ((VMMDevRequestHeader **)&req,
                      sizeof (VMMDevReqGuestCapabilities2),
                      VMMDevReq_SetGuestCapabilities);

    if (!RT_SUCCESS(rc))
        dprintf(("VBoxVideoSetGraphicsCap: failed to allocate a request, rc=%Rrc\n", rc));
    else
    {
        req->u32OrMask = isEnabled ? VMMDEV_GUEST_SUPPORTS_GRAPHICS : 0;
        req->u32NotMask = isEnabled ? 0 : VMMDEV_GUEST_SUPPORTS_GRAPHICS;

        rc = VbglGRPerform (&req->header);
        if (RT_FAILURE(rc))
            dprintf(("VBoxVideoSetGraphicsCap: request failed, rc = %Rrc\n", rc));
    }
    if (req != NULL)
        VbglGRFree (&req->header);
    return RT_SUCCESS(rc);
}


BOOLEAN FASTCALL VBoxVideoSetCurrentModePerform(PDEVICE_EXTENSION DeviceExtension,
        USHORT width, USHORT height, USHORT bpp
#ifdef VBOX_WITH_WDDM
        , ULONG offDisplay
#endif
        )
{
    /* set the mode characteristics */
    VBoxVideoCmnPortWriteUshort(VBE_DISPI_IOPORT_INDEX, VBE_DISPI_INDEX_XRES);
    VBoxVideoCmnPortWriteUshort(VBE_DISPI_IOPORT_DATA, width);
    VBoxVideoCmnPortWriteUshort(VBE_DISPI_IOPORT_INDEX, VBE_DISPI_INDEX_YRES);
    VBoxVideoCmnPortWriteUshort(VBE_DISPI_IOPORT_DATA, height);
    VBoxVideoCmnPortWriteUshort(VBE_DISPI_IOPORT_INDEX, VBE_DISPI_INDEX_BPP);
    VBoxVideoCmnPortWriteUshort(VBE_DISPI_IOPORT_DATA, bpp);
    /* enable the mode */
    VBoxVideoCmnPortWriteUshort(VBE_DISPI_IOPORT_INDEX, VBE_DISPI_INDEX_ENABLE);
    VBoxVideoCmnPortWriteUshort(VBE_DISPI_IOPORT_DATA, VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED);
#ifdef VBOX_WITH_WDDM
    /* encode linear offDisplay to xOffset & yOffset to ensure offset fits USHORT */
    ULONG cbLine = VBOXWDDM_ROUNDBOUND(((width * bpp) + 7) / 8, 4);
    ULONG xOffset = offDisplay % cbLine;
    if (bpp == 4)
    {
        xOffset <<= 1;
    }
    else
    {
        Assert(!(xOffset%((bpp + 7) >> 3)));
        xOffset /= ((bpp + 7) >> 3);
    }
    ULONG yOffset = offDisplay / cbLine;
    Assert(xOffset <= 0xffff);
    Assert(yOffset <= 0xffff);
    VBoxVideoCmnPortWriteUshort(VBE_DISPI_IOPORT_INDEX, VBE_DISPI_INDEX_X_OFFSET);
    VBoxVideoCmnPortWriteUshort(VBE_DISPI_IOPORT_DATA, (USHORT)xOffset);
    VBoxVideoCmnPortWriteUshort(VBE_DISPI_IOPORT_INDEX, VBE_DISPI_INDEX_Y_OFFSET);
    VBoxVideoCmnPortWriteUshort(VBE_DISPI_IOPORT_DATA, (USHORT)yOffset);
#endif
    /** @todo read from the port to see if the mode switch was successful */

    /* Tell the host that we now support graphics in the additions.
     * @todo: Keep old behaviour, because VBoxVideoResetDevice is called on every graphics
     *        mode switch and causes an OFF/ON sequence which is not handled by frontends
     *        (for example Qt GUI debug build asserts when seamless is being enabled).
     */
    // VBoxVideoSetGraphicsCap(TRUE);

    return TRUE;
}

#ifndef VBOX_WITH_WDDM

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

    DeviceExtension->CurrentModeWidth  = ModeInfo->VisScreenWidth;
    DeviceExtension->CurrentModeHeight = ModeInfo->VisScreenHeight;
    DeviceExtension->CurrentModeBPP    = ModeInfo->BitsPerPlane;

    if (DeviceExtension->iDevice > 0)
    {
        dprintf(("VBoxVideo::VBoxVideoSetCurrentMode: Skipping for non-primary display %d\n",
                 DeviceExtension->iDevice));
        return TRUE;
    }

    return VBoxVideoSetCurrentModePerform(DeviceExtension,
            (USHORT)ModeInfo->VisScreenWidth,
                    (USHORT)ModeInfo->VisScreenHeight,
                    (USHORT)ModeInfo->BitsPerPlane);
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

    /* Tell the host that we no longer support graphics in the additions
     * @todo: Keep old behaviour, see similar comment in VBoxVideoSetCurrentMode for details.
     */
    // VBoxVideoSetGraphicsCap(FALSE);
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

    dprintf(("VBoxVideo::VBoxVideoMapVideoMemory: fb offset 0x%x\n", DeviceExtension->ulFrameBufferOffset));

    FrameBuffer.QuadPart = VBE_DISPI_LFB_PHYSICAL_ADDRESS + DeviceExtension->ulFrameBufferOffset;

    MapInformation->VideoRamBase = RequestedAddress->RequestedVirtualAddress;
    MapInformation->VideoRamLength = DeviceExtension->pPrimary->u.primary.ulMaxFrameBufferSize;

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

        /* Save the new framebuffer size */
        DeviceExtension->ulFrameBufferSize = MapInformation->FrameBufferLength;
        HGSMIAreaInitialize (&DeviceExtension->areaDisplay,
                             MapInformation->FrameBufferBase,
                             MapInformation->FrameBufferLength,
                             DeviceExtension->ulFrameBufferOffset);
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
    HGSMIAreaClear (&DeviceExtension->areaDisplay);
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
#endif /* ifndef VBOX_WITH_WDDM */
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
      VBoxVideoCmnPortWriteUchar(0x03c8, (UCHAR)Entry);
      VBoxVideoCmnPortWriteUchar(0x03c9, ColorLookUpTable->LookupTable[Entry].RgbArray.Red);
      VBoxVideoCmnPortWriteUchar(0x03c9, ColorLookUpTable->LookupTable[Entry].RgbArray.Green);
      VBoxVideoCmnPortWriteUchar(0x03c9, ColorLookUpTable->LookupTable[Entry].RgbArray.Blue);
   }

   return TRUE;
}

#ifndef VBOX_WITH_WDDM

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
        if ((int)ChildEnumInfo->ChildIndex <= commonFromDeviceExt(pDevExt)->cDisplays)
        {
            *VideoChildType = Monitor;
            *pUId = ChildEnumInfo->ChildIndex;

            return VIDEO_ENUM_MORE_DEVICES;
        }
    }

    return ERROR_NO_MORE_DEVICES;
}


#ifndef VBOX_WITH_WDDM
VP_STATUS vboxWaitForSingleObjectVoid(IN PVOID  HwDeviceExtension, IN PVOID  Object, IN PLARGE_INTEGER  Timeout  OPTIONAL)
{
    return ERROR_INVALID_FUNCTION;
}

LONG vboxSetEventVoid(IN PVOID  HwDeviceExtension, IN PEVENT  pEvent)
{
    return 0;
}

VOID vboxClearEventVoid (IN PVOID  HwDeviceExtension, IN PEVENT  pEvent)
{
}

VP_STATUS vboxCreateEventVoid(IN PVOID  HwDeviceExtension, IN ULONG  EventFlag, IN PVOID  Unused, OUT PEVENT  *ppEvent)
{
    return ERROR_INVALID_FUNCTION;
}

VP_STATUS vboxDeleteEventVoid(IN PVOID  HwDeviceExtension, IN PEVENT  pEvent)
{
    return ERROR_INVALID_FUNCTION;
}

VP_STATUS vboxCreateSpinLockVoid (IN PVOID  HwDeviceExtension, OUT PSPIN_LOCK  *SpinLock)
{
    return ERROR_INVALID_FUNCTION;
}

VP_STATUS vboxDeleteSpinLockVoid (IN PVOID  HwDeviceExtension, IN PSPIN_LOCK  SpinLock)
{
    return ERROR_INVALID_FUNCTION;
}

VOID vboxAcquireSpinLockVoid (IN PVOID  HwDeviceExtension, IN PSPIN_LOCK  SpinLock, OUT PUCHAR  OldIrql)
{
}

VOID vboxReleaseSpinLockVoid (IN PVOID  HwDeviceExtension, IN PSPIN_LOCK  SpinLock, IN UCHAR  NewIrql)
{
}

VOID vboxAcquireSpinLockAtDpcLevelVoid (IN PVOID  HwDeviceExtension, IN PSPIN_LOCK  SpinLock)
{
}

VOID vboxReleaseSpinLockFromDpcLevelVoid (IN PVOID  HwDeviceExtension, IN PSPIN_LOCK  SpinLock)
{
}

PVOID vboxAllocatePoolVoid(IN PVOID  HwDeviceExtension, IN VBOXVP_POOL_TYPE  PoolType, IN size_t  NumberOfBytes, IN ULONG  Tag)
{
    return NULL;
}

VOID vboxFreePoolVoid(IN PVOID  HwDeviceExtension, IN PVOID  Ptr)
{
}

BOOLEAN vboxQueueDpcVoid(IN PVOID  HwDeviceExtension, IN PMINIPORT_DPC_ROUTINE  CallbackRoutine, IN PVOID  Context)
{
    return FALSE;
}

void VBoxSetupVideoPortFunctions(PDEVICE_EXTENSION PrimaryExtension, VBOXVIDEOPORTPROCS *pCallbacks, PVIDEO_PORT_CONFIG_INFO pConfigInfo)
{
    memset(pCallbacks, 0, sizeof(VBOXVIDEOPORTPROCS));

    if (vboxQueryWinVersion() <= WINNT4)
    {
        /* VideoPortGetProcAddress is available for >= win2k */
        pCallbacks->pfnWaitForSingleObject = vboxWaitForSingleObjectVoid;
        pCallbacks->pfnSetEvent = vboxSetEventVoid;
        pCallbacks->pfnClearEvent = vboxClearEventVoid;
        pCallbacks->pfnCreateEvent = vboxCreateEventVoid;
        pCallbacks->pfnDeleteEvent = vboxDeleteEventVoid;
        pCallbacks->pfnCreateSpinLock = vboxCreateSpinLockVoid;
        pCallbacks->pfnDeleteSpinLock = vboxDeleteSpinLockVoid;
        pCallbacks->pfnAcquireSpinLock = vboxAcquireSpinLockVoid;
        pCallbacks->pfnReleaseSpinLock = vboxReleaseSpinLockVoid;
        pCallbacks->pfnAcquireSpinLockAtDpcLevel = vboxAcquireSpinLockAtDpcLevelVoid;
        pCallbacks->pfnReleaseSpinLockFromDpcLevel = vboxReleaseSpinLockFromDpcLevelVoid;
        pCallbacks->pfnAllocatePool = vboxAllocatePoolVoid;
        pCallbacks->pfnFreePool = vboxFreePoolVoid;
        pCallbacks->pfnQueueDpc = vboxQueueDpcVoid;
        return;
    }

    pCallbacks->pfnWaitForSingleObject = (PFNWAITFORSINGLEOBJECT)(pConfigInfo->VideoPortGetProcAddress)
            (PrimaryExtension,
             (PUCHAR)"VideoPortWaitForSingleObject");
    Assert(pCallbacks->pfnWaitForSingleObject);

    pCallbacks->pfnSetEvent = (PFNSETEVENT)(pConfigInfo->VideoPortGetProcAddress)
            (PrimaryExtension,
             (PUCHAR)"VideoPortSetEvent");
    Assert(pCallbacks->pfnSetEvent);

    pCallbacks->pfnClearEvent = (PFNCLEAREVENT)(pConfigInfo->VideoPortGetProcAddress)
            (PrimaryExtension,
             (PUCHAR)"VideoPortClearEvent");
    Assert(pCallbacks->pfnClearEvent);

    pCallbacks->pfnCreateEvent = (PFNCREATEEVENT)(pConfigInfo->VideoPortGetProcAddress)
            (PrimaryExtension,
             (PUCHAR)"VideoPortCreateEvent");
    Assert(pCallbacks->pfnCreateEvent);

    pCallbacks->pfnDeleteEvent = (PFNDELETEEVENT)(pConfigInfo->VideoPortGetProcAddress)
            (PrimaryExtension,
             (PUCHAR)"VideoPortDeleteEvent");
    Assert(pCallbacks->pfnDeleteEvent);

    if(pCallbacks->pfnWaitForSingleObject
            && pCallbacks->pfnSetEvent
            && pCallbacks->pfnClearEvent
            && pCallbacks->pfnCreateEvent
            && pCallbacks->pfnDeleteEvent)
    {
        pCallbacks->fSupportedTypes |= VBOXVIDEOPORTPROCS_EVENT;
    }
    else
    {
        pCallbacks->pfnWaitForSingleObject = vboxWaitForSingleObjectVoid;
        pCallbacks->pfnSetEvent = vboxSetEventVoid;
        pCallbacks->pfnClearEvent = vboxClearEventVoid;
        pCallbacks->pfnCreateEvent = vboxCreateEventVoid;
        pCallbacks->pfnDeleteEvent = vboxDeleteEventVoid;
    }

    pCallbacks->pfnCreateSpinLock = (PFNCREATESPINLOCK)(pConfigInfo->VideoPortGetProcAddress)
            (PrimaryExtension,
             (PUCHAR)"VideoPortCreateSpinLock");
    Assert(pCallbacks->pfnCreateSpinLock);

    pCallbacks->pfnDeleteSpinLock = (PFNDELETESPINLOCK)(pConfigInfo->VideoPortGetProcAddress)
            (PrimaryExtension,
             (PUCHAR)"VideoPortDeleteSpinLock");
    Assert(pCallbacks->pfnDeleteSpinLock);

    pCallbacks->pfnAcquireSpinLock = (PFNACQUIRESPINLOCK)(pConfigInfo->VideoPortGetProcAddress)
            (PrimaryExtension,
             (PUCHAR)"VideoPortAcquireSpinLock");
    Assert(pCallbacks->pfnAcquireSpinLock);

    pCallbacks->pfnReleaseSpinLock = (PFNRELEASESPINLOCK)(pConfigInfo->VideoPortGetProcAddress)
            (PrimaryExtension,
             (PUCHAR)"VideoPortReleaseSpinLock");
    Assert(pCallbacks->pfnReleaseSpinLock);

    pCallbacks->pfnAcquireSpinLockAtDpcLevel = (PFNACQUIRESPINLOCKATDPCLEVEL)(pConfigInfo->VideoPortGetProcAddress)
            (PrimaryExtension,
             (PUCHAR)"VideoPortAcquireSpinLockAtDpcLevel");
    Assert(pCallbacks->pfnAcquireSpinLockAtDpcLevel);

    pCallbacks->pfnReleaseSpinLockFromDpcLevel = (PFNRELEASESPINLOCKFROMDPCLEVEL)(pConfigInfo->VideoPortGetProcAddress)
                (PrimaryExtension,
                 (PUCHAR)"VideoPortReleaseSpinLockFromDpcLevel");
    Assert(pCallbacks->pfnReleaseSpinLockFromDpcLevel);

    if(pCallbacks->pfnCreateSpinLock
            && pCallbacks->pfnDeleteSpinLock
            && pCallbacks->pfnAcquireSpinLock
            && pCallbacks->pfnReleaseSpinLock
            && pCallbacks->pfnAcquireSpinLockAtDpcLevel
            && pCallbacks->pfnReleaseSpinLockFromDpcLevel)
    {
        pCallbacks->fSupportedTypes |= VBOXVIDEOPORTPROCS_SPINLOCK;
    }
    else
    {
        pCallbacks->pfnCreateSpinLock = vboxCreateSpinLockVoid;
        pCallbacks->pfnDeleteSpinLock = vboxDeleteSpinLockVoid;
        pCallbacks->pfnAcquireSpinLock = vboxAcquireSpinLockVoid;
        pCallbacks->pfnReleaseSpinLock = vboxReleaseSpinLockVoid;
        pCallbacks->pfnAcquireSpinLockAtDpcLevel = vboxAcquireSpinLockAtDpcLevelVoid;
        pCallbacks->pfnReleaseSpinLockFromDpcLevel = vboxReleaseSpinLockFromDpcLevelVoid;
    }

    pCallbacks->pfnAllocatePool = (PFNALLOCATEPOOL)(pConfigInfo->VideoPortGetProcAddress)
            (PrimaryExtension,
             (PUCHAR)"VideoPortAllocatePool");
    Assert(pCallbacks->pfnAllocatePool);

    pCallbacks->pfnFreePool = (PFNFREEPOOL)(pConfigInfo->VideoPortGetProcAddress)
            (PrimaryExtension,
             (PUCHAR)"VideoPortFreePool");
    Assert(pCallbacks->pfnFreePool);

    if(pCallbacks->pfnAllocatePool
            && pCallbacks->pfnFreePool)
    {
        pCallbacks->fSupportedTypes |= VBOXVIDEOPORTPROCS_POOL;
    }
    else
    {
        pCallbacks->pfnAllocatePool = vboxAllocatePoolVoid;
        pCallbacks->pfnFreePool = vboxFreePoolVoid;
    }

    pCallbacks->pfnQueueDpc = (PFNQUEUEDPC)(pConfigInfo->VideoPortGetProcAddress)
            (PrimaryExtension,
             (PUCHAR)"VideoPortQueueDpc");
    Assert(pCallbacks->pfnQueueDpc);

    if(pCallbacks->pfnQueueDpc)
    {
        pCallbacks->fSupportedTypes |= VBOXVIDEOPORTPROCS_DPC;
    }
    else
    {
        pCallbacks->pfnQueueDpc = vboxQueueDpcVoid;
    }

#ifdef DEBUG_misha
    Assert(pCallbacks->fSupportedTypes & VBOXVIDEOPORTPROCS_EVENT);
    Assert(pCallbacks->fSupportedTypes & VBOXVIDEOPORTPROCS_SPINLOCK);
#endif
}
#endif


static DECLCALLBACK(void) vboxVbvaFlush (void *pvFlush)
{
    DEVICE_EXTENSION *pDevExt = (DEVICE_EXTENSION *)pvFlush;
    DEVICE_EXTENSION *pPrimaryDevExt = pDevExt? pDevExt->pPrimary: NULL;

    if (pPrimaryDevExt)
    {
        VMMDevVideoAccelFlush *req = (VMMDevVideoAccelFlush *)pPrimaryDevExt->u.primary.pvReqFlush;

        if (req)
        {
            int rc = VbglGRPerform (&req->header);

            if (RT_FAILURE(rc))
            {
                dprintf(("VBoxVideo::vbvaFlush: rc = %Rrc!\n", rc));
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
        DEVICE_EXTENSION *pPrimaryDevExt = pDevExt->pPrimary;

        dprintf(("VBoxVideo::vboxVbvaEnable: Skipping for non-primary display %d\n",
                 pDevExt->iDevice));

        if (   ulEnable
            && pPrimaryDevExt->u.primary.ulVbvaEnabled)
        {
            pVbvaResult->pVbvaMemory = &pVMMDevMemory->vbvaMemory;
            pVbvaResult->pfnFlush    = vboxVbvaFlush;
            pVbvaResult->pvFlush     = pDevExt;
        }
        else
        {
            VBoxVideoCmnMemZero(&pVbvaResult, sizeof(VBVAENABLERESULT));
        }

        return rc;
    }

    if (RT_SUCCESS(rc))
    {
        /* Allocate the memory block for VMMDevReq_VideoAccelFlush request. */
        if (pDevExt->u.primary.pvReqFlush == NULL)
        {
            VMMDevVideoAccelFlush *req = NULL;

            rc = VbglGRAlloc ((VMMDevRequestHeader **)&req,
                              sizeof (VMMDevVideoAccelFlush),
                              VMMDevReq_VideoAccelFlush);

            if (RT_SUCCESS (rc))
            {
                pDevExt->u.primary.pvReqFlush = req;
            }
            else
            {
                dprintf(("VBoxVideo::vboxVbvaEnable: VbglGRAlloc (VMMDevReq_VideoAccelFlush) rc = %Rrc!!!\n", rc));
            }
        }
    }
    else
    {
        dprintf(("VBoxVideo::vboxVbvaEnable: VbglQueryVMMDevMemory rc = %Rrc!!!\n", rc));
    }

    if (RT_SUCCESS(rc))
    {
        ULONG ulEnabled = 0;

        /*
         * Tell host that VBVA status is changed.
         */
        VMMDevVideoAccelEnable *req = NULL;

        rc = VbglGRAlloc ((VMMDevRequestHeader **)&req,
                          sizeof (VMMDevVideoAccelEnable),
                          VMMDevReq_VideoAccelEnable);

        if (RT_SUCCESS(rc))
        {
            req->u32Enable = ulEnable;
            req->cbRingBuffer = VBVA_RING_BUFFER_SIZE;
            req->fu32Status = 0;

            rc = VbglGRPerform (&req->header);

            if (RT_SUCCESS(rc))
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
                        VBoxVideoCmnMemZero(&pVbvaResult, sizeof(VBVAENABLERESULT));
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
                dprintf(("VBoxVideo::vboxVbvaEnable: rc = %Rrc!\n", rc));
            }

            VbglGRFree (&req->header);
        }
        else
        {
            dprintf(("VBoxVideo::vboxVbvaEnable: VbglGRAlloc rc = %Rrc!\n", rc));
        }

        pDevExt->pPrimary->u.primary.ulVbvaEnabled = ulEnabled;
    }

    return rc;
}
#endif
