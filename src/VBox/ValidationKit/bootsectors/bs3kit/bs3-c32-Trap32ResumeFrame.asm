; $Id$
;; @file
; BS3Kit - Trap, 32-bit resume.
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

%ifndef TMPL_32BIT
 %error "32-bit only template, at the moment"
%endif


;;
; @cproto  BS3_DECL(void) Bs3Trap32ResumeFrame(BS3TRAPFRAME BS3_FAR *pTrapFrame, uint16_t fFlags)
BS3_PROC_BEGIN_CMN Bs3Trap32ResumeFrame
        push    xBP
        mov     xBP, xSP

        mov     ebx, [xBP + 8]          ; pTrapFrame

        ;
        ; Restore control registers if they've changed.
        ;
        test    word [xBP + 12], BS3TRAPRESUME_F_SKIP_CRX
        jnz     .skip_control_regs

        mov     eax, [ebx + BS3TRAPFRAME.Ctx + BS3REGCTX.cr4]
        mov     edx, cr4
        cmp     eax, edx
        je      .skip_cr4
        mov     cr4, eax
.skip_cr4:

        mov     eax, [ebx + BS3TRAPFRAME.Ctx + BS3REGCTX.cr3]
        mov     edx, cr3
        cmp     eax, edx
        je      .skip_cr3
        mov     cr3, eax
.skip_cr3:

        mov     eax, [ebx + BS3TRAPFRAME.Ctx + BS3REGCTX.cr2]
        mov     edx, cr2
        cmp     eax, edx
        je      .skip_cr2
        mov     cr2, eax
.skip_cr2:

        mov     eax, [ebx + BS3TRAPFRAME.Ctx + BS3REGCTX.cr0]
        mov     edx, cr0
        cmp     eax, edx
        je      .skip_cr0
        mov     cr0, eax
.skip_cr0:

        ; LDTR
        sldt    ax
        cmp     ax, [ebx + BS3TRAPFRAME.Ctx + BS3REGCTX.ldtr]
        je      .skip_ldtr
        lldt    [ebx + BS3TRAPFRAME.Ctx + BS3REGCTX.ldtr]
.skip_ldtr:

        ; TR - complicated because we need to clear the busy bit. ASSUMES GDT.
        str     ax
        cmp     ax, [ebx + BS3TRAPFRAME.Ctx + BS3REGCTX.tr]
        je      .skip_tr

        movzx   eax, word [ebx + BS3TRAPFRAME.Ctx + BS3REGCTX.tr]
        or      eax, eax                ; check for null.
        jz      .load_tr

        sub     esp, 10h
        mov     dword [esp + 8], 0      ; paranoia^2
        sgdt    [esp + 6]
        add     eax, [esp + 8]          ; the limit.
        add     esp, 10h

        add     eax, X86DESCGENERIC_BIT_OFF_TYPE / 8
        and     byte [eax], ~(X86_SEL_TYPE_SYS_TSS_BUSY_MASK << (X86DESCGENERIC_BIT_OFF_TYPE % 8))
.load_tr:
        ltr     [ebx + BS3TRAPFRAME.Ctx + BS3REGCTX.tr]
.skip_tr:

.skip_control_regs:

        ;
        ; Branch into iret frame specific code.
        ;
        test    dword [ebx + BS3TRAPFRAME.Ctx + BS3REGCTX.rflags], X86_EFL_VM
        jnz     .iret_v8086
        mov     ax, ss
        mov     dx, [ebx + BS3TRAPFRAME.Ctx + BS3REGCTX.ss]
        and     ax, 3
        and     dx, 3
        cmp     ax, dx
        cli
        jne     .iret_other_cpl

        ;
        ; IRET to same CPL.  ASSUMES that we can stash an IRET frame and EBP at ESP.
        ;
        mov     ebp, [ebx + BS3TRAPFRAME.Ctx + BS3REGCTX.rsp]
        sub     ebp, 16                 ; IRET + EBP
        mov     eax, [ebx + BS3TRAPFRAME.Ctx + BS3REGCTX.rflags]
        mov     [ebp + 12], eax
        movzx   eax, word [ebx + BS3TRAPFRAME.Ctx + BS3REGCTX.cs]
        mov     [ebp + 8], eax
        mov     eax, [ebx + BS3TRAPFRAME.Ctx + BS3REGCTX.rip]
        mov     [ebp + 4], eax
        mov     eax, [ebx + BS3TRAPFRAME.Ctx + BS3REGCTX.rbp]
        mov     [ebp], eax

        mov     eax, [ebx + BS3TRAPFRAME.Ctx + BS3REGCTX.rax]
        mov     ecx, [ebx + BS3TRAPFRAME.Ctx + BS3REGCTX.rcx]
        mov     edx, [ebx + BS3TRAPFRAME.Ctx + BS3REGCTX.rdx]
        mov     esi, [ebx + BS3TRAPFRAME.Ctx + BS3REGCTX.rsi]
        mov     edi, [ebx + BS3TRAPFRAME.Ctx + BS3REGCTX.rdi]

        mov     gs, [ebx + BS3TRAPFRAME.Ctx + BS3REGCTX.gs]
        mov     fs, [ebx + BS3TRAPFRAME.Ctx + BS3REGCTX.fs]
        mov     es, [ebx + BS3TRAPFRAME.Ctx + BS3REGCTX.es]
        mov     ss, [ebx + BS3TRAPFRAME.Ctx + BS3REGCTX.ss]
        push    dword [ebx + BS3TRAPFRAME.Ctx + BS3REGCTX.ds]
        mov     ebx, [ebx + BS3TRAPFRAME.Ctx + BS3REGCTX.rbx]
        pop     ds
        leave
        iretd

        ;
        ; IRET to other CPL.
        ;
.iret_other_cpl:
        push    dword [ebx + BS3TRAPFRAME.Ctx + BS3REGCTX.ss]
        push    dword [ebx + BS3TRAPFRAME.Ctx + BS3REGCTX.rsp]
        push    dword [ebx + BS3TRAPFRAME.Ctx + BS3REGCTX.rflags]
        push    dword [ebx + BS3TRAPFRAME.Ctx + BS3REGCTX.cs]
        push    dword [ebx + BS3TRAPFRAME.Ctx + BS3REGCTX.rip]

        mov     eax, [ebx + BS3TRAPFRAME.Ctx + BS3REGCTX.rax]
        mov     ecx, [ebx + BS3TRAPFRAME.Ctx + BS3REGCTX.rcx]
        mov     edx, [ebx + BS3TRAPFRAME.Ctx + BS3REGCTX.rdx]
        mov     esi, [ebx + BS3TRAPFRAME.Ctx + BS3REGCTX.rsi]
        mov     edi, [ebx + BS3TRAPFRAME.Ctx + BS3REGCTX.rdi]

        mov     gs, [ebx + BS3TRAPFRAME.Ctx + BS3REGCTX.gs]
        mov     fs, [ebx + BS3TRAPFRAME.Ctx + BS3REGCTX.fs]
        mov     es, [ebx + BS3TRAPFRAME.Ctx + BS3REGCTX.es]
        push    dword [ebx + BS3TRAPFRAME.Ctx + BS3REGCTX.rbp]
        push    dword [ebx + BS3TRAPFRAME.Ctx + BS3REGCTX.ds]
        mov     ebx, [ebx + BS3TRAPFRAME.Ctx + BS3REGCTX.rbx]
        pop     ds
        pop     ebp
        iretd

        ;
        ; IRET to virtual 8086 mode.
        ;
.iret_v8086:
        push    dword [ebx + BS3TRAPFRAME.Ctx + BS3REGCTX.gs]
        push    dword [ebx + BS3TRAPFRAME.Ctx + BS3REGCTX.fs]
        push    dword [ebx + BS3TRAPFRAME.Ctx + BS3REGCTX.ds]
        push    dword [ebx + BS3TRAPFRAME.Ctx + BS3REGCTX.es]
        push    dword [ebx + BS3TRAPFRAME.Ctx + BS3REGCTX.ss]
        push    dword [ebx + BS3TRAPFRAME.Ctx + BS3REGCTX.rsp]
        push    dword [ebx + BS3TRAPFRAME.Ctx + BS3REGCTX.rflags]
        push    dword [ebx + BS3TRAPFRAME.Ctx + BS3REGCTX.cs]
        push    dword [ebx + BS3TRAPFRAME.Ctx + BS3REGCTX.rip]

        mov     eax, [ebx + BS3TRAPFRAME.Ctx + BS3REGCTX.rax]
        mov     ecx, [ebx + BS3TRAPFRAME.Ctx + BS3REGCTX.rcx]
        mov     edx, [ebx + BS3TRAPFRAME.Ctx + BS3REGCTX.rdx]
        mov     esi, [ebx + BS3TRAPFRAME.Ctx + BS3REGCTX.rsi]
        mov     edi, [ebx + BS3TRAPFRAME.Ctx + BS3REGCTX.rdi]
        mov     ebp, [ebx + BS3TRAPFRAME.Ctx + BS3REGCTX.rbp]
        mov     ebx, [ebx + BS3TRAPFRAME.Ctx + BS3REGCTX.rbx]
        iretd

BS3_PROC_END_CMN   Bs3Trap32ResumeFrame

