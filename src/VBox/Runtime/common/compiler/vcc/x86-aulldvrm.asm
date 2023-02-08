; $Id$
;; @file
; IPRT - Visual C++ Compiler - unsigned 64-bit division support, x86.
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


;*********************************************************************************************************************************
;*  External Symbols                                                                                                             *
;*********************************************************************************************************************************
extern NAME(RTVccUInt64Div)


;;
; Division of unsigned 64-bit values, returning both quotient and remainder.
;
; @returns  Quotient in edx:eax, remainder in ebx:ecx.
; @param    [ebp+08h]   Dividend (64-bit)
; @param    [ebp+10h]   Divisor (64-bit)
;
; @note     The remainder registers are swapped compared to Watcom's I8D and U8D.
;
BEGINPROC_RAW   __aulldvrm
        push    ebp
        mov     ebp, esp

%define DIVIDEND_LO ebp + 08h
%define DIVIDEND_HI ebp + 0ch
%define DIVISOR_LO  ebp + 10h
%define DIVISOR_HI  ebp + 14h

        ;
        ; If the divisor is only 32-bit wide as we can do a two-step division on 32-bit units.
        ;
        mov     ebx, [DIVISOR_HI]
        or      ebx, ebx
        jnz     .full_64_bit_divisor

        ; step 1: dividend_hi / divisor
        mov     ebx, [DIVISOR_LO]
        mov     eax, [DIVIDEND_HI]
        xor     edx, edx
        div     ebx
        mov     ecx, eax                    ; high quotient bits.

        ; step 2: (dividend_lo + step_1_remainder) / divisor
        mov     eax, [DIVIDEND_LO]          ; edx contains the remainder from the first step.
        div     ebx                         ; -> eax = low quotient, edx = remainder.

        xchg    edx, ecx                    ; ecx = (low) remainder, edx = saved high quotient from step 1
        xor     ebx, ebx                    ; ebx = high remainder is zero, since divisor is 32-bit.

        leave
        ret     10h

%if 1
        ;
        ; The divisor is larger than 32 bits.
        ;
        ; We can approximate the quotient by reducing the divisor to 32 bits
        ; (reducing the dividend accordingly) and perform a 32-bit division.
        ; The result will be at most one off.
        ;
        ; The remainder has to be calculated using multiplication and
        ; subtraction.
        ;
.full_64_bit_divisor:
        push    edi

        ; Find the shift count needed to reduce the divisor to 32-bit.
        bsr     ecx, ebx
        inc     cl
        test    cl, ~31
        jnz     .shift_32

        ; Shift the divisor into edi.
        mov     edi, [DIVISOR_LO]
        shrd    edi, ebx, cl                ; edi = reduced divisor

        ; Shift the dividend into edx:eax.
        mov     eax, [DIVIDEND_LO]
        mov     edx, [DIVIDEND_HI]
        shrd    eax, edx, cl
        shr     edx, cl
        jmp     .shifted

.shift_32:  ; simplified version.
        mov     edi, ebx
        mov     eax, [DIVIDEND_HI]
        xor     edx, edx
.shifted:

        ; Divide and save the approximate quotient (Qapprox) in edi.
        div     edi
        mov     edi, eax                    ; edi = Qapprox

        ; Now multiply Qapprox with the divisor.
        mul     dword [DIVISOR_HI]
        mov     ecx, eax                    ; temporary storage
        mov     eax, [DIVISOR_LO]
        mul     edi
        add     edx, ecx                    ; edx:eax = QapproxDividend = Qapprox * divisor

        ; Preload the dividend into ebx:ecx for remainder calculation and for adjusting Qapprox.
        mov     ecx, [DIVIDEND_LO]
        mov     ebx, [DIVIDEND_HI]

        ; If carry is set, the result overflowed 64 bits, so the quotient must be too large.
        jc      .quotient_is_one_above_and_calc_remainder

        ; Calculate the remainder, if this overflows (CF) it means Qapprox is
        ; one above and we need to reduce it and the adjust the remainder.
        sub     ecx, eax
        sbb     ebx, edx
        jc      .quotient_is_one_above
.done:
        mov     eax, edi
        xor     edx, edx

        pop     edi
        leave
        ret     10h

.quotient_is_one_above_and_calc_remainder:
        sub     ecx, eax
        sbb     ebx, edx
.quotient_is_one_above:
        add     ecx, [DIVISOR_LO]
        adc     ebx, [DIVISOR_HI]
        dec     edi
        jmp     .done

%else
        ;
        ; Fall back on a rather slow C implementation.
        ;
.full_64_bit_divisor:
        ; Call RTVccUInt64Div(RTUINT64U const *paDividendDivisor, RTUINT64U *paQuotientRemainder)
        sub     esp, 10h                    ; space for quotient and remainder.
        mov     edx, esp
        push    edx
        lea     ecx, [ebp + 8]
        push    ecx
        call    NAME(RTVccUInt64Div)

        ; Load the result.
        mov     eax, [ebp - 10h]
        mov     edx, [ebp - 10h + 4]

        mov     ecx, [ebp - 08h]
        mov     ebx, [ebp - 08h + 4]
        leave
        ret     10h

%endif
ENDPROC_RAW     __aulldvrm

