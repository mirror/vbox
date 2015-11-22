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
        push    xAX                     ; reserve space for far return
        push    xAX

        ; Far return, offset part. bits 63/31:16 must be zero.
        movzx   eax, word [xSP + xCB * 3] ; returning to 16-bit, so bit 16 and out must be zero.
        mov     [xSP + xCB * 2], xAX

        mov     ax, cs
        and     ax, 3
        shl     ax, BS3_SEL_RING_SHIFT  ; ring addend.
        add     ax, BS3_SEL_R0_CS16
        mov     [esp + xCB * 3], xAX

        ; Load 16-bit segment registers.
        add     ax, BS3_SEL_R0_SS16 - BS3_SEL_R0_CS16
        mov     ss, ax

        add     ax, BS3_SEL_R0_DS16 - BS3_SEL_R0_SS16
        mov     ds, ax
        mov     es, ax

        ; Restore and return.
        pop     xAX
        retf
%endif
BS3_PROC_END_CMN   Bs3SwitchTo16Bit

