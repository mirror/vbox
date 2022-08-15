; $Id$
;; @file
; IPRT - No-CRT remainderl - AMD64 & X86.
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
%include "iprt/x86.mac"


BEGINCODE

;;
; See SUS.
; @returns st(0)
; @param    lrd1    [rbp + 10h]
; @param    lrd2    [rbp + 20h]
RT_NOCRT_BEGINPROC remainderl
        push    xBP
        SEH64_PUSH_xBP
        mov     xBP, xSP
        SEH64_SET_FRAME_xBP 0
        SEH64_END_PROLOGUE

        ;
        ; Load the dividend into st0 and divisor into st1.
        ;
        fld     tword [xBP + 2*xCB + RTLRD_CB]
        fld     tword [xBP + 2*xCB]

        ;
        ; The fprem1 only does between 32 and 64 rounds, so we have to loop
        ; here till we've got a final result.  We count down in ECX to
        ; avoid getting stuck here...
        ;
        mov     ecx, 16384 / 32 + 4
.again:
        fprem1
        fstsw   ax
        test    ah, (X86_FSW_C2 >> 8)
        jz      .done
        dec     cx
        jnz     .again
%ifdef RT_STRICT
        int3
%endif

        ;
        ; Return the result.
        ;
.done:
        fstp    st1
        leave
        ret
ENDPROC   RT_NOCRT(remainderl)

