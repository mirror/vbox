; $Id$
;; @file
; BS3Kit - 32-bit Watcom C/C++, 64-bit integer multiplication.
;

;
; Copyright (C) 2007-2022 Oracle Corporation
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


;;
; 64-bit unsigned & signed integer multiplication.
;
; @returns  EDX:EAX as the product.
; @param    EDX:EAX     Factor 1 - edx=F1H, eax=F1L.
; @param    ECX:EBX     Factor 2 - ecx=F2H, ebx=F2L.
;
global $??I8M
$??I8M:
global $??U8M
$??U8M:
        ;
        ; If both the high dwords are zero, we can get away with
        ; a simple 32-bit multiplication.
        ;
        test    ecx, ecx
        jnz     .big
        test    edx, edx
        jnz     .big
        mul     ebx
        ret

.big:
        ;
        ; Imagine we use 4294967296-base (2^32), so each factor has two
        ; digits H and L, thus we have: F1H:F1L * F2H:F1L  which we can
        ; multipy like we learned in primary school.  Since the result
        ; is limited to 64-bit, we can skip F1H*F2H and discard the
        ; high 32-bit in F1L*F2H and F1H*F2L.
        ;       result = ((F1L*F2H) << 32)
        ;              + ((F1H*F2L) << 32)
        ;              +  (F1L*F2L);
        ;
        push    ecx                     ; Preserve ECX just to be nice.
        push    eax                     ; Stash F1L for later.
        push    edx                     ; Stash F1H for later.

        ; ECX = F1L*F2H
        mul     ecx
        mov     ecx, eax

        ; ECX += F1H * F2L
        pop     eax
        mul     ebx
        add     ecx, eax

        ; EDX:EAX = F1L * F2L
        pop     eax
        mul     ebx
        add     edx, ecx

        pop     ecx
        ret

