; $Id$
;; @file
; BS3Kit - Bs3A20Disable.
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


BS3_EXTERN_CMN Bs3KbdWait
BS3_EXTERN_CMN Bs3KbdRead
BS3_EXTERN_CMN Bs3KbdWrite


;;
; Disables the A20 gate.
;
; @uses     Nothing.
;
BS3_PROC_BEGIN_CMN Bs3A20Disable
        ; Must call both because they may be ORed together on real HW.
        call    BS3_CMN_NM(Bs3A20DisableViaKbd)
        call    BS3_CMN_NM(Bs3A20DisableViaPortA)
        ret
BS3_PROC_END_CMN   Bs3A20Disable


;;
; Disables the A20 gate via control port A (PS/2 style).
;
; @uses     Nothing.
;
BS3_PROC_BEGIN_CMN Bs3A20DisableViaPortA
        push    xAX

        ; Use Control port A, assuming a PS/2 style system.
        in      al, 092h
        test    al, 02h
        jz      .done                   ; avoid trouble writing back the same value.
        and     al, 0fdh                ; disable the A20 gate.
        out     092h, al

.done:
        pop     xAX
        ret
BS3_PROC_END_CMN   Bs3A20DisableViaPortA


;;
; Disables the A20 gate via the keyboard controller.
;
; @uses     Nothing.
;
BS3_PROC_BEGIN_CMN Bs3A20DisableViaKbd
        push    xBP
        mov     xBP, xSP
        push    xAX
        pushf
        cli

        call    Bs3KbdWait
        push    0d0h                    ; KBD_CCMD_READ_OUTPORT
        call    Bs3KbdRead

        and     al, 0fdh                ; ~2
        push    xAX
        push    0d1h                    ; KBD_CCMD_WRITE_OUTPORT
        call    Bs3KbdWrite

        add     xSP, xCB*3              ; Clean up both the above calls.

        mov     al, 0ffh                ; KBD_CMD_RESET
        out     64h, al
        call    Bs3KbdWait

        popf
        pop     xAX
        leave
        ret
BS3_PROC_END_CMN   Bs3A20DisableViaKbd

