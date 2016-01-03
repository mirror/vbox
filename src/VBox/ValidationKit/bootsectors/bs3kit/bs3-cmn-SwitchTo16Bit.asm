; $Id$
;; @file
; BS3Kit - Bs3SwitchTo16Bit
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
; @cproto   BS3_DECL(void) Bs3SwitchTo16Bit(void);
;
BS3_PROC_BEGIN_CMN Bs3SwitchTo16Bit
%if TMPL_BITS == 16
        ret
%else
        push    xAX
        xPUSHF
        cli

        ; Calc new CS.
        mov     ax, cs
        and     xAX, 3
        shl     xAX, BS3_SEL_RING_SHIFT  ; ring addend.
        add     xAX, BS3_SEL_R0_CS16

        ; Construct a far return for switching to 16-bit code.
        push    xAX
        push    .sixteen_bit
        xRETF

BS3_BEGIN_TEXT16
.sixteen_bit:
        ; Load 16-bit segment registers.
        ;; @todo support non-standard stacks?
        add     ax, BS3_SEL_R0_SS16 - BS3_SEL_R0_CS16
        mov     ss, ax

        add     ax, BS3_SEL_R0_DS16 - BS3_SEL_R0_SS16
        mov     ds, ax
        mov     es, ax

        popfd
 %if TMPL_BITS == 64
        add     sp, 4
 %endif
        pop     eax
 %if TMPL_BITS == 64
        add     sp, 4
 %endif
        ret     sCB - 2                 ; Return and pop 2 or 6 bytes of "parameters" (unused return value)
TMPL_BEGIN_TEXT
%endif
BS3_PROC_END_CMN   Bs3SwitchTo16Bit

