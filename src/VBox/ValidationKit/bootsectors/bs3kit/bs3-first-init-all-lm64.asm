; $Id$
;; @file
; BS3Kit - First Object, calling 32-bit paged protected mode main() after full init.
;

;
; Copyright (C) 2007-2020 Oracle Corporation
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


;
; Segment defs, grouping and related variables.
; Defines the entry point 'start' as well, leaving us in BS3TEXT16.
;
%include "bs3-first-common.mac"

extern NAME(Bs3InitAll_rm)
extern NAME(Bs3SwitchToLM64_rm)
extern NAME(Main_lm64)
extern NAME(Bs3Shutdown_c64)
extern BS3_DATA_NM(g_uBs3CpuDetected)
extern NAME(Bs3PrintStrN_c16)
extern NAME(Bs3Panic_c16)

;; Entry point.
        push    word 0                  ; zero return address.
        push    word 0                  ; zero caller BP
        mov     bp, sp

        ;
        ; Init all while we're in real mode.
        ;
        mov     ax, BS3_SEL_DATA16
        mov     es, ax
        mov     ds, ax
        call    NAME(Bs3InitAll_rm)

        ;
        ; Check that long mode is supported.
        ;
        test    word [BS3_DATA_NM(g_uBs3CpuDetected)], BS3CPU_F_LONG_MODE
        jnz     .long_mode_supported
        push    .s_szLongModeError_End - .s_szLongModeError
        push    cs
        push    .s_szLongModeError wrt CGROUP16
        call    NAME(Bs3PrintStrN_c16)
        call    NAME(Bs3Panic_c16)
.long_mode_supported:

        ;
        ; Switch to LM64 and call main.
        ;
        call    _Bs3SwitchToLM64_rm
        BITS 64
        call    NAME(Main_lm64)

        ; Try shutdown if it returns.
        call    NAME(Bs3Shutdown_c64)

.s_szLongModeError:
        db      'BS3 Error! Long mode not supported!', 0ah, 0dh
.s_szLongModeError_End:
        db      00h

