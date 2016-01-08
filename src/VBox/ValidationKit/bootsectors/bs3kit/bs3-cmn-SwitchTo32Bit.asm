; $Id$
;; @file
; BS3Kit - Bs3SwitchTo32Bit
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
; @cproto   BS3_DECL(void) Bs3SwitchTo32Bit(void);
;
; @remarks  Does not require 20h of parameter scratch space in 64-bit mode.
;
BS3_PROC_BEGIN_CMN Bs3SwitchTo32Bit
%if TMPL_BITS == 32
        ret
%else
 %if TMPL_BITS == 16
        push    ax                      ; Reserve space for larger return value (adjusted in 32-bit code).
        push    eax
        pushfd
 %else
        pushfq
        mov     [rsp + 4], eax
 %endif
        cli

        ; Calc ring addend.
        mov     ax, cs
        and     xAX, 3
        shl     xAX, BS3_SEL_RING_SHIFT
        add     xAX, BS3_SEL_R0_CS32

        ; Create far return for switching to 32-bit mode.
        push    sAX
 %if TMPL_BITS == 16
        push    dword .thirty_two_bit wrt FLAT
        o32 retf
 %else
        push    .thirty_two_bit
        o64 retf
 %endif

BS3_SET_BITS 32
.thirty_two_bit:
        ; Load 32-bit segment registers.
        add     eax, BS3_SEL_R0_SS32 - BS3_SEL_R0_CS32
        mov     ss, ax

        add     eax, BS3_SEL_R0_DS32 - BS3_SEL_R0_SS32
        mov     ds, ax
        mov     es, ax

 %if TMPL_BITS == 16
        ; Adjust the return address.
        movsx   eax, word [esp + 4*2 + 2]
        add     eax, BS3_ADDR_BS3TEXT16
        mov     [esp + 4*2], eax
 %endif

        ; Restore and return.
        popfd
        pop     eax
        ret
%endif
BS3_PROC_END_CMN   Bs3SwitchTo32Bit

