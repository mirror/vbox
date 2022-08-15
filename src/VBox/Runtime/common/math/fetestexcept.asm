; $Id$
;; @file
; IPRT - No-CRT fetestexcept - AMD64 & X86.
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
; Return the pending exceptions in the given mask.
;
; Basically a simpler fegetexceptflags function.
;
; @returns  eax = pending exceptions (X86_FSW_XCPT_MASK) & fXcptMask.
; @param    fXcptMask   32-bit: [xBP+8]; msc64: ecx; gcc64: edi; -- Exceptions to test for (X86_FSW_XCPT_MASK).
;                       Accepts X86_FSW_SF and will return it if given as input.
;
RT_NOCRT_BEGINPROC fetestexcept
        push    xBP
        SEH64_PUSH_xBP
        mov     xBP, xSP
        SEH64_SET_FRAME_xBP 0
        sub     xSP, 10h
        SEH64_ALLOCATE_STACK 10h
        SEH64_END_PROLOGUE

        ;
        ; Load the parameter into ecx (fXcptMask).
        ;
%ifdef ASM_CALL64_GCC
        mov     ecx, edi
%elifdef RT_ARCH_X86
        mov     ecx, [xBP + xCB*2]
%endif
%if 0
        and     ecx, X86_FSW_XCPT_MASK
%else
        or      eax, -1
        test    ecx, ~X86_FSW_XCPT_MASK
        jnz     .return
%endif

        ;
        ; Get the pending exceptions.
        ;

        ; Get x87 exceptions first.
        fnstsw  ax
        and     eax, ecx

%ifdef RT_ARCH_X86
        ; SSE supported (ecx preserved)?
        mov     ch, al                  ; Save the return value - it's only the lower 6 bits.
        extern  NAME(rtNoCrtHasSse)
        call    NAME(rtNoCrtHasSse)
        test    al, al
        mov     al, ch                  ; Restore the return value - no need for movzx here.
        jz      .return
%endif

        ; OR in the SSE exceptions (modifies ecx).
        stmxcsr [xBP - 10h]
        and     ecx, [xBP - 10h]
        and     ecx, X86_MXCSR_XCPT_FLAGS
        or      eax, ecx

.return:
        leave
        ret
ENDPROC   RT_NOCRT(fetestexcept)

