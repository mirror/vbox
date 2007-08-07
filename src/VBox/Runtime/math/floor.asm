; $Id$
;; @file
; innotek Portable Runtime - No-CRT floor - AMD64 & X86.
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

%ifdef RT_ARCH_AMD64
 %define _SP rsp
 %define _BP rbp
 %define _S  8
%else
 %define _SP esp
 %define _BP ebp
 %define _S  4
%endif

;;
; Compute the largest integral value not greater than rd.
; @returns 32-bit: st(0)   64-bit: xmm0
; @param    rd      32-bit: [ebp + 8]   64-bit: xmm0
BEGINPROC RT_NOCRT(floor)
    push    _BP
    mov     _BP, _SP
    sub     _SP, 10h

%ifdef RT_ARCH_AMD64
    movsd   [_SP], xmm0
    fld     qword [_SP]
%else
    fld     qword [_BP + _S*2]
%endif

    ; Make it round down by modifying the fpu control word.
    fstcw   [_BP - 10h]
    mov     eax, [_BP - 10h]
    or      eax, 00400h
    and     eax, 0f7ffh
    mov     [_BP - 08h], eax
    fldcw   [_BP - 08h]

    ; Round ST(0) to integer.
    frndint

    ; Restore the fpu control word.
    fldcw   [_BP - 10h]

%ifdef RT_ARCH_AMD64
    fstp    qword [_SP]
    movsd   xmm0, [_SP]
%endif
    leave
    ret
ENDPROC   RT_NOCRT(floor)

