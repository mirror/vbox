; $Id$
;; @file
; BS3Kit - Bs3EnteredMode
;

;
; Copyright (C) 2007-2016 Oracle Corporation
;
; This file is part of VirtualBox Open Source Edition (OSE), as
; available from http://www.virtualbox.org. This file is free software;
; you can redistribute it and/or modify it under the terms of the GNU
; General Public License (GPL) as published by the Free Software
; Foundation, in version 2 as it comes in the "COPYING" file of the
; VirtualBox OSE distribution. VirtualBox OSE is distributed in the
; hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
;
; The contents of this file may alternatively be used under the terms
; of the Common Development and Distribution License Version 1.0
; (CDDL) only, as it comes in the "COPYING.CDDL" file of the
; VirtualBox OSE distribution, in which case the provisions of the
; CDDL are applicable instead of those of the GPL.
;
; You may elect to license modified versions of this file under the
; terms and conditions of either the GPL or the CDDL or both.
;

%include "bs3kit-template-header.mac"


;;
; @cproto   BS3_DECL(void) Bs3EnteredMode(void);
;
; @uses     Nothing.
;
; @remarks  ASSUMES we're in ring-0 when not in some kind of real mode.
;
BS3_PROC_BEGIN_MODE Bs3EnteredMode
        push    xBP
        mov     xBP, xSP
        push    xAX

        ;
        ; Load selectors, IDTs and stuff as appropriate.
        ;
%ifdef TMPL_CMN_R86
BS3_BEGIN_DATA16
TMPL_BEGIN_TEXT
extern               TMPL_NM(Bs3TrapSystemCallHandler)
        xor     ax, ax
        mov     ss, ax
        mov     ax, BS3DATA16
        mov     ds, ax
        mov     es, ax

        mov     word [ss: BS3_TRAP_SYSCALL*4], TMPL_NM(Bs3TrapSystemCallHandler) wrt BS3TEXT16
        mov     word [ss: BS3_TRAP_SYSCALL*4 + 2], BS3TEXT16

%elif TMPL_BITS == 16
BS3_EXTERN_SYSTEM16 Bs3Lidt_Idt16
BS3_EXTERN_SYSTEM16 Bs3Gdte_Tss16
BS3_EXTERN_SYSTEM16 Bs3Gdte_Tss16DoubleFault
TMPL_BEGIN_TEXT
        jmp     BS3_SEL_R0_CS16:.reloaded_cs
.reloaded_cs:

        mov     ax, BS3_SEL_R0_SS16
        mov     ss, ax

        mov     ax, BS3_SEL_SYSTEM16
        mov     ds, ax
        lidt    [Bs3Lidt_Idt16]

        mov     ax, X86DESCGENERIC_BIT_OFF_TYPE + 1
        btr     [Bs3Gdte_Tss16DoubleFault], ax  ; mark it not busy
        btr     [Bs3Gdte_Tss16], ax             ; mark it not busy
        mov     ax, BS3_SEL_TSS16
        ltr     ax

        mov     ax, BS3_SEL_LDT
        lldt    ax

        mov     ax, BS3_SEL_R0_DS16
        mov     ds, ax
        mov     es, ax

 %ifndef TMPL_CMN_LM
;         BS3_EXTERN_CMN Bs3Trap32SetGate
;         extern         TMPL_NM(Bs3TrapSystemCallHandler)
;        push    0                       ; cParams
;        push    TMPL_NM(Bs3TrapSystemCallHandler) wrt BS3TEXT16
;        push    BS3_SEL_R0_CS16
;        push    3                       ; DPL
;        push    X86_SEL_TYPE_SYS_286_INT_GATE
;        push    BS3_TRAP_SYSCALL
;        BS3_CALL Bs3Trap16SetGate,6
;        add     sp, xCB * 6
 %endif

%elif TMPL_BITS == 32
BS3_EXTERN_SYSTEM16 Bs3Lidt_Idt32
BS3_EXTERN_SYSTEM16 Bs3Gdte_Tss32
BS3_EXTERN_SYSTEM16 Bs3Gdte_Tss32DoubleFault
TMPL_BEGIN_TEXT
        mov     ax, BS3_SEL_R0_SS32
        mov     ss, ax

        mov     ax, BS3_SEL_SYSTEM16
        mov     ds, ax
        lidt    [Bs3Lidt_Idt32]

        mov     ax, X86DESCGENERIC_BIT_OFF_TYPE + 1
        btr     [Bs3Gdte_Tss32DoubleFault], ax  ; mark it not busy
        btr     [Bs3Gdte_Tss32], ax             ; mark it not busy
        mov     ax, BS3_SEL_TSS32
        ltr     ax

        mov     ax, BS3_SEL_LDT
        lldt    ax

        mov     ax, BS3_SEL_R0_DS32
        mov     ds, ax
        mov     es, ax

 %ifndef TMPL_CMN_LM
        BS3_EXTERN_CMN Bs3Trap32SetGate
        extern         TMPL_NM(Bs3TrapSystemCallHandler)
        push    0                       ; cParams
        push    TMPL_NM(Bs3TrapSystemCallHandler) wrt FLAT
        push    BS3_SEL_R0_CS32
        push    3                       ; DPL
        push    X86_SEL_TYPE_SYS_386_INT_GATE
        push    BS3_TRAP_SYSCALL
        BS3_CALL Bs3Trap32SetGate,6
        add     esp, xCB * 6
 %endif

%elif TMPL_BITS == 64
BS3_EXTERN_SYSTEM16 Bs3Lidt_Idt64
BS3_EXTERN_SYSTEM16 Bs3Gdte_Tss64
TMPL_BEGIN_TEXT
        mov     ax, BS3_SEL_R0_DS64
        mov     ss, ax

        mov     ax, BS3_SEL_SYSTEM16
        mov     ds, ax
        lidt    [Bs3Lidt_Idt64]

        mov     eax, X86DESCGENERIC_BIT_OFF_TYPE + 1
        btr     [Bs3Gdte_Tss64], eax ; mark it not busy
        mov     ax, BS3_SEL_TSS64
        ltr     ax

        mov     ax, BS3_SEL_LDT
        lldt    ax

        mov     ax, BS3_SEL_R0_DS64
        mov     ds, ax
        mov     es, ax

%else
 %error "TMPL_BITS"
%endif

 %ifdef TMPL_CMN_LM
        ;
        ; In long mode we always use a 64-bit TSS and a IDT with all 64-bit gates.
        ;
  %if TMPL_BITS == 64
        push    rcx
        push    rdx
        push    r8
        push    r9
  %endif

        BS3_EXTERN_CMN Bs3Trap64SetGate
        extern         Bs3TrapSystemCallHandler_lm64
        push    0                       ; bIst
  %ifdef TMPL_64BIT
        push    Bs3TrapSystemCallHandler_lm64 wrt FLAT
  %else
        push    dword 0                 ; upper offset
        push    dword Bs3TrapSystemCallHandler_lm64 wrt FLAT
  %endif
        push    BS3_SEL_R0_CS64
        push    3                       ; DPL
        push    AMD64_SEL_TYPE_SYS_INT_GATE
        push    BS3_TRAP_SYSCALL
        BS3_CALL Bs3Trap64SetGate,6
        add     xSP, xCB * 5 + 8

  %if TMPL_BITS == 64
        pop     r9
        pop     r8
        pop     rdx
        pop     rcx
  %endif
 %endif

        pop     xAX
        leave
        ret
BS3_PROC_END_MODE   Bs3EnteredMode

