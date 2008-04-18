/** @file
 *
 * vrdpvd.sys - VirtualBox Windows NT/2000/XP guest mirror video driver
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

#include <dderror.h>
#include <miniport.h>
#include <ntddvdeo.h>
#include <video.h>

#include "vrdpvd.h"

/*
 * Miniport stubs.
 */

static BOOLEAN
MirrorResetHW(
    PVOID HwDeviceExtension,
    ULONG Columns,
    ULONG Rows
    )
{
    dprintf(("vrdpvd.sys::ResetHW\n"));

    return TRUE;
}

static BOOLEAN
MirrorVidInterrupt(
    PVOID HwDeviceExtension
    )
{
    dprintf(("vrdpvd.sys::VidInterrupt\n"));

    return TRUE;
}

static VP_STATUS
MirrorGetPowerState(
    PVOID                   HwDeviceExtension,
    ULONG                   HwId,
    PVIDEO_POWER_MANAGEMENT VideoPowerControl
    )
{
    dprintf(("vrdpvd.sys::GetPowerState\n"));

    return NO_ERROR;
}

static VP_STATUS
MirrorSetPowerState(
    PVOID                   HwDeviceExtension,
    ULONG                   HwId,
    PVIDEO_POWER_MANAGEMENT VideoPowerControl
    )
{
    dprintf(("vrdpvd.sys::SetPowerState\n"));

    return NO_ERROR;
}

static ULONG
MirrorGetChildDescriptor (
    IN  PVOID                  HwDeviceExtension,
    IN  PVIDEO_CHILD_ENUM_INFO ChildEnumInfo,
    OUT PVIDEO_CHILD_TYPE      pChildType,
    OUT PVOID                  pChildDescriptor,
    OUT PULONG                 pUId,
    OUT PULONG                 pUnused
    )
{
    dprintf(("vrdpvd.sys::GetChildDescriptor\n"));

    return ERROR_NO_MORE_DEVICES;
}

static VP_STATUS
MirrorFindAdapter(
   IN PVOID HwDeviceExtension,
   IN PVOID HwContext,
   IN PWSTR ArgumentString,
   IN OUT PVIDEO_PORT_CONFIG_INFO ConfigInfo,
   OUT PUCHAR Again
   )
{
    dprintf(("vrdpvd.sys::FindAdapter\n"));

    return NO_ERROR;
}

static BOOLEAN
MirrorInitialize(
   PVOID HwDeviceExtension
   )
{
    dprintf(("vrdpvd.sys::Initialize\n"));

    return TRUE;
}

static BOOLEAN
MirrorStartIO(
   PVOID HwDeviceExtension,
   PVIDEO_REQUEST_PACKET RequestPacket
   )
{
    dprintf(("vrdpvd.sys::StartIO\n"));

    return TRUE;
}

/*
 * The driver entry point.
 */

ULONG
DriverEntry (
    PVOID Context1,
    PVOID Context2
    )
{
    VIDEO_HW_INITIALIZATION_DATA hwInitData;
    ULONG initializationStatus;

    dprintf(("vrdpvd.sys::DriverEntry\n"));

    // Zero out structure.

    VideoPortZeroMemory(&hwInitData, sizeof(VIDEO_HW_INITIALIZATION_DATA));

    // Specify sizes of structure and extension.

    hwInitData.HwInitDataSize = sizeof(VIDEO_HW_INITIALIZATION_DATA);

    // Set entry points.

    hwInitData.HwFindAdapter             = &MirrorFindAdapter;
    hwInitData.HwInitialize              = &MirrorInitialize;
    hwInitData.HwStartIO                 = &MirrorStartIO;
    hwInitData.HwResetHw                 = &MirrorResetHW;
    hwInitData.HwInterrupt               = &MirrorVidInterrupt;
    hwInitData.HwGetPowerState           = &MirrorGetPowerState;
    hwInitData.HwSetPowerState           = &MirrorSetPowerState;
    hwInitData.HwGetVideoChildDescriptor = &MirrorGetChildDescriptor;

    hwInitData.HwLegacyResourceList      = NULL;
    hwInitData.HwLegacyResourceCount     = 0;

    // no device extension necessary
    hwInitData.HwDeviceExtensionSize = 0;

    hwInitData.AdapterInterfaceType = 0;
    hwInitData.AdapterInterfaceType = PCIBus;

    // Our DDK is at the Win2k3 level so we have to take special measures
    // for backwards compatibility.
    switch (vboxQueryWinVersion())
    {
        case WINNT4:
            hwInitData.HwInitDataSize = SIZE_OF_NT4_VIDEO_HW_INITIALIZATION_DATA;
            break;
        case WIN2K:
            hwInitData.HwInitDataSize = SIZE_OF_W2K_VIDEO_HW_INITIALIZATION_DATA;
            break;
    }

    initializationStatus = VideoPortInitialize(Context1,
                                               Context2,
                                               &hwInitData,
                                               NULL);

    dprintf(("vrdpvd.sys::DriverEntry status %08X\n", initializationStatus));

    return initializationStatus;

}
