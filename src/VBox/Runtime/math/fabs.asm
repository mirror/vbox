; $Id: $
;; @file
; InnoTek Portable Runtime - No-CRT fabs - AMD64 & X86.
;

;
; Copyright (C) 2006 InnoTek Systemberatung GmbH
;
; This file is part of VirtualBox Open Source Edition (OSE), as
; available from http://www.virtualbox.org. This file is free software;
; you can redistribute it and/or modify it under the terms of the GNU
; General Public License as published by the Free Software Foundation,
; in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
; distribution. VirtualBox OSE is distributed in the hope that it will
; be useful, but WITHOUT ANY WARRANTY of any kind.
;
; If you received this file as part of a commercial VirtualBox
; distribution, then only the terms of your commercial VirtualBox
; license agreement apply instead of the previous paragraph.
;

%include "iprt/asmdefs.mac"

BEGINCODE

%ifdef __AMD64__
 %define _SP rsp
 %define _BP rbp
 %define _S  8
%else
 %define _SP esp
 %define _BP ebp
 %define _S  4
%endif

;;
; Compute the absolute value of rd (|rd|).
; @returns 32-bit: st(0)   64-bit: xmm0
; @param    rd      32-bit: [ebp + 8]   64-bit: xmm0
BEGINPROC RT_NOCRT(fabs)
    push    _BP
    mov     _BP, _SP

%ifdef __AMD64__
    sub     _SP, 10h

    movsd   [_SP], xmm0
    fld     qword [_SP]

    fabs

    fstp    qword [_SP]
    movsd   xmm0, [_SP]

%else
    fld     qword [_BP + _S*2]
    fabs
%endif

    leave
    ret
ENDPROC   RT_NOCRT(fabs)

