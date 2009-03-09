/** @file
 * VirtualBox Video miniport driver
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

#ifndef VBOXVIDEO_H
#define VBOXVIDEO_H

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <iprt/assert.h>

#ifdef VBOX_WITH_HGSMI
#include <VBox/HGSMI/HGSMI.h>
#endif /* VBOX_WITH_HGSMI */

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
#define VBE_DISPI_INDEX_VBOX_VIDEO      0xa
#ifdef VBOX_WITH_HGSMI
#define VBE_DISPI_INDEX_VBVA_HOST       0xb
#define VBE_DISPI_INDEX_VBVA_GUEST      0xc
#endif /* VBOX_WITH_HGSMI */

#define VBE_DISPI_ID2                   0xB0C2
/* The VBOX interface id. Indicates support for VBE_DISPI_INDEX_VBOX_VIDEO. */
#define VBE_DISPI_ID_VBOX_VIDEO         0xBE00
#ifdef VBOX_WITH_HGSMI
#define VBE_DISPI_ID_HGSMI              0xBE01
#endif /* VBOX_WITH_HGSMI */
#define VBE_DISPI_DISABLED              0x00
#define VBE_DISPI_ENABLED               0x01
#define VBE_DISPI_LFB_ENABLED           0x40
#define VBE_DISPI_LFB_PHYSICAL_ADDRESS  0xE0000000
#define VBE_DISPI_TOTAL_VIDEO_MEMORY_MB 4
#define VBE_DISPI_TOTAL_VIDEO_MEMORY_KB	        (VBE_DISPI_TOTAL_VIDEO_MEMORY_MB * 1024)
#define VBE_DISPI_TOTAL_VIDEO_MEMORY_BYTES      (VBE_DISPI_TOTAL_VIDEO_MEMORY_KB * 1024)

typedef struct _DEVICE_EXTENSION
{
   struct _DEVICE_EXTENSION *pNext;            /* Next extension in the DualView extension list.
                                                * The primary extension is the first one.
                                                */

   struct _DEVICE_EXTENSION *pPrimary;         /* Pointer to the primary device extension. */

   ULONG iDevice;                              /* Device index: 0 for primary, otherwise a secondary device. */


   ULONG CurrentMode;                          /* Saved information about video modes */
   ULONG CurrentModeWidth;
   ULONG CurrentModeHeight;
   ULONG CurrentModeBPP;

   ULONG ulFrameBufferOffset;                  /* The framebuffer position in the VRAM. */
   ULONG ulFrameBufferSize;                    /* The size of the current framebuffer. */

   union {
       /* Information that is only relevant to the primary device or is the same for all devices. */
       struct {
           
           void *pvReqFlush;                   /* Pointer to preallocated generic request structure for
                                                * VMMDevReq_VideoAccelFlush. Allocated when VBVA status
                                                * is changed. Deallocated on HwReset.
                                                */

           
           ULONG ulVbvaEnabled;                /* Indicates that VBVA mode is enabled. */
           
           BOOLEAN bVBoxVideoSupported;        /* TRUE if VBoxVideo extensions, including DualView, are supported by the host. */
           
           int cDisplays;                      /* Number of displays. */

           ULONG cbVRAM;                       /* The VRAM size. */

           ULONG cbMiniportHeap;               /* The size of reserved VRAM for miniport driver heap.
                                                * It is at offset:
                                                *   cbAdapterMemorySize - VBOX_VIDEO_ADAPTER_INFORMATION_SIZE - cbMiniportHeap
                                                */
           PVOID pvMiniportHeap;               /* The pointer to the miniport heap VRAM. 
                                                * This is mapped by miniport separately.
                                                */

           PVOID pvAdapterInformation;         /* The pointer to the last 4K of VRAM.
                                                * This is mapped by miniport separately.
                                                */
           
           ULONG ulMaxFrameBufferSize;         /* The size of the VRAM allocated for the a single framebuffer. */
           
           ULONG ulDisplayInformationSize;     /* The size of the Display information, which is at offset:
                                                * ulFrameBufferOffset + ulMaxFrameBufferSize.
                                                */
           
#ifdef VBOX_WITH_HGSMI
           BOOLEAN bHGSMI;                     /* Whether HGSMI is enabled. */

           HGSMIAREA areaHostHeap;             /* Host heap VRAM area. */

           HGSMIHEAP hgsmiAdapterHeap;
#endif /* VBOX_WITH_HGSMI */
       } primary;
   
       /* Secondary device information. */
       struct {
           BOOLEAN bEnabled;                   /* Device enabled flag */
       } secondary;
   } u;

#ifdef VBOX_WITH_HGSMI
   HGSMIAREA areaDisplay;                      /* Entire VRAM chunk for this display device. */
#endif /* VBOX_WITH_HGSMI */
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

int VBoxMapAdapterMemory (PDEVICE_EXTENSION PrimaryExtension,
                          void **ppv,
                          ULONG ulOffset,
                          ULONG ulSize);

void VBoxUnmapAdapterMemory (PDEVICE_EXTENSION PrimaryExtension,
                             void **ppv);
                             
void VBoxComputeFrameBufferSizes (PDEVICE_EXTENSION PrimaryExtension);

#ifdef VBOX_WITH_HGSMI
BOOLEAN VBoxHGSMIIsSupported (void);

VOID VBoxSetupDisplaysHGSMI (PDEVICE_EXTENSION PrimaryExtension,
                             PVIDEO_PORT_CONFIG_INFO pConfigInfo,
                             ULONG AdapterMemorySize);
#endif /* VBOX_WITH_HGSMI */
} /* extern "C" */

#endif /* VBOXVIDEO_H */
