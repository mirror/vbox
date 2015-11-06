; $Id$
;; @file
; BS3Kit - First Object, calling real-mode main().
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

%include "bs3kit.mac"

;
; Just in case this helps the linker, define all the segments.
;
BS3_BEGIN_TEXT16

BS3_BEGIN_DATA16

BS3_BEGIN_TEXT32

BS3_BEGIN_DATA32

BS3_BEGIN_TEXT64

BS3_BEGIN_DATA64


;*********************************************************************************************************************************
;*  External Symbols                                                                                                             *
;*********************************************************************************************************************************
BS3_BEGIN_TEXT16
extern NAME(Bs3Shutdown_p16)
extern NAME(Main_rm)


;
; Nothing to init here, just call main and shutdown if it returns.
;
BS3_BEGIN_TEXT16
BITS 16
GLOBALNAME start
    mov     ax, BS3DATA16
    mov     es, ax
    mov     ds, ax
    call    NAME(Main_rm)
    call    NAME(Bs3Shutdown_p16)

