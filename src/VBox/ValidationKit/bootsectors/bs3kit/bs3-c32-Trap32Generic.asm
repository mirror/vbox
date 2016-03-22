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
BS3_EXTERN_SYSTEM16 Bs3Gdt
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
        push    ebp                     ; 0
        mov     ebp, esp
        pushfd                          ; -04h
        cli
        cld
        push    eax                     ; -08h
        push    edi                     ; -0ch
        push    ss                      ; -10h
        push    ds                      ; -14h

        ; Make sure we've got a flat DS (ASSUMES ring-0). It makes everything so much simpler.
        mov     ax, BS3_SEL_R0_DS32
        mov     ds, ax

        ;
        ; We may be comming from 16-bit code with a 16-bit SS.  Thunk it as
        ; the C code may assume flat SS and we'll mess up by using EBP/ESP/EDI
        ; instead of BP/SP/SS:DI. ASSUMES standard GDT selector.
        ;
        mov     ax, ss
        lar     eax, ax
        test    eax, X86LAR_F_D
        jz      .stack_thunk
        mov     ax, BS3_SEL_R0_SS32
        mov     ss, ax
        jmp     .stack_flat
.stack_thunk:
hlt
        mov     di, ss
        and     edi, X86_SEL_MASK_OFF_RPL
        mov     al, [X86DESCGENERIC_BIT_OFF_BASE_HIGH1 / 8 + edi + Bs3Gdt wrt FLAT]
        mov     ah, [X86DESCGENERIC_BIT_OFF_BASE_HIGH2 / 8 + edi + Bs3Gdt wrt FLAT]
        shl     eax, 16
        mov     ax, [X86DESCGENERIC_BIT_OFF_BASE_LOW / 8   + edi + Bs3Gdt wrt FLAT] ; eax = SS.base
        movzx   ebp, bp                 ; SS:BP -> flat EBP.
        add     ebp, eax
        movzx   edi, sp                 ; SS:SP -> flat ESP in EAX.
        add     eax, edi
        mov     di, BS3_SEL_R0_SS32
        mov     ss, di
        mov     esp, eax
.stack_flat:

        ; Reserve space for the the register and trap frame.
        mov     eax, (BS3TRAPFRAME_size + 7) / 8
AssertCompileSizeAlignment(BS3TRAPFRAME, 8)
.more_zeroed_space:
        push    dword 0
        push    dword 0
        dec     eax
        jnz     .more_zeroed_space
        mov     edi, esp                ; edi points to trapframe structure.

        ; Copy stuff from the stack over.
        mov     al, [ebp + 4]
        mov     [edi + BS3TRAPFRAME.bXcpt], al
        mov     eax, [ebp]
        mov     [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.rbp], eax
        mov     eax, [ebp - 04h]
        mov     [edi + BS3TRAPFRAME.fHandlerRfl], eax
        mov     eax, [ebp - 08h]
        mov     [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.rax], eax
        mov     eax, [ebp - 0ch]
        mov     [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.rdi], eax
        mov     ax, [ebp - 10h]
        mov     [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.ss], ax
        mov     [edi + BS3TRAPFRAME.uHandlerSs], ax
        mov     ax, [ebp - 14h]
        mov     [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.ds], ax

        lea     ebp, [ebp + 4]          ; iret - 4 (i.e. ebp frame chain location)
        jmp     bs3Trap32GenericCommon
BS3_PROC_END   bs3Trap32GenericTrapOrInt


;;
; Trap with error code.
;
BS3_PROC_BEGIN bs3Trap32GenericTrapErrCode
        push    ebp                     ; 0
        mov     ebp, esp
        pushfd                          ; -04h
        cli
        cld
        push    eax                     ; -08h
        push    edi                     ; -0ch
        push    ss                      ; -10h
        push    ds                      ; -14h

        ; Make sure we've got a flat DS (ASSUMES ring-0). It makes everything so much simpler.
        mov     ax, BS3_SEL_R0_DS32
        mov     ds, ax

        ;
        ; We may be comming from 16-bit code with a 16-bit SS.  Thunk it as
        ; the C code may assume flat SS and we'll mess up by using EBP/ESP/EDI
        ; instead of BP/SP/SS:DI. ASSUMES standard GDT selector.
        ;
        mov     ax, ss
        lar     eax, ax
        test    eax, X86LAR_F_D
        jz      .stack_thunk
        mov     ax, BS3_SEL_R0_SS16
        mov     ss, ax
        jmp     .stack_flat
.stack_thunk:
        mov     di, ss
        and     edi, X86_SEL_MASK_OFF_RPL
        mov     al, [X86DESCGENERIC_BIT_OFF_BASE_HIGH1 / 8 + edi + Bs3Gdt wrt FLAT]
        mov     ah, [X86DESCGENERIC_BIT_OFF_BASE_HIGH2 / 8 + edi + Bs3Gdt wrt FLAT]
        shl     eax, 16
        mov     ax, [X86DESCGENERIC_BIT_OFF_BASE_LOW / 8   + edi + Bs3Gdt wrt FLAT] ; eax = SS.base
        movzx   ebp, bp                 ; SS:BP -> flat EBP.
        add     ebp, eax
        movzx   edi, sp                 ; SS:SP -> flat ESP in EAX.
        add     eax, edi
        mov     di, BS3_SEL_R0_SS16
        mov     ss, di
        mov     esp, eax
.stack_flat:

        ; Reserve space for the the register and trap frame.
        mov     eax, (BS3TRAPFRAME_size + 7) / 8
AssertCompileSizeAlignment(BS3TRAPFRAME, 8)
.more_zeroed_space:
        push    dword 0
        push    dword 0
        dec     eax
        jnz     .more_zeroed_space
        mov     edi, esp                ; edi points to trapframe structure.

        ; Copy stuff from the stack over.
        mov     eax, [ebp + 8]
;; @todo Do voodoo checks for 'int xx' or misguided hardware interrupts.
        mov     [edi + BS3TRAPFRAME.uErrCd], eax
        mov     al, [ebp + 4]
        mov     [edi + BS3TRAPFRAME.bXcpt], al
        mov     eax, [ebp]
        mov     [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.rbp], eax
        mov     eax, [ebp - 04h]
        mov     [edi + BS3TRAPFRAME.fHandlerRfl], eax
        mov     eax, [ebp - 08h]
        mov     [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.rax], eax
        mov     eax, [ebp - 0ch]
        mov     [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.rdi], eax
        mov     ax, [ebp - 10h]
        mov     [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.ss], ax
        mov     [edi + BS3TRAPFRAME.uHandlerSs], ax
        mov     ax, [ebp - 14h]
        mov     [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.ds], ax

        lea     ebp, [ebp + 8]          ; iret - 4 (i.e. ebp frame chain location)
        jmp     bs3Trap32GenericCommon
BS3_PROC_END   bs3Trap32GenericTrapErrCode


;;
; Common context saving code and dispatching.
;
; @param    edi     Pointer to the trap frame.  The following members have been
;                   filled in by the previous code:
;                       - bXcpt
;                       - uErrCd
;                       - fHandlerRFL
;                       - uHandlerSs
;                       - Ctx.rax (except upper dword)
;                       - Ctx.rbp (except upper dword)
;                       - Ctx.rdi (except upper dword)
;                       - Ctx.ds
;                       - Ctx.ss
;
; @param    ebp     Pointer to the dword before the iret frame, i.e. where ebp
;                   would be saved if this was a normal call.
;
BS3_PROC_BEGIN bs3Trap32GenericCommon
        ;
        ; Fake EBP frame.
        ;
        mov     eax, [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.rbp]
        mov     [ebp], eax

        ;
        ; Save the remaining GPRs and segment registers.
        ;
        mov     [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.rcx], ecx
        mov     [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.rdx], edx
        mov     [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.rbx], ebx
        mov     [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.rsi], esi
        mov     [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.es], es
        mov     [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.fs], fs
        mov     [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.gs], gs

        ;
        ; Load 32-bit data selector for the DPL we're executing at into DS and ES.
        ; Save the handler CS value first.
        ;
        mov     ax, cs
        mov     [edi + BS3TRAPFRAME.uHandlerCs], ax
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
        mov     [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.bMode], al
        and     al, ~BS3_MODE_CODE_MASK
        or      al, BS3_MODE_CODE_32
        mov     [BS3_DATA16_WRT(g_bBs3CurrentMode)], al

        ;
        ; Copy iret info.
        ;
        mov     ecx, [ebp + 4]
        mov     [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.rip], ecx
        mov     ecx, [ebp + 12]
        mov     [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.rflags], ecx
        mov     cx, [ebp + 8]
        mov     [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.cs], cx
        test    dword [ebp + 12], X86_EFL_VM
        jnz     .iret_frame_v8086
        mov     ax, ss
        and     al, 3
        and     cl, 3
        mov     [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.bCpl], cl
        cmp     cl, al
        je      .iret_frame_same_cpl

.iret_frame_different_cpl:
        mov     ecx, [ebp + 16]
        mov     [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.rsp], ecx
        mov     cx, [ebp + 20]
        mov     [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.ss], cx
        lea     eax, [ebp + 24]
        mov     [edi + BS3TRAPFRAME.uHandlerRsp], eax
        jmp     .iret_frame_done

.iret_frame_same_cpl:
        lea     ecx, [ebp + 16]
        mov     [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.rsp], ecx
        mov     [edi + BS3TRAPFRAME.uHandlerRsp], ecx
        jmp     .iret_frame_done

.iret_frame_v8086:
        mov     byte [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.bCpl], 3
        or      byte [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.bMode], BS3_MODE_CODE_V86 ; paranoia ^ 2
        movzx   ecx, word [ebp + 16]
        mov     [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.rsp], ecx
        mov     cx, [ebp + 20]
        mov     [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.ss], cx
        mov     cx, [ebp + 24]
        mov     [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.es], cx
        mov     cx, [ebp + 28]
        mov     [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.ds], cx
        mov     cx, [ebp + 32]
        mov     [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.fs], cx
        mov     cx, [ebp + 36]
        mov     [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.gs], cx
        lea     eax, [ebp + 40]
        mov     [edi + BS3TRAPFRAME.uHandlerRsp], eax
        jmp     .iret_frame_done

.iret_frame_done:
        ;
        ; Control registers.
        ;
        mov     eax, cr0
        mov     [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.cr0], eax
        mov     eax, cr2
        mov     [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.cr2], eax
        mov     eax, cr3
        mov     [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.cr3], eax
        mov     eax, cr4
        mov     [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.cr4], eax
        str     ax
        mov     [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.tr], ax
        sldt    ax
        mov     [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.ldtr], ax

        ;
        ; Dispatch it to C code.
        ;
.dispatch_to_handler:                   ; The double fault code joins us here.
        movzx   ebx, byte [edi + BS3TRAPFRAME.bXcpt]
        mov     eax, [ebx * 4 + BS3_DATA16_WRT(_g_apfnBs3TrapHandlers_c32)]
        or      eax, eax
        jnz     .call_handler
        mov     eax, Bs3TrapDefaultHandler
.call_handler:
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
        sub     esp, 8+2
        sgdt    [esp]
        and     eax, 0fff8h
        add     eax, [esp + 2]          ; GDT base address.
        add     esp, 8+2

        ; Get the flat TSS address from the descriptor.
        mov     al, [eax + (X86DESCGENERIC_BIT_OFF_BASE_HIGH1 / 8)]
        mov     ah, [eax + (X86DESCGENERIC_BIT_OFF_BASE_HIGH2 / 8)]
        shl     eax, 16
        mov     ax, [eax + (X86DESCGENERIC_BIT_OFF_BASE_LOW / 8)]
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

        pushfd                          ; Get handler flags.
        pop     ecx

        xor     edx, edx                ; NULL register.

        ;
        ; Allocate a zero filled trap frame.
        ;
        mov     eax, (BS3TRAPFRAME_size + 7) / 8
AssertCompileSizeAlignment(BS3TRAPFRAME, 8)
.more_zeroed_space:
        push    edx
        push    edx
        dec     eax
        jz      .more_zeroed_space
        mov     edi, esp

        ;
        ; Fill in the non-context trap frame bits.
        ;
        mov     [edi + BS3TRAPFRAME.fHandlerRfl], ecx
        mov     word [edi + BS3TRAPFRAME.bXcpt], X86_XCPT_DF
        mov     [edi + BS3TRAPFRAME.uHandlerCs], cs
        mov     [edi + BS3TRAPFRAME.uHandlerSs], ss
        lea     ecx, [ebp + 12]
        mov     [edi + BS3TRAPFRAME.uHandlerRsp], ecx
        mov     ecx, [ebp + 8]
        mov     [edi + BS3TRAPFRAME.uErrCd], ecx

        ;
        ; Copy the register state from the previous task segment.
        ;

        ; Find our TSS.
        str     ax
        call    bs3Trap32TssInAxToFlatInEax

        ; Find the previous TSS.
        mov     ax, [eax + X86TSS32.selPrev]
        mov     [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.tr], ax
        call    bs3Trap32TssInAxToFlatInEax

        ; Do the copying.
        mov     ecx, [eax + X86TSS32.eax]
        mov     [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.rax], ecx
        mov     ecx, [eax + X86TSS32.ecx]
        mov     [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.rcx], ecx
        mov     ecx, [eax + X86TSS32.edx]
        mov     [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.rdx], ecx
        mov     ecx, [eax + X86TSS32.ebx]
        mov     [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.rbx], ecx
        mov     ecx, [eax + X86TSS32.esp]
        mov     [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.rsp], ecx
        mov     ecx, [eax + X86TSS32.ebp]
        mov     [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.rbp], ecx
        mov     [ebp], ecx              ; For better call stacks.
        mov     ecx, [eax + X86TSS32.esi]
        mov     [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.rsi], ecx
        mov     ecx, [eax + X86TSS32.edi]
        mov     [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.rdi], ecx
        mov     ecx, [eax + X86TSS32.esi]
        mov     [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.rsi], ecx
        mov     ecx, [eax + X86TSS32.eflags]
        mov     [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.rflags], ecx
        mov     ecx, [eax + X86TSS32.eip]
        mov     [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.rip], ecx
        mov     [ebp + 4], ecx          ; For better call stacks.
        mov     cx, [eax + X86TSS32.cs]
        mov     [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.cs], cx
        mov     cx, [eax + X86TSS32.ds]
        mov     [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.ds], cx
        mov     cx, [eax + X86TSS32.es]
        mov     [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.es], cx
        mov     cx, [eax + X86TSS32.fs]
        mov     [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.fs], cx
        mov     cx, [eax + X86TSS32.gs]
        mov     [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.gs], cx
        mov     cx, [eax + X86TSS32.ss]
        mov     [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.ss], cx
        mov     cx, [eax + X86TSS32.selLdt]             ; Note! This isn't necessarily the ldtr at the time of the fault.
        mov     [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.ldtr], cx
        mov     cx, [eax + X86TSS32.cr3]                ; Note! This isn't necessarily the cr3 at the time of the fault.
        mov     [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.cr3], ecx

        ;
        ; Set CPL; copy and update mode.
        ;
        mov     cl, [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.ss]
        and     cl, 3
        mov     [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.bCpl], cl

        mov     cl, [BS3_DATA16_WRT(g_bBs3CurrentMode)]
        mov     [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.bMode], cl
        and     cl, ~BS3_MODE_CODE_MASK
        or      cl, BS3_MODE_CODE_32
        mov     [BS3_DATA16_WRT(g_bBs3CurrentMode)], cl

        ;
        ; Control registers.
        ;
        mov     ecx, cr0
        mov     [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.cr0], ecx
        mov     ecx, cr2
        mov     [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.cr2], ecx
        mov     ecx, cr4
        mov     [edi + BS3TRAPFRAME.Ctx + BS3REGCTX.cr4], ecx

        ;
        ; Join code paths with the generic handler code.
        ;
        jmp     bs3Trap32GenericCommon.dispatch_to_handler
BS3_PROC_END   Bs3Trap32DoubleFaultHandler

