; $Id$
;; @file
; IPRT - No-CRT atan2 - AMD64 & X86.
;

;
; Copyright (C) 2022 Oracle Corporation
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


%include "iprt/asmdefs.mac"

BEGINCODE

;;
; Arctangent (partial).
;
; @returns st(0) / xmm0
; @param    rdY     [rbp + 8] / xmm0
; @param    rdX     [rbp + 8] / xmm0
RT_NOCRT_BEGINPROC atan2
    push    xBP
    mov     xBP, xSP

%ifdef RT_ARCH_AMD64
    sub     xSP, 20h
    movsd   [xSP + 10h], xmm1
    movsd   [xSP], xmm0
    fld     qword [xSP]
    fld     qword [xSP + 10h]
%else
    fld     qword [xBP + xCB*2]
    fld     qword [xBP + xCB*2 + 8]
%endif

    fpatan

%ifdef RT_ARCH_AMD64
    fstp    qword [xSP]
    movsd   xmm0, [xSP]
%endif
    leave
    ret
ENDPROC   RT_NOCRT(atan2)

