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

;*********************************************************************************************************************************
;*  Header Files                                                                                                                 *
;*********************************************************************************************************************************
%include "bs3kit-template-header.mac"

%ifndef TMPL_32BIT
 %error "32-bit only template"
%endif


;*********************************************************************************************************************************
;*  External Symbols                                                                                                             *
;*********************************************************************************************************************************
BS3_EXTERN_DATA16 g_bBs3CurrentMode
TMPL_BEGIN_TEXT
BS3_EXTERN_CMN Bs3TrapDefaultHandler
BS3_EXTERN_CMN Bs3RegCtxRestore
TMPL_BEGIN_TEXT


;*********************************************************************************************************************************
;*  Global Variables                                                                                                             *
;*********************************************************************************************************************************
BS3_BEGIN_DATA16
;; Easy to access flat address of Bs3Trap32GenericEntries.
BS3_GLOBAL_DATA g_Bs3Trap32GenericEntriesFlatAddr, 4
        dd Bs3Trap32GenericEntries wrt FLAT
;; Easy to access flat address of Bs3Trap32DoubleFaultHandler.
BS3_GLOBAL_DATA g_Bs3Trap32DoubleFaultHandlerFlatAddr, 4
        dd Bs3Trap32DoubleFaultHandler wrt FLAT

BS3_BEGIN_DATA32
;; Pointer C trap handlers.
BS3_GLOBAL_DATA g_apfnBs3TrapHandlers_c32, 1024
        resd 256



;;
; Generic entry points for IDT handlers, 8 byte spacing.
;
BS3_PROC_BEGIN Bs3Trap32GenericEntries
%macro Bs3Trap32GenericEntry 1
        db      06ah, i                 ; push imm8 - note that this is a signextended value.
        jmp     %1
        ALIGNCODE(8)
%assign i i+1
%endmacro

%assign i 0                             ; start counter.
        Bs3Trap32GenericEntry bs3Trap32GenericTrapOrInt   ; 0
        Bs3Trap32GenericEntry bs3Trap32GenericTrapOrInt   ; 1
        Bs3Trap32GenericEntry bs3Trap32GenericTrapOrInt   ; 2
        Bs3Trap32GenericEntry bs3Trap32GenericTrapOrInt   ; 3
        Bs3Trap32GenericEntry bs3Trap32GenericTrapOrInt   ; 4
        Bs3Trap32GenericEntry bs3Trap32GenericTrapOrInt   ; 5
        Bs3Trap32GenericEntry bs3Trap32GenericTrapOrInt   ; 6
        Bs3Trap32GenericEntry bs3Trap32GenericTrapOrInt   ; 7
        Bs3Trap32GenericEntry bs3Trap32GenericTrapErrCode ; 8
        Bs3Trap32GenericEntry bs3Trap32GenericTrapOrInt   ; 9
        Bs3Trap32GenericEntry bs3Trap32GenericTrapErrCode ; a
        Bs3Trap32GenericEntry bs3Trap32GenericTrapErrCode ; b
        Bs3Trap32GenericEntry bs3Trap32GenericTrapErrCode ; c
        Bs3Trap32GenericEntry bs3Trap32GenericTrapErrCode ; d
        Bs3Trap32GenericEntry bs3Trap32GenericTrapErrCode ; e
        Bs3Trap32GenericEntry bs3Trap32GenericTrapOrInt   ; f  (reserved)
        Bs3Trap32GenericEntry bs3Trap32GenericTrapOrInt   ; 10
        Bs3Trap32GenericEntry bs3Trap32GenericTrapErrCode ; 11
        Bs3Trap32GenericEntry bs3Trap32GenericTrapOrInt   ; 12
        Bs3Trap32GenericEntry bs3Trap32GenericTrapOrInt   ; 13
        Bs3Trap32GenericEntry bs3Trap32GenericTrapOrInt   ; 14
        Bs3Trap32GenericEntry bs3Trap32GenericTrapOrInt   ; 15 (reserved)
        Bs3Trap32GenericEntry bs3Trap32GenericTrapOrInt   ; 16 (reserved)
        Bs3Trap32GenericEntry bs3Trap32GenericTrapOrInt   ; 17 (reserved)
        Bs3Trap32GenericEntry bs3Trap32GenericTrapOrInt   ; 18 (reserved)
        Bs3Trap32GenericEntry bs3Trap32GenericTrapOrInt   ; 19 (reserved)
        Bs3Trap32GenericEntry bs3Trap32GenericTrapOrInt   ; 1a (reserved)
        Bs3Trap32GenericEntry bs3Trap32GenericTrapOrInt   ; 1b (reserved)
        Bs3Trap32GenericEntry bs3Trap32GenericTrapOrInt   ; 1c (reserved)
        Bs3Trap32GenericEntry bs3Trap32GenericTrapOrInt   ; 1d (reserved)
        Bs3Trap32GenericEntry bs3Trap32GenericTrapErrCode ; 1e
        Bs3Trap32GenericEntry bs3Trap32GenericTrapOrInt   ; 1f (reserved)
%rep 224
        Bs3Trap32GenericEntry bs3Trap32GenericTrapOrInt
%endrep
BS3_PROC_END  Bs3Trap32GenericEntries




;;
; Trap or interrupt (no error code).
;
BS3_PROC_BEGIN bs3Trap32GenericTrapOrInt
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
        jmp     bs3Trap32GenericCommon
BS3_PROC_END   bs3Trap32GenericTrapOrInt


;;
; Trap with error code.
;
BS3_PROC_BEGIN bs3Trap32GenericTrapErrCode
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
        jmp     bs3Trap32GenericCommon
BS3_PROC_END   bs3Trap32GenericTrapErrCode


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
BS3_PROC_BEGIN bs3Trap32GenericCommon
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
        add     ax, BS3_SEL_R0_DS32
        mov     ds, ax
        mov     es, ax

        ;
        ; Copy and update the mode now that we've got a flat DS.
        ;
        mov     al, [BS3_DATA16_WRT(g_bBs3CurrentMode)]
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.bMode], al
        and     al, ~BS3_MODE_CODE_MASK
        or      al, BS3_MODE_CODE_32
        mov     [BS3_DATA16_WRT(g_bBs3CurrentMode)], al

        ;
        ; Copy iret info.
        ;
        mov     ecx, [ebp + 4]
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.rip], ecx
        mov     ecx, [ebp + 12]
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.rflags], ecx
        mov     cl, [ebp + 8]
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.cs], cx
        test    dword [ebp + 12], X86_EFL_VM
        jnz     .iret_frame_v8086
        mov     ax, ss
        and     al, 3
        and     cl, 3
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.bCpl], cl
        cmp     cl, al
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
        mov     byte [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.bCpl], 3
        or      byte [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.bMode], BS3_MODE_CODE_V86 ; paranoia ^ 2
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
        str     ax
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.tr], ax
        sldt    ax
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.ldtr], ax

        ;
        ; Set context bit width and clear all upper dwords and unused register members.
        ;
.clear_and_dispatch_to_handler:         ; The double fault code joins us here.
        xor     edx, edx
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.abPadding], dx
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.abPadding + 2], edx
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
        mov     eax, [ebx * 4 + BS3_DATA16_WRT(_g_apfnBs3TrapHandlers_c32)]
        or      eax, eax
        jnz     .call_handler
        mov     eax, Bs3TrapDefaultHandler
.call_handler:
        mov     edi, esp
        push    edi
        call    eax

        ;
        ; Resume execution using trap frame.
        ;
        push    0
        add     edi, BS3TRAPFRAME.Ctx
        push    edi
        call    Bs3RegCtxRestore
.panic:
        hlt
        jmp     .panic
BS3_PROC_END   bs3Trap32GenericCommon


;;
; Helper.
;
; @retruns  Flat address in eax.
; @param    ax
; @uses     eax
;
bs3Trap32TssInAxToFlatInEax:
        ; Get the GDT base address and find the descriptor address (EAX)
        sub     esp, 16h
        sgdt    [esp + 2]               ; +2 for correct alignment.
        and     eax, 0fff8h
        add     eax, [esp + 4]          ; GDT base address.
        add     esp, 16h

        ; Get the flat TSS address from the descriptor.
        push    ecx
        mov     ecx, [eax + 4]
        and     eax, 0ffff0000h
        movzx   eax, word [eax]
        or      eax, ecx
        pop     ecx

        ret

;;
; Double fault handler.
;
; We don't have to load any selectors or clear anything in EFLAGS because the
; TSS specified sane values which got loaded during the task switch.
;
BS3_PROC_BEGIN Bs3Trap32DoubleFaultHandler
        push    0                       ; We'll copy the rip from the other TSS here later to create a more sensible call chain.
        push    ebp
        mov     ebp, esp

        ;
        ; Fill in the non-context trap frame bits.
        ;
        pushfd                          ; Get handler flags.
        pop     ecx
        xor     edx, edx                ; NULL register.

        sub     esp, BS3TRAPFRAME_size  ; Allocate trap frame.
        mov     [esp + BS3TRAPFRAME.fHandlerRfl], ecx
        mov     word [esp + BS3TRAPFRAME.bXcpt], X86_XCPT_DF
        mov     [esp + BS3TRAPFRAME.uHandlerCs], cs
        mov     [esp + BS3TRAPFRAME.uHandlerSs], ss
        lea     ecx, [ebp + 12]
        mov     [esp + BS3TRAPFRAME.uHandlerRsp], ecx
        mov     [esp + BS3TRAPFRAME.uHandlerRsp + 4], edx
        mov     ecx, [ebp + 8]
        mov     [esp + BS3TRAPFRAME.uErrCd], ecx
        mov     [esp + BS3TRAPFRAME.uErrCd + 4], edx

        ;
        ; Copy the register state from the previous task segment.
        ;

        ; Find our TSS.
        str     ax
        call    bs3Trap32TssInAxToFlatInEax

        ; Find the previous TSS.
        mov     ax, [eax + X86TSS32.selPrev]
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.tr], ax
        call    bs3Trap32TssInAxToFlatInEax

        ; Do the copying.
        mov     ecx, [eax + X86TSS32.eax]
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.rax], ecx
        mov     ecx, [eax + X86TSS32.ecx]
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.rcx], ecx
        mov     ecx, [eax + X86TSS32.edx]
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.rdx], ecx
        mov     ecx, [eax + X86TSS32.ebx]
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.rbx], ecx
        mov     ecx, [eax + X86TSS32.esp]
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.rsp], ecx
        mov     ecx, [eax + X86TSS32.ebp]
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.rbp], ecx
        mov     [ebp], ecx              ; For better call stacks.
        mov     ecx, [eax + X86TSS32.esi]
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.rsi], ecx
        mov     ecx, [eax + X86TSS32.edi]
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.rdi], ecx
        mov     ecx, [eax + X86TSS32.esi]
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.rsi], ecx
        mov     ecx, [eax + X86TSS32.eflags]
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.rflags], ecx
        mov     ecx, [eax + X86TSS32.eip]
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.rip], ecx
        mov     [ebp + 4], ecx          ; For better call stacks.
        mov     cx, [eax + X86TSS32.cs]
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.cs], cx
        mov     cx, [eax + X86TSS32.ds]
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.ds], cx
        mov     cx, [eax + X86TSS32.es]
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.es], cx
        mov     cx, [eax + X86TSS32.fs]
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.fs], cx
        mov     cx, [eax + X86TSS32.gs]
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.gs], cx
        mov     cx, [eax + X86TSS32.ss]
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.ss], cx
        mov     cx, [eax + X86TSS32.selLdt]             ; Note! This isn't necessarily the ldtr at the time of the fault.
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.ldtr], cx
        mov     cx, [eax + X86TSS32.cr3]                ; Note! This isn't necessarily the cr3 at the time of the fault.
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.cr3], ecx

        ;
        ; Set CPL; copy and update mode.
        ;
        mov     cl, [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.ss]
        and     cl, 3
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.bCpl], cl

        mov     cl, [BS3_DATA16_WRT(g_bBs3CurrentMode)]
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.bMode], cl
        and     cl, ~BS3_MODE_CODE_MASK
        or      cl, BS3_MODE_CODE_32
        mov     [BS3_DATA16_WRT(g_bBs3CurrentMode)], cl

        ;
        ; Control registers.
        ;
        mov     ecx, cr0
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.cr0], ecx
        mov     ecx, cr2
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.cr2], ecx
        mov     ecx, cr4
        mov     [esp + BS3TRAPFRAME.Ctx + BS3REGCTX.cr4], ecx

        ;
        ; Join code paths with the generic handler code.
        ;
        jmp     bs3Trap32GenericCommon.clear_and_dispatch_to_handler
BS3_PROC_END   Bs3Trap32DoubleFaultHandler

