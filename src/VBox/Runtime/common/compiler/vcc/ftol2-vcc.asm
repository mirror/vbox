; $Id$
;; @file
; IPRT - Floating Point to Integer related Visual C++ support routines.
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
%include "iprt/x86.mac"


;;
; Convert st0 to integer returning it in eax, popping st0.
;
; @returns  value in eax
; @param    st0     The value to convet.  Will be popped.
; @uses     eax, st0, FSW.TOP
;
GLOBALNAME_RAW  __ftol2_sse_excpt, function, RT_NOTHING
GLOBALNAME_RAW  __ftol2_sse, function, RT_NOTHING ;; @todo kind of expect __ftol2_sse to take input in xmm0 and return in edx:eax.
BEGINPROC_RAW   __ftoi2
        push    ebp
        mov     ebp, esp
        sub     esp, 8h
        fisttp  dword [esp]
        mov     eax, [esp]
        leave
        ret
ENDPROC_RAW     __ftoi2


;;
; Convert st0 to integer returning it in edx:eax, popping st0.
;
; @returns  value in edx:eax
; @param    st0     The value to convet.  Will be popped.
; @uses     eax, edx, st0, FSW.TOP
;
BEGINPROC_RAW   __ftol2
        push    ebp
        mov     ebp, esp
        sub     esp, 8h
        and     esp, 0fffffff8h             ; proper alignment.
        fisttp  qword [esp]
        mov     eax, [esp]
        mov     edx, [esp + 8]
        leave
        ret
ENDPROC_RAW     __ftol2

