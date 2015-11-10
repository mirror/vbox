; $Id$
;; @file
; BS3Kit - Bs3PrintStrColonSpaces, Common.
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


BS3_EXTERN_CMN Bs3PrintStr
BS3_EXTERN_CMN Bs3StrLen
BS3_EXTERN_CMN Bs3PrintChr

;; @todo If we can get 64-bit C code to link smoothly, this should be done in C!
;;       It's really annoying unreadable stuff in multi-mode assembly.

;;
; Prints a 32-bit unsigned integer value.
;
; @param    [xSP + xCB+sCB] The desired minimum length of the output. That is
;                           string + colon + spaces.
; @param    [xSP + xCB]     The string to print.

BS3_PROC_BEGIN_CMN Bs3PrintStrColonSpaces
        BS3_CALL_CONV_PROLOG 2
        push    xBP
        mov     xBP, xSP
        push    xAX
        push    xDI
        sub     esp, 20h

        mov     sAX, [xBP + xCB*2]
        mov     [xBP - 20h], sAX
        BS3_CALL Bs3PrintStr, 1

        mov     sAX, [xBP + xCB*2]
        mov     [xBP - 20h], sAX
        BS3_CALL Bs3StrLen, 1
        mov     di, ax
        mov     byte [xBP - 20h], ':'
        BS3_CALL Bs3PrintChr, 1
        inc     di
.next_space:
        mov     byte [xBP - 20h], ' '
        BS3_CALL Bs3PrintChr, 1
        inc     di
        cmp     di, word [xBP + xCB*2 + sCB]
        jb      .next_space

        pop     xDI
        pop     xAX
        leave
        BS3_CALL_CONV_EPILOG 2
        ret
BS3_PROC_END_CMN   Bs3PrintStrColonSpaces

