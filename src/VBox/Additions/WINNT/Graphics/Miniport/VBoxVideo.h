/** @file
 * VirtualBox Video miniport driver
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

#ifndef VBOXVIDEO_H
#define VBOXVIDEO_H

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <iprt/assert.h>

__BEGIN_DECLS
#include "dderror.h"
#include "devioctl.h"
#include "miniport.h"
#include "ntddvdeo.h"
#include "video.h"
__END_DECLS


#define VBE_DISPI_IOPORT_INDEX          0x01CE
#define VBE_DISPI_IOPORT_DATA           0x01CF
#define VBE_DISPI_INDEX_ID              0x0
#define VBE_DISPI_INDEX_XRES            0x1
#define VBE_DISPI_INDEX_YRES            0x2
#define VBE_DISPI_INDEX_BPP             0x3
#define VBE_DISPI_INDEX_ENABLE          0x4
#define VBE_DISPI_INDEX_VIRT_WIDTH      0x6
#define VBE_DISPI_INDEX_VIRT_HEIGHT     0x7
#define VBE_DISPI_INDEX_CMONITORS       0xa
#define VBE_DISPI_ID2                   0xB0C2
#define VBE_DISPI_DISABLED              0x00
#define VBE_DISPI_ENABLED               0x01
#define VBE_DISPI_LFB_ENABLED           0x40
#define VBE_DISPI_LFB_PHYSICAL_ADDRESS  0xE0000000
#define VBE_DISPI_TOTAL_VIDEO_MEMORY_MB 4
#define VBE_DISPI_TOTAL_VIDEO_MEMORY_KB	        (VBE_DISPI_TOTAL_VIDEO_MEMORY_MB * 1024)
#define VBE_DISPI_TOTAL_VIDEO_MEMORY_BYTES      (VBE_DISPI_TOTAL_VIDEO_MEMORY_KB * 1024)

typedef struct
{
   // Saved information about video modes
   ULONG CurrentMode;

   /* Pointer to preallocated generic request structure for VMMDevReq_VideoAccelFlush.
    * Allocated when VBVA status is changed. Deallocated on HwReset.
    */
   void *pvReqFlush;

   ULONG iDevice;       /* Device index (0 for primary) */
   PVOID pvPrimaryExt;  /* Pointer to primary device extension */
   BOOLEAN bEnabled;    /* Device enabled flag */
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

extern "C"
{

__BEGIN_DECLS
ULONG DriverEntry(IN PVOID Context1, IN PVOID Context2);
__END_DECLS

VP_STATUS VBoxVideoFindAdapter(
   IN PVOID HwDeviceExtension,
   IN PVOID HwContext,
   IN PWSTR ArgumentString,
   IN OUT PVIDEO_PORT_CONFIG_INFO ConfigInfo,
   OUT PUCHAR Again);

BOOLEAN VBoxVideoInitialize(PVOID HwDeviceExtension);

BOOLEAN VBoxVideoStartIO(
   PVOID HwDeviceExtension,
   PVIDEO_REQUEST_PACKET RequestPacket);

BOOLEAN VBoxVideoResetHW(
   PVOID HwDeviceExtension,
   ULONG Columns,
   ULONG Rows);

VP_STATUS VBoxVideoGetPowerState(
   PVOID HwDeviceExtension,
   ULONG HwId,
   PVIDEO_POWER_MANAGEMENT VideoPowerControl);

VP_STATUS VBoxVideoSetPowerState(
   PVOID HwDeviceExtension,
   ULONG HwId,
   PVIDEO_POWER_MANAGEMENT VideoPowerControl);

BOOLEAN FASTCALL VBoxVideoSetCurrentMode(
   PDEVICE_EXTENSION DeviceExtension,
   PVIDEO_MODE RequestedMode,
   PSTATUS_BLOCK StatusBlock);

BOOLEAN FASTCALL VBoxVideoResetDevice(
   PDEVICE_EXTENSION DeviceExtension,
   PSTATUS_BLOCK StatusBlock);

BOOLEAN FASTCALL VBoxVideoMapVideoMemory(
   PDEVICE_EXTENSION DeviceExtension,
   PVIDEO_MEMORY RequestedAddress,
   PVIDEO_MEMORY_INFORMATION MapInformation,
   PSTATUS_BLOCK StatusBlock);

BOOLEAN FASTCALL VBoxVideoUnmapVideoMemory(
   PDEVICE_EXTENSION DeviceExtension,
   PVIDEO_MEMORY VideoMemory,
   PSTATUS_BLOCK StatusBlock);

BOOLEAN FASTCALL VBoxVideoQueryNumAvailModes(
   PDEVICE_EXTENSION DeviceExtension,
   PVIDEO_NUM_MODES Modes,
   PSTATUS_BLOCK StatusBlock);

BOOLEAN FASTCALL VBoxVideoQueryAvailModes(
   PDEVICE_EXTENSION DeviceExtension,
   PVIDEO_MODE_INFORMATION ReturnedModes,
   PSTATUS_BLOCK StatusBlock);

BOOLEAN FASTCALL VBoxVideoQueryCurrentMode(
   PDEVICE_EXTENSION DeviceExtension,
   PVIDEO_MODE_INFORMATION VideoModeInfo,
   PSTATUS_BLOCK StatusBlock);

BOOLEAN FASTCALL VBoxVideoSetColorRegisters(
   PDEVICE_EXTENSION DeviceExtension,
   PVIDEO_CLUT ColorLookUpTable,
   PSTATUS_BLOCK StatusBlock);

VP_STATUS VBoxVideoGetChildDescriptor(
   PVOID HwDeviceExtension,
   PVIDEO_CHILD_ENUM_INFO ChildEnumInfo,
   PVIDEO_CHILD_TYPE VideoChildType,
   PUCHAR pChildDescriptor,
   PULONG pUId,
   PULONG pUnused);

} /* extern "C" */

#endif /* VBOXVIDEO_H */
