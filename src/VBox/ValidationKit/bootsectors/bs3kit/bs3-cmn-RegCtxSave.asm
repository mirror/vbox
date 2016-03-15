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
%if TMPL_BITS == 16
BS3_EXTERN_DATA16 g_uBs3CpuDetected
%endif
TMPL_BEGIN_TEXT


%if BS3REGCTX_size %


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
        xPUSHF                          ; save the incoming flags exactly.
        push    xAX                     ; save incoming xAX
        push    xCX                     ; save incoming xCX
        push    xDI
        BS3_ONLY_16BIT_STMT push    es
        BS3_ONLY_16BIT_STMT push    ds

        ;
        ; Prologue. Load ES:xDI with pRegCtx.
        ; (ASSUMES ds is BS3DATA16/FLAT of course.)
        ;
%if TMPL_BITS == 16
        les     di, [bp + 4]
%else
        mov     xDI, [xBP + xCB*2]
%endif

%if TMPL_BITS != 64
        ;
        ; Clear the whole structure first, unless 64-bit
        ;
        push    es
        les
        push    ds
        pop     ds
        xor     xAX, xAX
        cld
 %if TMPL_BITS == 16
        mov     xCX, BS3REGCTX_size / 2
        rep stosw
        mov     di, [bp + 4]
 %else
        mov     xCX, BS3REGCTX_size / 4
        rep stosd
        mov     xDI, [xBP + xCB*2]
 %endif
%endif

        ;
        ; Save the current mode.
        ;
        mov     cl, BS3_DATA16_WRT(g_bBs3CurrentMode)
        mov     [BS3_ONLY_16BIT(es:) xDI + BS3REGCTX.bMode], cl

%if TMPL_BITS == 16
        ;
        ; Check what the CPU can do.
        ;
        cmp     byte [BS3_DATA16_WRT(g_uBs3CpuDetected)], BS3CPU_80386
        jae     .save_full

        ; Do the 80286 specifics first and load ES into DS so we can save some segment prefix bytes.
        cmp     byte [es:BS3_DATA16_WRT(g_uBs3CpuDetected)], BS3CPU_80286
        push    es
        pop     ds
        jb      .save_16_bit_ancient

        smsw    [xDI + BS3REGCTX.cr0]
        sldt    [xDI + BS3REGCTX.ldtr]
        str     [xDI + BS3REGCTX.tr]

.save_16_bit_ancient:
        ; 16-bit GPRs not on the stack.
        mov     [xDI + BS3REGCTX.rdx], dx
        mov     [xDI + BS3REGCTX.rbx], bx
        mov     [xDI + BS3REGCTX.rsi], si

        ; flags
        mov     xAX, [xBP + xCB - xCB]
        mov     [xDI + BS3REGCTX.rflags], xAX

        jmp     .common
%endif


.save_full:
        ;
        ; 80386 or later.
        ;
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
%endif
%if TMPL_BITS == 16 ; Save high bits.
        mov     [xDI + BS3REGCTX.eax], eax
        mov     [xDI + BS3REGCTX.ecx], ecx
        mov     [xDI + BS3REGCTX.edi], edi
        mov     [xDI + BS3REGCTX.ebp], ebp
        mov     [xDI + BS3REGCTX.esp], esp
        pushfd
        pop     [xDI + BS3REGCTX.rflags]
%endif

        ; Control registers.
        mov     sAX, cr0
        mov     [xDI + BS3REGCTX.cr0], sAX
        mov     sAX, cr2
        mov     [xDI + BS3REGCTX.cr2], sAX
        mov     sAX, cr3
        mov     [xDI + BS3REGCTX.cr3], sAX
        mov     sAX, cr4
        mov     [xDI + BS3REGCTX.cr4], sAX
        str     [xDI + BS3REGCTX.tr]
        sldt    [xDI + BS3REGCTX.ldtr]

        ; Segment registers.
        mov     [xDI + BS3REGCTX.fs], fs
        mov     [xDI + BS3REGCTX.gs], gs

        ; Common stuff - stuff on the stack, 286 segment registers.
.common:
        mov     xAX, [xBP - xCB]
        mov     [xDI + BS3REGCTX.rflags], xAX
        mov     xAX, [xBP - xCB*2]
        mov     [xDI + BS3REGCTX.eax], xAX
        mov     xAX, [xBP - xCB*3]
        mov     [xDI + BS3REGCTX.ecx], xAX
        mov     xAX, [xBP - xCB*4]
        mov     [xDI + BS3REGCTX.edi], xAX
        mov     xAX, [xBP]
        mov     [xDI + BS3REGCTX.ebp], xAX
        mov     xAX, [xBP + xCB]
        mov     [xDI + BS3REGCTX.rip], xAX
        lea     xAX, [xBP + xCB*2]
        mov     [xDI + BS3REGCTX.esp], xAX

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

