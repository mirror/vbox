; $Id$
;; @file
; Disassembly testcase - Invalid invariants.
;
; The intention is to check in a binary using the --all-invalid mode
; of tstDisasm-2.
;

;
; Copyright (C) 2008 Sun Microsystems, Inc.
;
; This file is part of VirtualBox Open Source Edition (OSE), as
; available from http://www.virtualbox.org. This file is free software;
; you can redistribute it and/or modify it under the terms of the GNU
; General Public License (GPL) as published by the Free Software
; Foundation, in version 2 as it comes in the "COPYING" file of the
; VirtualBox OSE distribution. VirtualBox OSE is distributed in the
; hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
;
; Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
; Clara, CA 95054 USA or visit http://www.sun.com if you need
; additional information or have any questions.
;

%include "tstAsm.mac"

    BITS TEST_BITS
; The disassembler doesn't do imm32 right for  64-bit stuff, so disable it for now.
;%if TEST_BITS == 64
; %define WITH_64_BIT_TESTS
;%endif

    ;
    ; ADC
    ;
        ; 14 ib         ADC AL, imm8
    lock adc al, byte 8
        ; 15 i[wd]      ADC [ER]AX, immX
    lock adc ax, word 16
    lock adc eax, dword 128
%ifdef WITH_64_BIT_TESTS
    lock adc rax, dword 256
    lock adc rax, dword 0cc90cc90h
%endif
        ; 80 /2 ib      ADC reg/mem8, imm8 - with reg dst
    lock adc cl, byte 8
        ; 81 /2 i[wd]   ADC reg/memX, immX - with reg dst
    lock adc cx, word 1000h
    lock adc ecx, dword 100000h
%ifdef WITH_64_BIT_TESTS
    lock adc rcx, dword 100000h
%endif
        ; 83 /2 ib      ADC reg/memX, imm8 - with reg dst
    lock adc cx, byte 07fh
    lock adc ecx, byte 07fh
%ifdef WITH_64_BIT_TESTS
    lock adc rcx, byte 07fh
%endif

        ; 10 /r         ADC reg/mem8, reg8 - with reg dst
    lock adc cl, bl
        ; 11 /r         ADC reg/memX, regX - with reg dst
    lock adc cx, bx
    lock adc ecx, ebx
%ifdef WITH_64_BIT_TESTS
    lock adc rcx, rbx
%endif
        ; 12 /r         ADC reg8, reg/mem8
    lock adc cl, [0badh]
        ; 13 /r         ADC regX, reg/memX
    lock adc cx, [0badh]
    lock adc ecx, [0badh]
%ifdef WITH_64_BIT_TESTS
    lock adc rcx, [0badh]
%endif

    ; ADD
    ; AND
    ; BTC
    ; BTR
    ; BTS
    ; CMPXCHG
    ; CMPXCHG8B
    ; CMPXCHG16B
    ; DEC
    ; INC
    ; NEG
    ; NOT
    ; OR
    ; SBB
    ; SUB
    ; XADD
    ; XCHG
    ; XOR


