; $Id$
;; @file
; IEM - Native Recompiler Assembly Helpers.
;

;
; Copyright (C) 2023 Oracle and/or its affiliates.
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
; SPDX-License-Identifier: GPL-3.0-only
;

;*********************************************************************************************************************************
;*  Header Files                                                                                                                 *
;*********************************************************************************************************************************
%define RT_ASM_WITH_SEH64
%include "VBox/asmdefs.mac"


BEGINCODE

extern NAME(iemThreadedFunc_BltIn_LogCpuStateWorker)


;;
; This is wrapper function that saves and restores all volatile registers
; so the impact of inserting LogCpuState is minimal to the other TB code.
;
BEGINPROC   iemNativeHlpAsmSafeWrapLogCpuState
        push    xBP
        SEH64_PUSH_xBP
        mov     xBP, xSP
        SEH64_SET_FRAME_xBP 0
SEH64_END_PROLOGUE

        ;
        ; Save all volatile registers.
        ;
        push    xAX
        push    xCX
        push    xDX
%ifdef RT_OS_WINDOWS
        push    xSI
        push    xDI
%endif
        push    r8
        push    r9
        push    r10
        push    r11
        sub     rsp, 8+20h

        ;
        ; Call C function to do the actual work.
        ;
%ifdef RT_OS_WINDOWS
        mov     rcx, rbx                    ; IEMNATIVE_REG_FIXED_PVMCPU
        mov     rdx, [rbp + 10h]            ; Just in case we decide to put something there.
        xor     r8, r8
        xor     r9, r9
%else
        mov     rdi, rbx                    ; IEMNATIVE_REG_FIXED_PVMCPU
        mov     rsi, [rbp + 10h]            ; Just in case we decide to put something there.
        xor     ecx, ecx
        xor     edx, edx
%endif
        call    NAME(iemThreadedFunc_BltIn_LogCpuStateWorker)

        ;
        ; Restore volatile registers and return to the TB code.
        ;
        add     rsp, 8+20h
        pop     r11
        pop     r10
        pop     r9
        pop     r8
%ifdef RT_OS_WINDOWS
        pop     xDI
        pop     xSI
%endif
        pop     xDX
        pop     xCX
        pop     xAX
        leave
        ret
ENDPROC     iemNativeHlpAsmSafeWrapLogCpuState

