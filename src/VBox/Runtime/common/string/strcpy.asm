; $Id$
;; @file
; IPRT - No-CRT strcpy - AMD64 & X86.
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

;;
; @param    psz1   gcc: rdi  msc: rcx  x86:[esp+4]
; @param    psz2   gcc: rsi  msc: rdx  x86:[esp+8]
RT_NOCRT_BEGINPROC strcpy
        ; input
%ifdef RT_ARCH_AMD64
 %ifdef ASM_CALL64_MSC
  %define psz1 rcx
  %define psz2 rdx
 %else
  %define psz1 rdi
  %define psz2 rsi
 %endif
        mov     r8, psz1
%else
        mov     ecx, [esp + 4]
        mov     edx, [esp + 8]
  %define psz1 ecx
  %define psz2 edx
        push    psz1
%endif

        ;
        ; The loop.
        ;
.next:
        mov     al, [psz1]
        mov     [psz2], al
        test    al, al
        jz      .done

        mov     al, [psz1 + 1]
        mov     [psz2 + 1], al
        test    al, al
        jz      .done

        mov     al, [psz1 + 2]
        mov     [psz2 + 2], al
        test    al, al
        jz      .done

        mov     al, [psz1 + 3]
        mov     [psz2 + 3], al
        test    al, al
        jz      .done

        add     psz1, 4
        add     psz2, 4
        jmp     .next

.done:
%ifdef RT_ARCH_AMD64
        mov     rax, r8
%else
        pop     eax
%endif
        ret
ENDPROC RT_NOCRT(strcpy)

