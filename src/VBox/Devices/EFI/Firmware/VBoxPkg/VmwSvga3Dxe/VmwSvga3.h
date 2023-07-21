/** @file
  VMware SVGA 3 Video Controller Driver

  Copyright (c) 2023, Oracle and/or its affiliates.<BR>
  Copyright (c) 2006 - 2019, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

//
// VMware SVGA 3 Video Controller Driver
//

#ifndef _VMWSVGA3_H_
#define _VMWSVGA3_H_

#include <Uefi.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/PciIo.h>
#include <Protocol/DriverSupportedEfiVersion.h>
#include <Protocol/DevicePath.h>

#include <Library/DebugLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiLib.h>
#include <Library/PcdLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DevicePathLib.h>
#include <Library/TimerLib.h>
#include <Library/FrameBufferBltLib.h>

#include <IndustryStandard/Pci.h>
#include <IndustryStandard/Acpi.h>

//
// VMware SVGA 3 Video PCI Configuration Header values
//
#define PCI_VENDOR_ID_VMWARE            0x15ad
#define PCI_DEVICE_ID_VMWARE_SVGA3      0x0406

//
// VMware SVGA 3 Video Private Data Structure
//
#define VMWSVGA3_VIDEO_PRIVATE_DATA_SIGNATURE  SIGNATURE_32 ('V', 'M', 'W', '3')


typedef struct {
  UINT64                          Signature;
  EFI_HANDLE                      Handle;
  EFI_PCI_IO_PROTOCOL             *PciIo;
  UINT64                          OriginalPciAttributes;
  EFI_GRAPHICS_OUTPUT_PROTOCOL    GraphicsOutput;
  EFI_DEVICE_PATH_PROTOCOL        *GopDevicePath;

  FRAME_BUFFER_CONFIGURE          *FrameBufferBltConfigure;
  UINTN                           FrameBufferBltConfigureSize;
  UINT8                           FrameBufferVramBarIndex;

} VMWSVGA3_VIDEO_PRIVATE_DATA;

///
/// Card-specific Video Mode structures
///
typedef struct {
  UINT32    Width;
  UINT32    Height;
  UINT32    ColorDepth;
  UINT8     *CrtcSettings;
  UINT16    *SeqSettings;
  UINT8     MiscSetting;
} QEMU_VIDEO_CIRRUS_MODES;

#define VMWSVGA3_VIDEO_PRIVATE_DATA_FROM_GRAPHICS_OUTPUT_THIS(a) \
  CR(a, VMWSVGA3_VIDEO_PRIVATE_DATA, GraphicsOutput, VMWSVGA3_VIDEO_PRIVATE_DATA_SIGNATURE)

//
// Global Variables
//
extern UINT8                         AttributeController[];
extern UINT8                         GraphicsController[];
extern QEMU_VIDEO_CIRRUS_MODES       VmwSvga3VideoModes[];
extern EFI_DRIVER_BINDING_PROTOCOL   gVmwSvga3VideoDriverBinding;
extern EFI_COMPONENT_NAME_PROTOCOL   gVmwSvga3VideoComponentName;
extern EFI_COMPONENT_NAME2_PROTOCOL  gVmwSvga3VideoComponentName2;

//
// MMIO Registers defined by SVGA3
//
/** @todo */

//
// Graphics Output Hardware abstraction internal worker functions
//
EFI_STATUS
VmwSvga3VideoGraphicsOutputConstructor (
  VMWSVGA3_VIDEO_PRIVATE_DATA  *Private
  );

EFI_STATUS
VmwSvga3VideoGraphicsOutputDestructor (
  VMWSVGA3_VIDEO_PRIVATE_DATA  *Private
  );

//
// EFI_DRIVER_BINDING_PROTOCOL Protocol Interface
//

/**
  TODO: Add function description

  @param  This TODO: add argument description
  @param  Controller TODO: add argument description
  @param  RemainingDevicePath TODO: add argument description

  TODO: add return values

**/
EFI_STATUS
EFIAPI
VmwSvga3VideoControllerDriverSupported (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   Controller,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  );

/**
  TODO: Add function description

  @param  This TODO: add argument description
  @param  Controller TODO: add argument description
  @param  RemainingDevicePath TODO: add argument description

  TODO: add return values

**/
EFI_STATUS
EFIAPI
VmwSvga3VideoControllerDriverStart (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   Controller,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  );

/**
  TODO: Add function description

  @param  This TODO: add argument description
  @param  Controller TODO: add argument description
  @param  NumberOfChildren TODO: add argument description
  @param  ChildHandleBuffer TODO: add argument description

  TODO: add return values

**/
EFI_STATUS
EFIAPI
VmwSvga3VideoControllerDriverStop (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   Controller,
  IN UINTN                        NumberOfChildren,
  IN EFI_HANDLE                   *ChildHandleBuffer
  );

//
// EFI Component Name Functions
//

/**
  Retrieves a Unicode string that is the user readable name of the driver.

  This function retrieves the user readable name of a driver in the form of a
  Unicode string. If the driver specified by This has a user readable name in
  the language specified by Language, then a pointer to the driver name is
  returned in DriverName, and EFI_SUCCESS is returned. If the driver specified
  by This does not support the language specified by Language,
  then EFI_UNSUPPORTED is returned.

  @param  This[in]              A pointer to the EFI_COMPONENT_NAME2_PROTOCOL or
                                EFI_COMPONENT_NAME_PROTOCOL instance.

  @param  Language[in]          A pointer to a Null-terminated ASCII string
                                array indicating the language. This is the
                                language of the driver name that the caller is
                                requesting, and it must match one of the
                                languages specified in SupportedLanguages. The
                                number of languages supported by a driver is up
                                to the driver writer. Language is specified
                                in RFC 4646 or ISO 639-2 language code format.

  @param  DriverName[out]       A pointer to the Unicode string to return.
                                This Unicode string is the name of the
                                driver specified by This in the language
                                specified by Language.

  @retval EFI_SUCCESS           The Unicode string for the Driver specified by
                                This and the language specified by Language was
                                returned in DriverName.

  @retval EFI_INVALID_PARAMETER Language is NULL.

  @retval EFI_INVALID_PARAMETER DriverName is NULL.

  @retval EFI_UNSUPPORTED       The driver specified by This does not support
                                the language specified by Language.

**/
EFI_STATUS
EFIAPI
VmwSvga3VideoComponentNameGetDriverName (
  IN  EFI_COMPONENT_NAME_PROTOCOL  *This,
  IN  CHAR8                        *Language,
  OUT CHAR16                       **DriverName
  );

/**
  Retrieves a Unicode string that is the user readable name of the controller
  that is being managed by a driver.

  This function retrieves the user readable name of the controller specified by
  ControllerHandle and ChildHandle in the form of a Unicode string. If the
  driver specified by This has a user readable name in the language specified by
  Language, then a pointer to the controller name is returned in ControllerName,
  and EFI_SUCCESS is returned.  If the driver specified by This is not currently
  managing the controller specified by ControllerHandle and ChildHandle,
  then EFI_UNSUPPORTED is returned.  If the driver specified by This does not
  support the language specified by Language, then EFI_UNSUPPORTED is returned.

  @param  This[in]              A pointer to the EFI_COMPONENT_NAME2_PROTOCOL or
                                EFI_COMPONENT_NAME_PROTOCOL instance.

  @param  ControllerHandle[in]  The handle of a controller that the driver
                                specified by This is managing.  This handle
                                specifies the controller whose name is to be
                                returned.

  @param  ChildHandle[in]       The handle of the child controller to retrieve
                                the name of.  This is an optional parameter that
                                may be NULL.  It will be NULL for device
                                drivers.  It will also be NULL for a bus drivers
                                that wish to retrieve the name of the bus
                                controller.  It will not be NULL for a bus
                                driver that wishes to retrieve the name of a
                                child controller.

  @param  Language[in]          A pointer to a Null-terminated ASCII string
                                array indicating the language.  This is the
                                language of the driver name that the caller is
                                requesting, and it must match one of the
                                languages specified in SupportedLanguages. The
                                number of languages supported by a driver is up
                                to the driver writer. Language is specified in
                                RFC 4646 or ISO 639-2 language code format.

  @param  ControllerName[out]   A pointer to the Unicode string to return.
                                This Unicode string is the name of the
                                controller specified by ControllerHandle and
                                ChildHandle in the language specified by
                                Language from the point of view of the driver
                                specified by This.

  @retval EFI_SUCCESS           The Unicode string for the user readable name in
                                the language specified by Language for the
                                driver specified by This was returned in
                                DriverName.

  @retval EFI_INVALID_PARAMETER ControllerHandle is not a valid EFI_HANDLE.

  @retval EFI_INVALID_PARAMETER ChildHandle is not NULL and it is not a valid
                                EFI_HANDLE.

  @retval EFI_INVALID_PARAMETER Language is NULL.

  @retval EFI_INVALID_PARAMETER ControllerName is NULL.

  @retval EFI_UNSUPPORTED       The driver specified by This is not currently
                                managing the controller specified by
                                ControllerHandle and ChildHandle.

  @retval EFI_UNSUPPORTED       The driver specified by This does not support
                                the language specified by Language.

**/
EFI_STATUS
EFIAPI
VmwSvga3VideoComponentNameGetControllerName (
  IN  EFI_COMPONENT_NAME_PROTOCOL  *This,
  IN  EFI_HANDLE                   ControllerHandle,
  IN  EFI_HANDLE                   ChildHandle        OPTIONAL,
  IN  CHAR8                        *Language,
  OUT CHAR16                       **ControllerName
  );

//
// Local Function Prototypes
//
VOID
VmwSvga3InitializeGraphicsMode (
  VMWSVGA3_VIDEO_PRIVATE_DATA  *Private,
  QEMU_VIDEO_CIRRUS_MODES  *ModeData
  );

EFI_STATUS
VmwSvga3VideoModeSetup (
  VMWSVGA3_VIDEO_PRIVATE_DATA  *Private
  );

#endif
