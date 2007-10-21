; $Id$
;; @file
; innotek Portable Runtime - No-CRT remainder - AMD64 & X86.
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
%else
 %define _SP esp
 %define _BP ebp
%endif

;;
; See SUS.
; @returns st(0)
; @param    rd1    [ebp + 8h]  xmm0
; @param    rd2    [ebp + 10h]  xmm1
BEGINPROC RT_NOCRT(remainder)
    push    _BP
    mov     _BP, _SP
    sub     _SP, 20h
;int3

%ifdef RT_ARCH_AMD64
    movsd   [rsp + 10h], xmm1
    movsd   [rsp], xmm0
    fld     qword [rsp + 10h]
    fld     qword [rsp]
%else
    fld     qword [ebp + 10h]
    fld     qword [ebp + 8h]
%endif

    fprem1
    fstsw   ax
    test    ah, 04h
    jnz     .done
    fstp    st1

.done:
%ifdef RT_ARCH_AMD64
    fstp    qword [rsp]
    movsd   xmm0, [rsp]
%endif

    leave
    ret
ENDPROC   RT_NOCRT(remainder)

