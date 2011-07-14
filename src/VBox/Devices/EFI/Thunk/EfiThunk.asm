; $Id$
;; @file
; 16-bit EFI Thunk - 16-bit code executed immediately after CPU startup/reset,
;                    performs minimal setup, switches CPU to 32-bit mode
;                    and passes control to the 32-bit firmware entry point
;
;; @todo yasm 0.8.0 got binary sections which could simplify things in this file,
;        see: http://www.tortall.net/projects/yasm/manual/html/manual.html#objfmt-bin-section

;
; Copyright (C) 2009 Oracle Corporation
;
; This file is part of VirtualBox Open Source Edition (OSE), as
; available from http://www.virtualbox.org. This file is free software;
; you can redistribute it and/or modify it under the terms of the GNU
; General Public License (GPL) as published by the Free Software
; Foundation, in version 2 as it comes in the "COPYING" file of the
; VirtualBox OSE distribution. VirtualBox OSE is distributed in the
; hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
;

;*******************************************************************************
;*      Defined Constants And Macros                                           *
;*******************************************************************************
;; we'll use no more than 128 vectors atm
%define IDT_VECTORS     128
;; keep in sync with actual GDT size
%define GDT_SELECTORS   10


;*******************************************************************************
;*      Header Files                                                           *
;*******************************************************************************
%include "VBox/asmdefs.mac"
%include "iprt/x86.mac"
%include "DevEFI.mac"

;
; 0xfffff000/0xf000 - Where we start.
;
        ORG     0xf000

;
; 0xfffff000/0xf000 - Parameters passed by DevEFI, DEVEFIINFO.
;
DevEfiParameters:
        times DEVEFIINFO_size db 0

;
; The IDT.
;   The first 16 vectors have dedicated handlers to ease debugging.
;   The remaining uses a common handler.
;
align 16
efi_thunk_IDT:
%assign i 0
%rep 16
        dw      Trap_ %+ i, 0x10, 0x8e00, 0xffff
        %assign i i+1
%endrep
        times IDT_VECTORS-16 dw DefaultTrap, 0x10, 0x8e00, 0xffff


;
; The GDT.
; Note! Keep this in sync with GDT_SELECTORS.
;
align 16
efi_thunk_GDT:
        dw      0,      0, 0,      0            ; null selector
        dw      0,      0, 0,      0            ; ditto
        dw      0xffff, 0, 0x9b00, 0x00cf       ; 32 bit flat code segment (0x10)
        dw      0xffff, 0, 0x9300, 0x00cf       ; 32 bit flat data segment (0x18)
        dw      0xffff, 0, 0x9b00, 0x0000       ; 16 bit code segment base=0xf0000 limit=0xffff - FIXME: the base is 0, not f0000 here.
        dw      0xffff, 0, 0x9300, 0x0000       ; 16 bit data segment base=0x0 limit=0xffff     - FIXME: ditto.
        dw      0xffff, 0, 0x9300, 0x00cf       ; 32 bit flat stack segment (0x30)
        dw      0xffff, 0, 0x9a00, 0x00af       ; 64 bit flat code segment (0x38)
        dw      0xffff, 0, 0x8900, 0x0080       ; 64 bit TSS descriptor (0x40)
        dw      0,      0, 0,      0            ; ditto

;; For lidt
efi_thunk_idtr:
        dw      8*IDT_VECTORS-1                 ; limit 15:00
        dw      efi_thunk_IDT                   ; base  15:00
        db      0x0f                            ; base  23:16
        db      0x00                            ; unused

;; For lgdt
efi_thunk_gdtr:
        dw      8*GDT_SELECTORS-1               ; limit 15:00
        dw      efi_thunk_GDT                   ; base  15:00
        db      0x0f                            ; base  23:16
        db      0x00                            ; unused

BITS 32

;;
; The default trap/interrupt handler.
;
DefaultTrap:
        push    ebp
        mov     ebp, esp
        mov     eax, EFI_PANIC_CMD_THUNK_TRAP
        mov     edx, EFI_PANIC_PORT
        out     dx, al
        jmp     HaltForEver

;;
; Generate 16 Trap_N handlers that pushes trap number on the stack.
%assign i 0
%rep 16
Trap_ %+ i:
        push    ebp                             ; Create a valid stackframe for the debugger. (not
        push    byte i                          ; quite true if there is an error value pushed)
        jmp CommonTrap
        %assign i i+1
%endrep

;;
; Common trap handler for the 16 dedicated ones.
;
CommonTrap:
        lea     ebp, [esp + 4]                  ; stack frame part 2.
        push    edx
        push    eax
        mov     edx, EFI_PANIC_PORT
        mov     eax, EFI_PANIC_CMD_THUNK_TRAP
        out     dx, al

HaltForEver:
        cli
        hlt
        jmp short HaltForEver                   ; In case of NMI.

BITS 16
;;
; This i the place where we jump immediately after boot and
; switch the CPU into protected mode.
;
genesis:
%ifdef DISABLED_CODE
          ; Say 'Hi' to the granny!
        mov     al, 0x41
        mov     dx, EFI_DEBUG_PORT
        out     dx, al
%endif
        cli                                     ; paranoia


        ; enable a20
        in      al, 0x92
        or      al, 0x02
        out     0x92, al

        ; check that we loaded in the right place
        cmp word [cs:efi_thunk_gdtr], 8*GDT_SELECTORS-1
        je    load_ok
        ; panic if our offset is wrong, which most likely means invalid ORG
        mov     ax, EFI_PANIC_CMD_BAD_ORG
        mov     dx, EFI_PANIC_PORT
        out     dx, al
load_ok:

        ; load IDTR and GDTR.
        cs lidt [efi_thunk_idtr]
        cs lgdt [efi_thunk_gdtr]

        ; set PE bit in CR0, not paged
        mov     eax, cr0
        or      al, X86_CR0_PE
        mov     cr0, eax

        ; start protected mode code: ljmpl 0x10:code_32
        db      0x66, 0xea
        dw      code_32                         ; low offset word
        dw      0xffff                          ; high offset word
        dw      0x0010                          ; protected mode CS selector

        ;
        ; At this point we're in 32-bit protected mode
        ;
BITS 32
code_32:
        ; load some segments
        mov     ax, 0x18                        ; Flat 32-bit data segment
        mov     ds, ax
        mov     es, ax
        mov     ax, 0x30                        ; Flat 32-bit stack segment
        mov     ss, ax
        ; load the null selector into FS/GS (catches unwanted accesses)
        xor     ax, ax
        mov     gs, ax
        mov     fs, ax

        ;
        ; Switch stack, have it start at the last page before 2M
        ;
        mov     esp,  0xfffff000;

        ;
        ; Jump to 32-bit entry point of the firmware, interrupts still disabled.
        ;
        ; It's up to the firmware init code to setup a working IDT (and optionally
        ; GDT and TSS) before enabling interrupts. It may also switch the stack
        ; around all it wants for all we care.
        ;
        mov     eax,[0xfffff000 + DEVEFIINFO.fFlags]
        and      eax, DEVEFI_INFO_FLAGS_AMD64
        jnz trampoline_64
        mov     ebp, [0xfffff000 + DEVEFIINFO.PhysFwVol]
        mov     esi, [0xfffff000 + DEVEFIINFO.pfnFirmwareEP]
        mov     edi, [0xfffff000 + DEVEFIINFO.pfnPeiEP]
        jmp     [0xfffff000 + DEVEFIINFO.pfnFirmwareEP]
        jmp     HaltForEver
trampoline_64:
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
        mov cr3, eax


        mov eax,cr4
        or eax, X86_CR4_PAE|X86_CR4_OSFSXR|X86_CR4_OSXMMEEXCPT
        mov cr4,eax

        mov ecx, MSR_K6_EFER
        rdmsr
        or  eax, MSR_K6_EFER_LME
        wrmsr

        mov ax, 0x40
        ltr ax

        mov     eax, cr0
        or      eax, X86_CR0_PG
        mov     cr0, eax
        jmp compat
compat:
        jmp 0x38:0xffff0000 + efi_64
BITS 64
efi_64:
        mov     ebp, [0xff009] ;  DEVEFIINFO.PhysFwVol
        mov     esi, [0xff000]; + DEVEFIINFO.pfnFirmwareEP]
        mov     edi, [0xff000 + 0x28]; + DEVEFIINFO.pfnPeiEP]
        jmp     [0xff000]; + DEVEFIINFO.pfnFirmwareEP]
        jmp     HaltForEver

        ;
        ; 0xfffffff0/0xfff0 - This is where the CPU starts executing.
        ;
        ;; @todo yasm 0.8.0: SECTION .text start=0fff0h vstart=0fff0h ?
        times 0xff0-$+DevEfiParameters db 0cch     ; Note! $ isn't moved by ORG (yasm v0.6.2).
cpu_start:
        BITS 16
        jmp     genesis
        dw 0xdead
        dw 0xbeaf
        times (16 - 7) db 0cch
end:
