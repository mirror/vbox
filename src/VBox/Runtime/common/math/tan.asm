; $Id$
;; @file
; IPRT - No-CRT tan - AMD64 & X86.
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

;;
; Compute the sine of rd
; @returns st(0) / xmm0
; @param    rd      [xSP + xCB*2] / xmm0
RT_NOCRT_BEGINPROC tan
        push    xBP
        SEH64_PUSH_xBP
        mov     xBP, xSP
        SEH64_SET_FRAME_xBP 0
        sub     xSP, 20h
        SEH64_ALLOCATE_STACK 20h
        SEH64_END_PROLOGUE

%ifdef RT_OS_WINDOWS
        ;
        ; Make sure we use full precision and not the windows default of 53 bits.
        ;
        fnstcw  [xBP - 20h]
        mov     ax, [xBP - 20h]
        or      ax, X86_FCW_PC_64           ; includes both bits, so no need to clear the mask.
        mov     [xBP - 1ch], ax
        fldcw   [xBP - 1ch]
%endif

        ;
        ; Load the input into st0.
        ;
%ifdef RT_ARCH_AMD64
        movsd   [xBP - 10h], xmm0
        fld     qword [xBP - 10h]
%else
        fld     qword [xBP + xCB*2]
%endif

        ;
        ; Calculate the tangent.
        ;
        fptan
        fnstsw  ax
        test    ah, (X86_FSW_C2 >> 8)       ; C2 is set if the input was out of range.
        jz      .return_val

        ;
        ; Input was out of range, perform reduction to +/-2pi.
        ;
        fldpi
        fadd    st0
        fxch    st1
.again:
        fprem1
        fnstsw  ax
        test    ah, (X86_FSW_C2 >> 8)       ; C2 is set if partial result.
        jnz     .again                      ; Loop till C2 == 0 and we have a final result.

        fstp    st1

        fptan

        ;
        ; Run st0.
        ;
.return_val:
        ffreep  st0                         ; ignore the 1.0 fptan pushed
%ifdef RT_ARCH_AMD64
        fstp    qword [xBP - 10h]
        movsd   xmm0, [xBP - 10h]
%endif
%ifdef RT_OS_WINDOWS
        fldcw   [xBP - 20h]                 ; restore original
%endif
.return:
        leave
        ret
ENDPROC   RT_NOCRT(tan)

