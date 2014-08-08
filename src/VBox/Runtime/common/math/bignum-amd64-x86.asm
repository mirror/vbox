; $Id$
;; @file
; IPRT - Big Integer Numbers, AMD64 and X86 Assembly Workers
;

;
; Copyright (C) 2006-2014 Oracle Corporation
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


%define RT_ASM_WITH_SEH64
%include "iprt/asmdefs.mac"
%include "internal/bignum.mac"


BEGINCODE

;;
; Subtracts a number (pauSubtrahend) from a larger number (pauMinuend) and
; stores the result in pauResult.
;
; All three numbers are zero padded such that a borrow can be carried one (or
; two for 64-bit) elements beyond the end of the largest number.
;
; @returns nothing.
; @param    pauResult       x86:[ebp +  8]  gcc:rdi  msc:rcx
; @param    pauMinuend      x86:[ebp + 12]  gcc:rsi  msc:rdx
; @param    pauSubtrahend   x86:[ebp + 16]  gcc:rdx  msc:r8
; @param    cUsed           x86:[ebp + 20]  gcc:rcx  msc:r9
;
BEGINPROC rtBigNumMagnitudeSubAssemblyWorker
        push    xBP
        SEH64_PUSH_xBP
        mov     xBP, xSP
        SEH64_SET_FRAME_xBP 0
SEH64_END_PROLOGUE

%ifdef RT_ARCH_AMD64
 %ifdef ASM_CALL64_GCC
  %define pauResult         rdi
  %define pauMinuend        rsi
  %define pauSubtrahend     rdx
  %define cUsed             ecx
 %else
  %define pauResult         rcx
  %define pauMinuend        rdx
  %define pauSubtrahend     r8
  %define cUsed             r9d
 %endif
        xor     r11d, r11d              ; index register.

%if RTBIGNUM_ELEMENT_SIZE == 4
        add     cUsed, 1                ; cUsed = RT_ALIGN(cUsed, 2) / 2
        shr     cUsed, 1
%endif
        cmp     cUsed, 8                ; Skip the big loop if small number.
        jb      .small_job

        mov     r10d, cUsed
        shr     r10d, 3
        clc
.big_loop:
        mov     rax, [pauMinuend + r11]
        sbb     rax, [pauSubtrahend + r11]
        mov     [pauResult + r11], rax
        mov     rax, [pauMinuend    + r11 +  8]
        sbb     rax, [pauSubtrahend + r11 +  8]
        mov     [pauResult + r11 +  8], rax
        mov     rax, [pauMinuend    + r11 + 16]
        sbb     rax, [pauSubtrahend + r11 + 16]
        mov     [pauResult + r11 + 16], rax
        mov     rax, [pauMinuend    + r11 + 24]
        sbb     rax, [pauSubtrahend + r11 + 24]
        mov     [pauResult + r11 + 24], rax
        mov     rax, [pauMinuend    + r11 + 32]
        sbb     rax, [pauSubtrahend + r11 + 32]
        mov     [pauResult + r11 + 32], rax
        mov     rax, [pauMinuend    + r11 + 40]
        sbb     rax, [pauSubtrahend + r11 + 40]
        mov     [pauResult + r11 + 40], rax
        mov     rax, [pauMinuend    + r11 + 48]
        sbb     rax, [pauSubtrahend + r11 + 48]
        mov     [pauResult + r11 + 48], rax
        mov     rax, [pauMinuend    + r11 + 56]
        sbb     rax, [pauSubtrahend + r11 + 56]
        mov     [pauResult + r11 + 56], rax
        lea     r11, [r11 + 64]
        dec     r10d                    ; Does not change CF.
        jnz     .big_loop

        lahf                            ; Save CF
        and     cUsed, 7                ; Up to seven odd rounds.
        jz      .done
        sahf                            ; Restore CF.
        jmp     .small_loop             ; Skip CF=1 (clc).

.small_job:
        clc
.small_loop:
        mov     rax, [pauMinuend + r11]
        sbb     rax, [pauSubtrahend + r11]
        mov     [pauResult + r11], rax
        lea     r11, [r11 + 8]
        dec     cUsed                   ; does not change CF.
        jnz     .small_loop
 %ifdef RT_STRICT
        jnc     .done
        int3
 %endif
.done:

%elifdef RT_ARCH_X86
        push    edi
        push    esi
        push    ebx

        mov     edi, [ebp + 08h]        ; pauResult
 %define pauResult      edi
        mov     ecx, [ebp + 0ch]        ; pauMinuend
 %define pauMinuend     ecx
        mov     edx, [ebp + 10h]        ; pauSubtrahend
 %define pauSubtrahend  edx
        mov     esi, [ebp + 14h]        ; cUsed
 %define cUsed          esi

        xor     ebx, ebx                ; index register.

        cmp     cUsed, 8                ; Skip the big loop if small number.
        jb      .small_job

        shr     cUsed, 3
        clc
.big_loop:
        mov     eax, [pauMinuend + ebx]
        sbb     eax, [pauSubtrahend + ebx]
        mov     [pauResult + ebx], eax
        mov     eax, [pauMinuend    + ebx +  4]
        sbb     eax, [pauSubtrahend + ebx +  4]
        mov     [pauResult + ebx +  4], eax
        mov     eax, [pauMinuend    + ebx +  8]
        sbb     eax, [pauSubtrahend + ebx +  8]
        mov     [pauResult + ebx +  8], eax
        mov     eax, [pauMinuend    + ebx + 12]
        sbb     eax, [pauSubtrahend + ebx + 12]
        mov     [pauResult + ebx + 12], eax
        mov     eax, [pauMinuend    + ebx + 16]
        sbb     eax, [pauSubtrahend + ebx + 16]
        mov     [pauResult + ebx + 16], eax
        mov     eax, [pauMinuend    + ebx + 20]
        sbb     eax, [pauSubtrahend + ebx + 20]
        mov     [pauResult + ebx + 20], eax
        mov     eax, [pauMinuend    + ebx + 24]
        sbb     eax, [pauSubtrahend + ebx + 24]
        mov     [pauResult + ebx + 24], eax
        mov     eax, [pauMinuend    + ebx + 28]
        sbb     eax, [pauSubtrahend + ebx + 28]
        mov     [pauResult + ebx + 28], eax
        lea     ebx, [ebx + 32]
        dec     cUsed                   ; Does not change CF.
        jnz     .big_loop

        lahf                            ; Save CF
        mov     cUsed, [ebp + 14h]      ; Up to three final rounds.
        and     cUsed, 7
        jz      .done
        sahf                            ; Restore CF.
        jmp     .small_loop             ; Skip CF=1 (clc).

.small_job:
        clc
.small_loop:
        mov     eax, [pauMinuend + ebx]
        sbb     eax, [pauSubtrahend + ebx]
        mov     [pauResult + ebx], eax
        lea     ebx, [ebx + 4]
        dec     cUsed                   ; Does not change CF
        jnz     .small_loop
 %ifdef RT_STRICT
        jnc     .done
        int3
 %endif
.done:

        pop     ebx
        pop     esi
        pop     edi
%else
 %error "Unsupported arch"
%endif

        leave
        ret
%undef pauResult
%undef pauMinuend
%undef pauSubtrahend
%undef cUsed
ENDPROC rtBigNumMagnitudeSubAssemblyWorker



;;
; Subtracts a number (pauSubtrahend) from a larger number (pauMinuend) and
; stores the result in pauResult.
;
; All three numbers are zero padded such that a borrow can be carried one (or
; two for 64-bit) elements beyond the end of the largest number.
;
; @returns nothing.
; @param    pauResultMinuend    x86:[ebp +  8]  gcc:rdi  msc:rcx
; @param    pauSubtrahend       x86:[ebp + 12]  gcc:rsi  msc:rdx
; @param    cUsed               x86:[ebp + 16]  gcc:rdx  msc:r8
;
BEGINPROC rtBigNumMagnitudeSubThisAssemblyWorker
        push    xBP
        SEH64_PUSH_xBP
        mov     xBP, xSP
        SEH64_SET_FRAME_xBP 0
SEH64_END_PROLOGUE

%ifdef RT_ARCH_AMD64
 %ifdef ASM_CALL64_GCC
  %define   pauResultMinuend    rdi
  %define   pauSubtrahend       rsi
  %define   cUsed               edx
 %else
  %define   pauResultMinuend    rcx
  %define   pauSubtrahend       rdx
  %define   cUsed               r8d
 %endif
        xor     r11d, r11d              ; index register.

%if RTBIGNUM_ELEMENT_SIZE == 4
        add     cUsed, 1                ; cUsed = RT_ALIGN(cUsed, 2) / 2
        shr     cUsed, 1
%endif
        cmp     cUsed, 8                ; Skip the big loop if small number.
        jb      .small_job

        mov     r10d, cUsed
        shr     r10d, 3
        clc
.big_loop:
        mov     rax, [pauSubtrahend + r11]
        sbb     [pauResultMinuend + r11], rax
        mov     rax, [pauSubtrahend + r11 +  8]
        sbb     [pauResultMinuend + r11 +  8], rax
        mov     rax, [pauSubtrahend + r11 + 16]
        sbb     [pauResultMinuend + r11 + 16], rax
        mov     rax, [pauSubtrahend + r11 + 24]
        sbb     [pauResultMinuend + r11 + 24], rax
        mov     rax, [pauSubtrahend + r11 + 32]
        sbb     [pauResultMinuend + r11 + 32], rax
        mov     rax, [pauSubtrahend + r11 + 40]
        sbb     [pauResultMinuend + r11 + 40], rax
        mov     rax, [pauSubtrahend + r11 + 48]
        sbb     [pauResultMinuend + r11 + 48], rax
        mov     rax, [pauSubtrahend + r11 + 56]
        sbb     [pauResultMinuend + r11 + 56], rax
        lea     r11, [r11 + 64]
        dec     r10d                    ; Does not change CF.
        jnz     .big_loop

        lahf                            ; Save CF
        and     cUsed, 7                ; Up to seven odd rounds.
        jz      .done
        sahf                            ; Restore CF.
        jmp     .small_loop             ; Skip CF=1 (clc).

.small_job:
        clc
.small_loop:
        mov     rax, [pauSubtrahend + r11]
        sbb     [pauResultMinuend + r11], rax
        lea     r11, [r11 + 8]
        dec     cUsed                   ; does not change CF.
        jnz     .small_loop
 %ifdef RT_STRICT
        jnc     .done
        int3
 %endif
.done:

%elifdef RT_ARCH_X86
        push    edi
        push    ebx

        mov     edi, [ebp + 08h]        ; pauResultMinuend
 %define pauResultMinuend   edi
        mov     edx, [ebp + 0ch]        ; pauSubtrahend
 %define pauSubtrahend      edx
        mov     ecx, [ebp + 10h]        ; cUsed
 %define cUsed              ecx

        xor     ebx, ebx                ; index register.

        cmp     cUsed, 8                ; Skip the big loop if small number.
        jb      .small_job

        shr     cUsed, 3
        clc
.big_loop:
        mov     eax, [pauSubtrahend + ebx]
        sbb     [pauResultMinuend + ebx], eax
        mov     eax, [pauSubtrahend + ebx + 4]
        sbb     [pauResultMinuend + ebx + 4], eax
        mov     eax, [pauSubtrahend + ebx + 8]
        sbb     [pauResultMinuend + ebx + 8], eax
        mov     eax, [pauSubtrahend + ebx + 12]
        sbb     [pauResultMinuend + ebx + 12], eax
        mov     eax, [pauSubtrahend + ebx + 16]
        sbb     [pauResultMinuend + ebx + 16], eax
        mov     eax, [pauSubtrahend + ebx + 20]
        sbb     [pauResultMinuend + ebx + 20], eax
        mov     eax, [pauSubtrahend + ebx + 24]
        sbb     [pauResultMinuend + ebx + 24], eax
        mov     eax, [pauSubtrahend + ebx + 28]
        sbb     [pauResultMinuend + ebx + 28], eax
        lea     ebx, [ebx + 32]
        dec     cUsed                   ; Does not change CF.
        jnz     .big_loop

        lahf                            ; Save CF
        mov     cUsed, [ebp + 10h]      ; Up to seven odd rounds.
        and     cUsed, 7
        jz      .done
        sahf                            ; Restore CF.
        jmp     .small_loop             ; Skip CF=1 (clc).

.small_job:
        clc
.small_loop:
        mov     eax, [pauSubtrahend + ebx]
        sbb     [pauResultMinuend + ebx], eax
        lea     ebx, [ebx + 4]
        dec     cUsed                   ; Does not change CF
        jnz     .small_loop
 %ifdef RT_STRICT
        jnc     .done
        int3
 %endif
.done:

        pop     ebx
        pop     edi
%else
 %error "Unsupported arch"
%endif

        leave
        ret
ENDPROC rtBigNumMagnitudeSubThisAssemblyWorker


;;
; Shifts an element array one bit to the left, returning the final carry value.
;
; On 64-bit hosts the array is always zero padded to a multiple of 8 bytes, so
; we can use 64-bit operand sizes even if the element type is 32-bit.
;
; @returns The final carry value.
; @param    pauElements     x86:[ebp +  8]  gcc:rdi  msc:rcx
; @param    cUsed           x86:[ebp + 12]  gcc:rsi  msc:rdx
; @param    uCarry          x86:[ebp + 16]  gcc:rdx  msc:r8
;
BEGINPROC rtBigNumMagnitudeShiftLeftOneAssemblyWorker
        push    xBP
        SEH64_PUSH_xBP
        mov     xBP, xSP
        SEH64_SET_FRAME_xBP 0
SEH64_END_PROLOGUE

%ifdef RT_ARCH_AMD64
 %ifdef ASM_CALL64_GCC
  %define pauElements       rdi
  %define cUsed             esi
  %define uCarry            edx
 %else
  %define pauElements       rcx
  %define cUsed             edx
  %define uCarry            r8d
 %endif
%elifdef RT_ARCH_X86
  %define pauElements       ecx
        mov     pauElements, [ebp + 08h]
  %define cUsed             edx
        mov     cUsed, [ebp + 0ch]
  %define uCarry            eax
        mov     uCarry, [ebp + 10h]
%else
 %error "Unsupported arch."
%endif
        ; Lots to do?
        cmp     cUsed, 8
        jae     .big_loop_init

        ; Check for empty array.
        test    cUsed, cUsed
        jz      .no_elements
        jmp     .small_loop_init

        ; Big loop - 8 unrolled loop iterations.
.big_loop_init:
%ifdef RT_ARCH_AMD64
        mov     r11d, cUsed
%endif
        shr     cUsed, 3
        test    uCarry, uCarry          ; clear the carry flag
        jz      .big_loop
        stc
.big_loop:
%if RTBIGNUM_ELEMENT_SIZE == 8
        rcl     qword [pauElements], 1
        rcl     qword [pauElements + 8], 1
        rcl     qword [pauElements + 16], 1
        rcl     qword [pauElements + 24], 1
        rcl     qword [pauElements + 32], 1
        rcl     qword [pauElements + 40], 1
        rcl     qword [pauElements + 48], 1
        rcl     qword [pauElements + 56], 1
        lea     pauElements, [pauElements + 64]
%else
        rcl     dword [pauElements], 1
        rcl     dword [pauElements + 4], 1
        rcl     dword [pauElements + 8], 1
        rcl     dword [pauElements + 12], 1
        rcl     dword [pauElements + 16], 1
        rcl     dword [pauElements + 20], 1
        rcl     dword [pauElements + 24], 1
        rcl     dword [pauElements + 28], 1
        lea     pauElements, [pauElements + 32]
%endif
        dec     cUsed
        jnz     .big_loop

        ; More to do?
        lahf                            ; save carry flag (uCarry no longer used on x86).
%ifdef RT_ARCH_AMD64
        mov     cUsed, r11d
%else
        mov     cUsed, [ebp + 0ch]
%endif
        and     cUsed, 7
        jz      .restore_cf_and_return  ; Jump if we're good and done.
        sahf                            ; Restore CF.
        jmp     .small_loop             ; Deal with the odd rounds.
.restore_cf_and_return:
        sahf
        jmp     .carry_to_eax

        ; Small loop - One round at the time.
.small_loop_init:
        test    uCarry, uCarry          ; clear the carry flag
        jz      .small_loop
        stc
.small_loop:
%if RTBIGNUM_ELEMENT_SIZE == 8
        rcl     qword [pauElements], 1
        lea     pauElements, [pauElements + 8]
%else
        rcl     dword [pauElements], 1
        lea     pauElements, [pauElements + 4]
%endif
        dec     cUsed
        jnz     .small_loop

        ; Calculate return value.
.carry_to_eax:
        mov     eax, 0
        jnc     .return
        inc     eax
.return:
        leave
        ret

.no_elements:
        mov     eax, uCarry
        jmp     .return
ENDPROC rtBigNumMagnitudeShiftLeftOneAssemblyWorker

