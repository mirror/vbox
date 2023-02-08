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
; Division of unsigned 64-bit values, returning the quotient.
;
; @returns  Quotient in edx:eax.
; @param    [ebp+08h]   Dividend (64-bit)
; @param    [ebp+10h]   Divisor (64-bit)
;
BEGINPROC_RAW   __aulldiv
        push    ebp
        mov     ebp, esp
        sub     esp, 10h                    ; space for quotient and remainder.

        ; Call RTVccUInt64Div(RTUINT64U const *paDividendDivisor, RTUINT64U *paQuotientRemainder)
        mov     edx, esp
        push    edx
        lea     ecx, [ebp + 8]
        push    ecx
        call    NAME(RTVccUInt64Div)

        ; Load the result (quotient).
        mov     eax, [ebp - 10h]
        mov     edx, [ebp - 10h + 4]

        leave
        ret     10h
ENDPROC_RAW     __aulldiv

