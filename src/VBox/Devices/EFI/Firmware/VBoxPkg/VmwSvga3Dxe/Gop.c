/** @file
  Graphics Output Protocol functions for the VMware SVGA 3 video controller.

  Copyright (c) 2023, Oracle and/or its affiliates.<BR>
  Copyright (c) 2007 - 2018, Intel Corporation. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "VmwSvga3.h"

#define GRAPHICS_OUTPUT_INVALID_MODE_NUMBER  0xffff

STATIC EFI_GRAPHICS_OUTPUT_MODE_INFORMATION  mVmwSvga3ModeInfo[] = {
  {
    0,    // Version
    640,  // HorizontalResolution
    480,  // VerticalResolution
  },{
    0,    // Version
    800,  // HorizontalResolution
    600,  // VerticalResolution
  },{
    0,    // Version
    1024, // HorizontalResolution
    768,  // VerticalResolution
  }
  ,
  {
    0,    // Version
    1280, // HorizontalResolution
     720, // VerticalResolution
  },
  {
    0,    // Version
    1600, // HorizontalResolution
     900, // VerticalResolution
  },
  {
    0,    // Version
    1920, // HorizontalResolution
    1080, // VerticalResolution
  },
  {
    0,    // Version
    2560, // HorizontalResolution
    1440, // VerticalResolution
  }
};

STATIC
VOID
VmwSvga3VideoCompleteModeInfo (
  OUT EFI_GRAPHICS_OUTPUT_MODE_INFORMATION  *Info
  )
{
  Info->Version = 0;

  DEBUG ((DEBUG_INFO, "PixelBlueGreenRedReserved8BitPerColor\n"));
  Info->PixelFormat                   = PixelBlueGreenRedReserved8BitPerColor;
  Info->PixelInformation.RedMask      = 0;
  Info->PixelInformation.GreenMask    = 0;
  Info->PixelInformation.BlueMask     = 0;
  Info->PixelInformation.ReservedMask = 0;
  Info->PixelsPerScanLine = Info->HorizontalResolution;
}

STATIC
EFI_STATUS
VmwSvga3VideoCompleteModeData (
  IN  VMWSVGA3_VIDEO_PRIVATE_DATA        *Private,
  OUT EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE  *Mode
  )
{
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION  *Info;
  EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR     *FrameBufDesc;

  Info = Mode->Info;
  VmwSvga3VideoCompleteModeInfo (Info);

  Private->PciIo->GetBarAttributes (
                    Private->PciIo,
                    Private->FrameBufferVramBarIndex,
                    NULL,
                    (VOID **)&FrameBufDesc
                    );

  Mode->FrameBufferBase = FrameBufDesc->AddrRangeMin;
  Mode->FrameBufferSize = Info->HorizontalResolution * Info->VerticalResolution;
  Mode->FrameBufferSize = Mode->FrameBufferSize * 4;
  Mode->FrameBufferSize = EFI_PAGES_TO_SIZE (
                            EFI_SIZE_TO_PAGES (Mode->FrameBufferSize)
                            );
  DEBUG ((
    DEBUG_INFO,
    "FrameBufferBase: 0x%Lx, FrameBufferSize: 0x%Lx\n",
    Mode->FrameBufferBase,
    (UINT64)Mode->FrameBufferSize
    ));

  FreePool (FrameBufDesc);
  return EFI_SUCCESS;
}

//
// Graphics Output Protocol Member Functions
//
EFI_STATUS
EFIAPI
VmwSvga3VideoGraphicsOutputQueryMode (
  IN  EFI_GRAPHICS_OUTPUT_PROTOCOL          *This,
  IN  UINT32                                ModeNumber,
  OUT UINTN                                 *SizeOfInfo,
  OUT EFI_GRAPHICS_OUTPUT_MODE_INFORMATION  **Info
  )

/*++

Routine Description:

  Graphics Output protocol interface to query video mode

  Arguments:
    This                  - Protocol instance pointer.
    ModeNumber            - The mode number to return information on.
    Info                  - Caller allocated buffer that returns information about ModeNumber.
    SizeOfInfo            - A pointer to the size, in bytes, of the Info buffer.

  Returns:
    EFI_SUCCESS           - Mode information returned.
    EFI_BUFFER_TOO_SMALL  - The Info buffer was too small.
    EFI_DEVICE_ERROR      - A hardware error occurred trying to retrieve the video mode.
    EFI_NOT_STARTED       - Video display is not initialized. Call SetMode ()
    EFI_INVALID_PARAMETER - One of the input args was NULL.

--*/
{
  if ((Info == NULL) || (SizeOfInfo == NULL) || (ModeNumber >= ARRAY_SIZE(mVmwSvga3ModeInfo))) {
    return EFI_INVALID_PARAMETER;
  }

  *Info = AllocatePool (sizeof (EFI_GRAPHICS_OUTPUT_MODE_INFORMATION));
  if (*Info == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  *SizeOfInfo = sizeof (EFI_GRAPHICS_OUTPUT_MODE_INFORMATION);

  (*Info)->HorizontalResolution = mVmwSvga3ModeInfo[ModeNumber].HorizontalResolution;
  (*Info)->VerticalResolution   = mVmwSvga3ModeInfo[ModeNumber].VerticalResolution;
  VmwSvga3VideoCompleteModeInfo (*Info);

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
VmwSvga3VideoGraphicsOutputSetMode (
  IN  EFI_GRAPHICS_OUTPUT_PROTOCOL  *This,
  IN  UINT32                        ModeNumber
  )

/*++

Routine Description:

  Graphics Output protocol interface to set video mode

  Arguments:
    This             - Protocol instance pointer.
    ModeNumber       - The mode number to be set.

  Returns:
    EFI_SUCCESS      - Graphics mode was changed.
    EFI_DEVICE_ERROR - The device had an error and could not complete the request.
    EFI_UNSUPPORTED  - ModeNumber is not supported by this device.

--*/
{
  VMWSVGA3_VIDEO_PRIVATE_DATA    *Private;
  RETURN_STATUS                  Status;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Black;

  Private = VMWSVGA3_VIDEO_PRIVATE_DATA_FROM_GRAPHICS_OUTPUT_THIS (This);

  if (ModeNumber >= ARRAY_SIZE(mVmwSvga3ModeInfo)) {
    return EFI_UNSUPPORTED;
  }

  /** @todo */

  This->Mode->Mode                       = ModeNumber;
  This->Mode->Info->HorizontalResolution = mVmwSvga3ModeInfo[ModeNumber].HorizontalResolution;
  This->Mode->Info->VerticalResolution   = mVmwSvga3ModeInfo[ModeNumber].VerticalResolution;
  This->Mode->SizeOfInfo                 = sizeof (EFI_GRAPHICS_OUTPUT_MODE_INFORMATION);

  VmwSvga3VideoCompleteModeData (Private, This->Mode);

  //
  // Re-initialize the frame buffer configure when mode changes.
  //
  Status = FrameBufferBltConfigure (
             (VOID *)(UINTN)This->Mode->FrameBufferBase,
             This->Mode->Info,
             Private->FrameBufferBltConfigure,
             &Private->FrameBufferBltConfigureSize
             );
  if (Status == RETURN_BUFFER_TOO_SMALL) {
    //
    // Frame buffer configure may be larger in new mode.
    //
    if (Private->FrameBufferBltConfigure != NULL) {
      FreePool (Private->FrameBufferBltConfigure);
    }

    Private->FrameBufferBltConfigure =
      AllocatePool (Private->FrameBufferBltConfigureSize);
    ASSERT (Private->FrameBufferBltConfigure != NULL);

    //
    // Create the configuration for FrameBufferBltLib
    //
    Status = FrameBufferBltConfigure (
               (VOID *)(UINTN)This->Mode->FrameBufferBase,
               This->Mode->Info,
               Private->FrameBufferBltConfigure,
               &Private->FrameBufferBltConfigureSize
               );
  }

  ASSERT (Status == RETURN_SUCCESS);

  //
  // Per UEFI Spec, need to clear the visible portions of the output display to black.
  //
  ZeroMem (&Black, sizeof (Black));
  Status = FrameBufferBlt (
             Private->FrameBufferBltConfigure,
             &Black,
             EfiBltVideoFill,
             0,
             0,
             0,
             0,
             This->Mode->Info->HorizontalResolution,
             This->Mode->Info->VerticalResolution,
             0
             );
  ASSERT_RETURN_ERROR (Status);

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
VmwSvga3VideoGraphicsOutputBlt (
  IN  EFI_GRAPHICS_OUTPUT_PROTOCOL       *This,
  IN  EFI_GRAPHICS_OUTPUT_BLT_PIXEL      *BltBuffer  OPTIONAL,
  IN  EFI_GRAPHICS_OUTPUT_BLT_OPERATION  BltOperation,
  IN  UINTN                              SourceX,
  IN  UINTN                              SourceY,
  IN  UINTN                              DestinationX,
  IN  UINTN                              DestinationY,
  IN  UINTN                              Width,
  IN  UINTN                              Height,
  IN  UINTN                              Delta
  )

/*++

Routine Description:

  Graphics Output protocol instance to block transfer for CirrusLogic device

Arguments:

  This          - Pointer to Graphics Output protocol instance
  BltBuffer     - The data to transfer to screen
  BltOperation  - The operation to perform
  SourceX       - The X coordinate of the source for BltOperation
  SourceY       - The Y coordinate of the source for BltOperation
  DestinationX  - The X coordinate of the destination for BltOperation
  DestinationY  - The Y coordinate of the destination for BltOperation
  Width         - The width of a rectangle in the blt rectangle in pixels
  Height        - The height of a rectangle in the blt rectangle in pixels
  Delta         - Not used for EfiBltVideoFill and EfiBltVideoToVideo operation.
                  If a Delta of 0 is used, the entire BltBuffer will be operated on.
                  If a subrectangle of the BltBuffer is used, then Delta represents
                  the number of bytes in a row of the BltBuffer.

Returns:

  EFI_INVALID_PARAMETER - Invalid parameter passed in
  EFI_SUCCESS - Blt operation success

--*/
{
  EFI_STATUS               Status;
  EFI_TPL                  OriginalTPL;
  VMWSVGA3_VIDEO_PRIVATE_DATA  *Private;

  Private = VMWSVGA3_VIDEO_PRIVATE_DATA_FROM_GRAPHICS_OUTPUT_THIS (This);
  //
  // We have to raise to TPL Notify, so we make an atomic write the frame buffer.
  // We would not want a timer based event (Cursor, ...) to come in while we are
  // doing this operation.
  //
  OriginalTPL = gBS->RaiseTPL (TPL_NOTIFY);

  switch (BltOperation) {
    case EfiBltVideoToBltBuffer:
    case EfiBltBufferToVideo:
    case EfiBltVideoFill:
    case EfiBltVideoToVideo:
      Status = FrameBufferBlt (
                 Private->FrameBufferBltConfigure,
                 BltBuffer,
                 BltOperation,
                 SourceX,
                 SourceY,
                 DestinationX,
                 DestinationY,
                 Width,
                 Height,
                 Delta
                 );
      break;

    default:
      Status = EFI_INVALID_PARAMETER;
      break;
  }

  /** @todo Update screen. */

  gBS->RestoreTPL (OriginalTPL);

  return Status;
}

EFI_STATUS
VmwSvga3VideoGraphicsOutputConstructor (
  VMWSVGA3_VIDEO_PRIVATE_DATA  *Private
  )
{
  EFI_STATUS                    Status;
  EFI_GRAPHICS_OUTPUT_PROTOCOL  *GraphicsOutput;

  GraphicsOutput            = &Private->GraphicsOutput;
  GraphicsOutput->QueryMode = VmwSvga3VideoGraphicsOutputQueryMode;
  GraphicsOutput->SetMode   = VmwSvga3VideoGraphicsOutputSetMode;
  GraphicsOutput->Blt       = VmwSvga3VideoGraphicsOutputBlt;

  //
  // Initialize the private data
  //
  Status = gBS->AllocatePool (
                  EfiBootServicesData,
                  sizeof (EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE),
                  (VOID **)&Private->GraphicsOutput.Mode
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = gBS->AllocatePool (
                  EfiBootServicesData,
                  sizeof (EFI_GRAPHICS_OUTPUT_MODE_INFORMATION),
                  (VOID **)&Private->GraphicsOutput.Mode->Info
                  );
  if (EFI_ERROR (Status)) {
    goto FreeMode;
  }

  Private->GraphicsOutput.Mode->MaxMode = ARRAY_SIZE (mVmwSvga3ModeInfo);
  Private->GraphicsOutput.Mode->Mode    = GRAPHICS_OUTPUT_INVALID_MODE_NUMBER;
  Private->FrameBufferBltConfigure      = NULL;
  Private->FrameBufferBltConfigureSize  = 0;

  //
  // Initialize the hardware
  //
  Status = GraphicsOutput->SetMode (GraphicsOutput, 0);
  if (EFI_ERROR (Status)) {
    goto FreeInfo;
  }

  return EFI_SUCCESS;

FreeInfo:
  FreePool (Private->GraphicsOutput.Mode->Info);

FreeMode:
  FreePool (Private->GraphicsOutput.Mode);
  Private->GraphicsOutput.Mode = NULL;

  return Status;
}

EFI_STATUS
VmwSvga3VideoGraphicsOutputDestructor (
  VMWSVGA3_VIDEO_PRIVATE_DATA  *Private
  )

/*++

Routine Description:

Arguments:

Returns:

  None

--*/
{
  if (Private->FrameBufferBltConfigure != NULL) {
    FreePool (Private->FrameBufferBltConfigure);
  }

  if (Private->GraphicsOutput.Mode != NULL) {
    if (Private->GraphicsOutput.Mode->Info != NULL) {
      gBS->FreePool (Private->GraphicsOutput.Mode->Info);
    }

    gBS->FreePool (Private->GraphicsOutput.Mode);
  }

  return EFI_SUCCESS;
}
