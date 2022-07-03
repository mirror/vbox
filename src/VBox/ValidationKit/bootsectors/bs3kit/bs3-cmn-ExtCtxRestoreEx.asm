; $Id$
;; @file
; BS3Kit - Bs3ExtCtxRestoreEx.
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


%if ARCH_BITS != 64
BS3_EXTERN_DATA16 g_bBs3CurrentMode

BS3_BEGIN_TEXT64
extern  _Bs3ExtCtxRestore_c64
 %if ARCH_BITS == 16
extern  BS3_CMN_NM(Bs3SelProtFar16DataToFlat)
extern  _Bs3SwitchTo16Bit_c64
 %else
extern  _Bs3SwitchTo32Bit_c64
 %endif

TMPL_BEGIN_TEXT
extern  BS3_CMN_NM(Bs3SwitchTo64Bit)
 %if ARCH_BITS == 16
extern  BS3_CMN_NM(Bs3SelProtFar16DataToFlat)
 %endif
%endif

extern BS3_CMN_NM(Bs3ExtCtxRestore)



;;
; Restores the extended CPU context (FPU, SSE, AVX, ++), full 64-bit
; when in long mode.
;
; @param    pExtCtx
;
BS3_PROC_BEGIN_CMN Bs3ExtCtxRestoreEx, BS3_PBC_NEAR
%if ARCH_BITS == 64
        jmp     BS3_CMN_NM(Bs3ExtCtxRestore)
%else
        push    xBP
        mov     xBP, xSP
        push    sAX

        ;
        ; Check if we're in long mode.
        ;
        mov     al, [BS3_DATA16_WRT(g_bBs3CurrentMode)]
        and     al, BS3_MODE_SYS_MASK
        cmp     al, BS3_MODE_SYS_LM
        je      .in_long_mode

        ;
        ; Not in long mode, so do normal restore.
        ;
        pop     sAX
        leave
        jmp     BS3_CMN_NM(Bs3ExtCtxRestore)

        ;
        ; Switch to 64-bit to do the restoring so we can restore 64-bit only state.
        ;
.in_long_mode:
        push    sCX
BONLY16 push    sDX

        ; Load ecx with the flat pExtCtx address.
        mov     ecx, [xBP + xCB + cbCurRetAddr]

 %if ARCH_BITS == 16
        push    ecx
        call    BS3_CMN_NM(Bs3SelProtFar16DataToFlat)
        mov     ecx, edx
        shl     ecx, 16
        mov     cx, ax
 %endif

        ; Switch to 64-bit mode.
        call    BS3_CMN_NM(Bs3SwitchTo64Bit)
        BITS    64

        ; Do the restore.
        sub     rsp, 20h
        call    _Bs3ExtCtxRestore_c64
        add     rsp, 20h

        ; Switch back to the original mode.
 %if ARCH_BITS == 16
        call    _Bs3SwitchTo16Bit_c64
 %else
        call    _Bs3SwitchTo32Bit_c64
 %endif
        BITS    ARCH_BITS

        ; Restore context and return.
BONLY16 pop     sDX
        pop     sCX

        pop     sAX
        leave
        ret
%endif
BS3_PROC_END_CMN   Bs3ExtCtxRestoreEx

