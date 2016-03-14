; $Id$
;; @file
; BS3Kit - Bs3SwitchToPPV86
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


;;
; Switch to 16-bit v8086 paged protected mode with 32-bit sys+tss from any other mode.
;
; @cproto   BS3_DECL(void) Bs3SwitchToPPV86(void);
;
; @uses     Nothing (except high 32-bit register parts).
;
; @remarks  Obviously returns to 16-bit v8086 mode, even if the caller was
;           in 32-bit or 64-bit mode.
;
; @remarks  Does not require 20h of parameter scratch space in 64-bit mode.
;
BS3_PROC_BEGIN_MODE Bs3SwitchToPPV86
%if TMPL_MODE == BS3_MODE_PPV86
        ret

%else
        ;
        ; Switch to 32-bit PP32 and from there to V8086.
        ;
        extern  TMPL_NM(Bs3SwitchToPP32)
        call    TMPL_NM(Bs3SwitchToPP32)
        BS3_SET_BITS 32

        ;
        ; Switch to v8086 mode after adjusting the return address.
        ;
 %if TMPL_BITS == 16
        push    word [esp]
        mov     word [esp + 2], 0
 %elif TMPL_BITS == 64
        pop     dword [esp + 4]
 %endif
        extern  _Bs3SwitchTo16BitV86_c32
        jmp     _Bs3SwitchTo16BitV86_c32
%endif
BS3_PROC_END_MODE   Bs3SwitchToPPV86

