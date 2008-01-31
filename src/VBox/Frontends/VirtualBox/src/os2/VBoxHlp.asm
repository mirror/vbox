; $Id:$
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


include "iprt/asmdefs.mac"


BEGINCODE
extern _DLL_InitTerm

; Low-level DLL entry point - Forward to the C code.
..start:
    jmp _DLL_InitTerm


; emxomfld may generate references to this for weak symbols. It is usually
; found in in libend.lib.
ABSOLUTE 0
global WEAK$ZERO
WEAK$ZERO:

