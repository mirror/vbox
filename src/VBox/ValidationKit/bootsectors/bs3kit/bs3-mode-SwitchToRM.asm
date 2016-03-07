; $Id$
;; @file
; BS3Kit - Bs3SwitchToRM
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

BS3_EXTERN_SYSTEM16 Bs3Gdt
%if TMPL_MODE == BS3_MODE_PE16
BS3_EXTERN_DATA16 g_uBs3CpuDetected
%endif

TMPL_BEGIN_TEXT


;;
; Switch to real mode from any other mode.
;
; @cproto   BS3_DECL(void) Bs3SwitchToRM(void);
;
; @uses     Nothing (except high 32-bit register parts).
;
; @remarks  Obviously returns to 16-bit mode, even if the caller was
;           in 32-bit or 64-bit mode.
;
; @remarks  Does not require 20h of parameter scratch space in 64-bit mode.
;
BS3_PROC_BEGIN_MODE Bs3SwitchToRM
%ifdef TMPL_RM
        ret

%elif BS3_MODE_IS_V86(TMPL_MODE)
        ;
        ; V8086 - Switch to 16-bit ring-0 and call worker for that mode.
        ;
        extern  BS3_CMN_NM(Bs3SwitchToRing0)
        call    BS3_CMN_NM(Bs3SwitchToRing0)

 %if   TMPL_MODE == BS3_MODE_PE16_V86
        extern  _Bs3SwitchToRM_pe16
        jmp     _Bs3SwitchToRM_pe16
 %elif TMPL_MODE == BS3_MODE_PEV86
        extern  _Bs3SwitchToRM_pe32_16
        jmp     _Bs3SwitchToRM_pe32_16
 %elif TMPL_MODE == BS3_MODE_PP16_V86
        extern  _Bs3SwitchToRM_pp16
        jmp     _Bs3SwitchToRM_pp16
 %elif TMPL_MODE == BS3_MODE_PPV86
        extern  _Bs3SwitchToRM_pp32_16
        jmp     _Bs3SwitchToRM_pp32_16
 %elif TMPL_MODE == BS3_MODE_PAE16_V86
        extern  _Bs3SwitchToRM_pae16
        jmp     _Bs3SwitchToRM_pae16
 %elif TMPL_MODE == BS3_MODE_PAEV86
        extern  _Bs3SwitchToRM_pae32_16
        jmp     _Bs3SwitchToRM_pae32_16
 %else
  %error "Unexpected TMPL_MODE=" TMPL_MODE
 %endif

%else
        ;
        ; Protected mode.
        ;
        push    sAX
        push    sBX
        sPUSHF

        ;
        ; Get to 16-bit ring-0 and disable interrupts.
        ;
        extern  BS3_CMN_NM(Bs3SwitchToRing0)
        call    BS3_CMN_NM(Bs3SwitchToRing0)

        cli

 %if TMPL_MODE == BS3_MODE_PE16
        ;
        ; On 80286 we must reset the CPU to get back to real mode.
        ;
        mov     ax, BS3_SEL_DATA16
        mov     ds, ax
        cmp     byte [BS3_DATA16_WRT(g_uBs3CpuDetected)], BS3CPU_80286
        jne     .is_386_or_better
.implement_this_later:
        int3
        jmp     .implement_this_later

        jmp     .reload_cs

 %elif TMPL_BITS != 16
        ;
        ; Must be in 16-bit segment when calling Bs3SwitchTo16Bit.
        ;
        jmp     .sixteen_bit_segment wrt FLAT
BS3_BEGIN_TEXT16
        BS3_SET_BITS TMPL_BITS
.sixteen_bit_segment:

        extern  BS3_CMN_NM(Bs3SwitchTo16Bit)
        call    BS3_CMN_NM(Bs3SwitchTo16Bit)
        BS3_SET_BITS 16
 %endif

        ;
        ; Exit to real mode.
        ;
.is_386_or_better:
        mov     eax, cr0
        and     eax, X86_CR0_NO_PE_NO_PG
        mov     cr0, eax
        jmp     BS3TEXT16:.reload_cs
.reload_cs:

        ;
        ; Convert the stack (now 16-bit prot) to real mode.
        ;
        mov     ax, BS3_SEL_SYSTEM16
        mov     ds, ax
        mov     bx, ss
        and     bx, X86_SEL_MASK        ; ASSUMES GDT stack selector
        mov     al, [bx + 4 + Bs3Gdt]
        mov     ah, [bx + 7 + Bs3Gdt]
        add     sp, [bx + 2 + Bs3Gdt]   ; ASSUMES not expand down segment.
        adc     ax, 0
%ifdef BS3_STRICT
        test    ax, 0fff0h
        jz      .stack_conv_ok
        int3
.stack_conv_ok:
%endif
        shl     ax, 12
        mov     ss, ax
 %if TMPL_BITS != 16
        and     esp, 0ffffh
 %endif

        ;
        ; Call routine for doing mode specific setups.
        ;
        extern  NAME(Bs3EnteredMode_rm)
        call    NAME(Bs3EnteredMode_rm)

 %if TMPL_BITS == 64
        pop     eax
        popfd
        pop     ebx
        pop     ebx
        pop     eax
        pop     eax
 %else
        popfd
        pop     ebx
        pop     eax
 %endif
        retn    BS3_IF_16_32_64BIT(0, 2, 6)

 %if TMPL_BITS != 16
TMPL_BEGIN_TEXT
 %endif
%endif
BS3_PROC_END_MODE   Bs3SwitchToRM

