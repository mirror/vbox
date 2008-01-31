;; @file
;
; VBox frontends: Qt GUI ("VirtualBox"):
; Implementation of OS/2-specific helpers that require to reside in a DLL
;
; This stub is used to avoid linking the helper DLL to the C runtime.
;

;
; Copyright (C) 2008 innotek GmbH
; 
; This file is part of VirtualBox Open Source Edition (OSE), as
; available from http://www.virtualbox.org. This file is free software;
; you can redistribute it and/or modify it under the terms of the GNU
; General Public License as published by the Free Software Foundation,
; in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
; distribution. VirtualBox OSE is distributed in the hope that it will
; be useful, but WITHOUT ANY WARRANTY of any kind.
;

SEGMENT CODE32 CLASS=CODE USE32 FLAT PUBLIC

extern _DLL_InitTerm

; Low-level DLL entry point 
..start:
    jmp _DLL_InitTerm

; Shut up emxomfld (?)
global WEAK$ZERO
WEAK$ZERO:
