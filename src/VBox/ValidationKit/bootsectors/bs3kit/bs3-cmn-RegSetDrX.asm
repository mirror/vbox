; $Id$
;; @file
; BS3Kit - Bs3RegSetDrX
;

;
; Copyright (C) 2007-2018 Oracle Corporation
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


BS3_EXTERN_CMN Bs3Panic
%if TMPL_BITS == 16
BS3_EXTERN_CMN Bs3Syscall
BS3_EXTERN_DATA16 g_bBs3CurrentMode
%endif
TMPL_BEGIN_TEXT

;TONLY16 CPU 386

;;
; @cproto   BS3_CMN_PROTO_STUB(void, Bs3RegSetDrX,(uint8_t iReg, RTCCUINTXREG uValue));
;
; @returns  Register value.
; @param    iRegister   The source register
; @param    uValue      The new Value.

; @remarks  Does not require 20h of parameter scratch space in 64-bit mode.
;
; @uses     No GPRs.
;
BS3_PROC_BEGIN_CMN Bs3RegSetDrX, BS3_PBC_HYBRID_SAFE
        BS3_CALL_CONV_PROLOG 1
        push    xBP
        mov     xBP, xSP
        push    sSI
        push    xDX

        mov     dl, [xBP + xCB + cbCurRetAddr]
        mov     sSI, [xBP + xCB + cbCurRetAddr + xCB]

%if TMPL_BITS == 16
        ; If V8086 mode we have to go thru a syscall.
        test    byte [BS3_DATA16_WRT(g_bBs3CurrentMode)], BS3_MODE_CODE_V86
        jz      .direct_access
        push    ax

        mov     xAX, BS3_SYSCALL_SET_DRX
        call    Bs3Syscall

        pop     ax
        jmp     .return

.direct_access:
%endif
        ; Switch (iRegister)
        cmp     dl, 6
        jz      .set_dr6
        cmp     dl, 7
        jz      .set_dr7
        cmp     dl, 0
        jz      .set_dr0
        cmp     dl, 1
        jz      .set_dr1
        cmp     dl, 2
        jz      .set_dr2
        cmp     dl, 3
        jz      .set_dr3
        cmp     dl, 4
        jz      .set_dr4
        cmp     dl, 5
        jz      .set_dr5

        call    Bs3Panic

.set_dr0:
        mov     dr0, sSI
        jmp     .return
.set_dr1:
        mov     dr1, sSI
        jmp     .return
.set_dr2:
        mov     dr2, sSI
        jmp     .return
.set_dr3:
        mov     dr3, sSI
        jmp     .return
.set_dr4:
        mov     dr4, sSI
        jmp     .return
.set_dr5:
        mov     dr5, sSI
        jmp     .return
.set_dr7:
        mov     dr7, sSI
        jmp     .return
.set_dr6:
        mov     dr6, sSI

.return:
        pop     xDX
        pop     sSI
        pop     xBP
        BS3_CALL_CONV_EPILOG 1
        BS3_HYBRID_RET
BS3_PROC_END_CMN   Bs3RegSetDrX

