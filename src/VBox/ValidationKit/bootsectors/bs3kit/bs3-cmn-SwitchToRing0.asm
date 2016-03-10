; $Id$
;; @file
; BS3Kit - Bs3SwitchToRing0
;

;
; Copyright (C) 2007-2016 Oracle Corporation
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


%if TMPL_BITS == 16
BS3_EXTERN_DATA16 g_bBs3CurrentMode
%endif
TMPL_BEGIN_TEXT


;;
; @cproto   BS3_DECL(void) Bs3SwitchToRing0(void);
;
; @remarks  Does not require 20h of parameter scratch space in 64-bit mode.
;
BS3_PROC_BEGIN_CMN Bs3SwitchToRing0
        push    xAX

%if TMPL_BITS == 16
        ; Check the current mode.
        push    ds
        mov     ax, seg g_bBs3CurrentMode
        mov     ds, ax
        mov     al, [BS3_DATA16_WRT(g_bBs3CurrentMode)]
        pop     ds

        ; If real mode: Nothing to do, it's considered ring-0 here.
        cmp     al, BS3_MODE_RM
        je      .return

        ; If V8086 mode: Always do syscall and add a lock prefix to make sure it gets to the VMM.
        and     al, BS3_MODE_CODE_MASK
        cmp     al, BS3_MODE_CODE_V86
        jne     .not_v8086

        mov     xAX, BS3_SYSCALL_TO_RING0
        lock int BS3_TRAP_SYSCALL
        jmp     .return

.not_v8086:
%endif

        ; In protected mode: Check the CPL we're currently at skip syscall if ring-0 already.
        mov     ax, cs
        test    ax, 3
        jz      .return

        mov     xAX, BS3_SYSCALL_TO_RING0
        int     BS3_TRAP_SYSCALL

.return:
        pop     xAX
        ret
BS3_PROC_END_CMN   Bs3SwitchToRing0

