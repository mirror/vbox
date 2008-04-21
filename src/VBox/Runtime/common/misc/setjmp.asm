; $Id$
;; @file
; IPRT - No-CRT setjmp & longjmp - AMD64 & X86.
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

%include "iprt/asmdefs.mac"


BEGINCODE


BEGINPROC RT_NOCRT(setjmp)
%ifdef RT_ARCH_AMD64
        mov     rax, [rsp]
        mov     [rdi + 00h], rax        ; rip
        lea     rcx, [rsp + 8]
        mov     [rdi + 08h], rcx        ; rsp
        mov     [rdi + 10h], rbp
        mov     [rdi + 18h], r15
        mov     [rdi + 20h], r14
        mov     [rdi + 28h], r13
        mov     [rdi + 30h], r12
        mov     [rdi + 38h], rbx
%else
        mov     edx, [esp + 4h]
        mov     eax, [esp]
        mov     [edx + 00h], eax        ; eip
        lea     ecx, [esp + 4h]
        mov     [edx + 04h], ecx        ; esp
        mov     [edx + 08h], ebp
        mov     [edx + 0ch], ebx
        mov     [edx + 10h], edi
        mov     [edx + 14h], esi
%endif
        xor     eax, eax
        ret
ENDPROC RT_NOCRT(setjmp)


BEGINPROC RT_NOCRT(longjmp)
%ifdef RT_ARCH_AMD64
        mov     rbx, [rdi + 38h]
        mov     r12, [rdi + 30h]
        mov     r13, [rdi + 28h]
        mov     r14, [rdi + 20h]
        mov     r15, [rdi + 18h]
        mov     rbp, [rdi + 10h]
        mov     eax, esi
        test    eax, eax
        jnz     .fine
        inc     al
.fine:
        mov     rsp, [rdi + 08h]
        jmp     qword [rdi + 00h]
%else
        mov     edx, [esp + 4h]
        mov     eax, [esp + 8h]
        mov     esi, [edx + 14h]
        mov     edi, [edx + 10h]
        mov     ebx, [edx + 0ch]
        mov     ebp, [edx + 08h]
        test    eax, eax
        jnz     .fine
        inc     al
.fine:
        mov     esp, [edx + 04h]
        jmp     dword [edx + 00h]
%endif
ENDPROC RT_NOCRT(longjmp)

