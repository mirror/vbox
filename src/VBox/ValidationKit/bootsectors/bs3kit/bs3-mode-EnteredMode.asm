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
%if TMPL_BITS == 64
        push    rcx
        push    rdx
        push    r8
        push    r9
%endif

        ;
        ; Load stack selector (not always necessary) and sometimes CS too.
        ;
%ifdef TMPL_RM
        xor     ax, ax
%elifdef TMPL_CMN_V86
        extern  v86_versions_of_Bs3EnteredMode_should_not_be_dragged_into_the_link
        call    v86_versions_of_Bs3EnteredMode_should_not_be_dragged_into_the_link
%elif TMPL_BITS == 16
        jmp     BS3_SEL_R0_CS16:.reloaded_cs
.reloaded_cs:
        mov     ax, BS3_SEL_R0_SS16
%elif TMPL_BITS == 32
        mov     ax, BS3_SEL_R0_SS32
%elif TMPL_BITS == 64
        mov     ax, BS3_SEL_R0_DS64
%else
 %error "TMPL_BITS"
%endif
        mov     ss, ax

        ;
        ; Load selector appropriate for accessing BS3SYSTEM16 data.
        ;
%if TMPL_BITS == 16
        mov     ax, BS3_SEL_SYSTEM16
%else
        mov     ax, RT_CONCAT(BS3_SEL_R0_DS,TMPL_BITS)
%endif
        mov     ds, ax

        ;
        ; Load the appropritate IDT or IVT.
        ; Always 64-bit in long mode, otherwise according to TMPL_BITS.
        ;
%ifdef TMPL_CMN_R86
        BS3_EXTERN_SYSTEM16 Bs3Lidt_Ivt
        TMPL_BEGIN_TEXT
        lidt    [Bs3Lidt_Ivt]
%elifdef TMPL_CMN_LM
        BS3_EXTERN_SYSTEM16 Bs3Lidt_Idt64
        TMPL_BEGIN_TEXT
        lidt    [Bs3Lidt_Idt64 TMPL_WRT_SYSTEM16_OR_FLAT]
%else
        BS3_EXTERN_SYSTEM16 RT_CONCAT(Bs3Lidt_Idt,TMPL_BITS)
        TMPL_BEGIN_TEXT
        lidt    [RT_CONCAT(Bs3Lidt_Idt,TMPL_BITS) TMPL_WRT_SYSTEM16_OR_FLAT]
%endif

%ifndef TMPL_CMN_R86
        ;
        ; Load the appropriate task selector.
        ; Always 64-bit in long mode, otherwise according to TMPL_BITS.
        ;
        mov     ax, X86DESCGENERIC_BIT_OFF_TYPE + 1 ; For clearing the busy bit in the TSS descriptor type.
 %ifdef TMPL_CMN_LM
        BS3_EXTERN_SYSTEM16 Bs3Gdte_Tss64
        TMPL_BEGIN_TEXT
        btr     [Bs3Gdte_Tss64 TMPL_WRT_SYSTEM16_OR_FLAT], ax
        mov     ax, BS3_SEL_TSS64

 %elif TMPL_BITS == 16
        BS3_EXTERN_SYSTEM16 Bs3Gdte_Tss16
        BS3_EXTERN_SYSTEM16 Bs3Gdte_Tss16DoubleFault
        TMPL_BEGIN_TEXT
        btr     [Bs3Gdte_Tss16            TMPL_WRT_SYSTEM16_OR_FLAT], ax
        btr     [Bs3Gdte_Tss16DoubleFault TMPL_WRT_SYSTEM16_OR_FLAT], ax
        mov     ax, BS3_SEL_TSS16

 %elif TMPL_BITS == 32
        BS3_EXTERN_SYSTEM16 Bs3Gdte_Tss32
        BS3_EXTERN_SYSTEM16 Bs3Gdte_Tss32DoubleFault
        BS3_EXTERN_SYSTEM16 Bs3Tss32
        BS3_EXTERN_SYSTEM16 Bs3Tss32DoubleFault
        TMPL_BEGIN_TEXT
        btr     [Bs3Gdte_Tss32            TMPL_WRT_SYSTEM16_OR_FLAT], ax
        btr     [Bs3Gdte_Tss32DoubleFault TMPL_WRT_SYSTEM16_OR_FLAT], ax
        mov     eax, cr3
        mov     [X86TSS32.cr3 + Bs3Tss32            TMPL_WRT_SYSTEM16_OR_FLAT], eax
        mov     [X86TSS32.cr3 + Bs3Tss32DoubleFault TMPL_WRT_SYSTEM16_OR_FLAT], eax
        mov     ax, BS3_SEL_TSS32
 %else
 %error "TMPL_BITS"
 %endif
        ltr     ax
%endif ; !TMPL_CMN_R86

%ifndef TMPL_CMN_R86
        ;
        ; Load the LDT.
        ;
        mov     ax, BS3_SEL_LDT
        lldt    ax
%endif

        ;
        ; Load ds and es.
        ;
%ifdef TMPL_CMN_V86
        mov     ax, BS3_SEL_DATA16
%else
        mov     ax, RT_CONCAT(BS3_SEL_R0_DS,TMPL_BITS)
%endif
        mov     ds, ax
        mov     es, ax

        ;
        ; Install system call handler.
        ; Always 64-bit in long mode, otherwise according to TMPL_BITS.
        ;
%ifdef TMPL_CMN_RM
        mov     word [ss: BS3_TRAP_SYSCALL*4], TMPL_NM(Bs3TrapSystemCallHandler) wrt BS3TEXT16
        mov     word [ss: BS3_TRAP_SYSCALL*4 + 2], BS3TEXT16
%elifdef TMPL_CMN_LM
        BS3_EXTERN_CMN Bs3Trap64SetGate
        extern         Bs3TrapSystemCallHandler_lm64
        TMPL_BEGIN_TEXT
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

%elif TMPL_BITS == 16
        BS3_EXTERN_CMN Bs3Trap16SetGate
        extern         TMPL_NM(Bs3TrapSystemCallHandler)
        TMPL_BEGIN_TEXT
        push    0                       ; cParams
        push    TMPL_NM(Bs3TrapSystemCallHandler) wrt BS3TEXT16
        push    BS3_SEL_R0_CS16
        push    3                       ; DPL
        push    X86_SEL_TYPE_SYS_286_INT_GATE
        push    BS3_TRAP_SYSCALL
        BS3_CALL Bs3Trap16SetGate,6
        add     xSP, xCB * 6

%elif TMPL_BITS == 32
        BS3_EXTERN_CMN Bs3Trap32SetGate
        extern         TMPL_NM(Bs3TrapSystemCallHandler)
        TMPL_BEGIN_TEXT
        push    0                       ; cParams
        push    TMPL_NM(Bs3TrapSystemCallHandler) wrt FLAT
        push    BS3_SEL_R0_CS32
        push    3                       ; DPL
        push    X86_SEL_TYPE_SYS_386_INT_GATE
        push    BS3_TRAP_SYSCALL
        BS3_CALL Bs3Trap32SetGate,6
        add     xSP, xCB * 6
%else
 %error "TMPL_BITS"
%endif

        ;
        ; Epilogue.
        ;
%if TMPL_BITS == 64
        pop     r9
        pop     r8
        pop     rdx
        pop     rcx
%endif
        pop     xAX
        leave
        ret
BS3_PROC_END_MODE   Bs3EnteredMode

