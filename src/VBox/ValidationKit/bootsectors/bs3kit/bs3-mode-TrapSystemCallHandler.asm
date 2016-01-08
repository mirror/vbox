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


%ifdef TMPL_CMN_R86
; Make sure BS3DATA16 is defined so we can refere to it below.
BS3_BEGIN_DATA16
BS3_BEGIN_TEXT16
%endif


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
BS3_PROC_BEGIN_MODE Bs3TrapSystemCallHandler
        push    xBP
        mov     xBP, xSP
%ifndef TMPL_64BIT
        push    ds
 %ifdef TMPL_CMN_R86
        push    BS3DATA16
 %else
        push    RT_CONCAT(BS3_SEL_R0_DS,TMPL_BITS)
 %endif
        pop     ds
%endif
        push    xBX
        push    xAX
        push    xCX
        push    xDX

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
        dw      .to_ring0        wrt BS3TEXT16
        dw      .to_ring1        wrt BS3TEXT16
        dw      .to_ring2        wrt BS3TEXT16
        dw      .to_ring3        wrt BS3TEXT16
%else
        dd      .invalid_syscall wrt FLAT
        dd      .print_chr       wrt FLAT
        dd      .print_str       wrt FLAT
        dd      .to_ring0        wrt FLAT
        dd      .to_ring1        wrt FLAT
        dd      .to_ring2        wrt FLAT
        dd      .to_ring3        wrt FLAT
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
        jmp     .return
%ifndef TMPL_16BIT
TMPL_BEGIN_TEXT
%endif

        ;
        ; Print CX chars from string pointed to by DS:[E]DI
        ;
        ; We use the vga bios teletype interrupt to do the writing, so we must
        ; be in some kind of real mode for this to work.  16-bit code segment
        ; requried for the mode switching code.
        ;
.print_str:
        int3
        jmp     .return

.to_ring0:
        int3
        jmp     .return

.to_ring1:
        int3
        jmp     .return

.to_ring2:
        int3
        jmp     .return

.to_ring3:
        int3
        jmp     .return

        ;
        ; Return.
        ;
.return:
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
BS3_PROC_END_MODE   Bs3TrapSystemCallHandler

