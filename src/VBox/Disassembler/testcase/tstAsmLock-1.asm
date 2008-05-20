; $Id$
;; @file
; Disassembly testcase - Valid lock sequences and related instructions.
;
; This is a build test, that means it will be assembled, disassembled,
; then the disassembly output will be assembled and the new binary will
; compared with the original.
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

    BITS TEST_BITS

    ; ADC

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
    lock xchg [eax], eax
    lock xchg [ebx], eax
    lock xchg [ecx], eax
    lock xchg [edx], eax
    lock xchg [esp], eax
    lock xchg [ebp], eax
    lock xchg [esi], eax
    lock xchg [edi], eax
    lock xchg [eax], ebx
    lock xchg [ebx], ebx
    lock xchg [ecx], ebx
    lock xchg [edx], ebx
    lock xchg [esp], ebx
    lock xchg [ebp], ebx
    lock xchg [esi], ebx
    lock xchg [edi], ebx
    lock xchg [eax], ecx
    lock xchg [ebx], ecx
    lock xchg [ecx], ecx
    lock xchg [edx], ecx
    lock xchg [esp], ecx
    lock xchg [ebp], ecx
    lock xchg [esi], ecx
    lock xchg [edi], ecx
    lock xchg [eax], edx
    lock xchg [ebx], edx
    lock xchg [ecx], edx
    lock xchg [edx], edx
    lock xchg [esp], edx
    lock xchg [ebp], edx
    lock xchg [esi], edx
    lock xchg [edi], edx
    lock xchg [eax], esp
    lock xchg [ebx], esp
    lock xchg [ecx], esp
    lock xchg [edx], esp
    lock xchg [esp], esp
    lock xchg [ebp], esp
    lock xchg [esi], esp
    lock xchg [edi], esp
    lock xchg [eax], ebp
    lock xchg [ebx], ebp
    lock xchg [ecx], ebp
    lock xchg [edx], ebp
    lock xchg [esp], ebp
    lock xchg [ebp], ebp
    lock xchg [esi], ebp
    lock xchg [edi], ebp
    lock xchg [eax], esi
    lock xchg [ebx], esi
    lock xchg [ecx], esi
    lock xchg [edx], esi
    lock xchg [esp], esi
    lock xchg [ebp], esi
    lock xchg [esi], esi
    lock xchg [edi], esi
    lock xchg [eax], edi
    lock xchg [ebx], edi
    lock xchg [ecx], edi
    lock xchg [edx], edi
    lock xchg [esp], edi
    lock xchg [ebp], edi
    lock xchg [esi], edi
    lock xchg [edi], edi

    lock xchg [10], eax
    lock xchg [10], ebx
    lock xchg [10], ecx
    lock xchg [10], edx
    lock xchg [10], esp
    lock xchg [10], ebp
    lock xchg [10], esi
    lock xchg [10], edi

    lock xchg [10000], eax
    lock xchg [10000], ebx
    lock xchg [10000], ecx
    lock xchg [10000], edx
    lock xchg [10000], esp
    lock xchg [10000], ebp
    lock xchg [10000], esi
    lock xchg [10000], edi

    xchg [eax], eax
    xchg [ebx], eax
    xchg [ecx], eax
    xchg [edx], eax
    xchg [esp], eax
    xchg [ebp], eax
    xchg [esi], eax
    xchg [edi], eax
    xchg [eax], ebx
    xchg [ebx], ebx
    xchg [ecx], ebx
    xchg [edx], ebx
    xchg [esp], ebx
    xchg [ebp], ebx
    xchg [esi], ebx
    xchg [edi], ebx
    xchg [eax], ecx
    xchg [ebx], ecx
    xchg [ecx], ecx
    xchg [edx], ecx
    xchg [esp], ecx
    xchg [ebp], ecx
    xchg [esi], ecx
    xchg [edi], ecx
    xchg [eax], edx
    xchg [ebx], edx
    xchg [ecx], edx
    xchg [edx], edx
    xchg [esp], edx
    xchg [ebp], edx
    xchg [esi], edx
    xchg [edi], edx
    xchg [eax], esp
    xchg [ebx], esp
    xchg [ecx], esp
    xchg [edx], esp
    xchg [esp], esp
    xchg [ebp], esp
    xchg [esi], esp
    xchg [edi], esp
    xchg [eax], ebp
    xchg [ebx], ebp
    xchg [ecx], ebp
    xchg [edx], ebp
    xchg [esp], ebp
    xchg [ebp], ebp
    xchg [esi], ebp
    xchg [edi], ebp
    xchg [eax], esi
    xchg [ebx], esi
    xchg [ecx], esi
    xchg [edx], esi
    xchg [esp], esi
    xchg [ebp], esi
    xchg [esi], esi
    xchg [edi], esi
    xchg [eax], edi
    xchg [ebx], edi
    xchg [ecx], edi
    xchg [edx], edi
    xchg [esp], edi
    xchg [ebp], edi
    xchg [esi], edi
    xchg [edi], edi

    nop
    xchg ebx, eax
    xchg ecx, eax
    xchg edx, eax
    xchg esp, eax
    xchg ebp, eax
    xchg esi, eax
    xchg edi, eax
    xchg eax, ebx
    xchg ebx, ebx
    xchg ecx, ebx
    xchg edx, ebx
    xchg esp, ebx
    xchg ebp, ebx
    xchg esi, ebx
    xchg edi, ebx
    xchg eax, ecx
    xchg ebx, ecx
    xchg ecx, ecx
    xchg edx, ecx
    xchg esp, ecx
    xchg ebp, ecx
    xchg esi, ecx
    xchg edi, ecx
    xchg eax, edx
    xchg ebx, edx
    xchg ecx, edx
    xchg edx, edx
    xchg esp, edx
    xchg ebp, edx
    xchg esi, edx
    xchg edi, edx
    xchg eax, esp
    xchg ebx, esp
    xchg ecx, esp
    xchg edx, esp
    xchg esp, esp
    xchg ebp, esp
    xchg esi, esp
    xchg edi, esp
    xchg eax, ebp
    xchg ebx, ebp
    xchg ecx, ebp
    xchg edx, ebp
    xchg esp, ebp
    xchg ebp, ebp
    xchg esi, ebp
    xchg edi, ebp
    xchg eax, esi
    xchg ebx, esi
    xchg ecx, esi
    xchg edx, esi
    xchg esp, esi
    xchg ebp, esi
    xchg esi, esi
    xchg edi, esi
    xchg eax, edi
    xchg ebx, edi
    xchg ecx, edi
    xchg edx, edi
    xchg esp, edi
    xchg ebp, edi
    xchg esi, edi
    xchg edi, edi

    ; XOR
    lock xor [1011], eax
    lock xor [1011], ebx
    lock xor [1011], ecx
    lock xor [1011], edx
    lock xor [1011], esp
    lock xor [1011], ebp
    lock xor [1011], esi
    lock xor [1011], edi

    lock xor [1011], ax
    lock xor [1011], bx
    lock xor [1011], cx
    lock xor [1011], dx
    lock xor [1011], sp
    lock xor [1011], bp
    lock xor [1011], si
    lock xor [1011], di

    lock xor byte [11], 10
    lock xor word [11], 10433
    lock xor dword [11], 10433

    lock xor byte [eax], 1
    lock xor byte [ebx], 2
    lock xor byte [ecx], 3
    lock xor byte [edx], 4
    lock xor byte [esp], 5
    lock xor byte [ebp], 6
    lock xor byte [esi], 7
    lock xor byte [edi], 8

    lock xor word [eax], 11234
    lock xor word [ebx], 21234
    lock xor word [ecx], 31234
    lock xor word [edx], 41234
    lock xor word [esp], 51234
    lock xor word [ebp], 61234
    lock xor word [esi], 17234
    lock xor word [edi], 18234

    lock xor dword [eax], 1011234
    lock xor dword [ebx], 1021234
    lock xor dword [ecx], 1031234
    lock xor dword [edx], 1041234
    lock xor dword [esp], 1051234
    lock xor dword [ebp], 1061234
    lock xor dword [esi], 1071234
    lock xor dword [edi], 1081234

    lock xor [eax], eax
    lock xor [eax], ebx
    lock xor [eax], ecx
    lock xor [eax], edx
    lock xor [eax], esp
    lock xor [eax], ebp
    lock xor [eax], esi
    lock xor [eax], edi
    lock xor [ebx], eax
    lock xor [ebx], ebx
    lock xor [ebx], ecx
    lock xor [ebx], edx
    lock xor [ebx], esp
    lock xor [ebx], ebp
    lock xor [ebx], esi
    lock xor [ebx], edi
    lock xor [ecx], eax
    lock xor [ecx], ebx
    lock xor [ecx], ecx
    lock xor [ecx], edx
    lock xor [ecx], esp
    lock xor [ecx], ebp
    lock xor [ecx], esi
    lock xor [ecx], edi
    lock xor [edx], eax
    lock xor [edx], ebx
    lock xor [edx], ecx
    lock xor [edx], edx
    lock xor [edx], esp
    lock xor [edx], ebp
    lock xor [edx], esi
    lock xor [edx], edi
    lock xor [esp], eax
    lock xor [esp], ebx
    lock xor [esp], ecx
    lock xor [esp], edx
    lock xor [esp], esp
    lock xor [esp], ebp
    lock xor [esp], esi
    lock xor [esp], edi
    lock xor [ebp], eax
    lock xor [ebp], ebx
    lock xor [ebp], ecx
    lock xor [ebp], edx
    lock xor [ebp], esp
    lock xor [ebp], ebp
    lock xor [ebp], esi
    lock xor [ebp], edi
    lock xor [esi], eax
    lock xor [esi], ebx
    lock xor [esi], ecx
    lock xor [esi], edx
    lock xor [esi], esp
    lock xor [esi], ebp
    lock xor [esi], esi
    lock xor [esi], edi
    lock xor [edi], eax
    lock xor [edi], ebx
    lock xor [edi], ecx
    lock xor [edi], edx
    lock xor [edi], esp
    lock xor [edi], ebp
    lock xor [edi], esi
    lock xor [edi], edi

    lock xor [eax], ax
    lock xor [eax], bx
    lock xor [eax], cx
    lock xor [eax], dx
    lock xor [eax], sp
    lock xor [eax], bp
    lock xor [eax], si
    lock xor [eax], di
    lock xor [ebx], ax
    lock xor [ebx], bx
    lock xor [ebx], cx
    lock xor [ebx], dx
    lock xor [ebx], sp
    lock xor [ebx], bp
    lock xor [ebx], si
    lock xor [ebx], di
    lock xor [ecx], ax
    lock xor [ecx], bx
    lock xor [ecx], cx
    lock xor [ecx], dx
    lock xor [ecx], sp
    lock xor [ecx], bp
    lock xor [ecx], si
    lock xor [ecx], di
    lock xor [edx], ax
    lock xor [edx], bx
    lock xor [edx], cx
    lock xor [edx], dx
    lock xor [edx], sp
    lock xor [edx], bp
    lock xor [edx], si
    lock xor [edx], di
    lock xor [esp], ax
    lock xor [esp], bx
    lock xor [esp], cx
    lock xor [esp], dx
    lock xor [esp], sp
    lock xor [esp], bp
    lock xor [esp], si
    lock xor [esp], di
    lock xor [ebp], ax
    lock xor [ebp], bx
    lock xor [ebp], cx
    lock xor [ebp], dx
    lock xor [ebp], sp
    lock xor [ebp], bp
    lock xor [ebp], si
    lock xor [ebp], di
    lock xor [esi], ax
    lock xor [esi], bx
    lock xor [esi], cx
    lock xor [esi], dx
    lock xor [esi], sp
    lock xor [esi], bp
    lock xor [esi], si
    lock xor [esi], di
    lock xor [edi], ax
    lock xor [edi], bx
    lock xor [edi], cx
    lock xor [edi], dx
    lock xor [edi], sp
    lock xor [edi], bp
    lock xor [edi], si
    lock xor [edi], di

    lock xor [eax], al
    lock xor [eax], ah
    lock xor [eax], bl
    lock xor [eax], bh
    lock xor [eax], cl
    lock xor [eax], ch
    lock xor [eax], dl
    lock xor [eax], dh
    lock xor [ebx], al
    lock xor [ebx], ah
    lock xor [ebx], bl
    lock xor [ebx], bh
    lock xor [ebx], cl
    lock xor [ebx], ch
    lock xor [ebx], dl
    lock xor [ebx], dh
    lock xor [ecx], al
    lock xor [ecx], ah
    lock xor [ecx], bl
    lock xor [ecx], bh
    lock xor [ecx], cl
    lock xor [ecx], ch
    lock xor [ecx], dl
    lock xor [ecx], dh
    lock xor [edx], al
    lock xor [edx], ah
    lock xor [edx], bl
    lock xor [edx], bh
    lock xor [edx], cl
    lock xor [edx], ch
    lock xor [edx], dl
    lock xor [edx], dh
    lock xor [esp], al
    lock xor [esp], ah
    lock xor [esp], bl
    lock xor [esp], bh
    lock xor [esp], cl
    lock xor [esp], ch
    lock xor [esp], dl
    lock xor [esp], dh
    lock xor [ebp], al
    lock xor [ebp], ah
    lock xor [ebp], bl
    lock xor [ebp], bh
    lock xor [ebp], cl
    lock xor [ebp], ch
    lock xor [ebp], dl
    lock xor [ebp], dh
    lock xor [esi], al
    lock xor [esi], ah
    lock xor [esi], bl
    lock xor [esi], bh
    lock xor [esi], cl
    lock xor [esi], ch
    lock xor [esi], dl
    lock xor [esi], dh
    lock xor [edi], al
    lock xor [edi], ah
    lock xor [edi], bl
    lock xor [edi], bh
    lock xor [edi], cl
    lock xor [edi], ch
    lock xor [edi], dl
    lock xor [edi], dh

    xor [1011], eax
    xor [1011], ebx
    xor [1011], ecx
    xor [1011], edx
    xor [1011], esp
    xor [1011], ebp
    xor [1011], esi
    xor [1011], edi

    xor [1011], ax
    xor [1011], bx
    xor [1011], cx
    xor [1011], dx
    xor [1011], sp
    xor [1011], bp
    xor [1011], si
    xor [1011], di

    xor byte [11], 10
    xor word [11], 10433
    xor dword [11], 10433

    xor byte [eax], 1
    xor byte [ebx], 2
    xor byte [ecx], 3
    xor byte [edx], 4
    xor byte [esp], 5
    xor byte [ebp], 6
    xor byte [esi], 7
    xor byte [edi], 8

    xor word [eax], 11234
    xor word [ebx], 21234
    xor word [ecx], 31234
    xor word [edx], 41234
    xor word [esp], 51234
    xor word [ebp], 61234
    xor word [esi], 17234
    xor word [edi], 18234

    xor dword [eax], 1011234
    xor dword [ebx], 1021234
    xor dword [ecx], 1031234
    xor dword [edx], 1041234
    xor dword [esp], 1051234
    xor dword [ebp], 1061234
    xor dword [esi], 1071234
    xor dword [edi], 1081234

    xor [eax], eax
    xor [eax], ebx
    xor [eax], ecx
    xor [eax], edx
    xor [eax], esp
    xor [eax], ebp
    xor [eax], esi
    xor [eax], edi
    xor [ebx], eax
    xor [ebx], ebx
    xor [ebx], ecx
    xor [ebx], edx
    xor [ebx], esp
    xor [ebx], ebp
    xor [ebx], esi
    xor [ebx], edi
    xor [ecx], eax
    xor [ecx], ebx
    xor [ecx], ecx
    xor [ecx], edx
    xor [ecx], esp
    xor [ecx], ebp
    xor [ecx], esi
    xor [ecx], edi
    xor [edx], eax
    xor [edx], ebx
    xor [edx], ecx
    xor [edx], edx
    xor [edx], esp
    xor [edx], ebp
    xor [edx], esi
    xor [edx], edi
    xor [esp], eax
    xor [esp], ebx
    xor [esp], ecx
    xor [esp], edx
    xor [esp], esp
    xor [esp], ebp
    xor [esp], esi
    xor [esp], edi
    xor [ebp], eax
    xor [ebp], ebx
    xor [ebp], ecx
    xor [ebp], edx
    xor [ebp], esp
    xor [ebp], ebp
    xor [ebp], esi
    xor [ebp], edi
    xor [esi], eax
    xor [esi], ebx
    xor [esi], ecx
    xor [esi], edx
    xor [esi], esp
    xor [esi], ebp
    xor [esi], esi
    xor [esi], edi
    xor [edi], eax
    xor [edi], ebx
    xor [edi], ecx
    xor [edi], edx
    xor [edi], esp
    xor [edi], ebp
    xor [edi], esi
    xor [edi], edi

    xor eax, eax
    xor eax, ebx
    xor eax, ecx
    xor eax, edx
    xor eax, esp
    xor eax, ebp
    xor eax, esi
    xor eax, edi
    xor ebx, eax
    xor ebx, ebx
    xor ebx, ecx
    xor ebx, edx
    xor ebx, esp
    xor ebx, ebp
    xor ebx, esi
    xor ebx, edi
    xor ecx, eax
    xor ecx, ebx
    xor ecx, ecx
    xor ecx, edx
    xor ecx, esp
    xor ecx, ebp
    xor ecx, esi
    xor ecx, edi
    xor edx, eax
    xor edx, ebx
    xor edx, ecx
    xor edx, edx
    xor edx, esp
    xor edx, ebp
    xor edx, esi
    xor edx, edi
    xor esp, eax
    xor esp, ebx
    xor esp, ecx
    xor esp, edx
    xor esp, esp
    xor esp, ebp
    xor esp, esi
    xor esp, edi
    xor ebp, eax
    xor ebp, ebx
    xor ebp, ecx
    xor ebp, edx
    xor ebp, esp
    xor ebp, ebp
    xor ebp, esi
    xor ebp, edi
    xor esi, eax
    xor esi, ebx
    xor esi, ecx
    xor esi, edx
    xor esi, esp
    xor esi, ebp
    xor esi, esi
    xor esi, edi
    xor edi, eax
    xor edi, ebx
    xor edi, ecx
    xor edi, edx
    xor edi, esp
    xor edi, ebp
    xor edi, esi
    xor edi, edi


