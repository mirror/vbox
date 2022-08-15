; $Id$
;; @file
; IPRT - No-CRT fabs - AMD64 & X86.
;

;
; Copyright (C) 2006-2022 Oracle Corporation
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


%define RT_ASM_WITH_SEH64
%include "iprt/asmdefs.mac"


BEGINCODE

;;
; Compute the absolute value of rd (|rd|).
; @returns 32-bit: st(0)   64-bit: xmm0
; @param    rd      32-bit: [ebp + 8]   64-bit: xmm0
RT_NOCRT_BEGINPROC fabs
        push    xBP
        SEH64_PUSH_xBP
        mov     xBP, xSP
        SEH64_SET_FRAME_xBP 0
        SEH64_END_PROLOGUE

%ifdef RT_ARCH_AMD64
        andps   xmm0, [g_r64ClearSignMask xWrtRIP]
%else
        fld     qword [xBP + xCB*2]     ; This turns SNaN into QNaN.
        fabs
%endif

        leave
        ret
ENDPROC   RT_NOCRT(fabs)

ALIGNCODE(16)
g_r64ClearSignMask:
        dd      0ffffffffh
        dd      07fffffffh

        dd      0ffffffffh
        dd      07fffffffh

