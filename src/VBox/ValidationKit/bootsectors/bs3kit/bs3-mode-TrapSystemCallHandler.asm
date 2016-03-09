; $Id$
;; @file
; BS3Kit - System call trap handler.
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


BS3_EXTERN_DATA16 g_bBs3CurrentMode
BS3_EXTERN_DATA16 g_uBs3CpuDetected
TMPL_BEGIN_TEXT

%if TMPL_BITS == 32
BS3_EXTERN_CMN Bs3SelProtFar32ToFlat32
%endif
BS3_EXTERN_CMN Bs3RegCtxConvertToRingX
BS3_EXTERN_CMN Bs3RegCtxRestore
BS3_EXTERN_CMN Bs3Panic
TMPL_BEGIN_TEXT


;;
; System call handler.
;
; This is an assembly trap handler that is called in response to a system call
; request from 'user' code.  The only fixed parameter is [ER]AX which contains
; the system call number.  Other registers are assigned on a per system call
; basis, ditto for which registers are preserved and which are used to return
; stuff.  Generally, though, we preserve all registers not used as return
; values or otherwise implicitly transformed by the call.
;
; Note! The 16-bit versions of this code must be careful with using extended
;       registers as we wish this code to work on real 8086 CPUs too!
;
BS3_PROC_BEGIN_MODE Bs3TrapSystemCallHandler
        push    xBP
        mov     xBP, xSP
%ifndef TMPL_64BIT
 %define VAR_CALLER_DS      [xBP - xCB]
        push    ds
 %ifdef TMPL_CMN_R86
        push    BS3DATA16
 %else
        push    RT_CONCAT(BS3_SEL_R0_DS,TMPL_BITS)
 %endif
        pop     ds
 %define VAR_CALLER_BP      [xBP]
 %define VAR_CALLER_DS      [xBP -       - xCB]
 %define VAR_CALLER_BX      [xBP - xCB*1 - xCB]
 %define VAR_CALLER_AX      [xBP - xCB*2 - xCB]
 %define VAR_CALLER_CX      [xBP - xCB*3 - xCB]
 %define VAR_CALLER_DX      [xBP - xCB*4 - xCB]
 %define VAR_CALLER_MODE    [xBP - xCB*5 - xCB]
%else
 %define VAR_CALLER_BP      [xBP]
 %define VAR_CALLER_BX      [xBP - xCB*1]
 %define VAR_CALLER_AX      [xBP - xCB*2]
 %define VAR_CALLER_CX      [xBP - xCB*3]
 %define VAR_CALLER_DX      [xBP - xCB*4]
 %define VAR_CALLER_MODE    [xBP - xCB*5]
%endif
        push    xBX
        push    xAX
        push    xCX
        push    xDX

        ; VAR_CALLER_MODE: Save the current mode (important for v8086 with 16-bit kernel).
        xor     xBX, xBX
        mov     bl, [g_bBs3CurrentMode]
        push    xBX

        ;
        ; Dispatch the system call.
        ;
        cmp     ax, BS3_SYSCALL_LAST
        ja      .invalid_syscall
%ifdef TMPL_16BIT
        mov     bx, ax
        shl     bx, 1
        jmp     word [cs:.aoffSyscallHandlers + bx]
%else
        movzx   ebx, ax
        mov     ebx, [.aoffSyscallHandlers + ebx * 4]
        jmp     xBX
%endif
.aoffSyscallHandlers:
%ifdef TMPL_16BIT
        dw      .invalid_syscall wrt BS3TEXT16
        dw      .print_chr       wrt BS3TEXT16
        dw      .print_str       wrt BS3TEXT16
        dw      .to_ringX        wrt BS3TEXT16
        dw      .to_ringX        wrt BS3TEXT16
        dw      .to_ringX        wrt BS3TEXT16
        dw      .to_ringX        wrt BS3TEXT16
%else
        dd      .invalid_syscall wrt FLAT
        dd      .print_chr       wrt FLAT
        dd      .print_str       wrt FLAT
        dd      .to_ringX        wrt FLAT
        dd      .to_ringX        wrt FLAT
        dd      .to_ringX        wrt FLAT
        dd      .to_ringX        wrt FLAT
%endif

        ;
        ; Invalid system call.
        ;
.invalid_syscall:
        int3
        jmp     .return


        ;
        ; Print char in the CL register.
        ;
        ; We use the vga bios teletype interrupt to do the writing, so we must
        ; be in some kind of real mode for this to work.  16-bit code segment
        ; requried for the mode switching code.
        ;
%ifndef TMPL_16BIT
BS3_BEGIN_TEXT16
        BS3_SET_BITS TMPL_BITS
%endif
.print_chr:
        push    xDI
        push    xSI
%ifndef TMPL_CMN_R86
        ; Switch to real mode (20h param scratch area not required).
        extern  TMPL_NM(Bs3SwitchToRM)
        call    TMPL_NM(Bs3SwitchToRM)
        BS3_SET_BITS 16
%endif

        ; Print the character.
        mov     bx, 0ff00h
        mov     al, cl
        mov     ah, 0eh
        int     10h

%ifndef TMPL_CMN_R86
        ; Switch back (20h param scratch area not required).
        extern  RT_CONCAT3(_Bs3SwitchTo,TMPL_MODE_UNAME,_rm)
        call    RT_CONCAT3(_Bs3SwitchTo,TMPL_MODE_UNAME,_rm)
        BS3_SET_BITS TMPL_BITS
%endif
        pop     xSI
        pop     xDI
        jmp     .return
%ifndef TMPL_16BIT
TMPL_BEGIN_TEXT
%endif


        ;
        ; Print CX chars from string pointed to by DX:SI in 16-bit and v8086 mode,
        ; and ESI/RSI in 64-bit and 32-bit mode (flat).
        ;
        ; We use the vga bios teletype interrupt to do the writing, so we must
        ; be in some kind of real mode for this to work.  16-bit code segment
        ; requried for the mode switching code.
        ;
.print_str:
        push    xSI                     ; we setup ds:xSI to point to the thing.
%if TMPL_BITS != 64
        mov     bl, byte VAR_CALLER_MODE
        and     bl, BS3_MODE_CODE_MASK
        cmp     bl, BS3_MODE_CODE_V86
        jne     .print_str_not_v8086
        ;; @todo this gets complicated _fast_. Later.
.print_str_not_v8086:
%endif
        int3
        jmp     .return


        ;
        ; Switch the caller to ring-0, ring-1, ring-2 or ring-3.
        ;
        ; This implement this by saving the entire register context, calling
        ; a transformation function (C) and restoring the modified register
        ; context using a generic worker.
        ;
.to_ringX:
        sub     xSP, BS3REGCTX_size
        mov     xBX, xSP                ; xBP = BS3REGCTX pointer.
        call    .save_context

%if TMPL_BITS == 32
        ; Convert xBP to flat pointer in 32-bit
        push    ss
        push    xBX
        call    Bs3SelProtFar32ToFlat32
        add     sSP, 8
        mov     xBX, xAX
%endif
        ; Prepare the last call frame.
        BS3_ONLY_16BIT_STMT push    ss
        push    xBX

        ; Convert the register context from whatever it is to ring-0.
        BS3_ONLY_16BIT_STMT push    ss
        push    xBX
        mov     ax, VAR_CALLER_AX
        sub     ax, BS3_SYSCALL_TO_RING0
        push    xAX
        BS3_CALL Bs3RegCtxConvertToRingX, 2
        add     xSP, sCB

        ; Restore the register context (does not return).
        pop     xBX                     ; restore saved pointer.
        BS3_ONLY_16BIT_STMT push    ss
        push    xBX
        BS3_CALL Bs3RegCtxRestore, 1
        jmp     Bs3Panic


        ;
        ; Return.
        ;
.return:
        pop     xBX                     ; saved mode
%if TMPL_BITS == 16
        and     bl, BS3_MODE_CODE_MASK
        cmp     bl, BS3_MODE_CODE_V86
        je      .return_to_v8086_from_16bit_krnl
%endif
        pop     xDX
        pop     xCX
        pop     xAX
        pop     xBX
%ifndef TMPL_64BIT
        pop     ds
%endif
        leave
%ifdef TMPL_64BIT
        iretq
%else
        iret
%endif

%if TMPL_BITS == 16
.return_to_v8086_from_16bit_krnl:
        int3
        jmp     .return_to_v8086_from_16bit_krnl
%endif



        ;
        ; Internal function. ss:xBX = Pointer to register frame (BS3REGCTX).
        ; @uses xAX
        ;
.save_context:
%if TMPL_BITS == 16
        cmp     byte [g_uBs3CpuDetected], BS3CPU_80386
        jae     .save_context_full

        ;
        ; 80286 or earlier.
        ;

        ; Clear the state area first.
        push    di
        xor     di, di
.save_context_16_clear_loop:
        mov     word [ss:bx + di], 0
        mov     word [ss:bx + di + 2], 0
        mov     word [ss:bx + di + 4], 0
        mov     word [ss:bx + di + 6], 0
        add     di, 8
        cmp     di, BS3REGCTX_size
        jb      .save_context_16_clear_loop
        pop     di

        ; Do the 8086/80186/80286 state saving.
        mov     ax, VAR_CALLER_AX
        mov     [ss:bx + BS3REGCTX.rax], ax
        mov     cx, VAR_CALLER_CX
        mov     [ss:bx + BS3REGCTX.rcx], ax
        mov     ax, VAR_CALLER_DX
        mov     [ss:bx + BS3REGCTX.rdx], ax
        mov     ax, VAR_CALLER_BX
        mov     [ss:bx + BS3REGCTX.rbx], ax
        mov     [ss:bx + BS3REGCTX.rsi], si
        mov     [ss:bx + BS3REGCTX.rdi], di
        mov     ax, VAR_CALLER_BP
        mov     [ss:bx + BS3REGCTX.rbp], ax
        mov     [ss:bx + BS3REGCTX.es], es
        mov     ax, [xBP + xCB]
        mov     [ss:bx + BS3REGCTX.rip], ax
        mov     ax, [xBP + xCB*2]
        mov     [ss:bx + BS3REGCTX.cs], ax
        and     al, X86_SEL_RPL
        mov     [ss:bx + BS3REGCTX.bCpl], al
        cmp     al, 0
        je      .save_context_16_same
        mov     ax, [xBP + xCB*4]
        mov     [ss:bx + BS3REGCTX.rsp], ax
        mov     ax, [xBP + xCB*5]
        mov     [ss:bx + BS3REGCTX.ss], ax
        jmp     .save_context_16_done_stack
.save_context_16_same:
        mov     ax, bp
        add     ax, xCB * (1 + 3)
        mov     [ss:bx + BS3REGCTX.rsp], ax
        mov     ax, ss
        mov     [ss:bx + BS3REGCTX.ss], ax
.save_context_16_done_stack:
        mov     ax, [xBP + xCB*3]
        mov     [ss:bx + BS3REGCTX.rflags], ax
        mov     al, VAR_CALLER_MODE
        mov     [ss:bx + BS3REGCTX.bMode], al
        cmp     byte [g_uBs3CpuDetected], BS3CPU_80286
        jne     .save_context_16_return
        smsw    [ss:bx + BS3REGCTX.cr0]
        str     [ss:bx + BS3REGCTX.tr]
        sldt    [ss:bx + BS3REGCTX.ldtr]
.save_context_16_return:
        ret
%endif ; TMPL_BITS == 16

        ;
        ; 80386 or later.
        ;
.save_context_full:

        ; Clear the state area first unless 64-bit mode.
%if TMPL_BITS != 64
        push    xDI
        xor     xDI, xDI
.save_context_32_clear_loop:
        mov     dword [ss:xBX + xDI], 0
        mov     dword [ss:xBX + xDI + 4], 0
        add     xDI, 8
        cmp     xDI, BS3REGCTX_size
        jb      .save_context_32_clear_loop
        pop     xDI
%endif

        ; Do the 386+ state saving.
%if TMPL_BITS == 16                     ; save the high word of registered pushed on the stack.
        mov     [ss:bx + BS3REGCTX.rax], eax
        mov     [ss:bx + BS3REGCTX.rcx], ecx
        mov     [ss:bx + BS3REGCTX.rdx], edx
        mov     [ss:bx + BS3REGCTX.rbx], ebx
        mov     [ss:bx + BS3REGCTX.rbp], ebp
        mov     [ss:bx + BS3REGCTX.rsp], esp
        pushfd
        pop     dword [ss:bx + BS3REGCTX.rflags]
%endif
        mov     xAX, VAR_CALLER_AX
        mov     [BS3_NOT_64BIT(ss:) xBX + BS3REGCTX.rax], xAX
        mov     xCX, VAR_CALLER_CX
        mov     [BS3_NOT_64BIT(ss:) xBX + BS3REGCTX.rcx], xCX
        mov     xAX, VAR_CALLER_DX
        mov     [BS3_NOT_64BIT(ss:) xBX + BS3REGCTX.rdx], xAX
        mov     xAX, VAR_CALLER_BX
        mov     [BS3_NOT_64BIT(ss:) xBX + BS3REGCTX.rbx], xAX
        mov     [BS3_NOT_64BIT(ss:) xBX + BS3REGCTX.rsi], sSI
        mov     [BS3_NOT_64BIT(ss:) xBX + BS3REGCTX.rdi], sDI
        mov     xAX, VAR_CALLER_BP
        mov     [BS3_NOT_64BIT(ss:) xBX + BS3REGCTX.rbp], xAX
        mov     [BS3_NOT_64BIT(ss:) xBX + BS3REGCTX.es], es
        mov     [BS3_NOT_64BIT(ss:) xBX + BS3REGCTX.fs], fs
        mov     [BS3_NOT_64BIT(ss:) xBX + BS3REGCTX.gs], gs
        mov     xAX, [xBP + xCB]
        mov     [BS3_NOT_64BIT(ss:) xBX + BS3REGCTX.rip], xAX
        mov     ax, [xBP + xCB*2]
        mov     [BS3_NOT_64BIT(ss:) xBX + BS3REGCTX.cs], ax
%if TMPL_MODE != BS3_MODE_RM
        and     al, X86_SEL_RPL
        mov     [BS3_NOT_64BIT(ss:) xBX + BS3REGCTX.bCpl], al
        cmp     al, 0
        je      .save_context_full_same
        mov     xAX, [xBP + xCB*4]
        mov     [BS3_NOT_64BIT(ss:) xBX + BS3REGCTX.rsp], xAX
        mov     ax, [xBP + xCB*5]
        mov     [BS3_NOT_64BIT(ss:) xBX + BS3REGCTX.ss], ax
        jmp     .save_context_full_done_stack
%else
        mov     byte [BS3_NOT_64BIT(ss:) xBX + BS3REGCTX.bCpl], 0
%endif
.save_context_full_same:
        mov     xAX, xBP
        add     xAX, xCB * (1 + 3)
        mov     [BS3_NOT_64BIT(ss:) xBX + BS3REGCTX.rsp], xAX
        mov     ax, ss
        mov     [BS3_NOT_64BIT(ss:) xBX + BS3REGCTX.ss], ax
.save_context_full_done_stack:
        mov     xAX, [xBP + xCB*3]
        mov     [BS3_NOT_64BIT(ss:) xBX + BS3REGCTX.rflags], sAX

        mov     al, VAR_CALLER_MODE
        mov     [BS3_NOT_64BIT(ss:) xBX + BS3REGCTX.bMode], al
%if TMPL_BITS == 64
        mov     [BS3_NOT_64BIT(ss:) xBX + BS3REGCTX.r8], r8
        mov     [BS3_NOT_64BIT(ss:) xBX + BS3REGCTX.r9], r9
        mov     [BS3_NOT_64BIT(ss:) xBX + BS3REGCTX.r10], r10
        mov     [BS3_NOT_64BIT(ss:) xBX + BS3REGCTX.r11], r11
        mov     [BS3_NOT_64BIT(ss:) xBX + BS3REGCTX.r12], r12
        mov     [BS3_NOT_64BIT(ss:) xBX + BS3REGCTX.r13], r13
        mov     [BS3_NOT_64BIT(ss:) xBX + BS3REGCTX.r14], r14
        mov     [BS3_NOT_64BIT(ss:) xBX + BS3REGCTX.r15], r15
%endif
        str     [BS3_NOT_64BIT(ss:) xBX + BS3REGCTX.tr]
        sldt    [BS3_NOT_64BIT(ss:) xBX + BS3REGCTX.ldtr]
        mov     sAX, cr0
        mov     [BS3_NOT_64BIT(ss:) xBX + BS3REGCTX.cr0], sAX
        mov     sAX, cr2
        mov     [BS3_NOT_64BIT(ss:) xBX + BS3REGCTX.cr2], sAX
        mov     sAX, cr3
        mov     [BS3_NOT_64BIT(ss:) xBX + BS3REGCTX.cr3], sAX
        mov     sAX, cr4
        mov     [BS3_NOT_64BIT(ss:) xBX + BS3REGCTX.cr4], sAX

%if TMPL_BITS != 64
        ; Deal with extended v8086 frame.
 %if TMPL_BITS == 32
        test    dword [BS3_NOT_64BIT(ss:) xBX + BS3REGCTX.rflags], X86_EFL_VM
        jz      .save_context_full_return
 %else
        mov     al, VAR_CALLER_MODE
        and     al, BS3_MODE_CODE_MASK
        cmp     al, BS3_MODE_CODE_V86
        jne     .save_context_full_return
        mov     dword [BS3_NOT_64BIT(ss:) xBX + BS3REGCTX.rflags], X86_EFL_VM
 %endif
        mov     xAX, [xBP + xCB*4]
        mov     [BS3_NOT_64BIT(ss:) xBX + BS3REGCTX.rsp], xAX
        mov     ax, [xBP + xCB*5]
        mov     [BS3_NOT_64BIT(ss:) xBX + BS3REGCTX.ss], ax
        mov     ax, [xBP + xCB*6]
        mov     [BS3_NOT_64BIT(ss:) xBX + BS3REGCTX.es], ax
        mov     ax, [xBP + xCB*7]
        mov     [BS3_NOT_64BIT(ss:) xBX + BS3REGCTX.ds], ax
        mov     ax, [xBP + xCB*8]
        mov     [BS3_NOT_64BIT(ss:) xBX + BS3REGCTX.fs], ax
        mov     ax, [xBP + xCB*9]
        mov     [BS3_NOT_64BIT(ss:) xBX + BS3REGCTX.gs], ax
        mov     byte [BS3_NOT_64BIT(ss:) xBX + BS3REGCTX.bCpl], 3
%endif
.save_context_full_return:
        ret


BS3_PROC_END_MODE   Bs3TrapSystemCallHandler

