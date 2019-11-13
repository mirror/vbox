/** @file
  C based implemention of IA32 interrupt handling only
  requiring a minimal assembly interrupt entry point.

  Copyright (c) 2006 - 2015, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "CpuDxe.h"
#include "CpuGdt.h"

//
// Global descriptor table (GDT) Template
//
STATIC GDT_ENTRIES GdtTemplate = {
  //
  // NULL_SEL
  //
  {
    0x0,            // limit 15:0
    0x0,            // base 15:0
    0x0,            // base 23:16
    0x0,            // type
    0x0,            // limit 19:16, flags
    0x0,            // base 31:24
  },
  //
  // LINEAR_SEL
  //
  {
    0x0FFFF,        // limit 15:0
    0x0,            // base 15:0
    0x0,            // base 23:16
    0x092,          // present, ring 0, data, read/write
    0x0CF,          // page-granular, 32-bit
    0x0,
  },
  //
  // LINEAR_CODE_SEL
  //
  {
    0x0FFFF,        // limit 15:0
    0x0,            // base 15:0
    0x0,            // base 23:16
    0x09F,          // present, ring 0, code, execute/read, conforming, accessed
    0x0CF,          // page-granular, 32-bit
    0x0,
  },
  //
  // SYS_DATA_SEL
  //
  {
    0x0FFFF,        // limit 15:0
    0x0,            // base 15:0
    0x0,            // base 23:16
    0x093,          // present, ring 0, data, read/write, accessed
    0x0CF,          // page-granular, 32-bit
    0x0,
  },
  //
  // SYS_CODE_SEL
  //
  {
    0x0FFFF,        // limit 15:0
    0x0,            // base 15:0
    0x0,            // base 23:16
    0x09A,          // present, ring 0, code, execute/read
    0x0CF,          // page-granular, 32-bit
    0x0,
  },
  //
  // SPARE4_SEL
  //
  {
    0x0,            // limit 15:0
    0x0,            // base 15:0
    0x0,            // base 23:16
    0x0,            // type
    0x0,            // limit 19:16, flags
    0x0,            // base 31:24
  },
  //
  // LINEAR_DATA64_SEL
  //
  {
    0x0FFFF,        // limit 15:0
    0x0,            // base 15:0
    0x0,            // base 23:16
    0x092,          // present, ring 0, data, read/write
    0x0CF,          // page-granular, 32-bit
    0x0,
  },
  //
  // LINEAR_CODE64_SEL
  //
  {
    0x0FFFF,        // limit 15:0
    0x0,            // base 15:0
    0x0,            // base 23:16
    0x09A,          // present, ring 0, code, execute/read
    0x0AF,          // page-granular, 64-bit code
    0x0,            // base (high)
  },
  //
  // SPARE5_SEL
  //
  {
    0x0,            // limit 15:0
    0x0,            // base 15:0
    0x0,            // base 23:16
    0x0,            // type
    0x0,            // limit 19:16, flags
    0x0,            // base 31:24
  },
};

/**
  Initialize Global Descriptor Table.

**/
VOID
InitGlobalDescriptorTable (
  VOID
  )
{
  GDT_ENTRIES *gdt;
  IA32_DESCRIPTOR gdtPtr;

#ifndef VBOX
  //
  // Allocate Runtime Data for the GDT
  //
  gdt = AllocateRuntimePool (sizeof (GdtTemplate) + 8);
  ASSERT (gdt != NULL);
  gdt = ALIGN_POINTER (gdt, 8);
#else
   /*
    * With previous versions of the EDK2 changing AllocateRuntimePool to AllocateReservedPool
    * was enough to get earlier mac OS versions (Tiger up until at least Snow Leopard) not overwriting the GDT
    * when freeing not required EFI memory ranges anymore.
    * With the current version however the memory map (memmap command in EFI shell) returned to boot.efi
    * will have the Runtime flag always cleared no matter what. boot.efi overwrites the memory and
    * the VM triple faults because of the invalid GDT.
    * Using the AllocatePages() method with EfiRuntimeServicesData works around that and the GDT memory
    * has the proper Runtime flag set so boot.efi will leave it alone.
    */
  EFI_STATUS Status;
  EFI_PHYSICAL_ADDRESS PhysicalAddress = SIZE_4GB - 1;
  Status = gBS->AllocatePages(AllocateMaxAddress,
                              EfiRuntimeServicesData,
                              1,
                              &PhysicalAddress);
  ASSERT(!EFI_ERROR(Status));
  gdt = ALIGN_POINTER ((void *)(UINTN)PhysicalAddress, 8);
#endif

  //
  // Initialize all GDT entries
  //
  CopyMem (gdt, &GdtTemplate, sizeof (GdtTemplate));

  //
  // Write GDT register
  //
  gdtPtr.Base = (UINT32)(UINTN)(VOID*) gdt;
  gdtPtr.Limit = (UINT16) (sizeof (GdtTemplate) - 1);
  AsmWriteGdtr (&gdtPtr);

  //
  // Update selector (segment) registers base on new GDT
  //
  SetCodeSelector ((UINT16)CPU_CODE_SEL);
  SetDataSelectors ((UINT16)CPU_DATA_SEL);
}

