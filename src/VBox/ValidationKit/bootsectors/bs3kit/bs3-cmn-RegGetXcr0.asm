; $Id$
;; @file
; BS3Kit - Bs3RegGetXcr0
;

;
; Copyright (C) 2007-2022 Oracle Corporation
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

%include "bs3kit-template-header.mac"


;;
; @cproto   BS3_CMN_PROTO_STUB(uint64_t, Bs3RegGetXcr0,(void));
;
; @returns  Register value.
; @remarks  Does not require 20h of parameter scratch space in 64-bit mode.
;
; @uses     No GPRs, though 16-bit mode the upper 48-bits of RAX, RDX and RCX are cleared.
;
BS3_PROC_BEGIN_CMN Bs3RegGetXcr0, BS3_PBC_HYBRID_SAFE
        push    xBP
        mov     xBP, xSP
TONLY64 push    rdx

        ; Read the value.
TNOT16  push    sCX
        xor     ecx, ecx
        xgetbv
TNOT16  pop     sCX

        ; Move the edx:eax value into the appropriate return register(s).
%if TMPL_BITS == 16
        ; value [dx cx bx ax]
        ror     eax, 16
        mov     bx, ax
        mov     cx, dx
        shr     eax, 16
        shr     edx, 16
%elif TMPL_BITS == 64
        mov     eax, eax
        shr     rdx, 32
        or      rax, rdx
%endif

TONLY64 pop     rdx
        pop     xBP
        BS3_HYBRID_RET
BS3_PROC_END_CMN   Bs3RegGetXcr0

