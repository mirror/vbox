; $Id$
;; @file
; IPRT - ASMAtomicXchgU64().
;

;
; Copyright (C) 2006-2010 Sun Microsystems, Inc.
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

;;
; Atomically Exchange an unsigned 64-bit value, ordered.
;
; @param    pu64     x86:ebp+8   gcc:rdi  msc:rcx
; @param    u64New   x86:ebp+c   gcc:rsi  msc:rdx
;
; @returns  bool result: true if succesfully exchanged, false if not.
;           x86:al
;
BEGINPROC_EXPORTED ASMAtomicXchgU64
%ifdef RT_ARCH_AMD64
 %ifdef ASM_CALL64_MSC
        mov     rax, r8
.try_again:
        lock cmpxchg [rcx], rdx
 %else
.try_again:
        mov     rax, rcx
        lock cmpxchg [rdi], rsi
 %endif
        jnz     .try_again
        ret
%endif
%ifdef RT_ARCH_X86
        push    ebp
        mov     ebp, esp
        push    ebx
        push    edi

.try_again:
        mov     ebx, dword [ebp+0ch]
        mov     ecx, dword [ebp+0ch + 4]
        mov     edi, [ebp+08h]
        lock cmpxchg8b [edi]
        jnz     .try_again

        pop     edi
        pop     ebx
        leave
        ret
%endif
ENDPROC ASMAtomicXchgU64

