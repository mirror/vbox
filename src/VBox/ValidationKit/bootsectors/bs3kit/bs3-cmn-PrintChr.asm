; $Id$
;; @file
; BS3Kit - Bs3PrintChr.
;

;
; Copyright (C) 2007-2015 Oracle Corporation
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
; @cproto   BS3_DECL(void) Bs3PrintChr_c16(char ch);
;
BS3_PROC_BEGIN_CMN Bs3PrintChr
        BS3_CALL_CONV_PROLOG 1
        push    xBP
        mov     xBP, xSP
        push    xAX
        push    xCX
        push    xBX

%ifdef TMPL_16BIT
        ; If we're not in protected mode, call the VGA BIOS directly.
        smsw    bx
        test    bx, X86_CR0_PE
        jnz     .protected_mode

        mov     al, [xBP + xCB*2]       ; Load the char
        mov     bx, 0ff00h
        mov     ah, 0eh
        int     10h
        jmp     .return

.protected_mode:
%endif

        mov     cl, [xBP + xCB*2]       ; Load the char
        mov     ax, BS3_SYSCALL_PRINT_CHR
        int     BS3_TRAP_SYSCALL

.return:
        pop     xBX
        pop     xCX
        pop     xAX
        leave
        BS3_CALL_CONV_EPILOG 1
        ret
BS3_PROC_END_CMN   Bs3PrintChr

