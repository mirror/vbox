; $Id$
;; @file


;
; Copyright (C) 2012 Oracle Corporation
;
; This file is part of VirtualBox Open Source Edition (OSE), as
; available from http://www.virtualbox.org. This file is free software;
; you can redistribute it and/or modify it under the terms of the GNU
; General Public License (GPL) as published by the Free Software
; Foundation, in version 2 as it comes in the "COPYING" file of the
; VirtualBox OSE distribution. VirtualBox OSE is distributed in the
; hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
;

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

%macro fill_pkt 1
%%loop:
        mov [ebx],eax
        xor edx,edx
        mov [ebx + 4], edx
        add ebx, 8
        add eax, %1
        loop %%loop
%endmacro

%define base 0x800000;0xfffff000

%endif

BITS    32

;
; Modified:  EAX
;
Transition32FlatTo64Flat:

%ifndef VBOX
    mov     eax, ((ADDR_OF_START_OF_RESET_CODE & ~0xfff) - 0x1000)
%else ; !VBOX
        mov ecx, 0x800 ; pde size
        mov ebx, base - (6 << X86_PAGE_4K_SHIFT)
        xor eax, eax
        ;; or flags to eax
        or eax, (X86_PDE_P|X86_PDE_A|X86_PDE_PS|X86_PDE_PCD|X86_PDE_RW|RT_BIT(6))
        fill_pkt (1 << X86_PAGE_2M_SHIFT)

        ;; pdpt (1st 4 entries describe 4Gb)
        mov ebx, base - (2 << X86_PAGE_4K_SHIFT)
        mov eax, base - (6 << X86_PAGE_4K_SHIFT) ;;
        or eax, (X86_PDPE_P|X86_PDPE_RW|X86_PDPE_A|X86_PDPE_PCD)
        mov [ebx],eax
        xor edx,edx
        mov [ebx + 4], edx
        add ebx, 8

        mov eax, base - 5 * (1 << X86_PAGE_4K_SHIFT) ;;
        or eax, (X86_PDPE_P|X86_PDPE_RW|X86_PDPE_A|X86_PDPE_PCD)
        mov [ebx],eax
        xor edx,edx
        mov [ebx + 4], edx
        add ebx, 8

        mov eax, base - 4 * (1 << X86_PAGE_4K_SHIFT) ;;
        or eax, (X86_PDPE_P|X86_PDPE_RW|X86_PDPE_A|X86_PDPE_PCD)
        mov [ebx],eax
        xor edx,edx
        mov [ebx + 4], edx
        add ebx, 8

        mov eax, base - 3 * (1 << X86_PAGE_4K_SHIFT) ;;
        or eax, (X86_PDPE_P|X86_PDPE_RW|X86_PDPE_A|X86_PDPE_PCD)
        mov [ebx],eax
        xor edx,edx
        mov [ebx + 4], edx
        add ebx, 8

        mov ecx, 0x1f7 ; pdte size
        mov ebx, base - 2 * (1 << X86_PAGE_4K_SHIFT) + 4 * 8
        mov eax, base - 6 * (1 << X86_PAGE_4K_SHIFT);;
        or eax, (X86_PDPE_P|X86_PDPE_RW|X86_PDPE_A|X86_PDPE_PCD)
        ;; or flags to eax
        fill_pkt 3 * (1 << X86_PAGE_4K_SHIFT)

        mov ecx, 0x200 ; pml4 size
        mov ebx, base - (1 << X86_PAGE_4K_SHIFT)
        mov eax, base - 2 * (1 << X86_PAGE_4K_SHIFT) ;;
        or eax, (X86_PML4E_P|X86_PML4E_PCD|X86_PML4E_A|X86_PML4E_RW)
        ;; or flags to eax
        fill_pkt 0

        mov eax, base - (1 << X86_PAGE_4K_SHIFT)
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

