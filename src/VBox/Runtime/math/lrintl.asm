; $Id$
;; @file
; innotek Portable Runtime - No-CRT lrintl - AMD64 & X86.
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
; Round rd to the nearest integer value, rounding according to the current rounding direction.
; @returns 32-bit: eax  64-bit: rax
; @param    lrd     [rbp + _S*2]
BEGINPROC RT_NOCRT(lrintl)
    push    _BP
    mov     _BP, _SP
    sub     _SP, 10h

    fld     tword [_BP + _S*2]
%ifdef RT_ARCH_AMD64
    fistp   qword [_SP]
    fwait
    mov     rax, [_SP]
%else
    fistp   dword [_SP]
    fwait
    mov     eax, [_SP]
%endif

    leave
    ret
ENDPROC   RT_NOCRT(lrintl)

