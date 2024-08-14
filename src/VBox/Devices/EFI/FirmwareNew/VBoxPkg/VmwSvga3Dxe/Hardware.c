/** @file
  This driver is a sample implementation of the Graphics Output Protocol for
  the VMware SVGA 3 video controller.

  Copyright (c) 2023, Oracle and/or its affiliates.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "VmwSvga3.h"


/**
  Read 4-bytes width VMware SVGA3 MMIO register.

  @param  VmwSvga3     The VMware SVGA3 Instance.
  @param  Offset       The offset of the 4-bytes width operational register.

  @return The register content read.
  @retval If err, return 0xFFFFFFFF.

**/
UINT32
VmwSvga3ReadReg (
  IN  VMWSVGA3_VIDEO_PRIVATE_DATA  *VmwSvga3,
  IN  UINT32                       Offset
  )
{
  UINT32      Data;
  EFI_STATUS  Status;

  Status = VmwSvga3->PciIo->Mem.Read (
                                      VmwSvga3->PciIo,
                                      EfiPciIoWidthUint32,
                                      VMWSVGA3_MMIO_BAR,
                                      Offset,
                                      1,
                                      &Data
                                      );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "VmwSvga3ReadReg: Pci Io Read error - %r at %d\n", Status, Offset));
    Data = 0xFFFFFFFF;
  }

  return Data;
}

/**
  Write the data to the 4-bytes width VMware SVGA3 MMIO register.

  @param  VmwSvga3 The VMware SVGA3 Instance.
  @param  Offset   The offset of the 4-bytes width operational register.
  @param  Data     The data to write.

**/
VOID
VmwSvga3WriteReg (
  IN  VMWSVGA3_VIDEO_PRIVATE_DATA  *VmwSvga3,
  IN UINT32                        Offset,
  IN UINT32                        Data
  )
{
  EFI_STATUS  Status;

  Status = VmwSvga3->PciIo->Mem.Write (
                                       VmwSvga3->PciIo,
                                       EfiPciIoWidthUint32,
                                       VMWSVGA3_MMIO_BAR,
                                       Offset,
                                       1,
                                       &Data
                                       );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "VmwSvga3WriteReg: Pci Io Write error: %r at %d\n", Status, Offset));
  }
}

/**
  Initializes the given VMware SVGA3 video controller instance.

  @param  This                   The device instance.

  @retval EFI_SUCCESS            SVGA3 controller successfully initialised.
  @retval EFI_UNSUPPORTED        This device isn't supported.
  @retval EFI_OUT_OF_RESOURCES   Failed to allocate resources.

**/
EFI_STATUS
VmwSvga3DeviceInit(
                   IN  VMWSVGA3_VIDEO_PRIVATE_DATA  *This
                  )
{
  UINTN NumberOfBytes = EFI_PAGES_TO_SIZE(1);
  VMWSVGA3_DC_CMD_START_STOP_CTX *CmdDcStart;
  EFI_STATUS Status;

  VmwSvga3WriteReg(This, VMWSVGA3_REG_ID, VMWSVGA3_REG_ID_SVGA3);

  if (VmwSvga3ReadReg(This, VMWSVGA3_REG_ID) != VMWSVGA3_REG_ID_SVGA3)
    return EFI_UNSUPPORTED;

  Status = This->PciIo->AllocateBuffer(This->PciIo, AllocateAnyPages,
                                       EfiRuntimeServicesData, 1, (VOID **)&This->CmdBufHdr, 0);
  if (EFI_ERROR(Status)) {
    DEBUG ((DEBUG_ERROR, "VmwSvga3DeviceInit: memory allocation failed\n"));
    return Status;
  }

  Status = This->PciIo->Map (
                           This->PciIo,
                           EfiPciIoOperationBusMasterCommonBuffer,
                           This->CmdBufHdr,
                           &NumberOfBytes,
                           &This->PhysicalAddressCmdBufHdr,
                           &This->CmdBufMapping
                           );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "VmwSvga3DeviceInit: Status=%x\n", Status));
    return Status;
  }

  This->PhysicalAddressCmdBuf   = This->PhysicalAddressCmdBufHdr + sizeof(*This->CmdBufHdr);
  This->CmdBuf                  = This->CmdBufHdr + 1;
  This->FrameBufferVramBarIndex = VMWSVGA3_VRAM_BAR;

  VmwSvga3WriteReg(This, VMWSVGA3_REG_IRQMASK,     0); /* No interrupts enabled. */
  VmwSvga3WriteReg(This, VMWSVGA3_REG_IRQ_STATUS,  0);
  VmwSvga3WriteReg(This, VMWSVGA3_REG_ENABLE,      0);
  VmwSvga3WriteReg(This, VMWSVGA3_REG_CONFIG_DONE, 0);

  //
  // Set up the device context
  //
  CmdDcStart = (VMWSVGA3_DC_CMD_START_STOP_CTX *)This->CmdBuf;

  CmdDcStart->CmdId    = VMWSVGA3_CMD_DC_START_STOP_CTX;
  CmdDcStart->Enable   = 1;
  CmdDcStart->Context  = VMWSVGA3_CB_CTX_0;

  Status = VmwSvga3CmdBufProcess(This, sizeof(VMWSVGA3_DC_CMD_START_STOP_CTX), VMWSVGA3_CB_CTX_DEVICE);
  if (EFI_ERROR (Status)) {
    This->PciIo->Unmap (This->PciIo, This->CmdBufMapping);
    This->PciIo->FreeBuffer (This->PciIo, 1, This->CmdBufHdr);
    return Status;
  }

  return EFI_SUCCESS;
}

/**
  Uninitializes the given VMware SVGA3 video controller instance, freeing all resources
  acquired in VmwSvga3DeviceInit().

  @param  This                   The device instance.

  @retval EFI_SUCCESS            SVGA3 controller successfully destroyed.

**/
EFI_STATUS
VmwSvga3DeviceUninit (
  IN  VMWSVGA3_VIDEO_PRIVATE_DATA  *This
  )
{
  EFI_STATUS Status;

  //
  // Teardown device context
  //
  VMWSVGA3_DC_CMD_START_STOP_CTX *CmdDcStart = (VMWSVGA3_DC_CMD_START_STOP_CTX *)This->CmdBuf;

  CmdDcStart->CmdId    = VMWSVGA3_CMD_DC_START_STOP_CTX;
  CmdDcStart->Enable   = 0;
  CmdDcStart->Context  = VMWSVGA3_CB_CTX_0;

  Status = VmwSvga3CmdBufProcess(This, sizeof(VMWSVGA3_DC_CMD_START_STOP_CTX), VMWSVGA3_CB_CTX_DEVICE);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "VmwSvga3DeviceUninit: Disabling device context failed -> Status=%x\n", Status));
  }

  This->PciIo->Unmap (This->PciIo, This->CmdBufMapping);
  This->PciIo->FreeBuffer (This->PciIo, 1, This->CmdBufHdr);

  return EFI_SUCCESS;
}

EFI_STATUS
VmwSvga3CmdBufProcess (
  IN  VMWSVGA3_VIDEO_PRIVATE_DATA  *This,
  IN  UINTN                        NumberOfBytes,
  IN  UINT32                       Context
  )
{
  EFI_STATUS                Status;
  VMWSVGA3_CB_HDR           *CmdBufHdr = This->CmdBufHdr;

  CmdBufHdr->Status          = VMWSVGA3_CB_STATUS_NONE;
  CmdBufHdr->Flags           = VMWSVGA3_CB_FLAG_NO_IRQ;
  CmdBufHdr->PhysicalAddress = This->PhysicalAddressCmdBuf;
  CmdBufHdr->Length          = NumberOfBytes;

  DEBUG ((DEBUG_INFO, "VmwSvga3CmdBufProcess: PhysicalAddressCmdBufHdr=%Lx Context=%x\n", This->PhysicalAddressCmdBufHdr, Context));

  VmwSvga3WriteReg(This, VMWSVGA3_REG_COMMAND_HIGH, (UINT32)(This->PhysicalAddressCmdBufHdr >> 32));
  VmwSvga3WriteReg(This, VMWSVGA3_REG_COMMAND_LOW,  (UINT32)This->PhysicalAddressCmdBufHdr | Context);

  //
  // Wait until the command buffer has completed.
  //
  while (CmdBufHdr->Status == VMWSVGA3_CB_STATUS_NONE);

  if (CmdBufHdr->Status == VMWSVGA3_CB_STATUS_COMPLETED)
    Status = EFI_SUCCESS;
  else
    Status = EFI_ABORTED;

  return Status;
}

