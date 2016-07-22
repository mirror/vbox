/* $Id$ */
/** @file
 * VBoxVga.c
 */

/*
 * Copyright (C) 2009-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

/*
  This code is based on:

  Cirrus Logic 5430 Controller Driver.
  This driver is a sample implementation of the UGA Draw and Graphics Output
  Protocols for the Cirrus Logic 5430 family of PCI video controllers.
  This driver is only usable in the EFI pre-boot environment.
  This sample is intended to show how the UGA Draw and Graphics output Protocol
  is able to function.
  The UGA I/O Protocol is not implemented in this sample.
  A fully compliant EFI UGA driver requires both
  the UGA Draw and the UGA I/O Protocol.  Please refer to Microsoft's
  documentation on UGA for details on how to write a UGA driver that is able
  to function both in the EFI pre-boot environment and from the OS runtime.

  Copyright (c) 2006 - 2009, Intel Corporation
  All rights reserved. This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

*/

//
// Cirrus Logic 5430 Controller Driver
//
#include "VBoxVga.h"
#include "iprt/asm.h"
#include <IndustryStandard/Acpi.h>

EFI_DRIVER_BINDING_PROTOCOL gVBoxVgaDriverBinding = {
  VBoxVgaControllerDriverSupported,
  VBoxVgaControllerDriverStart,
  VBoxVgaControllerDriverStop,
  0x10,
  NULL,
  NULL
};

///
/// Generic Attribute Controller Register Settings
///
UINT8  AttributeController[21] = {
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
  0x41, 0x00, 0x0F, 0x00, 0x00
};

///
/// Generic Graphics Controller Register Settings
///
UINT8 GraphicsController[9] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x05, 0x0F, 0xFF
};

//
// 640 x 480 x 256 color @ 60 Hertz
//
UINT8 Crtc_640_480_256_60[25] = {
    /* r0  =  */0x5f,  /* r1  =  */0x4f,  /* r2  =  */0x50,  /* r3  =  */0x82,
    /* r4  =  */0x54,  /* r5  =  */0x80,  /* r6  =  */0x0b,  /* r7  =  */0x3e,
    /* r8  =  */0x00,  /* r9  =  */0x40,  /* r10 =  */0x00,  /* r11 =  */0x00,
    /* r12 =  */0x00,  /* r13 =  */0x00,  /* r14 =  */0x00,  /* r15 =  */0x00,
    /* r16 =  */0xea,  /* r17 =  */0x0c,  /* r18 =  */0xdf,  /* r19 =  */0x28,
    /* r20 =  */0x4f,  /* r21 =  */0xe7,  /* r22 =  */0x04,  /* r23 =  */0xe3,
    /* r24 =  */0xff
};
UINT8 Seq_640_480_256_60[5] = {
 0x01,  0x01,  0x0f,  0x00,  0x0a
};

//
// 800 x 600 x 256 color @ 60 Hertz
//
UINT8 Crtc_800_600_256_60[25] = {
    /* r0  =  */0x5f,  /* r1  =  */0x4f,  /* r2  =  */0x50,  /* r3  =  */0x82,
    /* r4  =  */0x54,  /* r5  =  */0x80,  /* r6  =  */0x0b,  /* r7  =  */0x3e,
    /* r8  =  */0x00,  /* r9  =  */0x40,  /* r10 =  */0x00,  /* r11 =  */0x00,
    /* r12 =  */0x00,  /* r13 =  */0x00,  /* r14 =  */0x00,  /* r15 =  */0x00,
    /* r16 =  */0xea,  /* r17 =  */0x0c,  /* r18 =  */0xdf,  /* r19 =  */0x28,
    /* r20 =  */0x4f,  /* r21 =  */0xe7,  /* r22 =  */0x04,  /* r23 =  */0xe3,
    /* r24 =  */0xff

};

UINT8 Seq_800_600_256_60[5] = {
 0x01,  0x01,  0x0f,  0x00,  0x0a
};

//
// 1024 x 768 x 256 color @ 60 Hertz
//
UINT8 Crtc_1024_768_256_60[25] = {
    /* r0  =  */0xa3,  /* r1  =  */0x7f,  /* r2  =  */0x81,  /* r3  =  */0x90,
    /* r4  =  */0x88,  /* r5  =  */0x05,  /* r6  =  */0x28,  /* r7  =  */0xfd,
    /* r8  =  */0x00,  /* r9  =  */0x60,  /* r10 =  */0x00,  /* r11 =  */0x00,
    /* r12 =  */0x00,  /* r13 =  */0x00,  /* r14 =  */0x00,  /* r15 =  */0x00,
    /* r16 =  */0x06,  /* r17 =  */0x0f,  /* r18 =  */0xff,  /* r19 =  */0x40,
    /* r20 =  */0x4f,  /* r21 =  */0x05,  /* r22 =  */0x1a,  /* r23 =  */0xe3,
    /* r24 =  */0xff
};

UINT8 Seq_1024_768_256_60[5] = {
 0x01,  0x01,  0x0f,  0x00,  0x0a
};

//
// 1280x1024
//
UINT8 Crtc_1280_1024_256_60[25] = {
    /* r0  =  */0xa3,  /* r1  =  */0x9f,  /* r2  =  */0x81,  /* r3  =  */0x90,
    /* r4  =  */0x88,  /* r5  =  */0x05,  /* r6  =  */0x28,  /* r7  =  */0xbd,
    /* r8  =  */0x00,  /* r9  =  */0x60,  /* r10 =  */0x00,  /* r11 =  */0x00,
    /* r12 =  */0x00,  /* r13 =  */0x00,  /* r14 =  */0x00,  /* r15 =  */0x00,
    /* r16 =  */0x06,  /* r17 =  */0x0f,  /* r18 =  */0x3f,  /* r19 =  */0x40,
    /* r20 =  */0x4f,  /* r21 =  */0x05,  /* r22 =  */0x1a,  /* r23 =  */0xe3,
    /* r24 =  */0xff
};

UINT8 Seq_1280_1024_256_60[5] = {
 0x01,  0x01,  0x0f,  0x00,  0x0a
};

//
// 1440x900
//
UINT8 Crtc_1440_900_256_60[25] = {
    /* r0  =  */0xa3,  /* r1  =  */0xb3,  /* r2  =  */0x81,  /* r3  =  */0x90,
    /* r4  =  */0x88,  /* r5  =  */0x05,  /* r6  =  */0x28,  /* r7  =  */0xbd,
    /* r8  =  */0x00,  /* r9  =  */0x60,  /* r10 =  */0x00,  /* r11 =  */0x00,
    /* r12 =  */0x00,  /* r13 =  */0x00,  /* r14 =  */0x00,  /* r15 =  */0x00,
    /* r16 =  */0x06,  /* r17 =  */0x0f,  /* r18 =  */0x38,  /* r19 =  */0x40,
    /* r20 =  */0x4f,  /* r21 =  */0x05,  /* r22 =  */0x1a,  /* r23 =  */0xe3,
    /* r24 =  */0xff
};

UINT8 Seq_1440_900_256_60[5] = {
 0x01,  0x01,  0x0f,  0x00,  0x0a
};

//
// 1920x1200
//
UINT8 Crtc_1920_1200_256_60[25] = {
    /* r0  =  */0xa3,  /* r1  =  */0xef,  /* r2  =  */0x81,  /* r3  =  */0x90,
    /* r4  =  */0x88,  /* r5  =  */0x05,  /* r6  =  */0x28,  /* r7  =  */0xbd,
    /* r8  =  */0x00,  /* r9  =  */0x60,  /* r10 =  */0x00,  /* r11 =  */0x00,
    /* r12 =  */0x00,  /* r13 =  */0x00,  /* r14 =  */0x00,  /* r15 =  */0x00,
    /* r16 =  */0x06,  /* r17 =  */0x0f,  /* r18 =  */0x50,  /* r19 =  */0x40,
    /* r20 =  */0x4f,  /* r21 =  */0x05,  /* r22 =  */0x1a,  /* r23 =  */0xe3,
    /* r24 =  */0xff
};

UINT8 Seq_1920_1200_256_60[5] = {
 0x01,  0x01,  0x0f,  0x00,  0x0a
};

///
/// Table of supported video modes
///
VBOX_VGA_VIDEO_MODES  VBoxVgaVideoModes[] =
{
  {  640, 480, 32, 60, Crtc_640_480_256_60,  Seq_640_480_256_60,  0xe3 },
  {  800, 600, 32, 60, Crtc_800_600_256_60,  Seq_800_600_256_60,  0x23 },
  { 1024, 768, 32, 60, Crtc_1024_768_256_60, Seq_1024_768_256_60, 0xef },
  { 1280, 1024, 32, 60, Crtc_1280_1024_256_60, Seq_1280_1024_256_60, 0xef },
  { 1440, 900, 32, 60, Crtc_1440_900_256_60, Seq_1440_900_256_60, 0xef },
  { 1920, 1200, 32, 60, Crtc_1920_1200_256_60, Seq_1920_1200_256_60, 0xef }
};

typedef struct _APPLE_FRAMEBUFFERINFO_PROTOCOL APPLE_FRAMEBUFFERINFO_PROTOCOL;

typedef
EFI_STATUS
(EFIAPI *APPLE_FRAMEBUFFERINFO_PROTOCOL_GET_INFO) (
                   IN  APPLE_FRAMEBUFFERINFO_PROTOCOL   *This,
                   OUT UINT32                           *BaseAddr,
                   OUT UINT32                           *Something,
                   OUT UINT32                           *RowBytes,
                   OUT UINT32                           *Width,
                   OUT UINT32                           *Height,
                   OUT UINT32                           *Depth);

struct _APPLE_FRAMEBUFFERINFO_PROTOCOL {
  APPLE_FRAMEBUFFERINFO_PROTOCOL_GET_INFO         GetInfo;
  VBOX_VGA_PRIVATE_DATA                           *Private;
};

EFI_STATUS EFIAPI
GetFrameBufferInfo(IN  APPLE_FRAMEBUFFERINFO_PROTOCOL   *This,
                   OUT UINT32                           *BaseAddr,
                   OUT UINT32                           *Something,
                   OUT UINT32                           *RowBytes,
                   OUT UINT32                           *Width,
                   OUT UINT32                           *Height,
                   OUT UINT32                           *Depth);

static APPLE_FRAMEBUFFERINFO_PROTOCOL gAppleFrameBufferInfo =
{
    GetFrameBufferInfo,
    NULL
};


/*
 *   @todo move this function to the library.
 */
UINT32 VBoxVgaGetVmVariable(UINT32 Variable, CHAR8* Buffer, UINT32 Size)
{
    UINT32 VarLen, i;

    ASMOutU32(EFI_INFO_PORT, Variable);
    VarLen = ASMInU32(EFI_INFO_PORT);

    for (i = 0; i < VarLen && i < Size; i++)
        Buffer[i] = ASMInU8(EFI_INFO_PORT);

    return VarLen;
}


/**
  VBoxVgaControllerDriverSupported

  TODO:    This - add argument and description to function comment
  TODO:    Controller - add argument and description to function comment
  TODO:    RemainingDevicePath - add argument and description to function comment
**/
EFI_STATUS
EFIAPI
VBoxVgaControllerDriverSupported (
  IN EFI_DRIVER_BINDING_PROTOCOL    *This,
  IN EFI_HANDLE                     Controller,
  IN EFI_DEVICE_PATH_PROTOCOL       *RemainingDevicePath
  )
{
  EFI_STATUS          Status;
  EFI_PCI_IO_PROTOCOL *PciIo;
  PCI_TYPE00          Pci;
  EFI_DEV_PATH        *Node;

  //
  // Open the PCI I/O Protocol
  //
  Status = gBS->OpenProtocol (
                  Controller,
                  &gEfiPciIoProtocolGuid,
                  (VOID **) &PciIo,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status)) {
    DEBUG((DEBUG_INFO, "%a:%d status:%r\n", __FILE__, __LINE__, Status));
    return Status;
  }

  //
  // Read the PCI Configuration Header from the PCI Device
  //
  Status = PciIo->Pci.Read (
                        PciIo,
                        EfiPciIoWidthUint32,
                        0,
                        sizeof (Pci) / sizeof (UINT32),
                        &Pci
                        );
  if (EFI_ERROR (Status)) {
    DEBUG((DEBUG_INFO, "%a:%d status:%r\n", __FILE__, __LINE__, Status));
    goto Done;
  }

  Status = EFI_UNSUPPORTED;
  //
  // See if the I/O enable is on.  Most systems only allow one VGA device to be turned on
  // at a time, so see if this is one that is turned on.
  //
  //  if (((Pci.Hdr.Command & 0x01) == 0x01)) {
  //
  // See if this is a Cirrus Logic PCI controller
  //
  if (Pci.Hdr.VendorId == VBOX_VENDOR_ID) {
    if (Pci.Hdr.DeviceId == VBOX_VGA_DEVICE_ID) {

      Status = EFI_SUCCESS;
      if (RemainingDevicePath != NULL) {
        Node = (EFI_DEV_PATH *) RemainingDevicePath;
        //
        // Check if RemainingDevicePath is the End of Device Path Node,
        // if yes, return EFI_SUCCESS
        //
        if (!IsDevicePathEnd (Node)) {
          //
          // If RemainingDevicePath isn't the End of Device Path Node,
          // check its validation
          //
          if (Node->DevPath.Type != ACPI_DEVICE_PATH ||
              Node->DevPath.SubType != ACPI_ADR_DP ||
              DevicePathNodeLength(&Node->DevPath) != sizeof(ACPI_ADR_DEVICE_PATH)) {
            DEBUG((DEBUG_INFO, "%a:%d status:%r\n", __FILE__, __LINE__, Status));
            Status = EFI_UNSUPPORTED;
          }
        }
      }
    }
  }

Done:
  //
  // Close the PCI I/O Protocol
  //
  gBS->CloseProtocol (
        Controller,
        &gEfiPciIoProtocolGuid,
        This->DriverBindingHandle,
        Controller
        );

  DEBUG((DEBUG_INFO, "%a:%d status:%r\n", __FILE__, __LINE__, Status));
  return Status;
}

/**
  VBoxVgaControllerDriverStart

  TODO:    This - add argument and description to function comment
  TODO:    Controller - add argument and description to function comment
  TODO:    RemainingDevicePath - add argument and description to function comment
**/
EFI_STATUS
EFIAPI
VBoxVgaControllerDriverStart (
  IN EFI_DRIVER_BINDING_PROTOCOL    *This,
  IN EFI_HANDLE                     Controller,
  IN EFI_DEVICE_PATH_PROTOCOL       *RemainingDevicePath
  )
{
  EFI_STATUS                      Status;
  VBOX_VGA_PRIVATE_DATA  *Private;
  BOOLEAN                         PciAttributesSaved;
  EFI_DEVICE_PATH_PROTOCOL        *ParentDevicePath;
  ACPI_ADR_DEVICE_PATH            AcpiDeviceNode;

  PciAttributesSaved = FALSE;
  //
  // Allocate Private context data for UGA Draw interface.
  //
  Private = AllocateZeroPool (sizeof (VBOX_VGA_PRIVATE_DATA));
  if (Private == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Error;
  }
  gAppleFrameBufferInfo.Private = Private;
  //
  // Set up context record
  //
  Private->Signature  = VBOX_VGA_PRIVATE_DATA_SIGNATURE;
  Private->Handle     = NULL;

  //
  // Open PCI I/O Protocol
  //
  Status = gBS->OpenProtocol (
                  Controller,
                  &gEfiPciIoProtocolGuid,
                  (VOID **) &Private->PciIo,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status)) {
    goto Error;
  }

  //
  // Save original PCI attributes
  //
  Status = Private->PciIo->Attributes (
                    Private->PciIo,
                    EfiPciIoAttributeOperationGet,
                    0,
                    &Private->OriginalPciAttributes
                    );

  if (EFI_ERROR (Status)) {
    goto Error;
  }
  PciAttributesSaved = TRUE;

  Status = Private->PciIo->Attributes (
                            Private->PciIo,
                            EfiPciIoAttributeOperationEnable,
                            EFI_PCI_DEVICE_ENABLE | EFI_PCI_IO_ATTRIBUTE_VGA_MEMORY | EFI_PCI_IO_ATTRIBUTE_VGA_IO,
                            NULL
                            );
  if (EFI_ERROR (Status)) {
    goto Error;
  }

  //
  // Get ParentDevicePath
  //
  Status = gBS->HandleProtocol (
                  Controller,
                  &gEfiDevicePathProtocolGuid,
                  (VOID **) &ParentDevicePath
                  );
  if (EFI_ERROR (Status)) {
    goto Error;
  }

  if (FeaturePcdGet (PcdSupportGop)) {
    //
    // Set Gop Device Path
    //
    if (RemainingDevicePath == NULL) {
      ZeroMem (&AcpiDeviceNode, sizeof (ACPI_ADR_DEVICE_PATH));
      AcpiDeviceNode.Header.Type = ACPI_DEVICE_PATH;
      AcpiDeviceNode.Header.SubType = ACPI_ADR_DP;
      AcpiDeviceNode.ADR = ACPI_DISPLAY_ADR (1, 0, 0, 1, 0, ACPI_ADR_DISPLAY_TYPE_VGA, 0, 0);
      SetDevicePathNodeLength (&AcpiDeviceNode.Header, sizeof (ACPI_ADR_DEVICE_PATH));

      Private->GopDevicePath = AppendDevicePathNode (
                                          ParentDevicePath,
                                          (EFI_DEVICE_PATH_PROTOCOL *) &AcpiDeviceNode
                                          );
    } else if (!IsDevicePathEnd (RemainingDevicePath)) {
      //
      // If RemainingDevicePath isn't the End of Device Path Node,
      // only scan the specified device by RemainingDevicePath
      //
      Private->GopDevicePath = AppendDevicePathNode (ParentDevicePath, RemainingDevicePath);
    } else {
      //
      // If RemainingDevicePath is the End of Device Path Node,
      // don't create child device and return EFI_SUCCESS
      //
      Private->GopDevicePath = NULL;
    }

    if (Private->GopDevicePath != NULL) {
      //
      // Create child handle and device path protocol first
      //
      Private->Handle = NULL;
      Status = gBS->InstallMultipleProtocolInterfaces (
                      &Private->Handle,
                      &gEfiDevicePathProtocolGuid,
                      Private->GopDevicePath,
                      NULL
                      );
    }
  }

  //
  // Construct video mode buffer
  //
  Status = VBoxVgaVideoModeSetup (Private);
  if (EFI_ERROR (Status)) {
    goto Error;
  }


  if (FeaturePcdGet (PcdSupportUga)) {
    //
    // Start the UGA Draw software stack.
    //
    Status = VBoxVgaUgaDrawConstructor (Private);
    ASSERT_EFI_ERROR (Status);

    Private->UgaDevicePath = ParentDevicePath;
    Status = gBS->InstallMultipleProtocolInterfaces (
                    &Controller,
                    //&gEfiUgaDrawProtocolGuid,
                    //&Private->UgaDraw,
                    &gEfiDevicePathProtocolGuid,
                    Private->UgaDevicePath,
                    NULL
                    );
    Status = gBS->InstallMultipleProtocolInterfaces (
                    &Controller,
                    &gEfiUgaDrawProtocolGuid,
                    &Private->UgaDraw,
                    NULL
                    );

  } else if (FeaturePcdGet (PcdSupportGop)) {
    if (Private->GopDevicePath == NULL) {
      //
      // If RemainingDevicePath is the End of Device Path Node,
      // don't create child device and return EFI_SUCCESS
      //
      Status = EFI_SUCCESS;
    } else {

      //
      // Start the GOP software stack.
      //
      Status = VBoxVgaGraphicsOutputConstructor (Private);
      ASSERT_EFI_ERROR (Status);

      Status = gBS->InstallMultipleProtocolInterfaces (
                      &Private->Handle,
                      &gEfiGraphicsOutputProtocolGuid,
                      &Private->GraphicsOutput,
                      &gEfiEdidDiscoveredProtocolGuid,
                      &Private->EdidDiscovered,
                      &gEfiEdidActiveProtocolGuid,
                      &Private->EdidActive,
                      NULL
                      );
    }
  } else {
    //
    // This driver must support eithor GOP or UGA or both.
    //
    ASSERT (FALSE);
    Status = EFI_UNSUPPORTED;
  }


Error:
  if (EFI_ERROR (Status)) {
    if (Private) {
      if (Private->PciIo) {
        if (PciAttributesSaved == TRUE) {
          //
          // Restore original PCI attributes
          //
          Private->PciIo->Attributes (
                          Private->PciIo,
                          EfiPciIoAttributeOperationSet,
                          Private->OriginalPciAttributes,
                          NULL
                          );
        }
        //
        // Close the PCI I/O Protocol
        //
        gBS->CloseProtocol (
              Private->Handle,
              &gEfiPciIoProtocolGuid,
              This->DriverBindingHandle,
              Private->Handle
              );
      }

      gBS->FreePool (Private);
    }
  }

  return Status;
}

/**
  VBoxVgaControllerDriverStop

  TODO:    This - add argument and description to function comment
  TODO:    Controller - add argument and description to function comment
  TODO:    NumberOfChildren - add argument and description to function comment
  TODO:    ChildHandleBuffer - add argument and description to function comment
  TODO:    EFI_SUCCESS - add return value to function comment
**/
EFI_STATUS
EFIAPI
VBoxVgaControllerDriverStop (
  IN EFI_DRIVER_BINDING_PROTOCOL    *This,
  IN EFI_HANDLE                     Controller,
  IN UINTN                          NumberOfChildren,
  IN EFI_HANDLE                     *ChildHandleBuffer
  )
{
  EFI_UGA_DRAW_PROTOCOL           *UgaDraw;
  EFI_GRAPHICS_OUTPUT_PROTOCOL    *GraphicsOutput;

  EFI_STATUS                      Status;
  VBOX_VGA_PRIVATE_DATA  *Private;

  if (FeaturePcdGet (PcdSupportUga)) {
    Status = gBS->OpenProtocol (
                    Controller,
                    &gEfiUgaDrawProtocolGuid,
                    (VOID **) &UgaDraw,
                    This->DriverBindingHandle,
                    Controller,
                    EFI_OPEN_PROTOCOL_GET_PROTOCOL
                    );
    if (EFI_ERROR (Status)) {
      return Status;
    }
    //
    // Get our private context information
    //
    Private = VBOX_VGA_PRIVATE_DATA_FROM_UGA_DRAW_THIS (UgaDraw);
    VBoxVgaUgaDrawDestructor (Private);

    if (FeaturePcdGet (PcdSupportGop)) {
      VBoxVgaGraphicsOutputDestructor (Private);
      //
      // Remove the UGA and GOP protocol interface from the system
      //
      Status = gBS->UninstallMultipleProtocolInterfaces (
                      Private->Handle,
                      &gEfiUgaDrawProtocolGuid,
                      &Private->UgaDraw,
                      &gEfiGraphicsOutputProtocolGuid,
                      &Private->GraphicsOutput,
                      NULL
                      );
    } else {
      //
      // Remove the UGA Draw interface from the system
      //
      Status = gBS->UninstallMultipleProtocolInterfaces (
                      Private->Handle,
                      &gEfiUgaDrawProtocolGuid,
                      &Private->UgaDraw,
                      NULL
                      );
    }
  } else {
    Status = gBS->OpenProtocol (
                    Controller,
                    &gEfiGraphicsOutputProtocolGuid,
                    (VOID **) &GraphicsOutput,
                    This->DriverBindingHandle,
                    Controller,
                    EFI_OPEN_PROTOCOL_GET_PROTOCOL
                    );
    if (EFI_ERROR (Status)) {
      return Status;
    }

    //
    // Get our private context information
    //
    Private = VBOX_VGA_PRIVATE_DATA_FROM_GRAPHICS_OUTPUT_THIS (GraphicsOutput);

    VBoxVgaGraphicsOutputDestructor (Private);
    //
    // Remove the GOP protocol interface from the system
    //
    Status = gBS->UninstallMultipleProtocolInterfaces (
                    Private->Handle,
                    &gEfiUgaDrawProtocolGuid,
                    &Private->UgaDraw,
                    &gEfiGraphicsOutputProtocolGuid,
                    &Private->GraphicsOutput,
                    NULL
                    );
  }

  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Restore original PCI attributes
  //
  Private->PciIo->Attributes (
                  Private->PciIo,
                  EfiPciIoAttributeOperationSet,
                  Private->OriginalPciAttributes,
                  NULL
                  );

  //
  // Close the PCI I/O Protocol
  //
  gBS->CloseProtocol (
        Controller,
        &gEfiPciIoProtocolGuid,
        This->DriverBindingHandle,
        Controller
        );

  //
  // Free our instance data
  //
  gBS->FreePool (Private);

  return EFI_SUCCESS;
}

/**
  VBoxVgaUgaDrawDestructor

  TODO:    Private - add argument and description to function comment
  TODO:    EFI_SUCCESS - add return value to function comment
**/
EFI_STATUS
VBoxVgaUgaDrawDestructor (
  VBOX_VGA_PRIVATE_DATA  *Private
  )
{
  return EFI_SUCCESS;
}

#define inb(ignore, port) ASMInU8((port))
#define inw(ignore, port) ASMInU16((port))
#define outb(ignore, port, val) ASMOutU8((port), (val))
#define outw(ignore, port, val) ASMOutU16((port), (val))


/**
  TODO: Add function description

  @param  Private TODO: add argument description
  @param  Index TODO: add argument description
  @param  Red TODO: add argument description
  @param  Green TODO: add argument description
  @param  Blue TODO: add argument description

  TODO: add return values

**/
VOID
SetPaletteColor (
  VBOX_VGA_PRIVATE_DATA  *Private,
  UINTN                           Index,
  UINT8                           Red,
  UINT8                           Green,
  UINT8                           Blue
  )
{
  outb (Private, PALETTE_INDEX_REGISTER, (UINT8) Index);
  outb (Private, PALETTE_DATA_REGISTER, (UINT8) (Red >> 2));
  outb (Private, PALETTE_DATA_REGISTER, (UINT8) (Green >> 2));
  outb (Private, PALETTE_DATA_REGISTER, (UINT8) (Blue >> 2));
}

/**
  TODO: Add function description

  @param  Private TODO: add argument description

  TODO: add return values

**/
VOID
SetDefaultPalette (
  VBOX_VGA_PRIVATE_DATA  *Private
  )
{
#if 1
  UINTN Index;
  UINTN RedIndex;
  UINTN GreenIndex;
  UINTN BlueIndex;
  Index = 0;
  for (RedIndex = 0; RedIndex < 8; RedIndex++) {
    for (GreenIndex = 0; GreenIndex < 8; GreenIndex++) {
      for (BlueIndex = 0; BlueIndex < 4; BlueIndex++) {
        SetPaletteColor (Private, Index, (UINT8) (RedIndex << 5), (UINT8) (GreenIndex << 5), (UINT8) (BlueIndex << 6));
        Index++;
      }
    }
  }
#else
     {
         int i;
         static const UINT8 s_a3bVgaDac[64*3] =
         {
             0x00, 0x00, 0x00,
             0x00, 0x00, 0x2A,
             0x00, 0x2A, 0x00,
             0x00, 0x2A, 0x2A,
             0x2A, 0x00, 0x00,
             0x2A, 0x00, 0x2A,
             0x2A, 0x2A, 0x00,
             0x2A, 0x2A, 0x2A,
             0x00, 0x00, 0x15,
             0x00, 0x00, 0x3F,
             0x00, 0x2A, 0x15,
             0x00, 0x2A, 0x3F,
             0x2A, 0x00, 0x15,
             0x2A, 0x00, 0x3F,
             0x2A, 0x2A, 0x15,
             0x2A, 0x2A, 0x3F,
             0x00, 0x15, 0x00,
             0x00, 0x15, 0x2A,
             0x00, 0x3F, 0x00,
             0x00, 0x3F, 0x2A,
             0x2A, 0x15, 0x00,
             0x2A, 0x15, 0x2A,
             0x2A, 0x3F, 0x00,
             0x2A, 0x3F, 0x2A,
             0x00, 0x15, 0x15,
             0x00, 0x15, 0x3F,
             0x00, 0x3F, 0x15,
             0x00, 0x3F, 0x3F,
             0x2A, 0x15, 0x15,
             0x2A, 0x15, 0x3F,
             0x2A, 0x3F, 0x15,
             0x2A, 0x3F, 0x3F,
             0x15, 0x00, 0x00,
             0x15, 0x00, 0x2A,
             0x15, 0x2A, 0x00,
             0x15, 0x2A, 0x2A,
             0x3F, 0x00, 0x00,
             0x3F, 0x00, 0x2A,
             0x3F, 0x2A, 0x00,
             0x3F, 0x2A, 0x2A,
             0x15, 0x00, 0x15,
             0x15, 0x00, 0x3F,
             0x15, 0x2A, 0x15,
             0x15, 0x2A, 0x3F,
             0x3F, 0x00, 0x15,
             0x3F, 0x00, 0x3F,
             0x3F, 0x2A, 0x15,
             0x3F, 0x2A, 0x3F,
             0x15, 0x15, 0x00,
             0x15, 0x15, 0x2A,
             0x15, 0x3F, 0x00,
             0x15, 0x3F, 0x2A,
             0x3F, 0x15, 0x00,
             0x3F, 0x15, 0x2A,
             0x3F, 0x3F, 0x00,
             0x3F, 0x3F, 0x2A,
             0x15, 0x15, 0x15,
             0x15, 0x15, 0x3F,
             0x15, 0x3F, 0x15,
             0x15, 0x3F, 0x3F,
             0x3F, 0x15, 0x15,
             0x3F, 0x15, 0x3F,
             0x3F, 0x3F, 0x15,
             0x3F, 0x3F, 0x3F
          };

          for (i = 0; i < 64; ++i)
          {
              outb(Private, 0x3c8, (UINT8)i);
              outb(Private, 0x3c9, s_a3bVgaDac[i*3 + 0]);
              outb(Private, 0x3c9, s_a3bVgaDac[i*3 + 1]);
              outb(Private, 0x3c9, s_a3bVgaDac[i*3 + 2]);
          }
     }

#endif
}

/**
  TODO: Add function description

  @param  Private TODO: add argument description

  TODO: add return values

**/
VOID
ClearScreen (
  VBOX_VGA_PRIVATE_DATA  *Private
  )
{
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL blt;
  blt.Blue     = 0;
  blt.Green    = 0;
  blt.Red      = 0;
  blt.Reserved = 0;
  Private->PciIo->Mem.Write (
                        Private->PciIo,
                        EfiPciIoWidthFillUint32,
                        0,
                        0,
                          Private->ModeData[Private->CurrentMode].HorizontalResolution
                        * Private->ModeData[Private->CurrentMode].VerticalResolution,
                        &blt
                        );
}

/**
  TODO: Add function description

  @param  Private TODO: add argument description

  TODO: add return values

**/
VOID
DrawLogo (
  VBOX_VGA_PRIVATE_DATA  *Private,
  UINTN                           ScreenWidth,
  UINTN                           ScreenHeight
  )
{
  DEBUG((DEBUG_INFO, "UGA is %a GOP is %a\n",
        FeaturePcdGet(PcdSupportUga) ? "on" : "off",
        FeaturePcdGet(PcdSupportGop) ? "on" : "off"
  ));
}

/**
  TODO: Add function description

  @param  Private TODO: add argument description
  @param  ModeData TODO: add argument description

  TODO: add return values

**/
VOID
InitializeGraphicsMode (
  VBOX_VGA_PRIVATE_DATA  *Private,
  VBOX_VGA_VIDEO_MODES   *ModeData
  )
{
  UINT16 DeviceId;
  EFI_STATUS Status;
  int i;

  Status = Private->PciIo->Pci.Read (
             Private->PciIo,
             EfiPciIoWidthUint16,
             PCI_DEVICE_ID_OFFSET,
             1,
             &DeviceId
             );
    outb(Private, 0x3c2, 0xc3);
    outb(Private, 0x3c4, 0x04);
    outb(Private, 0x3c5, 0x02);
  //
  // Read the PCI Configuration Header from the PCI Device
  //
#define BOUTB(storage, count, aport, dport)  \
     do {                                \
         for (i = 0 ; i < count; ++i)    \
         {                               \
             outb(Private, (aport), (UINT8)i);\
             outb(Private, (dport), storage[i]);    \
         }                               \
     } while (0)

  ASSERT_EFI_ERROR (Status);
  inb(Private, INPUT_STATUS_1_REGISTER);
  outb(Private, ATT_ADDRESS_REGISTER, 0);
  outb(Private, CRTC_ADDRESS_REGISTER, 0x11);
  outb(Private, CRTC_DATA_REGISTER, 0x0);
  /*
   * r0 = 1
   * boutb(1, 0x3c4, 0x3c5);
   */
  outb(Private, SEQ_ADDRESS_REGISTER, 0);
  outb(Private, SEQ_DATA_REGISTER, 1);

    outw(Private, 0x1ce, 0x00); outw(Private, 0x1cf, 0xb0c0);  // ENABLE
    outw(Private, 0x1ce, 0x04); outw(Private, 0x1cf, 0);  // ENABLE
    outw(Private, 0x1ce, 0x01); outw(Private, 0x1cf, (UINT16)ModeData->Width);    // XRES
    outw(Private, 0x1ce, 0x02); outw(Private, 0x1cf, (UINT16)ModeData->Height);    // YRES
    outw(Private, 0x1ce, 0x03); outw(Private, 0x1cf, (UINT16)ModeData->ColorDepth);  // BPP
    outw(Private, 0x1ce, 0x05); outw(Private, 0x1cf, 0);  // BANK
    outw(Private, 0x1ce, 0x06); outw(Private, 0x1cf, (UINT16)ModeData->Width);    // VIRT_WIDTH
    outw(Private, 0x1ce, 0x07); outw(Private, 0x1cf, (UINT16)ModeData->Height);    // VIRT_HEIGHT
    outw(Private, 0x1ce, 0x08); outw(Private, 0x1cf, 0);  // X_OFFSET
    outw(Private, 0x1ce, 0x09); outw(Private, 0x1cf, 0);  // Y_OFFSET
    outw(Private, 0x1ce, 0x04); outw(Private, 0x1cf, 1);  // ENABLE
    outb(Private, MISC_OUTPUT_REGISTER, ModeData->MiscSetting);
    BOUTB(ModeData->SeqSettings, 5, SEQ_ADDRESS_REGISTER, SEQ_DATA_REGISTER);
    /*
     * r0 = 3
     * boutb(1, 0x3c4, 0x3c5);
     */
  outb(Private, SEQ_ADDRESS_REGISTER, 0);
  outb(Private, SEQ_DATA_REGISTER, 3);

  BOUTB(ModeData->CrtcSettings, 25, CRTC_ADDRESS_REGISTER, CRTC_DATA_REGISTER);
  BOUTB(GraphicsController, 9, GRAPH_ADDRESS_REGISTER , GRAPH_DATA_REGISTER);

  inb (Private, INPUT_STATUS_1_REGISTER);

  BOUTB(AttributeController, 21, ATT_ADDRESS_REGISTER, ATT_ADDRESS_REGISTER);

  outw(Private, 0x1ce, 0x05); outw(Private, 0x1cf, 0x0);

  outb (Private, ATT_ADDRESS_REGISTER, 0x20);

#if 0
  outw (Private, GRAPH_ADDRESS_REGISTER, 0x0009);
  outw (Private, GRAPH_ADDRESS_REGISTER, 0x000a);
  outw (Private, GRAPH_ADDRESS_REGISTER, 0x000b);
  outb (Private, DAC_PIXEL_MASK_REGISTER, 0xff);

  SetDefaultPalette (Private);
#endif
  ClearScreen (Private);
}

/** Aka know as AppleGraphInfoProtocolGuid in other sources. */
#define EFI_UNKNOWN_2_PROTOCOL_GUID \
  { 0xE316E100, 0x0751, 0x4C49, {0x90, 0x56, 0x48, 0x6C, 0x7E, 0x47, 0x29, 0x03} }

EFI_GUID gEfiAppleFrameBufferInfoGuid = EFI_UNKNOWN_2_PROTOCOL_GUID;

EFI_STATUS EFIAPI
GetFrameBufferInfo(IN  APPLE_FRAMEBUFFERINFO_PROTOCOL   *This,
                   OUT UINT32                           *BaseAddr,
                   OUT UINT32                           *Something,
                   OUT UINT32                           *RowBytes,
                   OUT UINT32                           *Width,
                   OUT UINT32                           *Height,
                   OUT UINT32                           *Depth)
{
    /* @todo: figure out from current running mode */
    EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR     *FrameBufDesc;
    UINT32 W, H, BPP;
    VBOX_VGA_PRIVATE_DATA  *Private = This->Private;
    UINTN CurrentModeNumber = Private->CurrentMode;
    VBOX_VGA_MODE_DATA const *pCurrentMode = &Private->ModeData[CurrentModeNumber];

    W = pCurrentMode->HorizontalResolution;
    H = pCurrentMode->VerticalResolution;
    BPP = pCurrentMode->ColorDepth;
    DEBUG((DEBUG_INFO, "%a:%d GetFrameBufferInfo: %dx%d bpp:%d\n", __FILE__, __LINE__, W, H, BPP));

    Private->PciIo->GetBarAttributes (
                        Private->PciIo,
                        0,
                        NULL,
                        (VOID**) &FrameBufDesc
                        );


    /* EFI firmware remaps it here */
    *BaseAddr = (UINT32)FrameBufDesc->AddrRangeMin;
    *RowBytes = W * BPP / 8;
    *Width = W;
    *Height = H;
    *Depth = BPP;
    // what *Something shall be?

    return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
InitializeVBoxVga (
  IN EFI_HANDLE           ImageHandle,
  IN EFI_SYSTEM_TABLE     *SystemTable
  )
{
  EFI_STATUS              Status;

  Status = EfiLibInstallDriverBindingComponentName2 (
             ImageHandle,
             SystemTable,
             &gVBoxVgaDriverBinding,
             ImageHandle,
             &gVBoxVgaComponentName,
             &gVBoxVgaComponentName2
             );
  ASSERT_EFI_ERROR (Status);

  //
  // Install EFI Driver Supported EFI Version Protocol required for
  // EFI drivers that are on PCI and other plug in cards.
  //
  gVBoxVgaDriverSupportedEfiVersion.FirmwareVersion = PcdGet32 (PcdDriverSupportedEfiVersion);
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &ImageHandle,
                  &gEfiDriverSupportedEfiVersionProtocolGuid,
                  &gVBoxVgaDriverSupportedEfiVersion,
                  &gEfiAppleFrameBufferInfoGuid,
                  &gAppleFrameBufferInfo,
                  NULL
                  );
  ASSERT_EFI_ERROR (Status);

  return Status;
}
