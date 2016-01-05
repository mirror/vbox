; $Id$
;; @file
; BS3Kit - Trap, 32-bit assembly handlers.
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
 %error "32-bit only template"
%endif

BS3_BEGIN_DATA32
;; Easy to access flat address of Bs3Idt32GenericEntries.
BS3_GLOBAL_DATA g_Bs3Idt32GenericEntriesFlatAddr, 4
        dd Bs3Idt32GenericEntries

;; Pointer C trap handlers.
BS3_GLOBAL_DATA g_apfnBs3Trap32Handlers, 1024
        resd 256


TMPL_BEGIN_TEXT
BS3_EXTERN_CMN Bs3Trap32DefaultHandler
BS3_EXTERN_CMN Bs3Trap32ResumeFrame


;;
; Generic entry points for IDT handlers, 8 byte spacing.
;
BS3_PROC_BEGIN Bs3Idt32GenericEntries
%macro Bs3Idt32GenericEntry 1
        db      06ah, i                 ; push imm8 - note that this is a signextended value.
        jmp     %1
        ALIGNCODE(8)
%assign i i+1
%endmacro

%assign i 0                             ; start counter.
        Bs3Idt32GenericEntry bs3Idt32GenericTrapOrInt   ; 0
        Bs3Idt32GenericEntry bs3Idt32GenericTrapOrInt   ; 1
        Bs3Idt32GenericEntry bs3Idt32GenericTrapOrInt   ; 2
        Bs3Idt32GenericEntry bs3Idt32GenericTrapOrInt   ; 3
        Bs3Idt32GenericEntry bs3Idt32GenericTrapOrInt   ; 4
        Bs3Idt32GenericEntry bs3Idt32GenericTrapOrInt   ; 5
        Bs3Idt32GenericEntry bs3Idt32GenericTrapOrInt   ; 6
        Bs3Idt32GenericEntry bs3Idt32GenericTrapOrInt   ; 7
        Bs3Idt32GenericEntry bs3Idt32GenericTrapErrCode ; 8
        Bs3Idt32GenericEntry bs3Idt32GenericTrapOrInt   ; 9
        Bs3Idt32GenericEntry bs3Idt32GenericTrapErrCode ; a
        Bs3Idt32GenericEntry bs3Idt32GenericTrapErrCode ; b
        Bs3Idt32GenericEntry bs3Idt32GenericTrapErrCode ; c
        Bs3Idt32GenericEntry bs3Idt32GenericTrapErrCode ; d
        Bs3Idt32GenericEntry bs3Idt32GenericTrapErrCode ; e
        Bs3Idt32GenericEntry bs3Idt32GenericTrapOrInt   ; f  (reserved)
        Bs3Idt32GenericEntry bs3Idt32GenericTrapOrInt   ; 10
        Bs3Idt32GenericEntry bs3Idt32GenericTrapErrCode ; 11
        Bs3Idt32GenericEntry bs3Idt32GenericTrapOrInt   ; 12
        Bs3Idt32GenericEntry bs3Idt32GenericTrapOrInt   ; 13
        Bs3Idt32GenericEntry bs3Idt32GenericTrapOrInt   ; 14
        Bs3Idt32GenericEntry bs3Idt32GenericTrapOrInt   ; 15 (reserved)
        Bs3Idt32GenericEntry bs3Idt32GenericTrapOrInt   ; 16 (reserved)
        Bs3Idt32GenericEntry bs3Idt32GenericTrapOrInt   ; 17 (reserved)
        Bs3Idt32GenericEntry bs3Idt32GenericTrapOrInt   ; 18 (reserved)
        Bs3Idt32GenericEntry bs3Idt32GenericTrapOrInt   ; 19 (reserved)
        Bs3Idt32GenericEntry bs3Idt32GenericTrapOrInt   ; 1a (reserved)
        Bs3Idt32GenericEntry bs3Idt32GenericTrapOrInt   ; 1b (reserved)
        Bs3Idt32GenericEntry bs3Idt32GenericTrapOrInt   ; 1c (reserved)
        Bs3Idt32GenericEntry bs3Idt32GenericTrapOrInt   ; 1d (reserved)
        Bs3Idt32GenericEntry bs3Idt32GenericTrapErrCode ; 1e
        Bs3Idt32GenericEntry bs3Idt32GenericTrapOrInt   ; 1f (reserved)
%rep 224
        Bs3Idt32GenericEntry bs3Idt32GenericTrapOrInt
%endrep
BS3_PROC_END  Bs3Idt32GenericEntries




;;
; Trap or interrupt (no error code).
;
BS3_PROC_BEGIN bs3Idt32GenericTrapOrInt
        pushfd
        cli
        cld

        sub     esp, BS3TRAPFRAME_size
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.rax], eax
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.rbp], ebp
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.rdx], edx
        lea     ebp, [esp + BS3TRAPFRAME_size + 4] ; iret - 4 (i.e. ebp frame chain location)

        mov     edx, [esp + BS3TRAPFRAME_size]
        mov     [esp + BS3TRAPFRAME.fHandlerRfl], edx

        movzx   edx, byte [esp + BS3TRAPFRAME_size + 4]
        mov     [esp + BS3TRAPFRAME.bXcpt], edx

        xor     edx, edx
        mov     [esp + BS3TRAPFRAME.uErrCd], edx
        mov     [esp + BS3TRAPFRAME.uErrCd + 4], edx
        jmp     bs3Idt32GenericCommon
BS3_PROC_END   bs3Idt32GenericTrapOrInt


;;
; Trap with error code.
;
BS3_PROC_BEGIN bs3Idt32GenericTrapErrCode
        pushfd
        cli
        cld

        sub     esp, BS3TRAPFRAME_size
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.rax], eax
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.rbp], ebp
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.rdx], edx
        lea     ebp, [esp + BS3TRAPFRAME_size + 8] ; iret - 4 (i.e. ebp frame chain location)

        mov     edx, [esp + BS3TRAPFRAME_size]
        mov     [esp + BS3TRAPFRAME.fHandlerRfl], edx

        movzx   edx, byte [esp + BS3TRAPFRAME_size + 4]
        mov     [esp + BS3TRAPFRAME.bXcpt], edx

        mov     edx, [esp + BS3TRAPFRAME_size + 8]
;; @todo Do voodoo checks for 'int xx' or misguided hardware interrupts.
        mov     [esp + BS3TRAPFRAME.uErrCd], edx
        xor     edx, edx
        mov     [esp + BS3TRAPFRAME.uErrCd + 4], edx
        jmp     bs3Idt32GenericCommon
BS3_PROC_END   bs3Idt32GenericTrapErrCode


;;
; Common context saving code and dispatching.
;
; @param    esp     Pointer to the trap frame.  The following members have been
;                   filled in by the previous code:
;                       - bXcpt
;                       - uErrCd
;                       - fHandlerRFL
;                       - Ctx.eax (except upper dword)
;                       - Ctx.edx (except upper dword)
;                       - Ctx.ebp (except upper dword)
;
; @param    ebp     Pointer to the dword before the iret frame, i.e. where ebp
;                   would be saved if this was a normal call.
; @param    edx     Zero (0).
;
BS3_PROC_BEGIN bs3Idt32GenericCommon
        ;
        ; Fake EBP frame.
        ;
        mov     eax, [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.rbp]
        mov     [ebp], eax

        ;
        ; Save the remaining GPRs and segment registers.
        ;
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.rcx], ecx
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.rbx], ebx
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.rdi], edi
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.rsi], esi
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.ds], ds
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.es], es
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.fs], fs
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.gs], gs

        ;
        ; Load 32-bit data selector for the DPL we're executing at into DS and ES.
        ; Save the handler SS and CS values first.
        ;
        mov     ax, cs
        mov     [esp + BS3TRAPFRAME.uHandlerCs], ax
        mov     ax, ss
        mov     [esp + BS3TRAPFRAME.uHandlerSs], ax
        and     ax, 3
        mov     cx, ax
        shl     ax, BS3_SEL_RING_SHIFT
        or      ax, cx
        and     ax, BS3_SEL_R0_DS32
        mov     ds, ax
        mov     es, ax

        ;
        ; Copy iret info.
        ;
        mov     ecx, [ebp + 4]
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.rip], ecx
        mov     ecx, [ebp + 12]
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.rflags], ecx
        mov     cx, [ebp + 8]
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.cs], cx
        test    dword [ebp + 12], X86_EFL_VM
        jnz     .iret_frame_v8086
        mov     ax, ss
        and     ax, 3
        and     cx, 3
        cmp     ax, ax
        je      .iret_frame_same_cpl

        mov     ecx, [ebp + 16]
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.rsp], ecx
        mov     cx, [ebp + 20]
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.ss], cx
        lea     eax, [ebp + 24]
        mov     [esp + BS3TRAPFRAME.uHandlerRsp], eax
        jmp     .iret_frame_done

.iret_frame_same_cpl:
        lea     ecx, [ebp + 12]
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.rsp], ecx
        mov     cx, ss
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.ss], cx
        lea     eax, [ebp + 16]
        mov     [esp + BS3TRAPFRAME.uHandlerRsp], eax
        jmp     .iret_frame_done

.iret_frame_v8086:
        lea     ecx, [ebp + 12]
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.rsp], ecx
        mov     cx, [ebp + 20]
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.ss], cx
        mov     cx, [ebp + 24]
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.es], cx
        mov     cx, [ebp + 28]
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.ds], cx
        mov     cx, [ebp + 32]
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.fs], cx
        mov     cx, [ebp + 36]
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.gs], cx
        lea     eax, [ebp + 40]
        mov     [esp + BS3TRAPFRAME.uHandlerRsp], eax
        jmp     .iret_frame_done

.iret_frame_done:
        ;
        ; Control registers.
        ;
        mov     eax, cr0
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.cr0], eax
        mov     eax, cr2
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.cr2], eax
        mov     eax, cr3
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.cr3], eax
        mov     eax, cr4
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.cr4], eax

        ;
        ; Set context bit width and clear all upper dwords and unused register members.
        ;
        mov     dword [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.cBits], 32
        xor     edx, edx
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.rax    + 4], edx
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.rcx    + 4], edx
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.rdx    + 4], edx
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.rbx    + 4], edx
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.rsp    + 4], edx
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.rbp    + 4], edx
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.rsi    + 4], edx
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.rdi    + 4], edx
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.r8     + 4], edx
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.r9     + 4], edx
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.r10    + 4], edx
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.r11    + 4], edx
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.r12    + 4], edx
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.r13    + 4], edx
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.r14    + 4], edx
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.r15    + 4], edx
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.rflags + 4], edx
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.rip    + 4], edx
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.cr0    + 4], edx
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.cr2    + 4], edx
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.cr3    + 4], edx
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.cr4    + 4], edx

        ;
        ; Dispatch it to C code.
        ;
        movzx   ebx, byte [esp + BS3TRAPFRAME.bXcpt]
        mov     eax, [BS3_DATA_NM(g_apfnBs3Trap32Handlers) + ebx * 4]
        or      eax, eax
        jnz     .call_handler
        mov     eax, Bs3Trap32DefaultHandler
.call_handler:
        mov     edi, esp
        push    edi
        call    eax

        ;
        ; Resume execution using trap frame.
        ;
        push    0
        push    edi
        call    Bs3Trap32ResumeFrame
.panic:
        int3
        hlt
        jmp     .panic
BS3_PROC_END   bs3Idt32GenericCommon

