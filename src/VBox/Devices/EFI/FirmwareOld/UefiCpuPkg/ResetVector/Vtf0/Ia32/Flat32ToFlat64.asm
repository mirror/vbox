;------------------------------------------------------------------------------
; @file
; Transition from 32 bit flat protected mode into 64 bit flat protected mode
;
; Copyright (c) 2008 - 2009, Intel Corporation. All rights reserved.<BR>
; This program and the accompanying materials
; are licensed and made available under the terms and conditions of the BSD License
; which accompanies this distribution.  The full text of the license may be found at
; http://opensource.org/licenses/bsd-license.php
;
; THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
; WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
;
;------------------------------------------------------------------------------

%ifdef VBOX
 %include "VBox/nasm.mac"
 %include "iprt/x86.mac"

 ;;
 ; Macro for filling in page table entries with EAX as base content.
 ;
 ; @param    %1  What to increment EAX with for each iteration.
 ; @param    %2  Start address.
 ; @param    %3  End address (exclusive).
 ; @uses     EAX, EBX, EFLAGS
 ;
 %macro FILL_ENTRIES 3
     mov     ebx, %2
 %%myloop:
     mov     [ebx], eax
     mov     dword [ebx + 4], 0
     add     eax, %1
     add     ebx, 8
     cmp     ebx, %3
     jb      %%myloop
 %endmacro

 ;;
 ; Macro for filling in page table entries with zeros.
 ;
 ; @param    %1 Start address.
 ; @param    %2  End address (exclusive).
 ; @uses     EAX, EBX, EFLAGS
 ;
 %macro ZERO_ENTRIES 2
     mov     ebx, %1
     xor     eax, eax
 %%myloop:
     mov     [ebx], eax
     mov     dword [ebx + 4], eax
     add     ebx, 8
     cmp     ebx, %2
     jb      %%myloop
 %endmacro

 ;;
 ; The address of the page tables.
 %define VBOX_PDPT_ADDR     (0x00800000 - 0x6000)
 %define VBOX_PDPTR_ADDR    (0x00800000 - 0x2000)
 %define VBOX_PML4_ADDR     (0x00800000 - 0x1000)

%endif ; VBOX


BITS    32

;
%ifndef VBOX
; Modified:  EAX
%else
; Modified:  EAX, EBX
%endif
;
Transition32FlatTo64Flat:

%ifndef VBOX
    mov     eax, ((ADDR_OF_START_OF_RESET_CODE & ~0xfff) - 0x1000)
%else ; !VBOX
    ;
    ; Produce our own page table that does not reside in ROM, since the PGM
    ; pool code cannot quite cope with write monitoring ROM pages.
    ;

    ; First, set up page directories with 2MB pages for the first 4GB.
    mov     eax, ( X86_PDE4M_P | X86_PDE4M_A | X86_PDE4M_PS | X86_PDE4M_PCD | X86_PDE4M_RW | X86_PDE4M_D )
    FILL_ENTRIES X86_PAGE_2M_SIZE, VBOX_PDPT_ADDR, VBOX_PDPT_ADDR + 0x4000

    ; Second, set up page a directory pointer table with 4 entries pointing to
    ; the PTPDs we created above and the remainder as zeros.
    mov     eax, ( X86_PDPE_P | X86_PDPE_RW | X86_PDPE_A | X86_PDPE_PCD ) + VBOX_PDPT_ADDR
    FILL_ENTRIES X86_PAGE_4K_SIZE, VBOX_PDPTR_ADDR, VBOX_PDPTR_ADDR + 4*8
    ZERO_ENTRIES VBOX_PDPTR_ADDR + 4*8, VBOX_PDPTR_ADDR + 0x1000

    ; Third, set up a PML4 with the first entry pointing to the previous table.
    mov     dword [VBOX_PML4_ADDR], ( X86_PML4E_P | X86_PML4E_PCD | X86_PML4E_A | X86_PML4E_RW ) + VBOX_PDPTR_ADDR
    mov     dword [VBOX_PML4_ADDR + 4], 0
    ZERO_ENTRIES VBOX_PML4_ADDR + 1*8, VBOX_PML4_ADDR + 0x1000

    mov     eax, VBOX_PML4_ADDR
%endif
    mov     cr3, eax

    mov     eax, cr4
    bts     eax, 5                      ; enable PAE
    mov     cr4, eax

    mov     ecx, 0xc0000080
    rdmsr
    bts     eax, 8                      ; set LME
    wrmsr

    mov     eax, cr0
    bts     eax, 31                     ; set PG
    mov     cr0, eax                    ; enable paging

    jmp     LINEAR_CODE64_SEL:ADDR_OF(jumpTo64BitAndLandHere)
BITS    64
jumpTo64BitAndLandHere:

    debugShowPostCode POSTCODE_64BIT_MODE

    OneTimeCallRet Transition32FlatTo64Flat

