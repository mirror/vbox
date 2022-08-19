; $Id$
;; @file
; IPRT - No-CRT pow - AMD64 & X86.
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
%include "iprt/x86.mac"


BEGINCODE

extern NAME(rtNoCrtMathPowCore)

;;
; Compute the rdBase to the power of rdExp.
; @returns st(0) / xmm0
; @param    rdBase      [xSP + xCB*2] / xmm0
; @param    rdExp       [xSP + xCB*2 + RTLRD_CB] / xmm1
RT_NOCRT_BEGINPROC pow
        push    xBP
        SEH64_PUSH_xBP
        mov     xBP, xSP
        SEH64_SET_FRAME_xBP 0
        push    xBX
        SEH64_PUSH_GREG rbx
        sub     xSP, 30h - xCB
        SEH64_ALLOCATE_STACK 30h - xCB
        SEH64_END_PROLOGUE

        ;
        ; Load rdBase into st1 and rdExp into st0.
        ;
%ifdef RT_ARCH_AMD64
        movsd   [xBP - 20h], xmm0
        fld     qword [xBP - 20h]
        fxam
        fnstsw  ax
        mov     dx, ax                      ; dx=fxam(base)

        movsd   [xBP - 30h], xmm1
        fld     qword [xBP - 30h]
%else
        fld     qword [xBP + xCB*2]
        fxam
        fnstsw  ax
        mov     dx, ax                      ; dx=fxam(base)

        fld     qword [xBP + xCB*2 + RTLRD_CB]
%endif

        ;
        ; Call common worker for the calculation.
        ;
        mov     ebx, 1                      ; float
        call    NAME(rtNoCrtMathPowCore)

        ;
        ; Normally, we return with eax==0 and we have to load the result
        ; from st0 and into xmm0.
        ;
        cmp     eax, 0
        jne     .return_input_reg

        fstp    qword [xSP - 30h]
        movsd   xmm0, [xSP - 30h]

.return:
        lea     xSP, [xBP - xCB]
        pop     xBX
        leave
        ret

        ;
        ; But sometimes, like if we have NaN or other special inputs, we should
        ; return the input as-is and ditch the st0 value.
        ;
.return_input_reg:
        ffreep  st0
        cmp     eax, 2
        je      .return_exp
%ifdef RT_STRICT
        cmp     eax, 1
        je      .return_base
        int3
%endif
.return_base:
        jmp     .return

.return_exp:
        movsd   xmm0, xmm1
        jmp     .return
ENDPROC   RT_NOCRT(pow)

