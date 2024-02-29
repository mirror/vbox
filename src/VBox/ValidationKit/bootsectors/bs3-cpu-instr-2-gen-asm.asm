; $Id$
;; @file
; BS3Kit - bs3-cpu-instr-2-gen - assembly helpers for test data generator.
;

;
; Copyright (C) 2024 Oracle and/or its affiliates.
;
; This file is part of VirtualBox base platform packages, as
; available from https://www.virtualbox.org.
;
; This program is free software; you can redistribute it and/or
; modify it under the terms of the GNU General Public License
; as published by the Free Software Foundation, in version 3 of the
; License.
;
; This program is distributed in the hope that it will be useful, but
; WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
; General Public License for more details.
;
; You should have received a copy of the GNU General Public License
; along with this program; if not, see <https://www.gnu.org/licenses>.
;
; The contents of this file may alternatively be used under the terms
; of the Common Development and Distribution License Version 1.0
; (CDDL), a copy of it is provided in the "COPYING.CDDL" file included
; in the VirtualBox distribution, in which case the provisions of the
; CDDL are applicable instead of those of the GPL.
;
; You may elect to license modified versions of this file under the
; terms and conditions of either the GPL or the CDDL or both.
;
; SPDX-License-Identifier: GPL-3.0-only OR CDDL-1.0
;


;*********************************************************************************************************************************
;*  Header Files                                                                                                                 *
;*********************************************************************************************************************************
%include "iprt/asmdefs.mac"

BEGINCODE

%ifdef ASM_CALL64_MSC
 %define EFLAGS_PARAM_REG   r8d
%else
 %define EFLAGS_PARAM_REG   ecx
%endif


;;
; @param 1 instruction
; @param 2 Whether it takes carry in.
; @param 3 Whether it has an 8-bit form.
%macro DO_BINARY 3

 %if %3 != 0
BEGINPROC   GenU8_ %+ %1
  %if %2 != 0
        lahf
        and     ah, 0xfe
        shl     EFLAGS_PARAM_REG, 8
        or      eax, EFLAGS_PARAM_REG
        sahf
  %endif
  %ifdef ASM_CALL64_MSC
        %1      cl, dl
        mov     [r9], cl
  %else
        %1      sil, dil
        mov     [rdx], sil
  %endif
        pushf
        pop     rax
        ret
ENDPROC     GenU8_ %+ %1
 %endif

BEGINPROC   GenU16_ %+ %1
 %if %2 != 0
        lahf
        and     ah, 0xfe
        shl     EFLAGS_PARAM_REG, 8
        or      eax, EFLAGS_PARAM_REG
        sahf
 %endif
 %ifdef ASM_CALL64_MSC
        %1      cx, dx
        mov     [r9], cx
 %else
        %1      si, di
        mov     [rdx], si
 %endif
        pushf
        pop     rax
        ret
ENDPROC     GenU16_ %+ %1

BEGINPROC   GenU32_ %+ %1
 %if %2 != 0
        lahf
        and     ah, 0xfe
        shl     EFLAGS_PARAM_REG, 8
        or      eax, EFLAGS_PARAM_REG
        sahf
 %endif
 %ifdef ASM_CALL64_MSC
        %1      ecx, edx
        mov     [r9], ecx
 %else
        %1      esi, edi
        mov     [rdx], esi
 %endif
        pushf
        pop     rax
        ret
ENDPROC     GenU32_ %+ %1

BEGINPROC   GenU64_ %+ %1
 %if %2 != 0
        lahf
        and     ah, 0xfe
        shl     EFLAGS_PARAM_REG, 8
        or      eax, EFLAGS_PARAM_REG
        sahf
 %endif
 %ifdef ASM_CALL64_MSC
        %1      rcx, rdx
        mov     [r9], rcx
 %else
        %1      rsi, rdi
        mov     [rdx], rsi
 %endif
        pushf
        pop     rax
        ret
ENDPROC     GenU64_ %+ %1

%endmacro

DO_BINARY and,  0, 1
DO_BINARY or,   0, 1
DO_BINARY xor,  0, 1
DO_BINARY test, 0, 1

DO_BINARY add,  0, 1
DO_BINARY adc,  1, 1
DO_BINARY sub,  0, 1
DO_BINARY sbb,  1, 1
DO_BINARY cmp,  0, 1

DO_BINARY bt,   0, 0
DO_BINARY btc,  0, 0
DO_BINARY btr,  0, 0
DO_BINARY bts,  0, 0

