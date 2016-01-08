; $Id$
;; @file
; BS3Kit - Bs3SwitchToLM64
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
; Switch to 64-bit long mode from any other mode.
;
; @cproto   BS3_DECL(void) Bs3SwitchToLM64(void);
;
; @uses     Nothing (except possibly high 32-bit and/or upper 64-bit register parts).
;
; @remarks  Obviously returns to 64-bit mode, even if the caller was in 16-bit
;           or 32-bit mode.  It doesn't not preserve the callers ring, but
;           instead changes to ring-0.
;
BS3_PROC_BEGIN_MODE Bs3SwitchToLM64
%ifdef TMPL_LM64
        ret

%elifdef TMPL_CMN_LM
        ;
        ; Already in long mode, just switch to 64-bit.
        ;
        extern  BS3_CMN_NM(Bs3SwitchTo64Bit)
        jmp     BS3_CMN_NM(Bs3SwitchTo64Bit)

%else
        ;
        ; Switch to LM32 and then switch to 64-bits (IDT & TSS are the same for
        ; LM16, LM32 and LM64, unlike the rest).
        ;
        ; (The long mode switching code is going via 32-bit protected mode, so
        ; Bs3SwitchToLM32 contains the actual code for switching to avoid
        ; unnecessary 32-bit -> 64-bit -> 32-bit trips.)
        ;
 %ifdef TMPL_16BIT
        and     esp, 0ffffh
        push    word [esp]              ; copy return address.
        and     word [esp + 2], 0       ; clear upper return address
        add     dword [esp], BS3_ADDR_BS3TEXT16 ; Add base of return segment, completing 32-bit conversion.
 %endif
        extern  TMPL_NM(Bs3SwitchToLM32)
        call    TMPL_NM(Bs3SwitchToLM32)
        BS3_SET_BITS 32

        extern  _Bs3SwitchTo64Bit_c32
        jmp     _Bs3SwitchTo64Bit_c32
%endif
BS3_PROC_END_MODE   Bs3SwitchToLM64

