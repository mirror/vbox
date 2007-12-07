; $Id: $
;; @file
; innotek Portable Runtime - No-CRT fabsf - AMD64 & X86.
;

;
; Copyright (C) 2006-2007 innotek GmbH
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
; Compute the absolute value of rf (|rf|).
; @returns 32-bit: st(0)   64-bit: xmm0
; @param    rf      32-bit: [ebp + 8]   64-bit: xmm0
BEGINPROC RT_NOCRT(fabsf)
    push    _BP
    mov     _BP, _SP

%ifdef RT_ARCH_AMD64
    sub     _SP, 10h

    movsd   [_SP], xmm0
    fld     dword [_SP]

    fabs

    fstp    dword [_SP]
    movsd   xmm0, [_SP]

%else
    fld     dword [_BP + _S*2]
    fabs
%endif

    leave
    ret
ENDPROC   RT_NOCRT(fabsf)

