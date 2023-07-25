/** @file
  This driver is a sample implementation of the Graphics Output Protocol for
  the VMware SVGA 3 video controller.

  Copyright (c) 2023, Oracle and/or its affiliates.<BR>
  Copyright (c) 2006 - 2019, Intel Corporation. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "VmwSvga3.h"
#include <IndustryStandard/Acpi.h>

EFI_DRIVER_BINDING_PROTOCOL  gVmwSvga3VideoDriverBinding = {
  VmwSvga3VideoControllerDriverSupported,
  VmwSvga3VideoControllerDriverStart,
  VmwSvga3VideoControllerDriverStop,
  0x10,
  NULL,
  NULL
};


/**
  Check if this device is supported.

  @param  This                   The driver binding protocol.
  @param  Controller             The controller handle to check.
  @param  RemainingDevicePath    The remaining device path.

  @retval EFI_SUCCESS            The bus supports this controller.
  @retval EFI_UNSUPPORTED        This device isn't supported.

**/
EFI_STATUS
EFIAPI
VmwSvga3VideoControllerDriverSupported (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   Controller,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  )
{
  EFI_STATUS           Status;
  EFI_PCI_IO_PROTOCOL  *PciIo;
  PCI_TYPE00           Pci;

  //
  // Open the PCI I/O Protocol
  //
  Status = gBS->OpenProtocol (
                  Controller,
                  &gEfiPciIoProtocolGuid,
                  (VOID **)&PciIo,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status)) {
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
    goto Done;
  }

  Status = EFI_UNSUPPORTED;
  if (!IS_PCI_DISPLAY (&Pci)) {
    goto Done;
  }

  if (   Pci.Hdr.ClassCode[1] == PCI_CLASS_DISPLAY_VGA
      && Pci.Hdr.VendorId == PCI_VENDOR_ID_VMWARE
      && Pci.Hdr.DeviceId == PCI_DEVICE_ID_VMWARE_SVGA3) {
    DEBUG ((DEBUG_INFO, "VmwSvga3Video: SVGA3 detected\n"));
    Status = EFI_SUCCESS;
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

  return Status;
}

/**
  Start to process the controller.

  @param  This                   The USB bus driver binding instance.
  @param  Controller             The controller to check.
  @param  RemainingDevicePath    The remaining device patch.

  @retval EFI_SUCCESS            The controller is controlled by the usb bus.
  @retval EFI_ALREADY_STARTED    The controller is already controlled by the usb
                                 bus.
  @retval EFI_OUT_OF_RESOURCES   Failed to allocate resources.

**/
EFI_STATUS
EFIAPI
VmwSvga3VideoControllerDriverStart (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   Controller,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  )
{
  EFI_TPL                     OldTpl;
  EFI_STATUS                  Status;
  VMWSVGA3_VIDEO_PRIVATE_DATA *Private;
  EFI_DEVICE_PATH_PROTOCOL    *ParentDevicePath;
  ACPI_ADR_DEVICE_PATH        AcpiDeviceNode;
  PCI_TYPE00                  Pci;
  EFI_PCI_IO_PROTOCOL         *ChildPciIo;

  OldTpl = gBS->RaiseTPL (TPL_CALLBACK);

  //
  // Allocate Private context data for GOP interface.
  //
  Private = AllocateZeroPool (sizeof (VMWSVGA3_VIDEO_PRIVATE_DATA));
  if (Private == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto RestoreTpl;
  }

  //
  // Set up context record
  //
  Private->Signature = VMWSVGA3_VIDEO_PRIVATE_DATA_SIGNATURE;

  //
  // Open PCI I/O Protocol
  //
  Status = gBS->OpenProtocol (
                  Controller,
                  &gEfiPciIoProtocolGuid,
                  (VOID **)&Private->PciIo,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status)) {
    goto FreePrivate;
  }

  //
  // Read the PCI Configuration Header from the PCI Device
  //
  Status = Private->PciIo->Pci.Read (
                                 Private->PciIo,
                                 EfiPciIoWidthUint32,
                                 0,
                                 sizeof (Pci) / sizeof (UINT32),
                                 &Pci
                                 );
  if (EFI_ERROR (Status)) {
    goto ClosePciIo;
  }

  //
  // Check whether this a supported device
  //
  if (   Pci.Hdr.ClassCode[1] != PCI_CLASS_DISPLAY_VGA
      || Pci.Hdr.VendorId != PCI_VENDOR_ID_VMWARE
      || Pci.Hdr.DeviceId != PCI_DEVICE_ID_VMWARE_SVGA3) {
    Status = EFI_DEVICE_ERROR;
    goto ClosePciIo;
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
    goto ClosePciIo;
  }

  //
  // Set new PCI attributes
  //
  Status = Private->PciIo->Attributes (
                             Private->PciIo,
                             EfiPciIoAttributeOperationEnable,
                             (EFI_PCI_DEVICE_ENABLE |
                              EFI_PCI_IO_ATTRIBUTE_BUS_MASTER),
                             NULL
                             );
  if (EFI_ERROR (Status)) {
    goto ClosePciIo;
  }

  //
  // Initialize the device.
  //
  Status = VmwSvga3DeviceInit(Private);
  if (EFI_ERROR (Status)) {
    goto RestoreAttributes;
  }

  //
  // Get ParentDevicePath
  //
  Status = gBS->HandleProtocol (
                  Controller,
                  &gEfiDevicePathProtocolGuid,
                  (VOID **)&ParentDevicePath
                  );
  if (EFI_ERROR (Status)) {
    goto RestoreAttributes;
  }

  //
  // Set Gop Device Path
  //
  ZeroMem (&AcpiDeviceNode, sizeof (ACPI_ADR_DEVICE_PATH));
  AcpiDeviceNode.Header.Type    = ACPI_DEVICE_PATH;
  AcpiDeviceNode.Header.SubType = ACPI_ADR_DP;
  AcpiDeviceNode.ADR            = ACPI_DISPLAY_ADR (1, 0, 0, 1, 0, ACPI_ADR_DISPLAY_TYPE_VGA, 0, 0);
  SetDevicePathNodeLength (&AcpiDeviceNode.Header, sizeof (ACPI_ADR_DEVICE_PATH));

  Private->GopDevicePath = AppendDevicePathNode (
                             ParentDevicePath,
                             (EFI_DEVICE_PATH_PROTOCOL *)&AcpiDeviceNode
                             );
  if (Private->GopDevicePath == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto RestoreAttributes;
  }

  //
  // Create new child handle and install the device path protocol on it.
  //
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Private->Handle,
                  &gEfiDevicePathProtocolGuid,
                  Private->GopDevicePath,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    goto FreeGopDevicePath;
  }

  //
  // Start the GOP software stack.
  //
  Status = VmwSvga3VideoGraphicsOutputConstructor (Private);
  if (EFI_ERROR (Status)) {
    goto UninstallGopDevicePath;
  }

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Private->Handle,
                  &gEfiGraphicsOutputProtocolGuid,
                  &Private->GraphicsOutput,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    goto DestructVmwSvga3VideoGraphics;
  }

  //
  // Reference parent handle from child handle.
  //
  Status = gBS->OpenProtocol (
                  Controller,
                  &gEfiPciIoProtocolGuid,
                  (VOID **)&ChildPciIo,
                  This->DriverBindingHandle,
                  Private->Handle,
                  EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER
                  );
  if (EFI_ERROR (Status)) {
    goto UninstallGop;
  }

  gBS->RestoreTPL (OldTpl);
  return EFI_SUCCESS;

UninstallGop:
  gBS->UninstallProtocolInterface (
         Private->Handle,
         &gEfiGraphicsOutputProtocolGuid,
         &Private->GraphicsOutput
         );

DestructVmwSvga3VideoGraphics:
  VmwSvga3VideoGraphicsOutputDestructor (Private);

UninstallGopDevicePath:
  gBS->UninstallProtocolInterface (
         Private->Handle,
         &gEfiDevicePathProtocolGuid,
         Private->GopDevicePath
         );

FreeGopDevicePath:
  FreePool (Private->GopDevicePath);

RestoreAttributes:
  Private->PciIo->Attributes (
                    Private->PciIo,
                    EfiPciIoAttributeOperationSet,
                    Private->OriginalPciAttributes,
                    NULL
                    );

ClosePciIo:
  gBS->CloseProtocol (
         Controller,
         &gEfiPciIoProtocolGuid,
         This->DriverBindingHandle,
         Controller
         );

FreePrivate:
  FreePool (Private);

RestoreTpl:
  gBS->RestoreTPL (OldTpl);

  return Status;
}

/**
  Stop this device

  @param  This                   The USB bus driver binding protocol.
  @param  Controller             The controller to release.
  @param  NumberOfChildren       The number of children of this device that
                                 opened the controller BY_CHILD.
  @param  ChildHandleBuffer      The array of child handle.

  @retval EFI_SUCCESS            The controller or children are stopped.
  @retval EFI_DEVICE_ERROR       Failed to stop the driver.

**/
EFI_STATUS
EFIAPI
VmwSvga3VideoControllerDriverStop (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   Controller,
  IN UINTN                        NumberOfChildren,
  IN EFI_HANDLE                   *ChildHandleBuffer
  )
{
  EFI_STATUS                    Status;
  EFI_GRAPHICS_OUTPUT_PROTOCOL  *GraphicsOutput;
  VMWSVGA3_VIDEO_PRIVATE_DATA   *Private;

  if (NumberOfChildren == 0) {
    //
    // Close the PCI I/O Protocol
    //
    gBS->CloseProtocol (
           Controller,
           &gEfiPciIoProtocolGuid,
           This->DriverBindingHandle,
           Controller
           );
    return EFI_SUCCESS;
  }

  //
  // free all resources for whose access we need the child handle, because the
  // child handle is going away
  //
  ASSERT (NumberOfChildren == 1);
  Status = gBS->OpenProtocol (
                  ChildHandleBuffer[0],
                  &gEfiGraphicsOutputProtocolGuid,
                  (VOID **)&GraphicsOutput,
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
  Private = VMWSVGA3_VIDEO_PRIVATE_DATA_FROM_GRAPHICS_OUTPUT_THIS (GraphicsOutput);
  ASSERT (Private->Handle == ChildHandleBuffer[0]);

  VmwSvga3VideoGraphicsOutputDestructor (Private);
  //
  // Remove the GOP protocol interface from the system
  //
  Status = gBS->UninstallMultipleProtocolInterfaces (
                  Private->Handle,
                  &gEfiGraphicsOutputProtocolGuid,
                  &Private->GraphicsOutput,
                  NULL
                  );

  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Teardown the device.
  //
  Status = VmwSvga3DeviceUninit (Private);
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

  gBS->CloseProtocol (
         Controller,
         &gEfiPciIoProtocolGuid,
         This->DriverBindingHandle,
         Private->Handle
         );

  gBS->UninstallProtocolInterface (
         Private->Handle,
         &gEfiDevicePathProtocolGuid,
         Private->GopDevicePath
         );
  FreePool (Private->GopDevicePath);

  //
  // Free our instance data
  //
  gBS->FreePool (Private);

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
InitializeVmwSvga3Video (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;

  Status = EfiLibInstallDriverBindingComponentName2 (
             ImageHandle,
             SystemTable,
             &gVmwSvga3VideoDriverBinding,
             ImageHandle,
             &gVmwSvga3VideoComponentName,
             &gVmwSvga3VideoComponentName2
             );
  ASSERT_EFI_ERROR (Status);

  return Status;
}
