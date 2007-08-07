;; @file
; innotek Portable Runtime - ASMMultU64ByU32DivByU32().
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

;*******************************************************************************
;* Header Files                                                                *
;*******************************************************************************
%include "iprt/asmdefs.mac"

BEGINCODE

;;
; Multiple a 64-bit by a 32-bit integer and divide the result by a 32-bit integer
; using a 96 bit intermediate result.
; 
; @returns (u64A * u32B) / u32C. (rax)
; @param   u64A    rcx      The 64-bit value.
; @param   u32B    edx      The 32-bit value to multiple by A.
; @param   u32C    r8d      The 32-bit value to divide A*B by.
; 
BEGINPROC_EXPORTED ASMMultU64ByU32DivByU32
        mov     r9d, edx                        ; r9  = B
        mov     rax, rcx                        ; rax = A
        xor     edx, edx                        ; rdx = 0
        mul     r9                              ; rax:rdx = A*B
        mov     ecx, r8d                        ; rcx = C (overly careful)
        div     rcx                             ; rax = A*B/C (rdx=reminder)
        ret
ENDPROC ASMMultU64ByU32DivByU32

