; $Id$
;; @file
; innotek Portable Runtime - No-CRT memchr - AMD64 & X86.
;

;
;  Copyright (C) 2006-2007 innotek GmbH
; 
;  This file is part of VirtualBox Open Source Edition (OSE), as
;  available from http://www.virtualbox.org. This file is free software;
;  you can redistribute it and/or modify it under the terms of the GNU
;  General Public License as published by the Free Software Foundation,
;  in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
;  distribution. VirtualBox OSE is distributed in the hope that it will
;  be useful, but WITHOUT ANY WARRANTY of any kind.

%include "iprt/asmdefs.mac"

BEGINCODE

;;
; @param    pv      gcc: rdi  msc: ecx  x86:[esp+4]
; @param    ch      gcc: esi  msc: edx  x86:[esp+8]
; @param    cb      gcc: rdx  msc: r8   x86:[esp+0ch]
BEGINPROC RT_NOCRT(memchr)
        cld
%ifdef RT_ARCH_AMD64
 %ifdef ASM_CALL64_MSC
        or      r8, r8
        jz      .not_found_early

        mov     r9, rdi                 ; save rdi
        mov     eax, edx
        mov     rdi, rcx
        mov     rcx, r8
 %else
        mov     rcx, rdx
        jrcxz   .not_found_early

        mov     eax, esi
 %endif

%else
        mov     ecx, [esp + 0ch]
        jecxz   .not_found_early
        mov     edx, edi                ; save edi
        mov     eax, [esp + 8]
        mov     edi, [esp + 4]
%endif

        ; do the search
        repne   scasb
        jne     .not_found

        ; found it
        lea     xAX, [xDI - 1]
%ifdef ASM_CALL64_MSC
        mov     rdi, r9
%endif
%ifdef RT_ARCH_X86
        mov     edi, edx
%endif
        ret

.not_found:
%ifdef ASM_CALL64_MSC
        mov     rdi, r9
%endif
%ifdef RT_ARCH_X86
        mov     edi, edx
%endif
.not_found_early:
        xor     eax, eax
        ret
ENDPROC RT_NOCRT(memchr)

