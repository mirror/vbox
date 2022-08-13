; $Id$
;; @file
; IPRT - No-CRT exp2 - AMD64 & X86.
;

;
; Copyright (C) 2022 Oracle Corporation
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


%define RT_ASM_WITH_SEH64
%include "iprt/asmdefs.mac"
%include "iprt/x86.mac"

BEGINCODE

;;
; Calculate two to the power of @a rd.
;
; @returns st(0) / xmm0
; @param    rd      [rbp + 8] / xmm0
RT_NOCRT_BEGINPROC exp2
        push    xBP
        SEH64_PUSH_xBP
        mov     xBP, xSP
        SEH64_SET_FRAME_xBP 0
%ifdef RT_ARCH_AMD64
        sub     xSP, 10h
        SEH64_ALLOCATE_STACK 10h
%endif
        SEH64_END_PROLOGUE

        ;
        ; Load the value into st(0).
        ;
%ifdef RT_ARCH_AMD64
        movsd   [xSP], xmm0
        fld     qword [xSP]
%else
        fld     qword [xBP + xCB*2]
%endif

        ;
        ; Return immediately if NaN or infinity.
        ;
        fxam
        fstsw   ax
        test    ax, X86_FSW_C0          ; C0 is set for NaN, Infinity and Empty register. The latter is not the case.
        jz      .input_ok
%ifdef RT_ARCH_AMD64
        ffreep  st0                     ; return the xmm0 register value unchanged, as FLD changes SNaN to QNaN.
%endif
        test    ax, X86_FSW_C2          ; C2 is clear for NaN (and Empty) but set for Infinity.
        jz      .return_val2
        test    ax, X86_FSW_C1          ; C1 = sign bit
        jz      .return_val2            ; Not sign, return +Inf.
%ifndef RT_ARCH_AMD64
        ffreep  st0
%endif
        fldz                            ; Signed, so return zero as that's a good approximation for 2**-Inf.
        jmp     .return_val
.input_ok:

        ;
        ; Split the job in two on the fraction and integer input parts.
        ;
        fld     st0                     ; Push a copy of the input on the stack.
        frndint                         ; st0 = (int)input
        fsub    st1, st0                ; st1 = input - (int)input; i.e. st1 = fraction, st0 = integer.
        fxch                            ; st0 = fraction, st1 = integer.

        ; 1. Calculate on the fraction.
        f2xm1                           ; st0 = 2**fraction - 1.0
        fld1
        faddp                           ; st0 = 2**fraction

        ; 2. Apply the integer power of two.
        fscale                          ; st0 = result; st1 = integer part of input.
        fstp    st1                     ; st0 = result; no st1.

.return_val:
%ifdef RT_ARCH_AMD64
        fstp    qword [xSP]
        movsd   xmm0, [xSP]
%endif
.return_val2:
        leave
        ret
ENDPROC   RT_NOCRT(exp2)

