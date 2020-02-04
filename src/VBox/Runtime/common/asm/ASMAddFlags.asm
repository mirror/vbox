; $Id$
;; @file
; IPRT - ASMSetFlags().
;

;
; Copyright (C) 2006-2020 Oracle Corporation
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


;*******************************************************************************
;* Header Files                                                                *
;*******************************************************************************
%include "iprt/asmdefs.mac"

BEGINCODE

;;
; @param rcx/rdi  eflags to add
BEGINPROC_EXPORTED ASMAddFlags
%if    ARCH_BITS == 64
        pushfq
        mov     rax, [rsp]
 %ifdef ASM_CALL64_GCC
        or      rdi, rax
        mov     [rsp], rdi
 %else
        or      rcx, rax
        mov     [rsp], rcx
 %endif
        popfq
%elif  ARCH_BITS == 32
        mov     ecx, [esp + 4]
        pushfd
        mov     eax, [esp]
        or      ecx, eax
        mov     [esp], ecx
        popfd
%elif  ARCH_BITS == 16
        push    bp
        mov     bp, sp
        pushf
        pop     ax
        push    word [bp + 2 + 2]
        or      [bp - 2], ax
        popf
        leave
%else
 %error ARCH_BITS
%endif
        ret
ENDPROC ASMAddFlags

