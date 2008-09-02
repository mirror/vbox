; $Id$
;; @file
; VirtualBox Support Driver - Windows NT specific assembly parts.
;

;
; Copyright (C) 2006-2007 Sun Microsystems, Inc.
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
; Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
; Clara, CA 95054 USA or visit http://www.sun.com if you need
; additional information or have any questions.
;

;*******************************************************************************
;* Header Files                                                                *
;*******************************************************************************
%include "iprt/asmdefs.mac"

BEGINCODE
%ifdef RT_ARCH_AMD64
%define _DbgPrint DbgPrint
%endif
extern _DbgPrint

;;
; Kind of alias for DbgPrint
BEGINPROC AssertMsg2
        jmp     _DbgPrint
ENDPROC AssertMsg2

;;
; Kind of alias for DbgPrint
BEGINPROC SUPR0Printf
        jmp     _DbgPrint
ENDPROC SUPR0Printf


%ifdef SUPDRV_WITH_UNWIND_HACK
 %if 0 ; def RT_ARCH_AMD64
;;
; @cproto DECLASM(int) supdrvNtWrapVMMR0EntryEx(PFNRT pfnVMMR0EntryEx, PVM pVM, unsigned uOperation, PSUPVMMR0REQHDR pReq, uint64_t u64Arg, PSUPDRVSESSION pSession);
;
; @param    pfnVMMR0EntryEx     rcx
; @param    pVM                 rdx
; @param    uOperation          r8
; @param    pReq                r9
; @param    u64Arg              [rsp + 28h] / [rbp + 30h]
; @param    pSession            [rsp + 30h] / [rbp + 38h]
;
BEGINPROC supdrvNtWrapVMMR0EntryEx
[proc_frame supdrvNtWrapVMMR0EntryEx]
        push    rbp
        [pushreg rbp]
        mov     rbp, rsp
        [setframe rbp,0]
        sub     rsp, 80h
        [allocstack 0x80]
        ;; @todo save more regs?
[endprolog]

        ;
        ; Create a stack marker with the rbp. The marker is 32 byte big.
        ; This is 32-byte aligned and 32 byte in size.
        ;
        lea     r10, [rbp - 30h]
        and     r10, ~1fh               ; 32-byte align it.
        mov     dword [r10    ], 0x20080901
        mov     dword [r10 + 4], 0x20080902
        mov     qword [r10 + 8], rbp
        mov     dword [r10 +16], 0x20080903
        mov     dword [r10 +20], 0x20080904
        mov     qword [r10 +24], rbp

        ;
        ; Forward the call.
        ;
        mov     rax, rcx
        mov     rcx, rdx
        mov     rdx, r8
        mov     r8, r9
        mov     r9, [rbp + 30h]
        mov     r11, [rbp + 38h]
        mov     [rsp + 20h], r11
        call    rax

        ;
        ; Trash the stack marker.
        ;
        lea     r10, [rbp - 30h]
        and     r10, ~1fh               ; 32-byte align it.
        mov     qword [r10    ], rax
        mov     qword [r10 + 8], rax
        mov     qword [r10 +16], rax
        mov     qword [r10 +24], rax

        leave
        ret
[endproc_frame supdrvNtWrapVMMR0EntryEx]
ENDPROC   supdrvNtWrapVMMR0EntryEx


extern RTSemEventMultiWaitNoResume

;;
; @cproto DECLASM(int) supdrvNtWrapRTSemEventMultiWaitNoResume(RTSEMEVENTMULTI EventMultiSem, unsigned cMillies);
;
; @param    EventMultiSem       rcx
; @param    cMillies            rdx
;
BEGINPROC supdrvNtWrapRTSemEventMultiWaitNoResume
[proc_frame supdrvNtWrapRTSemEventMultiWaitNoResume]
        push    rbp
        [pushreg rbp]
        mov     rbp, rsp
        [setframe rbp,0]
        sub     rsp, 40h
        [allocstack 0x40]
        mov     [rbp - 8h], rdi
        [savereg rdi, 0x38]
        ;; @todo save more?
        ;mov     [rbp - 10h], rsi
        ;[savereg rsi, 0x30]
[endprolog]

        ;
        ; Find the stack marker with the rbp of the entry frame.
        ; Search a maximum of 4096 bytes.
        ;
        mov     rax, rbp
        and     rax, ~1fh               ; 32-byte align it.
        mov     r10, 1000h / 20h
.again:
        dec     r10
        jz      .not_found
        add     rax, 20h

        cmp     dword [rax], 0x20080901
        jne     .again
        cmp     dword [rax + 4], 0x20080902
        jne     .again
        cmp     dword [rax + 16], 0x20080903
        jne     .again
        cmp     dword [rax + 20], 0x20080904
        jne     .again
        mov     r11, [rax + 8]
        cmp     r11, [rax + 24]
        jne     .again

        ; found it, change rbp.
        mov     rdi, rbp
        mov     rbp, r11

        ;
        ; Forward the call.
        ;
.resume:
        call    RTSemEventMultiWaitNoResume

        ;
        ; Restore rbp and any saved registers before returning.
        ;
        mov     rbp, rdi
        ;mov     rsi, [rbp - 10h]
        mov     rdi, [rbp - 8h]
        leave
        ret

.not_found:
        int3
        mov     rdi, rbp
        jmp     .resume
[endproc_frame supdrvNtWrapRTSemEventMultiWaitNoResume]
ENDPROC   supdrvNtWrapRTSemEventMultiWaitNoResume

 %endif ; RT_ARCH_AMD64
%endif ; SUPDRV_WITH_UNWIND_HACK

