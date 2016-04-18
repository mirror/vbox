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


;*********************************************************************************************************************************
;*  Header Files                                                                                                                 *
;*********************************************************************************************************************************

;
; Segment defs, grouping and related variables.
; Defines the entry point 'start' as well, leaving us in BS3TEXT16.
;
%include "bs3-first-common.mac"


;*********************************************************************************************************************************
;*  External Symbols                                                                                                             *
;*********************************************************************************************************************************
BS3_EXTERN_SYSTEM16 Bs3Lgdt_Gdt
BS3_EXTERN_SYSTEM16 Bs3Lidt_Idt16

extern Bs3SelProtFar32ToFlat32_c64
extern Bs3PrintChr_c64
extern Bs3Printf_c64
extern Bs3TestTerm_c64
extern Bs3TestSub_c64
extern Bs3TestInit_c64

BS3_BEGIN_TEXT16
EXTERN Bs3InitMemory_rm

%if 0
EXTERN Main_pe16
EXTERN Bs3SwitchToPE16_rm
EXTERN Bs3SwitchToRM_pe16
EXTERN Bs3SwitchToPE32_rm
EXTERN Bs3SwitchTo32Bit_c16
EXTERN Bs3SwitchTo32Bit_c32
EXTERN Bs3SwitchTo16Bit_c16
EXTERN Bs3SwitchTo16Bit_c32
EXTERN Bs3SwitchToPP16_rm
EXTERN Bs3SwitchToPP32_rm
EXTERN Bs3SwitchToPAE16_rm
EXTERN Bs3SwitchToPAE32_rm
EXTERN Bs3SwitchToLM64_rm
EXTERN Bs3SwitchToRM_pe32
EXTERN Bs3SwitchToRM_pp16
EXTERN Bs3SwitchToRM_pp32
EXTERN Bs3SwitchToRM_pae16
EXTERN Bs3SwitchToRM_pae32
extern Bs3SwitchToRM_lm64
extern _Bs3PrintChr_c16
extern _Bs3PrintChr_c32
extern Bs3PrintChr_c64
%endif

BS3_EXTERN_CMN Bs3Shutdown
BS3_EXTERN_CMN Bs3Trap32Init


BS3_BEGIN_TEXT16
    ;
    ; We need to enter 16-bit protected mode before we can call Main_pe16.
    ;
    call    NAME(Bs3InitMemory_rm)      ; Initialize the memory (must be done from real mode).
    call    Bs3Trap32Init
    sub     xSP, 20h                    ; for 64-bit calls.
%if 0
    call    NAME(Bs3SwitchToPE16_rm)
    push    '1'
    call    NAME(Bs3PrintChr_c16)

    call    NAME(Bs3SwitchTo32Bit_c16)
    BS3_SET_BITS 32
    call    NAME(Bs3SwitchTo16Bit_c32)
    BS3_SET_BITS 16
    push    '2'
    call    NAME(Bs3PrintChr_c16)

    call    NAME(Bs3SwitchToRM_pe16)

    call    NAME(Bs3SwitchToPE32_rm)
    BS3_SET_BITS 32
    push    '3'
    call    NAME(Bs3PrintChr_c32)
    call    NAME(Bs3SwitchToRM_pe32)
    BS3_SET_BITS 16
    push    '4'
    call    NAME(Bs3PrintChr_c16)

    call    NAME(Bs3SwitchToPE16_rm)
    push    '5'
    call    NAME(Bs3PrintChr_c16)

    call    NAME(Bs3SwitchToRM_pe16)
    push    '6'
    call    NAME(Bs3PrintChr_c16)

    call    NAME(Bs3SwitchToPP16_rm)
    push    '7'
    call    NAME(Bs3PrintChr_c16)

    call    NAME(Bs3SwitchToRM_pp16)
    push    '8'
    call    NAME(Bs3PrintChr_c16)

    call    NAME(Bs3SwitchToPP32_rm)
    BS3_SET_BITS 32
    push    '9'
    call    NAME(Bs3PrintChr_c32)

    call    NAME(Bs3SwitchToRM_pp32)
    BS3_SET_BITS 16
    push    'a'
    call    NAME(Bs3PrintChr_c16)

    call    NAME(Bs3SwitchToPAE32_rm)
    BS3_SET_BITS 32
    push    'b'
    call    NAME(Bs3PrintChr_c32)

    call    NAME(Bs3SwitchToRM_pae32)
    BS3_SET_BITS 16
    push    'c'
    call    NAME(Bs3PrintChr_c16)

    call    NAME(Bs3SwitchToPAE16_rm)
    push    'd'
    call    NAME(Bs3PrintChr_c16)

    call    NAME(Bs3SwitchToRM_pae16)
    push    'e'
    call    NAME(Bs3PrintChr_c16)

    call    NAME(Bs3SwitchToLM64_rm)
    BS3_SET_BITS 64
    push    'f'
    BS3_CALL Bs3PrintChr_c64,1

    call    Bs3SwitchToRM_lm64
    BS3_SET_BITS 16
    push    'g'
    call    NAME(Bs3PrintChr_c16)
%endif

    ;
    ; Call main, if it returns shutdown the system.
    ;
.halt:
hlt
jmp .halt
;    call    NAME(Main_pe16)
    call    Bs3Shutdown

