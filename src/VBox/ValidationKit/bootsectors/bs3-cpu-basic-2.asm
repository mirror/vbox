; $Id$
;; @file
; BS3Kit - bs3-cpu-basic-2
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
%include "bs3kit.mac"

;
; Segment defs, grouping and related variables.
; Defines the entry point 'start' as well, leaving us in BS3TEXT16.
;
%include "bs3-first-common.mac"


;
; We start in real mode.
;
%define TMPL_RM
%include "bs3kit-template-header.mac"

BS3_BEGIN_TEXT16
BS3_EXTERN_TMPL Bs3InitMemory
BS3_EXTERN_CMN  Bs3Trap32Init
BS3_EXTERN_CMN  Bs3Shutdown


BS3_BEGIN_TEXT16
BS3_PROC_BEGIN Bs3CpuBasic2_Main
        push    word 0                  ; zero return address.
        push    word 0                  ; zero caller BP
        mov     bp, sp
        sub     sp, 20h                 ; reserve 20h for 64-bit calls (we're doing them MSC style, remember).

        ;
        ; Do bs3kit init.
        ;
        call    Bs3InitMemory           ; Initialize the memory (must be done from real mode).
        call    Bs3Trap32Init

        ;
        ; Start testing.
        ;

        ;
        ; Done.
        ;
.shutdown:
        call    Bs3Shutdown
        jmp     .shutdown
BS3_PROC_END   Bs3CpuBasic2_Main



