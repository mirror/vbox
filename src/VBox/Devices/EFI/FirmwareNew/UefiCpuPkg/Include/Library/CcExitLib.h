/** @file
  Public header file for the CcExitLib.

  This library class defines some routines used for below CcExit handler.
   - Invoking the VMGEXIT instruction in support of SEV-ES and to handle
     #VC exceptions.
   - Handle #VE exception in TDX.

  Copyright (C) 2020, Advanced Micro Devices, Inc. All rights reserved.<BR>
  Copyright (c) 2020 - 2022, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef CC_EXIT_LIB_H_
#define CC_EXIT_LIB_H_

#include <Protocol/DebugSupport.h>
#include <Register/Amd/Ghcb.h>

#define VE_EXCEPTION  20

/**
  Perform VMGEXIT.

  Sets the necessary fields of the GHCB, invokes the VMGEXIT instruction and
  then handles the return actions.

  @param[in, out]  Ghcb       A pointer to the GHCB
  @param[in]       ExitCode   VMGEXIT code to be assigned to the SwExitCode
                              field of the GHCB.
  @param[in]       ExitInfo1  VMGEXIT information to be assigned to the
                              SwExitInfo1 field of the GHCB.
  @param[in]       ExitInfo2  VMGEXIT information to be assigned to the
                              SwExitInfo2 field of the GHCB.

  @retval  0                  VMGEXIT succeeded.
  @return                     Exception number to be propagated, VMGEXIT
                              processing did not succeed.

**/
UINT64
EFIAPI
CcExitVmgExit (
  IN OUT GHCB    *Ghcb,
  IN     UINT64  ExitCode,
  IN     UINT64  ExitInfo1,
  IN     UINT64  ExitInfo2
  );

/**
  Perform pre-VMGEXIT initialization/preparation.

  Performs the necessary steps in preparation for invoking VMGEXIT. Must be
  called before setting any fields within the GHCB.

  @param[in, out]  Ghcb            A pointer to the GHCB
  @param[in, out]  InterruptState  A pointer to hold the current interrupt
                                   state, used for restoring in CcExitVmgDone ()

**/
VOID
EFIAPI
CcExitVmgInit (
  IN OUT GHCB     *Ghcb,
  IN OUT BOOLEAN  *InterruptState
  );

/**
  Perform post-VMGEXIT cleanup.

  Performs the necessary steps to cleanup after invoking VMGEXIT. Must be
  called after obtaining needed fields within the GHCB.

  @param[in, out]  Ghcb            A pointer to the GHCB
  @param[in]       InterruptState  An indicator to conditionally (re)enable
                                   interrupts

**/
VOID
EFIAPI
CcExitVmgDone (
  IN OUT GHCB     *Ghcb,
  IN     BOOLEAN  InterruptState
  );

/**
  Marks a specified offset as valid in the GHCB.

  The ValidBitmap area represents the areas of the GHCB that have been marked
  valid. Set the bit in ValidBitmap for the input offset.

  @param[in, out]  Ghcb       A pointer to the GHCB
  @param[in]       Offset     Qword offset in the GHCB to mark valid

**/
VOID
EFIAPI
CcExitVmgSetOffsetValid (
  IN OUT GHCB           *Ghcb,
  IN     GHCB_REGISTER  Offset
  );

/**
  Checks if a specified offset is valid in the GHCB.

  The ValidBitmap area represents the areas of the GHCB that have been marked
  valid. Return whether the bit in the ValidBitmap is set for the input offset.

  @param[in]  Ghcb            A pointer to the GHCB
  @param[in]  Offset          Qword offset in the GHCB to mark valid

  @retval TRUE                Offset is marked valid in the GHCB
  @retval FALSE               Offset is not marked valid in the GHCB

**/
BOOLEAN
EFIAPI
CcExitVmgIsOffsetValid (
  IN GHCB           *Ghcb,
  IN GHCB_REGISTER  Offset
  );

/**
  Handle a #VC exception.

  Performs the necessary processing to handle a #VC exception.

  The base library function returns an error equal to VC_EXCEPTION,
  to be propagated to the standard exception handling stack.

  @param[in, out]  ExceptionType  Pointer to an EFI_EXCEPTION_TYPE to be set
                                  as value to use on error.
  @param[in, out]  SystemContext  Pointer to EFI_SYSTEM_CONTEXT

  @retval  EFI_SUCCESS            Exception handled
  @retval  EFI_UNSUPPORTED        #VC not supported, (new) exception value to
                                  propagate provided
  @retval  EFI_PROTOCOL_ERROR     #VC handling failed, (new) exception value to
                                  propagate provided

**/
EFI_STATUS
EFIAPI
CcExitHandleVc (
  IN OUT EFI_EXCEPTION_TYPE  *ExceptionType,
  IN OUT EFI_SYSTEM_CONTEXT  SystemContext
  );

/**
  Handle a #VE exception.

  Performs the necessary processing to handle a #VE exception.

  The base library function returns an error equal to VE_EXCEPTION,
  to be propagated to the standard exception handling stack.

  @param[in, out]  ExceptionType  Pointer to an EFI_EXCEPTION_TYPE to be set
                                  as value to use on error.
  @param[in, out]  SystemContext  Pointer to EFI_SYSTEM_CONTEXT

  @retval  EFI_SUCCESS            Exception handled
  @retval  EFI_UNSUPPORTED        #VE not supported, (new) exception value to
                                  propagate provided
  @retval  EFI_PROTOCOL_ERROR     #VE handling failed, (new) exception value to
                                  propagate provided

**/
EFI_STATUS
EFIAPI
CcExitHandleVe (
  IN OUT EFI_EXCEPTION_TYPE  *ExceptionType,
  IN OUT EFI_SYSTEM_CONTEXT  SystemContext
  );

#endif
