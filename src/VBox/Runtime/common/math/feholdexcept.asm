; $Id$
;; @file
; IPRT - No-CRT feholdexcept - AMD64 & X86.
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
; Gets the FPU+SSE environment and disables (masks) all exceptions.
;
; @returns  eax = x87 exception mask (X86_FCW_XCPT_MASK)
; @param    pEnv    32-bit: [xBP+8]     msc64: rcx      gcc64: rdi
;
RT_NOCRT_BEGINPROC feholdexcept
        push    xBP
        SEH64_PUSH_xBP
        mov     xBP, xSP
        SEH64_SET_FRAME_xBP 0
        SEH64_END_PROLOGUE

        ;
        ; Load the parameter into rcx.
        ;
%ifdef ASM_CALL64_GCC
        mov     rcx, rdi
%elifdef RT_ARCH_X86
        mov     ecx, [xBP + xCB*2]
%endif

        ;
        ; Save the FPU environment and MXCSR.
        ;
        fnstenv [xCX]
        mov     al, [xCX]               ; Save FCW.
        or      byte [xCX], X86_FCW_MASK_ALL
        fldcw   [xCX]
        mov     [xCX], al               ; Restore FCW.

%ifdef RT_ARCH_X86
        ; SSE supported (ecx preserved)?
        and     dword [xCX + 28], 0h
        extern  NAME(rtNoCrtHasSse)
        call    NAME(rtNoCrtHasSse)
        test    al, al
        jz      .return_nosse
%endif
        stmxcsr [xCX + 28]
        mov     eax, [xCX + 28]         ; Save MXCSR.
        or      dword [xCX + 28], X86_MXCSR_XCPT_MASK
        ldmxcsr [xCX + 28]
        mov     [xCX + 28], eax         ; Restore MXCSR.

.return_nosse:

        ;
        ; Return success.
        ;
        xor     eax, eax
        leave
        ret
ENDPROC   RT_NOCRT(feholdexcept)

