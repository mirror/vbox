; $Id$
;; @file
; VBoxOs2AdditionsInstallA - Watcom assembly file that defines the stack.
;

;
; Copyright (C) 2022 Oracle Corporation
;
; This file is part of VirtualBox Open Source Edition (OSE), as
; available from http://www.virtualbox.org. This file is free software;
; you can redistribute it and/or modify it under the terms of the GNU
; General Public License (GPL) as published by the Free Software
; Foundation, in version 2 as it comes in the "COPYING" file of the
; VirtualBox OSE distribution. VirtualBox OSE is distributed in the
; hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
;

        .386p

DGROUP  group CONST,CONST2,_DATA,_BSS,STACK

CONST   segment use32 dword public 'DATA'
CONST   ends

CONST2  segment use32 dword public 'DATA'
CONST2  ends

_DATA   segment use32 dword public 'DATA'
_DATA   ends

_BSS    segment use32 dword public 'BSS'
_BSS    ends

STACK   segment use32 para stack 'STACK'
        db      1000h dup(?)
STACK   ends

        end

