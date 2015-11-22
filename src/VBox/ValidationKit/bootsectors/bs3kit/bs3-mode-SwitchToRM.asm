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


;;
; Switch to real mode from any other mode.
;
; @cproto   BS3_DECL(void) Bs3SwitchToRealMode(void);
;
; @uses     Nothing (except high 32-bit register parts).
;
; @remarks  Obviously returns to 16-bit mode, even if the caller was
;           in 32-bit or 64-bit mode.
;
BS3_PROC_BEGIN_MODE Bs3SwitchToRM
%ifdef TMPL_RM
        ret

%else
        ;
        ; Get to 16-bit ring-0.
        ;
        extern  BS3_CMN_NM(Bs3SwitchToRing0)
        call    BS3_CMN_NM(Bs3SwitchToRing0)

 %if TMPL_BITS != 16
        ; Adjust the return address.
        push    xAX
        mov     ax, [xSP + xCB]
        mov     [xSP + xCB + xCB - 2], ax
        pop     xAX

        ; Must be in 16-bit segment when calling Bs3SwitchTo16Bit.
        jmp     .sixteen_bit_segment
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
%ifdef TMPL_LM
 extern TODO_FIX_LM_MODE_RETURN
%endif
        push    eax
        mov     eax, cr0
        and     eax, X86_CR0_NO_PE_NO_PG
        mov     cr0, eax
        jmp     BS3TEXT16:.reload_cs
.reload_cs:
        pop     eax

        ;
        ; Call rountine for doing mode specific setups.
        ;
        extern  NAME(Bs3EnteredMode_rm)
        call    NAME(Bs3EnteredMode_rm)

        ret

 %if TMPL_BITS != 16
TMPL_BEGIN_TEXT
 %endif
%endif
BS3_PROC_END_MODE   Bs3SwitchToRM

