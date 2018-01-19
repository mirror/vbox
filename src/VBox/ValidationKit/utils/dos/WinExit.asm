; $Id$
;; @file
; 16-bit windows program that exits windows.
;
; Build: wcl -I%WATCOM%\h\win -l=windows -k4096 -fm WinExit.asm
;

;
; Copyright (C) 2018 Oracle Corporation
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



;.stack 4096
STACK   segment para stack 'STACK'
STACK   ends


extrn INITTASK:FAR
extrn INITAPP:FAR
extrn EXITWINDOWS:FAR
extrn WAITEVENT:FAR

_TEXT   segment word public 'CODE'
start:
        push    bp
        mov     bp, sp

        ;
        ; Initialize the windows app.
        ;
        call    INITTASK

        xor     ax, ax
        push    ax
        call    WAITEVENT

        push    di                      ; hInstance
        push    di
        call    INITAPP

        ;
        ; Do what we're here for, exitting windows.
        ;
        xor     ax, ax
        xor     cx, cx
        xor     dx, dx
        push    ax
        push    ax
        push    ax
        push    ax
        call    EXITWINDOWS

        ;
        ; Exit via DOS interrupt.
        ;
        xor     al, al
        mov     ah,04cH
        int     021h

        mov     sp, bp
        pop     bp
        ret

_TEXT   ends

end start

