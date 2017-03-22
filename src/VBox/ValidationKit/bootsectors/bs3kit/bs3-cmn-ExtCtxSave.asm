; $Id$
;; @file
; BS3Kit - Bs3ExtCtxSave.
;

;
; Copyright (C) 2007-2017 Oracle Corporation
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
; Saves the extended CPU context (FPU, SSE, AVX, ++).
;
; @param    pExtCtx
;
BS3_PROC_BEGIN_CMN Bs3ExtCtxSave, BS3_PBC_NEAR
        push    xBP
        mov     xBP, xSP
        push    xAX
BONLY32 push    xCX
BONLY16 push    es
BONLY16 push    bx

%if ARCH_BITS == 16
        les     bx, [xBP + xCB + cbCurRetAddr]
        mov     al, [es:bx + BS3EXTCTX.enmMethod]
        cmp     al, BS3EXTCTXMETHOD_XSAVE
        je      .do_16_xsave
        cmp     al, BS3EXTCTXMETHOD_FXSAVE
        je      .do_16_fxsave
        cmp     al, BS3EXTCTXMETHOD_ANCIENT
        je      .do_16_ancient
        int3

.do_16_ancient:
        fnsave  [es:bx + BS3EXTCTX.Ctx]
        jmp     .return

.do_16_xsave:
        xsave   [es:bx + BS3EXTCTX.Ctx]
        jmp     .return

.do_16_fxsave:
        fxsave  [es:bx + BS3EXTCTX.Ctx]
        ;jmp     .return

%else
BONLY32 mov     ecx, [xBP + xCB + cbCurRetAddr]

        mov     al, [xCX + BS3EXTCTX.enmMethod]
        cmp     al, BS3EXTCTXMETHOD_XSAVE
        je      .do_xsave
        cmp     al, BS3EXTCTXMETHOD_FXSAVE
        je      .do_fxsave
        cmp     al, BS3EXTCTXMETHOD_ANCIENT
        je      .do_ancient
        int3

.do_ancient:
        fnsave  [xCX + BS3EXTCTX.Ctx]
        jmp     .return

.do_xsave:
BONLY32 xsave   [xCX + BS3EXTCTX.Ctx]
BONLY64 xsave64 [xCX + BS3EXTCTX.Ctx]
        jmp     .return

.do_fxsave:
BONLY32 fxsave  [xCX + BS3EXTCTX.Ctx]
BONLY64 fxsave64 [xCX + BS3EXTCTX.Ctx]
        ;jmp     .return

%endif

.return:
BONLY16 pop     bx
BONLY16 pop     es
BONLY32 pop     xCX
        pop     xAX
        mov     xSP, xBP
        pop     xBP
        ret
BS3_PROC_END_CMN   Bs3ExtCtxSave

