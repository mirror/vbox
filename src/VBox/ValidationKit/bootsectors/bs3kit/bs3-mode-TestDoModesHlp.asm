; $Id$
;; @file
; BS3Kit - Bs3TestDoModes helpers
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


;BS3_DECL(uint8_t) TMPL_NM(Bs3TestCallDoerInRM)(uint16_t offCallback);
;BS3_DECL(uint8_t) TMPL_NM(Bs3TestCallDoerInPE16)(uint16_t offCallback);
;BS3_DECL(uint8_t) TMPL_NM(Bs3TestCallDoerInPE16_32)(uint32_t offCallback);
;BS3_DECL(uint8_t) TMPL_NM(Bs3TestCallDoerInPE16_V86)(uint16_t offCallback);
;BS3_DECL(uint8_t) TMPL_NM(Bs3TestCallDoerInPE32)(uint32_t offCallback);
;BS3_DECL(uint8_t) TMPL_NM(Bs3TestCallDoerInPE32_16)(uint16_t offCallback);
;BS3_DECL(uint8_t) TMPL_NM(Bs3TestCallDoerInPEV86)(uint16_t offCallback);
;BS3_DECL(uint8_t) TMPL_NM(Bs3TestCallDoerInPP16)(uint16_t offCallback);
;BS3_DECL(uint8_t) TMPL_NM(Bs3TestCallDoerInPP16_32)(uint32_t offCallback);
;BS3_DECL(uint8_t) TMPL_NM(Bs3TestCallDoerInPP16_V86)(uint16_t offCallback);
;BS3_DECL(uint8_t) TMPL_NM(Bs3TestCallDoerInPP32)(uint32_t offCallback);
;BS3_DECL(uint8_t) TMPL_NM(Bs3TestCallDoerInPP32_16)(uint16_t offCallback);
;BS3_DECL(uint8_t) TMPL_NM(Bs3TestCallDoerInPPV86)(uint16_t offCallback);
;BS3_DECL(uint8_t) TMPL_NM(Bs3TestCallDoerInPAE16)(uint16_t offCallback);
;BS3_DECL(uint8_t) TMPL_NM(Bs3TestCallDoerInPAE16_32)(uint32_t offCallback);
;BS3_DECL(uint8_t) TMPL_NM(Bs3TestCallDoerInPAE16_V86)(uint16_t offCallback);
;BS3_DECL(uint8_t) TMPL_NM(Bs3TestCallDoerInPAE32)(uint32_t offCallback);
;BS3_DECL(uint8_t) TMPL_NM(Bs3TestCallDoerInPAE32_16)(uint16_t offCallback);
;BS3_DECL(uint8_t) TMPL_NM(Bs3TestCallDoerInPAEV86)(uint16_t offCallback);
;BS3_DECL(uint8_t) TMPL_NM(Bs3TestCallDoerInLM16)(uint16_t offCallback);
;BS3_DECL(uint8_t) TMPL_NM(Bs3TestCallDoerInLM32)(uint32_t offCallback);
;BS3_DECL(uint8_t) TMPL_NM(Bs3TestCallDoerInLM64)(uint32_t offCallback);

;*********************************************************************************************************************************
;*  External Symbols                                                                                                             *
;*********************************************************************************************************************************
extern TMPL_NM(Bs3SwitchToRM)
extern TMPL_NM(Bs3SwitchToPE16)
extern TMPL_NM(Bs3SwitchToPE16_32)
extern RT_CONCAT3(_Bs3SwitchTo,TMPL_MODE_UNAME,_rm)
extern RT_CONCAT3(_Bs3SwitchTo,TMPL_MODE_UNAME,_pe16)
extern RT_CONCAT3(_Bs3SwitchTo,TMPL_MODE_UNAME,_pe16_32)
;extern RT_CONCAT3(_Bs3SwitchTo,TMPL_MODE_UNAME,_pe16_v86)
;extern RT_CONCAT3(_Bs3SwitchTo,TMPL_MODE_UNAME,_pe16_)


;;
;
; @param %1     Non-zero if 16-bit target.
;
%macro MyProlog 1
        BS3_CALL_CONV_PROLOG 1
        push    xBP
        mov     xBP, xSP

        ; Save non-volatile registers so the DO function doesn't have to.
        push    xBX
        push    xCX
        push    xDX
        push    xSI
        push    xDI
%if TMPL_BITS != 64
        push    ds
        push    es
        push    ss
 %if TMPL_BITS != 16
        push    fs
        push    gs
 %endif
%endif
%if TMPL_BITS == 64
        push    r8
        push    r9
        push    r10
        push    r11
        push    r12
        push    r13
        push    r14
        push    r15
%endif

%if TMPL_BITS != 16 && %1 != 0
        ; Change to 16-bit segment
        jmp     .sixteen_bit_segment
BS3_BEGIN_TEXT16
        BS3_SET_BITS TMPL_BITS
.sixteen_bit_segment:
%endif

        ; Load the call address into ax or eax.
%if %1 != 0
        mov     ax, [xBP + xCB*2]
%else
        mov     eax, [xBP + xCB*2]
%endif

%endmacro


;;
;
; @param %1     Non-zero if 16-bit target.
;
%macro MyEpilog 1
%if TMPL_BITS != 16
        ; Change back to the default template segment.
        jmp     .template_segment
TMPL_BEGIN_TEXT
        BS3_SET_BITS TMPL_BITS
.template_segment:
%endif

        ; Restore registers.
%if TMPL_BITS == 16
        sub     bp, 0ah
        mov     sp, bp
%elif TMPL_BITS == 32
        lea     xSP, [xBP - 14h]
%else
        lea     xSP, [xBP - 68h]
        pop     r15
        pop     r14
        pop     r13
        pop     r12
        pop     r11
        pop     r10
        pop     r9
        pop     r8
%endif
%if TMPL_BITS != 64
 %if TMPL_BITS != 16
        pop     gs
        pop     fs
 %endif
        pop     ss
        pop     es
        pop     ds
%endif
        pop     xDI
        pop     xSI
        pop     xDX
        pop     xCX
        pop     xBX
        pop     xBP
%endmacro


;;
; @cproto   BS3_DECL(uint8_t) Bs3TestCallDoerInRM(uint16_t offBs3Text16);
; @uses     rax
BS3_PROC_BEGIN_MODE Bs3TestCallDoerInRM
        MyProlog 16

        ; Mode switch, make the call, switch back.
        call    TMPL_NM(Bs3SwitchToRM)
        BS3_SET_BITS 16
        call    ax
        call    RT_CONCAT3(_Bs3SwitchTo,TMPL_MODE_UNAME,_rm)
        BS3_SET_BITS TMPL_BITS

        MyEpilog 16
        ret
BS3_PROC_END_MODE   Bs3TestCallDoerInRM


;;
; @cproto   BS3_DECL(uint8_t) Bs3TestCallDoerInPE16(uint16_t offBs3Text16);
; @uses     rax
BS3_PROC_BEGIN_MODE Bs3TestCallDoerInPE16
        MyProlog 16

        ; Mode switch, make the call, switch back.
        call    TMPL_NM(Bs3SwitchToPE16)
        BS3_SET_BITS 16
        call    ax
        call    RT_CONCAT3(_Bs3SwitchTo,TMPL_MODE_UNAME,_pe16)
        BS3_SET_BITS TMPL_BITS

        MyEpilog 16
        ret
BS3_PROC_END_MODE   Bs3TestCallDoerInPE16

;;
; @cproto   BS3_DECL(uint8_t) Bs3TestCallDoerInPE16_32(uint16_t offBs3Text16);
; @uses     rax
BS3_PROC_BEGIN_MODE Bs3TestCallDoerInPE16_32
        MyProlog 0

        ; Mode switch, make the call, switch back.
        call    TMPL_NM(Bs3SwitchToPE16_32)
        BS3_SET_BITS 32
        call    eax
        call    RT_CONCAT3(_Bs3SwitchTo,TMPL_MODE_UNAME,_pe16_32)
        BS3_SET_BITS TMPL_BITS

        MyEpilog 0
        ret
BS3_PROC_END_MODE   Bs3TestCallDoerInPE16_32

