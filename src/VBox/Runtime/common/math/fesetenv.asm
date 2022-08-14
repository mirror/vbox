; $Id$
;; @file
; IPRT - No-CRT fesetenv - AMD64 & X86.
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

%define RT_NOCRT_FE_DFL_ENV     1
%define RT_NOCRT_FE_NOMASK_ENV  2


BEGINCODE

;;
; Sets the FPU+SSE environment.
;
; @returns  eax = 0 on success, -1 on failure.
; @param    pEnv    32-bit: [xBP+8]     msc64: rcx      gcc64: rdi  -  Saved environment to restore.
;
RT_NOCRT_BEGINPROC fesetenv
        push    xBP
        SEH64_PUSH_xBP
        mov     xBP, xSP
        SEH64_SET_FRAME_xBP 0
        sub     xBP, 20h
        SEH64_ALLOCATE_STACK 20h
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
        ; For the x87 state we only set FSW.XCPT, FCW.XCPT, FCW.RC and FCW.PC.
        ; So we save the current environment, merge those fields and load it.
        ;
        fnstenv [xBP - 20h]

        cmp     xCX, RT_NOCRT_FE_DFL_ENV
        je      .x87_default_env
        cmp     xCX, RT_NOCRT_FE_NOMASK_ENV
        je      .x87_default_nomask_env
        jmp     .x87_regular

.x87_default_env:
        and     word [xCX + X86FSTENV32P.FCW], ~(X86_FCW_MASK_ALL | X86_FCW_PC_MASK | X86_FCW_RC_MASK    | X86_FCW_IC_MASK)
        or      word [xCX + X86FSTENV32P.FCW],   X86_FCW_MASK_ALL | X86_FCW_PC_64   | X86_FCW_RC_NEAREST | X86_FCW_IC_PROJECTIVE
        and     word [xCX + X86FSTENV32P.FSW], ~X86_FSW_XCPT_ES_MASK
        jmp     .x87_common

.x87_default_nomask_env:
        and     word [xCX + X86FSTENV32P.FCW], ~(X86_FCW_MASK_ALL | X86_FCW_PC_MASK | X86_FCW_RC_MASK    | X86_FCW_IC_MASK)
        or      word [xCX + X86FSTENV32P.FCW],         X86_FCW_DM | X86_FCW_PC_64   | X86_FCW_RC_NEAREST | X86_FCW_IC_PROJECTIVE
        and     word [xCX + X86FSTENV32P.FSW], ~X86_FSW_XCPT_ES_MASK
        jmp     .x87_common

.x87_regular:
        ; FCW:
        mov     ax, [xCX + X86FSTENV32P.FCW]
        mov     dx, [xBP - 20h + X86FSTENV32P.FCW]
        and     ax,   X86_FCW_MASK_ALL | X86_FCW_RC_MASK | X86_FCW_PC_MASK
        and     dx, ~(X86_FCW_MASK_ALL | X86_FCW_RC_MASK | X86_FCW_PC_MASK)
        or      dx, ax
        mov     [xBP - 20h + X86FSTENV32P.FCW], dx
        ; FSW
        mov     ax, [xCX + X86FSTENV32P.FSW]
        mov     dx, [xBP - 20h + X86FSTENV32P.FSW]
        and     ax,   X86_FSW_XCPT_MASK
        and     dx, ~(X86_FSW_XCPT_MASK)
        or      dx, ax
        mov     [xBP - 20h + X86FSTENV32P.FSW], dx

.x87_common:
        ; Clear the exception info.
        xor     eax, eax
        mov     [xBP - 20h + X86FSTENV32P.FPUIP], eax
        mov     [xBP - 20h + X86FSTENV32P.FPUCS], eax ; covers FOP too.
        mov     [xBP - 20h + X86FSTENV32P.FPUDP], eax
        mov     [xBP - 20h + X86FSTENV32P.FPUDS], eax

        ; Load the merged and cleaned up environment.
        fldenv  [xBP - 20h]

        ;
        ; Now for SSE, if supported, where we'll restore everything as is.
        ;
%ifdef RT_ARCH_X86
        ; SSE supported (ecx preserved)?
        extern  NAME(rtNoCrtHasSse)
        call    NAME(rtNoCrtHasSse)
        test    al, al
        jz      .return_okay
%endif

        cmp     xCX, RT_NOCRT_FE_DFL_ENV
        je      .sse_special_env
        cmp     xCX, RT_NOCRT_FE_NOMASK_ENV
        je      .sse_special_env
        ldmxcsr [xCX + 28]
        jmp     .return_okay

.sse_special_env:
        stmxcsr [xBP - 10h]
        mov     eax, [xBP - 10h]
        and     eax, ~(X86_MXCSR_XCPT_FLAGS | X86_MXCSR_XCPT_MASK | X86_MXCSR_RC_MASK | X86_MXCSR_DAZ | X86_MXCSR_FZ)
        or      eax, X86_MXCSR_RC_NEAREST | X86_MXCSR_DM
        cmp     xCX, RT_NOCRT_FE_NOMASK_ENV
        je      .sse_special_load_eax
        or      eax, X86_MXCSR_RC_NEAREST | X86_MXCSR_XCPT_MASK ; default enviornment masks all exceptions
.sse_special_load_eax:
        mov     [xBP - 10h], eax
        ldmxcsr [xBP - 10h]

        ;
        ; Return success.
        ;
.return_okay:
        xor     eax, eax
        leave
        ret
ENDPROC   RT_NOCRT(fesetenv)

