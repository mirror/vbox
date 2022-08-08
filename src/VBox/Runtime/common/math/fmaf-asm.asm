; $Id$
;; @file
; IPRT - No-CRT fmaf alternatives - AMD64 & X86.
;

;
; Copyright (C) 2006-2022 Oracle Corporation
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

BEGINCODE

;;
; Fused multiplication and add, intel version.
;
; @returns  st(0) / xmm0
; @param    r32Factor1      [rbp + 08h] / xmm0
; @param    r32Factor2      [rbp + 0ch] / xmm1
; @param    r32Addend       [rbp + 10h] / xmm2
BEGINPROC rtNoCrtMathFma3f
        push    xBP
        SEH64_PUSH_xBP
        mov     xBP, xSP
        SEH64_SET_FRAME_xBP 0
        SEH64_END_PROLOGUE

%ifdef RT_ARCH_X86
        movss   xmm0, dword [xBP + xCB*2 + 00h]
        movss   xmm1, dword [xBP + xCB*2 + 04h]
        movss   xmm2, dword [xBP + xCB*2 + 08h]
%endif

        vfmadd132ss xmm0, xmm2, xmm1    ; xmm0 = xmm0 * xmm1 + xmm2  (132 = multiply op1 with op3 and add op2)

%ifdef RT_ARCH_X86
        sub     xSP, 10h
        movss   [xSP], xmm0
        fld     dword [xSP]
%endif
        leave
        ret
ENDPROC   rtNoCrtMathFma3f


;;
; Fused multiplication and add, amd version.
;
; @returns  st(0) / xmm0
; @param    r32Factor1      [rbp + 08h] / xmm0
; @param    r32Factor2      [rbp + 10h] / xmm1
; @param    r32Addend       [rbp + 18h] / xmm2
BEGINPROC rtNoCrtMathFma4f
        push    xBP
        SEH64_PUSH_xBP
        mov     xBP, xSP
        SEH64_SET_FRAME_xBP 0
        SEH64_END_PROLOGUE

%ifdef RT_ARCH_X86
        movss   xmm0, dword [xBP + xCB*2 + 00h]
        movss   xmm1, dword [xBP + xCB*2 + 04h]
        movss   xmm2, dword [xBP + xCB*2 + 08h]
%endif

        vfmaddss xmm0, xmm0, xmm1, xmm2    ; xmm0 = xmm0 * xmm1 + xmm2

%ifdef RT_ARCH_X86
        sub     xSP, 10h
        movss   [xSP], xmm0
        fld     dword [xSP]
%endif
        leave
        ret
ENDPROC   rtNoCrtMathFma4f

