; $Id$
;; @file
; BS3Kit - Bs3SwitchToPE16
;

;
; Copyright (C) 2007-2015 Oracle Corporation
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
; Switch to 16-bit unpaged protected mode from any other mode.
;
; @cproto   BS3_DECL(void) Bs3SwitchToPE16(void);
;
; @uses     Nothing (except high 32-bit register parts).
;
; @remarks  Obviously returns to 16-bit mode, even if the caller was
;           in 32-bit or 64-bit mode.
;
; @remarks  Does not require 20h of parameter scratch space in 64-bit mode.
;
BS3_PROC_BEGIN_MODE Bs3SwitchToPE16
%ifdef TMPL_PE16
        extern  BS3_CMN_NM(Bs3SwitchToRing0)
        call    BS3_CMN_NM(Bs3SwitchToRing0)
        push    ax
        mov     ax, BS3_SEL_R0_DS16
        mov     ds, ax
        mov     es, ax
        pop     ax
        ret

%elif BS3_MODE_IS_V86(TMPL_MODE)
        ;
        ; V8086 - Switch to 16-bit ring-0 and call worker for that mode.
        ;
        extern  BS3_CMN_NM(Bs3SwitchToRing0)
        call    BS3_CMN_NM(Bs3SwitchToRing0)
        extern %[BS3_MODE_R0_NM_ %+ TMPL_MODE](Bs3SwitchToPE16)
        jmp    %[BS3_MODE_R0_NM_ %+ TMPL_MODE](Bs3SwitchToPE16)

%else
        ;
        ; Switch to 16-bit mode and prepare for returning in 16-bit mode.
        ;
 %if TMPL_BITS != 16
        shl     xPRE [xSP + xCB], TMPL_BITS - 16    ; Adjust the return address.
        add     xSP, xCB - 2

        ; Must be in 16-bit segment when calling Bs3SwitchTo16Bit.
        jmp     .sixteen_bit_segment
BS3_BEGIN_TEXT16
        BS3_SET_BITS TMPL_BITS
.sixteen_bit_segment:
 %endif

        ;
        ; Switch to real mode.
        ;
        extern  TMPL_NM(Bs3SwitchToRM)
        call    TMPL_NM(Bs3SwitchToRM)
        BS3_SET_BITS 16

        push    ax
        push    cx
        pushf
        cli

        ;
        ; Load the GDT and enable PE16.
        ;
BS3_EXTERN_SYSTEM16 Bs3Lgdt_Gdt
BS3_BEGIN_TEXT16
        mov     ax, BS3SYSTEM16
        mov     ds, ax
        lgdt    [Bs3Lgdt_Gdt]

        smsw    ax
        or      ax, X86_CR0_PE
        lmsw    ax

        ;
        ; Convert from real mode stack to protected mode stack.
        ;
        mov     ax, .p16_stack
        extern  NAME(Bs3ConvertRMStackToP16UsingCxReturnToAx_c16)
        jmp     NAME(Bs3ConvertRMStackToP16UsingCxReturnToAx_c16)
.p16_stack:

        ;
        ; Call routine for doing mode specific setups.
        ;
        extern  NAME(Bs3EnteredMode_pe16)
        call    NAME(Bs3EnteredMode_pe16)

        popf
        pop     cx
        pop     ax
        ret

 %if TMPL_BITS != 16
TMPL_BEGIN_TEXT
 %endif
%endif
BS3_PROC_END_MODE   Bs3SwitchToPE16

