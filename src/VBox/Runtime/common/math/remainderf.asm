; $Id$
;; @file
; innotek Portable Runtime - No-CRT remainderf - AMD64 & X86.
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
; @param    rf1    [ebp + 08h]  xmm0
; @param    rf2    [ebp + 0ch]  xmm1
BEGINPROC RT_NOCRT(remainderf)
    push    _BP
    mov     _BP, _SP
    sub     _SP, 20h

%ifdef RT_ARCH_AMD64
    movss   [rsp], xmm1
    movss   [rsp + 10h], xmm0
    fld     dword [rsp]
    fld     dword [rsp + 10h]
%else
    fld     dword [ebp + 0ch]
    fld     dword [ebp + 8h]
%endif

    fprem1
    fstsw   ax
    test    ah, 04h
    jnz     .done
    fstp    st1

.done:
%ifdef RT_ARCH_AMD64
    fstp    dword [rsp]
    movss   xmm0, [rsp]
%endif

    leave
    ret
ENDPROC   RT_NOCRT(remainderf)


