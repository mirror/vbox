; $Id$
;; @file
; BS3Kit - Bs3TestNow.
;

;
; Copyright (C) 2007-2021 Oracle Corporation
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
%include "VBox/VMMDevTesting.mac"

BS3_EXTERN_DATA16 g_fbBs3VMMDevTesting
TMPL_BEGIN_TEXT

;;
; @cproto   BS3_DECL(uint64_t) Bs3TestNow(void);
;
BS3_PROC_BEGIN_CMN Bs3TestNow, BS3_PBC_HYBRID
        BS3_CALL_CONV_PROLOG 0
        push    xBP
        mov     xBP, xSP
%if __BITS__ == 16
BONLY16 push    sAX
%else
        push    xCX
BONLY64 push    xDX
%endif

        cmp     byte [BS3_DATA16_WRT(g_fbBs3VMMDevTesting)], 0
        je      .no_vmmdev

        ; Read the lower timestamp.
        mov     dx, VMMDEV_TESTING_IOPORT_TS_LOW
        in      eax, dx
%if __BITS__ == 16
        mov     bx, ax                  ; Save the first word in BX (returned in DX).
        shr     eax, 16
        mov     cx, ax                  ; The second word is returned in CX.
%else
        mov     ecx, eax
%endif

        ; Read the high timestamp (latached in above read).
        mov     dx, VMMDEV_TESTING_IOPORT_TS_HIGH
        in      eax, dx
%if __BITS__ == 16
        mov     dx, bx                  ; The first word is returned in DX.
        mov     bx, ax                  ; The third word is returned in BX.
        shr     eax, 16                 ; The fourth word is returned in AX.
%elif __BITS__ == 32
        mov     edx, eax
        mov     eax, eax
%else
        shr     rax, 32
        or      rax, rcx
%endif

.return:
%if __BITS__ == 16
        mov     [bp - sCB], ax          ; Update the AX part of the saved EAX.
        pop     sAX
%else
        pop     xCX
BONLY64 pop     xDX
%endif
        pop     xBP
        BS3_CALL_CONV_EPILOG 0
        BS3_HYBRID_RET

.no_vmmdev:
        ; No fallback, just zero the result.
%if __BITS__ == 16
        xor     ax, ax
        xor     bx, bx
        xor     cx, cx
        xor     dx, dx
%else
        xor     eax, eax
BONLY32 xor     edx, edx
%endif
        jmp     .return
BS3_PROC_END_CMN   Bs3TestNow

