/** @file

  Copyright (c) 2014 - 2022, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _SEC_CORE_H_
#define _SEC_CORE_H_

#include <PiPei.h>
#include <Ppi/TemporaryRamSupport.h>

#include <Library/BaseLib.h>
#include <Library/IoLib.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PciCf8Lib.h>
#include <Library/SerialPortLib.h>
#include <Library/FspSwitchStackLib.h>
#include <Library/FspCommonLib.h>
#include <Library/CpuLib.h>
#include <FspEas.h>

typedef
VOID
(EFIAPI *PEI_CORE_ENTRY)(
  IN CONST  EFI_SEC_PEI_HAND_OFF    *SecCoreData,
  IN CONST  EFI_PEI_PPI_DESCRIPTOR  *PpiList
  );

typedef struct _SEC_IDT_TABLE {
  //
  // Reserved 8 bytes preceding IDT to store EFI_PEI_SERVICES**, since IDT base
  // address should be 8-byte alignment.
  // Note: For IA32, only the 4 bytes immediately preceding IDT is used to store
  // EFI_PEI_SERVICES**
  //
  UINT64                      PeiService;
  IA32_IDT_GATE_DESCRIPTOR    IdtTable[FixedPcdGet8 (PcdFspMaxInterruptSupported)];
} SEC_IDT_TABLE;

/**
  Switch the stack in the temporary memory to the one in the permanent memory.

  This function must be invoked after the memory migration immediately. The relative
  position of the stack in the temporary and permanent memory is same.

  @param[in] TemporaryMemoryBase  Base address of the temporary memory.
  @param[in] PermenentMemoryBase  Base address of the permanent memory.
**/
VOID
EFIAPI
SecSwitchStack (
  IN UINTN  TemporaryMemoryBase,
  IN UINTN  PermenentMemoryBase
  );

/**
  This service of the TEMPORARY_RAM_SUPPORT_PPI that migrates temporary RAM into
  permanent memory.

  @param[in] PeiServices            Pointer to the PEI Services Table.
  @param[in] TemporaryMemoryBase    Source Address in temporary memory from which the SEC or PEIM will copy the
                                Temporary RAM contents.
  @param[in] PermanentMemoryBase    Destination Address in permanent memory into which the SEC or PEIM will copy the
                                Temporary RAM contents.
  @param[in] CopySize               Amount of memory to migrate from temporary to permanent memory.

  @retval EFI_SUCCESS           The data was successfully returned.
  @retval EFI_INVALID_PARAMETER PermanentMemoryBase + CopySize > TemporaryMemoryBase when
                                TemporaryMemoryBase > PermanentMemoryBase.

**/
EFI_STATUS
EFIAPI
SecTemporaryRamSupport (
  IN CONST EFI_PEI_SERVICES  **PeiServices,
  IN EFI_PHYSICAL_ADDRESS    TemporaryMemoryBase,
  IN EFI_PHYSICAL_ADDRESS    PermanentMemoryBase,
  IN UINTN                   CopySize
  );

/**

  Entry point to the C language phase of SEC. After the SEC assembly
  code has initialized some temporary memory and set up the stack,
  the control is transferred to this function.


  @param[in] SizeOfRam          Size of the temporary memory available for use.
  @param[in] TempRamBase        Base address of temporary ram
  @param[in] BootFirmwareVolume Base address of the Boot Firmware Volume.
  @param[in] PeiCore            PeiCore entry point.
  @param[in] BootLoaderStack    BootLoader stack.
  @param[in] ApiIdx             the index of API.

  @return This function never returns.

**/
VOID
EFIAPI
SecStartup (
  IN UINT32          SizeOfRam,
  IN UINT32          TempRamBase,
  IN VOID            *BootFirmwareVolume,
  IN PEI_CORE_ENTRY  PeiCore,
  IN UINTN           BootLoaderStack,
  IN UINT32          ApiIdx
  );

/**

  Return value of esp.

  @return  value of esp.

**/
UINTN
EFIAPI
AsmReadStackPointer (
  VOID
  );

#endif
