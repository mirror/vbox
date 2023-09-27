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
// Used commands
//
#define VMWSVGA3_CMD_UPDATE             1

//
// Command buffer context definitions
//
#define VMWSVGA3_CB_CTX_DEVICE          0x3f
#define VMWSVGA3_CB_CTX_0               0

//
// Command buffer status definitions
//
#define VMWSVGA3_CB_STATUS_NONE         0
#define VMWSVGA3_CB_STATUS_COMPLETED    1

//
// Command buffer flags
//
#define VMWSVGA3_CB_FLAG_NONE           0
#define VMWSVGA3_CB_FLAG_NO_IRQ         (1 << 0)

//
// Device context commands
//
#define VMWSVGA3_CMD_DC_START_STOP_CTX  1

//
// Command buffer header
//
typedef struct
{
   volatile UINT32  Status;       // Modified by device
   volatile UINT32  ErrorOffset;  // Modified by device
   UINT64           Id;
   UINT32           Flags;
   UINT32           Length;
   UINT64           PhysicalAddress;
   UINT32           Offset;
   UINT32           DxContext;    // Valid if DX_CONTEXT flag set, must be zero otherwise
   UINT32           Reserved[6];
} VMWSVGA3_CB_HDR;

//
// Update command definition
//
typedef struct {
   UINT32           CmdId;
   UINT32           x;
   UINT32           y;
   UINT32           Width;
   UINT32           Height;
} VMWSVGA3_CTX_CMD_UPDATE;

//
// Start/Stop context command definition
//
typedef struct {
   UINT32           CmdId;
   UINT32           Enable;
   UINT32           Context;
} VMWSVGA3_DC_CMD_START_STOP_CTX;

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

  VMWSVGA3_CB_HDR                 *CmdBufHdr;
  VOID                            *CmdBuf;

  EFI_PHYSICAL_ADDRESS            PhysicalAddressCmdBufHdr;
  EFI_PHYSICAL_ADDRESS            PhysicalAddressCmdBuf;

  VOID                            *CmdBufMapping;
} VMWSVGA3_VIDEO_PRIVATE_DATA;

#define VMWSVGA3_VIDEO_PRIVATE_DATA_FROM_GRAPHICS_OUTPUT_THIS(a) \
  CR(a, VMWSVGA3_VIDEO_PRIVATE_DATA, GraphicsOutput, VMWSVGA3_VIDEO_PRIVATE_DATA_SIGNATURE)

//
// Global Variables
//
extern UINT8                         AttributeController[];
extern UINT8                         GraphicsController[];
extern EFI_DRIVER_BINDING_PROTOCOL   gVmwSvga3VideoDriverBinding;
extern EFI_COMPONENT_NAME_PROTOCOL   gVmwSvga3VideoComponentName;
extern EFI_COMPONENT_NAME2_PROTOCOL  gVmwSvga3VideoComponentName2;

//
// PCI BARs defined by SVGA3
//
#define VMWSVGA3_MMIO_BAR            0
#define VMWSVGA3_VRAM_BAR            2

//
// MMIO Registers defined by SVGA3
//
#define VMWSVGA3_REG_ID              0
# define VMWSVGA3_REG_ID_SVGA3       0x90000003
#define VMWSVGA3_REG_ENABLE          4
#define VMWSVGA3_REG_WIDTH           8
#define VMWSVGA3_REG_HEIGHT          12
#define VMWSVGA3_REG_DEPTH           24
#define VMWSVGA3_REG_BITS_PER_PIXEL  28
#define VMWSVGA3_REG_CONFIG_DONE     80
#define VMWSVGA3_REG_IRQMASK         132
#define VMWSVGA3_REG_TRACES          180
#define VMWSVGA3_REG_COMMAND_LOW     192
#define VMWSVGA3_REG_COMMAND_HIGH    196
#define VMWSVGA3_REG_IRQ_STATUS      328

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
EFI_STATUS
VmwSvga3VideoModeSetup (
  VMWSVGA3_VIDEO_PRIVATE_DATA  *Private
  );

UINT32
VmwSvga3ReadReg (
  IN  VMWSVGA3_VIDEO_PRIVATE_DATA  *VmwSvga3,
  IN  UINT32                       Offset
  );

VOID
VmwSvga3WriteReg (
  IN  VMWSVGA3_VIDEO_PRIVATE_DATA  *VmwSvga3,
  IN UINT32                        Offset,
  IN UINT32                        Data
  );

EFI_STATUS
VmwSvga3DeviceInit (
  IN  VMWSVGA3_VIDEO_PRIVATE_DATA  *This
  );

EFI_STATUS
VmwSvga3DeviceUninit (
  IN  VMWSVGA3_VIDEO_PRIVATE_DATA  *This
  );

EFI_STATUS
VmwSvga3CmdBufProcess (
  IN  VMWSVGA3_VIDEO_PRIVATE_DATA  *This,
  IN  UINTN                        NumberOfBytes,
  IN  UINT32                       Context
  );

#endif
