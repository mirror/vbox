; $Id$
;; @file
; BS3Kit - Bs3TrapSetJmp.
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


;*********************************************************************************************************************************
;*  Header Files                                                                                                                 *
;*********************************************************************************************************************************
%include "bs3kit-template-header.mac"


;*********************************************************************************************************************************
;*  External Symbols                                                                                                             *
;*********************************************************************************************************************************
BS3_EXTERN_CMN Bs3RegCtxSave
%if TMPL_BITS == 16
BS3_EXTERN_CMN Bs3SelFar32ToFlat32
%endif
BS3_EXTERN_DATA16 g_Bs3TrapSetJmpCtx
BS3_EXTERN_DATA16 g_pBs3TrapSetJmpFrame
TMPL_BEGIN_TEXT


;;
; Sets up a one-shot set-jmp-on-trap.
;
; @uses     See, applicable C calling convention.
;
BS3_PROC_BEGIN_CMN Bs3TrapSetJmp
        BS3_CALL_CONV_PROLOG 1
        push    xBP
        mov     xBP, xSP
        push    xBX
        BS3_ONLY_64BIT_STMT sub     xSP, 20h

        ;
        ; Save the current register context.
        ;
        BS3_ONLY_16BIT_STMT push    ds
        BS3_LEA_MOV_WRT_RIP(xAX, BS3_DATA16_WRT(g_Bs3TrapSetJmpCtx))
        push    xAX
        BS3_CALL Bs3RegCtxSave, 1
        add     xSP, sCB

        ;
        ; Adjust the return context a little.
        ;
        BS3_LEA_MOV_WRT_RIP(xBX, BS3_DATA16_WRT(g_Bs3TrapSetJmpCtx))
        mov     xAX, [xBP + xCB]        ; The return address of this function
        mov     [xBX + BS3REGCTX.rip], xAX
        mov     xAX, [xBP]
        mov     [xBX + BS3REGCTX.rbp], xAX
        lea     xAX, [xBP + xCB*2]
        mov     [xBX + BS3REGCTX.rsp], xAX
        mov     xAX, [xBP - xCB]
        mov     [xBX + BS3REGCTX.rbx], xAX
        xor     xAX, xAX
        mov     [xBX + BS3REGCTX.rax], xAX ; the return value.

        ;
        ; Save the (flat) pointer to the trap frame return structure.
        ;
%if TMPL_BITS == 16
        xor     ax, ax
        push    ax
        push    word [xBP + xCB*2 + 2]
        push    word [xBP + xCB*2]
        call    Bs3SelFar32ToFlat32
        add     sp, 6h
        mov     [BS3_DATA16_WRT(g_pBs3TrapSetJmpFrame)], ax
        mov     [2 + BS3_DATA16_WRT(g_pBs3TrapSetJmpFrame)], dx
%else
        mov     xAX, [xBP + xCB*2]
        mov     [BS3_DATA16_WRT(g_pBs3TrapSetJmpFrame)], eax
%endif

        ;
        ; Return 'true'.
        ;
        mov     xAX, 1
        BS3_ONLY_64BIT_STMT add     xSP, 20h
        pop     xBX
        pop     xBP
        BS3_CALL_CONV_EPILOG 1
        ret
BS3_PROC_END_CMN   Bs3TrapSetJmp

