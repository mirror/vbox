; $Id$
;; @file
; IPRT - No-CRT remainderf - AMD64 & X86.
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
; @returns  st(0) / xmm0
; @param    rf1    [ebp + 8h]   xmm0    Dividend.
; @param    rf2    [ebp + 10h]  xmm1    Divisor.
RT_NOCRT_BEGINPROC remainderf
        push    xBP
        SEH64_PUSH_xBP
        mov     xBP, xSP
        SEH64_SET_FRAME_xBP 0
%ifdef RT_ARCH_AMD64
        sub     xSP, 20h
        SEH64_ALLOCATE_STACK 20h
%endif
        SEH64_END_PROLOGUE

        ;
        ; Load the dividend into st0 and divisor into st1.
        ;
%ifdef RT_ARCH_AMD64
        movss   [xBP - 20h], xmm1
        movss   [xBP - 10h], xmm0
        fld     dword [xBP - 20h]
        fld     dword [xBP - 10h]
%else
        fld     dword [ebp + 0ch]
        fld     dword [ebp + 08h]
%endif

        ;
        ; The fprem1 only does between 32 and 64 rounds, so we have to loop
        ; here till we've got a final result.  We count down in ECX to
        ; avoid getting stuck here...
        ;
        mov     ecx, 256 / 32 + 4
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
%ifdef RT_ARCH_AMD64
        fstp    dword [rsp]
        movss   xmm0, [rsp]
%endif

        leave
        ret
ENDPROC   RT_NOCRT(remainderf)

