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
extern NAME(iemNativeHlpCheckTlbLookup)


;;
; This does the epilogue of a TB, given the RBP for the frame and eax value to return.
;
; @param    pFrame  (gcc:rdi, msc:rcx)  The frame pointer.
; @param    rc      (gcc:esi, msc:edx)  The return value.
;
; @note This doesn't really work for MSC since xmm6 thru xmm15 are non-volatile
;       and since we don't save them in the TB prolog we'll potentially return
;       with different values if any functions on the calling stack uses them
;       as they're unlikely to restore them till they return.
;
;       For the GCC calling convention all xmm registers are volatile and the
;       only worry would be someone fiddling the control bits of MXCSR or FCW
;       without restoring them.  This is highly unlikely, unless we're doing
;       it ourselves, I think.
;
BEGINPROC   iemNativeTbLongJmp
%ifdef ASM_CALL64_MSC
        mov     rbp, rcx
        mov     eax, edx
%else
        mov     rbp, rdi
        mov     eax, esi
%endif
        SEH64_PUSH_xBP      ; non-sense, but whatever.
SEH64_END_PROLOGUE

        ;
        ; This must exactly match what iemNativeEmitEpilog does.
        ;
%ifdef ASM_CALL64_MSC
        lea     rsp, [rbp - 5 * 8]
%else
        lea     rsp, [rbp - 7 * 8]
%endif
        pop     r15
        pop     r14
        pop     r13
        pop     r12
%ifdef ASM_CALL64_MSC
        pop     rdi
        pop     rsi
%endif
        pop     rbx
        leave
        ret
ENDPROC     iemNativeTbLongJmp



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


;;
; This is wrapper function that saves and restores all volatile registers
; so the impact of inserting CheckTlbLookup is minimal to the other TB code.
;
BEGINPROC   iemNativeHlpAsmSafeWrapCheckTlbLookup
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
        mov     rcx, [rbp + 10h]
        mov     rdx, [rbp + 18h]
        mov     r8,  [rbp + 20h]
        mov     r9,  [rbp + 28h]
%else
        mov     rdi, [rbp + 10h]
        mov     rsi, [rbp + 18h]
        mov     ecx, [rbp + 20h]
        mov     edx, [rbp + 28h]
%endif
        call    NAME(iemNativeHlpCheckTlbLookup)

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
        ret     20h
ENDPROC     iemNativeHlpAsmSafeWrapCheckTlbLookup

