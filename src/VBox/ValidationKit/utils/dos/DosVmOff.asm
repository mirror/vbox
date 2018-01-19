; $Id$
;; @file
; 16-bit DOS COM program that powers off the VM.
;
; Build: yasm -f bin -i../../../../../include/ DosVmOff.asm -o DosVmOff.com
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



%include "VBox/bios.mac"

        org 100h

segment text
main:
%if 0
        ; Setup stack.
        mov     ax, stack
        mov     ss, ax
        mov     sp, top_of_stack
%endif

        ; Do the shutdown thing.
        mov     ax, cs
        mov     ds, ax

        mov     bl, 64
        mov     dx, VBOX_BIOS_SHUTDOWN_PORT
        mov     ax, VBOX_BIOS_OLD_SHUTDOWN_PORT
.retry:
        mov     cx, 8
        mov     si, .s_szShutdown
        rep outsb
        xchg    ax, dx                  ; alternate between the new (VBox) and old (Bochs) ports.
        dec     bl
        jnz     .retry


        ; Probably not a VBox VM, exit the program with errorlevel 1.
.whatever:
        mov     ax, 04c01h
        int     021h
        hlt
        jmp     .whatever

.s_szShutdown:
        db      'Shutdown', 0

