; $Id$
;; @file
; BS3Kit - Bs3RegCtxSave.
;

;
; Copyright (C) 2007-2016 Oracle Corporation
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


BS3_EXTERN_SYSTEM16 Bs3Gdt
BS3_EXTERN_DATA16 g_bBs3CurrentMode
%if TMPL_BITS != 64
BS3_EXTERN_DATA16 g_uBs3CpuDetected
%endif
TMPL_BEGIN_TEXT



;;
; Restores the given register context.
;
; @param        pRegCtx
; @uses         None.
;
BS3_PROC_BEGIN_CMN Bs3RegCtxSave
        BS3_CALL_CONV_PROLOG 1
        push    xBP
        mov     xBP, xSP
        xPUSHF                          ; xBP - xCB*1: save the incoming flags exactly.
        push    xAX                     ; xBP - xCB*2: save incoming xAX
        push    xCX                     ; xBP - xCB*3: save incoming xCX
        push    xDI                     ; xBP - xCB*4: save incoming xDI
        BS3_ONLY_16BIT_STMT push    es  ; xBP - xCB*5
        BS3_ONLY_16BIT_STMT push    ds  ; xBP - xCB*6

        ;
        ; Prologue. Load ES:xDI with pRegCtx.
        ; (ASSUMES ds is BS3DATA16/FLAT of course.)
        ;
%if TMPL_BITS == 16
        les     di, [bp + 4]
%else
        mov     xDI, [xBP + xCB*2]
%endif

        ;
        ; Clear the whole structure first.
        ;
        xor     xAX, xAX
        cld
        AssertCompileSizeAlignment(BS3REGCTX, 4)
%if TMPL_BITS == 16
        les     xDI, [xBP + xCB*2]
        mov     xCX, BS3REGCTX_size / 2
        rep stosw
%else
        mov     xDI, [xBP + xCB*2]
        mov     xCX, BS3REGCTX_size / 4
        rep stosd
%endif
        mov     xDI, [xBP + xCB*2]

        ;
        ; Save the current mode.
        ;
        mov     cl, [BS3_DATA16_WRT(g_bBs3CurrentMode)]
        mov     [BS3_ONLY_16BIT(es:) xDI + BS3REGCTX.bMode], cl
%if TMPL_BITS == 16

        ;
        ; In 16-bit mode we could be running on really ancient CPUs, so check
        ; mode and detected CPU and proceed with care.
        ;
        cmp     cl, BS3_MODE_PP16
        jae     .save_full

        mov     cl, [BS3_DATA16_WRT(g_uBs3CpuDetected)]
        cmp     cl, BS3CPU_80386
        jae     .save_full

        ; load ES into DS so we can save some segment prefix bytes.
        push    es
        pop     ds

        ; 16-bit GPRs not on the stack.
        mov     [xDI + BS3REGCTX.rdx], dx
        mov     [xDI + BS3REGCTX.rbx], bx
        mov     [xDI + BS3REGCTX.rsi], si

        ; Join the common code.
        cmp     cl, BS3CPU_80286
        jb      .common_ancient

        smsw    [xDI + BS3REGCTX.cr0]
        jmp     .common_80286
%endif


.save_full:
        ;
        ; 80386 or later.
        ;
%if TMPL_BITS != 64
        ; Check for CR4 here while we've got a working DS in all contexts.
        test    byte [1 + BS3_DATA16_WRT(g_uBs3CpuDetected)], (BS3CPU_F_CPUID >> 8)
        jnz     .save_full_have_cr4
        or      byte [BS3_ONLY_16BIT(es:) xDI + BS3REGCTX.fbFlags], BS3REG_CTX_F_NO_CR4
.save_full_have_cr4:
%endif
%if TMPL_BITS == 16
        ; Load es into ds so we can save ourselves some segment prefix bytes.
        push    es
        pop     ds
%endif

        ; GPRs first.
        mov     [xDI + BS3REGCTX.rdx], sDX
        mov     [xDI + BS3REGCTX.rbx], sBX
        mov     [xDI + BS3REGCTX.rsi], sSI
%if TMPL_BITS == 64
        mov     [xDI + BS3REGCTX.r8], r8
        mov     [xDI + BS3REGCTX.r9], r9
        mov     [xDI + BS3REGCTX.r10], r10
        mov     [xDI + BS3REGCTX.r11], r11
        mov     [xDI + BS3REGCTX.r12], r12
        mov     [xDI + BS3REGCTX.r13], r13
        mov     [xDI + BS3REGCTX.r14], r14
        mov     [xDI + BS3REGCTX.r15], r15
%else
        or      byte [xDI + BS3REGCTX.fbFlags], BS3REG_CTX_F_NO_AMD64
%endif
%if TMPL_BITS == 16 ; Save high bits.
        mov     [xDI + BS3REGCTX.rax], eax
        mov     [xDI + BS3REGCTX.rcx], ecx
        mov     [xDI + BS3REGCTX.rdi], edi
        mov     [xDI + BS3REGCTX.rbp], ebp
        mov     [xDI + BS3REGCTX.rsp], esp
        pushfd
        pop     dword [xDI + BS3REGCTX.rflags]
%endif

        ; 386 segment registers.
        mov     [xDI + BS3REGCTX.fs], fs
        mov     [xDI + BS3REGCTX.gs], gs

%if TMPL_BITS == 16 ; v8086 and real mode woes.
        mov     cl, [xDI + BS3REGCTX.bMode]
        cmp     cl, BS3_MODE_RM
        je      .common_full_control_regs
        test    cl, BS3_MODE_CODE_V86
        jnz     .common_full_no_control_regs
%endif
        mov     ax, ss
        test    al, 3
        jnz     .common_full_no_control_regs

        ; Control registers (ring-0 and real-mode only).
.common_full_control_regs:
        mov     sAX, cr0
        mov     [xDI + BS3REGCTX.cr0], sAX
        mov     sAX, cr2
        mov     [xDI + BS3REGCTX.cr2], sAX
        mov     sAX, cr3
        mov     [xDI + BS3REGCTX.cr3], sAX
%if TMPL_BITS != 64
        test    byte [xDI + BS3REGCTX.fbFlags], BS3REG_CTX_F_NO_CR4
        jnz     .common_80286
%endif
        mov     sAX, cr4
        mov     [xDI + BS3REGCTX.cr4], sAX
        jmp     .common_80286

.common_full_no_control_regs:
        or      byte [xDI + BS3REGCTX.fbFlags], BS3REG_CTX_F_NO_CR

        ; 80286 control registers.
.common_80286:
        str     [xDI + BS3REGCTX.tr]
        sldt    [xDI + BS3REGCTX.ldtr]

        ; Common stuff - stuff on the stack, 286 segment registers.
.common_ancient:
        mov     xAX, [xBP - xCB*1]
        mov     [xDI + BS3REGCTX.rflags], xAX
        mov     xAX, [xBP - xCB*2]
        mov     [xDI + BS3REGCTX.rax], xAX
        mov     xAX, [xBP - xCB*3]
        mov     [xDI + BS3REGCTX.rcx], xAX
        mov     xAX, [xBP - xCB*4]
        mov     [xDI + BS3REGCTX.rdi], xAX
        mov     xAX, [xBP]
        mov     [xDI + BS3REGCTX.rbp], xAX
        mov     xAX, [xBP + xCB]
        mov     [xDI + BS3REGCTX.rip], xAX
        lea     xAX, [xBP + xCB*2]
        mov     [xDI + BS3REGCTX.rsp], xAX

        mov     [xDI + BS3REGCTX.cs], cs
%if TMPL_BITS == 16
        mov     ax, [xBP - xCB*6]
        mov     [xDI + BS3REGCTX.ds], ax
        mov     ax, [xBP - xCB*5]
        mov     [xDI + BS3REGCTX.es], ax
%else
        mov     [xDI + BS3REGCTX.ds], ds
        mov     [xDI + BS3REGCTX.es], es
%endif
        mov     [xDI + BS3REGCTX.ss], ss

        ;
        ; Return.
        ;
.return:
        BS3_ONLY_16BIT_STMT pop     ds
        BS3_ONLY_16BIT_STMT pop     es
        pop     xDI
        pop     xCX
        pop     xAX
        xPOPF
        pop     xBP
        ret
BS3_PROC_END_CMN   Bs3RegCtxSave

